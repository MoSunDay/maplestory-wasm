// 召唤兽 owner 离场双客户端 e2e（summon-owner-leave 修复的真实浏览器验收）：
//   1) 静止傀儡：A(BM) 施放 Puppet(3111002) → B 观察 count=1/skill 匹配；A 登出 →
//      B 侧 ms_summon_count 归零（客户端 RemoveSummonHandler 不再因 GONE owner
//      早退而残留幽灵——服务端本来就会广播 REMOVE_SPECIAL_MAPOBJECT）。
//   2) 跟随召唤：A 重登施放 Phoenix(3121006) → B count=1；A 登出 → B count=0
//      （依赖服务端补丁 0022 对非静止召唤补广播 removeSummon）。
//   3) 无未注册 opcode：B 控制台不得出现 "Opcode: 182/183"（MOVE/REMOVE_DRAGON
//      已注册；本图无 Evan 龙时同样不得刷 unhandled 警告）且无浏览器异常。
// 技能与键位经 DB 幂等注入（J=Puppet、K=Phoenix，Keytable 36/37）。
// Run: CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/summon-owner-leave-flow.mjs
import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';
import { logoutToLogin, waitForAccountLoggedOut } from './logout.mjs';
import { loginAndEnterGame } from './pq-harness-login.mjs';
import { openDb, resetLogin, characterId, mysqlRun } from './remote-pet-db.mjs';

// ---- 环境与账号（默认复用 scripts/test_accounts 共享测试号，env 可覆盖） ----
const url = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPortBase = Number(process.env.E2E_DEBUG_PORT || 9281);
const artifactDir = process.env.E2E_ARTIFACT_DIR || '/tmp/summon-owner-leave-flow';
const primary = { // A：召唤方（Bowmaster，Puppet/Phoenix 均在 312 职业线内）
  account: process.env.PRIMARY_ACCOUNT || 'test2',
  password: process.env.PRIMARY_PASSWORD || 'test2',
  character: process.env.PRIMARY_CHARACTER || 'test2BM',
  card: Number(process.env.PRIMARY_CARD || 2)
};
const secondary = { // B：观察方（与 A 同图）
  account: process.env.SECONDARY_ACCOUNT || 'test1',
  password: process.env.SECONDARY_PASSWORD || 'test1',
  character: process.env.SECONDARY_CHARACTER || 'test1Hero',
  card: Number(process.env.SECONDARY_CARD || 0)
};

const MAP_ID = 40001, TOTAL = 3;
const PUPPET = 3111002, PHOENIX = 3121006;
// Keytable 下标（src/client/IO/Keyboard.cpp）：36=J、37=K。
const KEY_J = { key: 'j', code: 'KeyJ', vk: 74, index: 36 };
const KEY_K = { key: 'k', code: 'KeyK', vk: 75, index: 37 };

let failed = false, passed = 0;
let driverA = null, driverB = null, bId = 0;

// ---- 基础 helper（remote-pet-desync-flow.mjs 同款） ----
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
function probe(driver, name, args = [], argTypes = args.map(() => 'number')) {
  return driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', ` +
    `${JSON.stringify(argTypes)}, ${JSON.stringify(args)})`);
}
async function waitFor(label, predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) { console.log(`PASS  ${label}`); return true; }
    await sleep(250);
  }
  throw new Error(`Timed out: ${label}`);
}
async function pollFor(predicate, seconds) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return await predicate();
}
const uiState = async driver => { try { return await probe(driver, 'msui_state'); } catch { return -1; } };
const summonCount = async driver => { try { return await probe(driver, 'ms_summon_count'); } catch { return -1; } };
const summonSkillAt = async (driver, index) => {
  try { return await probe(driver, 'ms_summon_skill_at', [index]); } catch { return -1; }
};

async function bootBrowser(port, tag) {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort: port, artifactDir,
    profileDir: path.join(artifactDir, `chrome-${tag}`) });
  await driver.start();
  await driver.navigate(url);
  return driver;
}
const enterGame = (driver, credentials) => loginAndEnterGame(driver, {
  account: credentials.account, password: credentials.password,
  cardIndex: credentials.card, pageUrl: url
});
const fullyInGame = async driver =>
  await uiState(driver) === 5 && await probe(driver, 'msmap_id') === MAP_ID;

async function dumpFailure(tag) {
  try {
    for (const [label, driver] of [['a', driverA], ['b', driverB]])
      if (driver) await driver.screenshot(`fail-${tag}-${label}`).catch(() => {});
  } catch { /* 截图尽力而为 */ }
}
async function runScenario(name, body) {
  try {
    const ok = await body();
    if (ok) { passed += 1; console.log(`PASS  scenario ${name}`); }
    else console.log(`FAIL  scenario ${name}`);
  } catch (error) {
    console.error(`FAIL  scenario ${name}: ${error.message}`);
    await dumpFailure(name);
  }
}

// ---- DB 幂等注入：技能 + 键位 + 双号落图/登出复位 ----
function prepareDatabase(db, aCharId, bCharId) {
  mysqlRun(db, `UPDATE characters SET map=${MAP_ID}, spawnpoint=0 WHERE id IN (${aCharId}, ${bCharId});`);
  for (const skill of [PUPPET, PHOENIX]) {
    mysqlRun(db, `DELETE FROM skills WHERE characterid=${aCharId} AND skillid=${skill};`);
    mysqlRun(db, `INSERT INTO skills (characterid, skillid, skilllevel, masterlevel) ` +
      `VALUES (${aCharId}, ${skill}, 30, 30);`);
  }
  for (const binding of [[KEY_J.index, PUPPET], [KEY_K.index, PHOENIX]]) {
    mysqlRun(db, `DELETE FROM keymap WHERE characterid=${aCharId} AND \`key\`=${binding[0]};`);
    mysqlRun(db, `INSERT INTO keymap (characterid, \`key\`, type, action) ` +
      `VALUES (${aCharId}, ${binding[0]}, 1, ${binding[1]});`);
  }
  resetLogin(db, [primary.account, secondary.account]);
  report(true, `DB: skills ${PUPPET}/${PHOENIX} lvl30 + keymap J/K bound for ${primary.character} (${aCharId})`);
}

// A 侧按键施放：先点画布夺焦点，按键后以 B 侧观测为准（A 侧 UI 不参与断言）。
async function castSummon(driver, binding, label) {
  await driver.click(400, 300);
  await sleep(300);
  await driver.key(binding.key, binding.code, binding.vk);
  await sleep(1200);
  console.log(`INFO  ${label} cast (key ${binding.code})`);
}

async function ownerLeavesAndObserverSettles(label, expectSkill) {
  await logoutToLogin(driverA, { log: message => console.log(`INFO  [logout-a] ${message}`) });
  const freed = await waitForAccountLoggedOut(primary.account, { timeoutMs: 15000 });
  if (!freed.ok) throw new Error(`A account still loggedin=${freed.lastValue} in DB`);
  // 核心断言：owner 离图（REMOVE_CHAR 先到、owner GONE）后，服务端的
  // REMOVE_SPECIAL_MAPOBJECT 必须把 B 侧实体清零，不得残留幽灵。
  const cleared = await waitFor(`${label}: B summon count settled to 0 after owner left`,
    async () => await summonCount(driverB) === 0, 20);
  await driverB.screenshot(`${label}-after-leave`);
  const skillBefore = expectSkill;
  return cleared && skillBefore > 0;
}

// ---- 场景 1+2：静止傀儡与跟随召唤的 owner 离场收敛 ----
async function scenarioOwnerLeave(summoner, phase) {
  await castSummon(driverA, summoner.binding, summoner.label);
  const seen = await waitFor(`${summoner.label}: B sees exactly one summon`,
    async () => await summonCount(driverB) === 1 &&
      await summonSkillAt(driverB, 0) === summoner.skill, 20);
  const skillAt = await summonSkillAt(driverB, 0);
  report(seen && skillAt === summoner.skill,
    `B observed ${summoner.label} (count=1, skill=${skillAt}, expect ${summoner.skill})`);
  await driverB.screenshot(`0${phase}-${summoner.label}-visible`);
  return await ownerLeavesAndObserverSettles(summoner.label, skillAt);
}

// ---- 场景 3：无 unhandled 182/183、无浏览器异常 ----
async function scenarioNoUnhandledOpcodes() {
  const dragonWarns = (driverB.logs || [])
    .filter(line => /Opcode:\s*(182|183)/.test(line));
  const aWarns = (driverA.logs || [])
    .filter(line => /Opcode:\s*(182|183)/.test(line));
  report(dragonWarns.length === 0, `B console free of Opcode 182/183 warns (hits=${dragonWarns.length})`);
  report(aWarns.length === 0, `A console free of Opcode 182/183 warns (hits=${aWarns.length})`);
  const errors = [...(driverB.errors || []), ...(driverA.errors || [])];
  report(errors.length === 0, `no browser exceptions (errors=${errors.length})`);
  return dragonWarns.length === 0 && aWarns.length === 0 && errors.length === 0;
}

async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const db = openDb();
  const aCharId = characterId(db, primary.account, primary.character);
  const bCharId = characterId(db, secondary.account, secondary.character);
  if (!Number.isInteger(aCharId) || !Number.isInteger(bCharId))
    throw new Error(`character ids unresolved (A=${aCharId}, B=${bCharId})`);
  prepareDatabase(db, aCharId, bCharId);

  // B 先进图占位，再让 A 进图（A 进图广播 SPAWN 即刻到达 B）。
  driverB = await bootBrowser(debugPortBase + 1, 'b');
  await enterGame(driverB, secondary);
  bId = await probe(driverB, 'msplayer_id');
  report(bId === bCharId, `B player id ${bId} == DB characters.id ${bCharId}`);
  await waitFor(`B on map ${MAP_ID}`, async () => await probe(driverB, 'msmap_id') === MAP_ID, 45);
  await waitFor('B baseline summon count 0', async () => await summonCount(driverB) === 0, 10);

  driverA = await bootBrowser(debugPortBase, 'a');
  await enterGame(driverA, primary);
  await waitFor(`A on map ${MAP_ID}`, async () => await probe(driverA, 'msmap_id') === MAP_ID, 45);
  await sleep(1500); // 等 SPAWN_CHAR/外围封包静置

  await runScenario('puppet-stationary-leave', () =>
    scenarioOwnerLeave({ label: 'puppet', skill: PUPPET, binding: KEY_J }, 1));

  // A 重登（上一轮已登出到登录页），随后跟随召唤轮。
  if (await uiState(driverA) !== 5) {
    await enterGame(driverA, primary);
    await waitFor(`A back on map ${MAP_ID}`, async () => await probe(driverA, 'msmap_id') === MAP_ID, 45);
    await sleep(1500);
  }
  await runScenario('phoenix-follow-leave', () =>
    scenarioOwnerLeave({ label: 'phoenix', skill: PHOENIX, binding: KEY_K }, 2));

  await runScenario('no-unhandled-dragon-opcodes', scenarioNoUnhandledOpcodes);
}
main()
  .catch(async error => { failed = true; console.error(`FAIL  ${error.message}`); await dumpFailure('fatal'); })
  .finally(async () => {
    for (const [tag, driver] of [['a', driverA], ['b', driverB]]) {
      if (!driver) continue;
      if (driver.errors?.length)
        fs.writeFileSync(path.join(artifactDir, `${tag}-browser-errors.log`), `${driver.errors.join('\n')}\n`);
      fs.writeFileSync(path.join(artifactDir, `${tag}-browser-console.log`), `${driver.logs.join('\n')}\n`);
      await driver.stop();
    }
    try { resetLogin(openDb(), [primary.account, secondary.account]); } catch (error) { console.log(`WARN  收尾 reset_login 失败: ${error.message}`); }
    console.log(`INFO  产物目录 ${artifactDir}`);
    console.log(`SUMMARY summon-owner-leave-flow ${passed}/${TOTAL} scenarios OK`);
    process.exitCode = failed ? 1 : 0;
  });
