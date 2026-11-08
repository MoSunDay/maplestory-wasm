// 远端宠物不同步双客户端 e2e（remote-pet-desync-source-contract.mjs 的真实浏览器验收）：
//   1) 基线：A 召唤宠物 → B 进图可见（数量/itemid/坐标探针）；
//   2) 召唤竞态：A 重登后立刻 召唤→收回→再召唤（~300ms 间隔），B 侧 10s 收敛且两拍稳定，3 轮取多数；
//   3) 幽灵竞态：A/B 并行重登、A 进图立即再登出（与 B 地图加载竞速），B 侧 250ms×5s 采样
//      mschar_visible(A)==0 且 mschar_pet_count(A)==-1（无幽灵角色/幽灵宠物）；
//   4) 回归：A 回图后跑 runCoopSync（行走/表情/攻击）+ 无浏览器异常。
// 复用 cdp/logout/pq-harness-login/coop-sync 模块；DB 注入与 CASH 召唤复刻 cash-shop-pet-flow。
// Run: CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/remote-pet-desync-flow.mjs
import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';
import { logoutToLogin, waitForAccountLoggedOut } from './logout.mjs';
import { loginAndEnterGame } from './pq-harness-login.mjs';
import { runCoopSync } from './coop-sync.mjs';
import { openDb, resetLogin, characterId, injectPet, mysqlRun, PET_ITEM } from './remote-pet-db.mjs';

// ---- 环境与账号（默认复用 scripts/test_accounts 的共享测试号，env 可覆盖） ----
const url = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPortBase = Number(process.env.E2E_DEBUG_PORT || 9261); // A=base, B=base+1
const artifactDir = process.env.E2E_ARTIFACT_DIR || '/tmp/remote-pet-desync-flow';
const primary = {
  account: process.env.PRIMARY_ACCOUNT || 'test1',
  password: process.env.PRIMARY_PASSWORD || 'test1',
  character: process.env.PRIMARY_CHARACTER || 'test1Hero',
  card: Number(process.env.PRIMARY_CARD || 0)
};
const secondary = {
  account: process.env.SECONDARY_ACCOUNT || 'test2',
  password: process.env.SECONDARY_PASSWORD || 'test2',
  character: process.env.SECONDARY_CHARACTER || 'test2ILA',
  card: Number(process.env.SECONDARY_CARD || 0)
};

const MAP_ID = 40001, MISSING = -32768, TOTAL = 4;

let failed = false, passed = 0;
let driverA = null, driverB = null, aId = 0, bId = 0;

// ---- 基础 helper（social-flow.mjs / cash-shop-pet-flow.mjs 同款） ----
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
// 竞态轮内的软断言：失败只记 WARN 计入该轮失败，不影响多数裁决。
const note = (ok, message) => console.log(`${ok ? 'PASS' : 'WARN'}  ${message}`);
function probe(driver, name, args = [], argTypes = args.map(() => 'number')) {
  return driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', ` +
    `${JSON.stringify(argTypes)}, ${JSON.stringify(args)})`);
}
async function waitFor(label, predicate, seconds = 45) { // runCoopSync 依赖该签名
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) { console.log(`PASS  ${label}`); return true; }
    await sleep(250);
  }
  throw new Error(`Timed out: ${label}`);
}
async function pollFor(predicate, seconds) { // 不抛错轮询，供软等待/竞态采样
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return await predicate();
}
async function dumpFailure(label) {
  for (const [tag, driver] of [['a', driverA], ['b', driverB]]) {
    if (!driver) continue;
    try { await driver.screenshot(`fail-${label}-${tag}`); } catch (error) {}
    if (driver.errors?.length) console.log(`ERROR ${tag} 浏览器异常:\n${driver.errors.join('\n')}`);
  }
}

// ---- 双浏览器启动（social-flow.mjs boot 模式；profileDir 按客户端分开避免档案互撞） ----
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

// ---- 状态恢复（任意 UI 态拉回游戏）与失败轮隔离（整浏览器重启防跨轮传染） ----
const uiStateSafe = async driver => {
  try { return await probe(driver, 'msui_state'); } catch { return -1; }
};
async function restartDriver(tag, credentials) {
  const current = tag === 'a' ? driverA : driverB;
  const port = tag === 'a' ? debugPortBase : debugPortBase + 1;
  try { await current?.stop(); } catch { /* 已死则忽略 */ }
  const driver = await bootBrowser(port, tag);
  if (tag === 'a') driverA = driver; else driverB = driver;
  await ensureInGame(driver, credentials);
}

// game 态还须地图就位：快速重登偶发"半进图"（UI=5 但 Stage 未建立、按键全无响应）
const fullyInGame = async driver =>
  await pollFor(async () => await uiStateSafe(driver) === 5 &&
    await probe(driver, 'msmap_id') === MAP_ID, 5);
async function ensureInGame(driver, credentials) {
  if (await fullyInGame(driver)) return true;
  if (await uiStateSafe(driver) === 1) {
    await enterGame(driver, credentials);
    return await fullyInGame(driver);
  }
  // world(2)→characters(3)：世界确认后按卡位双击（social-flow enterCharacter 同款）
  await driver.retryClick(510, 216,
    async () => await uiStateSafe(driver) === 3, 4, 1000).catch(() => {});
  const x = 105 + 120 * credentials.card + 25;
  for (let click = 0; click < 4 && await uiStateSafe(driver) === 3; click += 1) {
    await driver.doubleClick(x, 210);
    if (await fullyInGame(driver)) return true;
  }
  await driver.navigate(url); // 兜底：整页重载后走完整登录
  await pollFor(async () => await uiStateSafe(driver) === 1, 90);
  await enterGame(driver, credentials);
  return await fullyInGame(driver);
}

// ---- 物品栏/宠物 UI（cash-shop-pet-flow.mjs + cash-cover-look-flow.mjs 同款） ----
const petSummoned = async driver =>
  await probe(driver, 'mspet_active') === 1 && await probe(driver, 'mspet_item_id') === PET_ITEM;
async function openItemInventory(driver) {
  // 首个键入可能被公告弹窗/输入焦点吞掉：点画布中心清焦点后再按 I
  await driver.click(400, 300);
  await sleep(400);
  for (let attempt = 0; attempt < 5 && await probe(driver, 'msiteminventory_active') !== 1; attempt += 1) {
    await driver.key('i', 'KeyI', 73, 'i'); // I 键开物品栏
    await sleep(900);
  }
  if (await probe(driver, 'msiteminventory_active') === 1) return true;
  for (const [x, y] of [[711, 574], [537, 544]]) { // 状态栏物品菜单兜底（两代 HUD）
    if (await probe(driver, 'msitems_menu_x') < 0) { await driver.click(x, y); await sleep(500); }
    const menuX = await probe(driver, 'msitems_menu_x'), menuY = await probe(driver, 'msitems_menu_y');
    if (menuX < 0 || menuY < 0) continue;
    await driver.click(menuX, menuY);
    if (await pollFor(async () => await probe(driver, 'msiteminventory_active') === 1, 5)) return true;
  }
  return false;
}
async function closeItemInventory(driver) {
  for (let attempt = 0; attempt < 3 && await probe(driver, 'msiteminventory_active') === 1; attempt += 1) {
    await driver.key('i', 'KeyI', 73, 'i');
    await sleep(400);
  }
  return await probe(driver, 'msiteminventory_active') === 0;
}
async function switchToCashTab(driver) {
  const tabX = await probe(driver, 'msiteminventory_tab_x', [PET_ITEM]);
  const tabY = await probe(driver, 'msiteminventory_tab_y', [PET_ITEM]);
  const tabReady = async () => await probe(driver, 'msiteminventory_tab') === 5;
  if (tabX > 0 && tabY > 0) await driver.retryClick(tabX, tabY, tabReady, 4, 1000);
  else { // 旧构建兜底：CASH 是第 5 个标签，32px 间距（cash-shop-pet-flow 同款坐标）
    const originX = await probe(driver, 'msiteminventory_x');
    const originY = await probe(driver, 'msiteminventory_y');
    await driver.retryClick(originX + 9 + 32 * 4 + 13, originY + 24 + 8, tabReady, 4, 1000);
  }
  report(await probe(driver, 'msiteminventory_tab') === 5,
    `CASH tab active (tab=${await probe(driver, 'msiteminventory_tab')})`);
}
async function findCashPetSlot(driver) {
  for (let slot = 1; slot <= 24; slot += 1)
    if (await probe(driver, 'msinv_slot_item', [5, slot]) === PET_ITEM) return slot;
  return 0;
}
// 格心坐标：窗口原点 + 4 列 36px / 行距 35、34 两代网格候选，msiteminventory_slot_at 命中即用。
async function resolveCashSlotPoint(driver, slot, first) {
  const originX = await probe(driver, 'msiteminventory_x');
  const originY = await probe(driver, 'msiteminventory_y');
  const column = (slot - first) % 4, row = Math.floor((slot - first) / 4);
  for (const [dx, dy, pitchY] of [[29, 68, 35], [26, 67, 34]]) {
    const x = originX + dx + column * 36;
    const y = originY + dy + row * pitchY;
    if (await probe(driver, 'msiteminventory_slot_at', [x, y]) === slot) return { x, y };
  }
  throw new Error(`cash slot ${slot} hit test failed (window ${originX},${originY}, first=${first})`);
}
async function doubleClickUntil(driver, point, isDone, tries = 4) {
  if (await isDone()) return true; // 已达态（如登出后宠物本就收回）直接跳过
  for (let attempt = 0; attempt < tries; attempt += 1) {
    await driver.doubleClick(point.x, point.y);
    if (await pollFor(isDone, 3)) return true;
  }
  return await isDone();
}
// 打开物品栏 → CASH 页 → 解出宠物槽格心（窗口保持打开供连续双击）。
// assert 传 note 时用于竞态轮内软断言（多数裁决不放大单轮 UI 噪声）。
async function openCashPetSlot(driver, assert = report) {
  if (!await openItemInventory(driver)) throw new Error('item inventory did not open');
  await switchToCashTab(driver, assert);
  const slot = await findCashPetSlot(driver);
  if (slot < 1) throw new Error(`pet ${PET_ITEM} missing in CASH tab`);
  const first = await probe(driver, 'msiteminventory_first_slot');
  const last = await probe(driver, 'msiteminventory_last_slot');
  assert(slot >= first && slot <= last, `pet slot ${slot} on visible CASH page [${first}..${last}]`);
  return resolveCashSlotPoint(driver, slot, first);
}
async function summonViaUI(driver, label) {
  const point = await openCashPetSlot(driver);
  await doubleClickUntil(driver, point, () => petSummoned(driver));
  await closeItemInventory(driver);
  await waitFor(`${label}: pet summoned via double-click`, () => petSummoned(driver), 10);
}

// ---- 场景 1：基线（A 召唤 → B 进图可见） ----
async function scenarioBase(db, dbPrimaryId, dbSecondaryId) {
  driverA = await bootBrowser(debugPortBase, 'a');
  await enterGame(driverA, primary);
  await waitFor(`A on map ${MAP_ID}`, async () => await probe(driverA, 'msmap_id') === MAP_ID, 30);
  aId = await probe(driverA, 'msplayer_id');
  report(aId === dbPrimaryId, `A player id ${aId} == DB characters.id ${dbPrimaryId}`);
  await summonViaUI(driverA, 'A');
  await driverA.screenshot('01-a-pet-summoned');
  driverB = await bootBrowser(debugPortBase + 1, 'b');
  await enterGame(driverB, secondary);
  bId = await probe(driverB, 'msplayer_id');
  report(bId === dbSecondaryId, `B player id ${bId} == DB characters.id ${dbSecondaryId}`);
  await waitFor(`B on map ${MAP_ID} seeing A`, async () =>
    await probe(driverB, 'msmap_id') === MAP_ID &&
    await probe(driverB, 'mschar_visible', [aId]) === 1, 45);
  const count = await probe(driverB, 'mschar_pet_count', [aId]);
  const remoteItem = await probe(driverB, 'mschar_pet_itemid', [aId, 0]);
  const localItem = await probe(driverA, 'mspet_item_id');
  const petX = await probe(driverB, 'mschar_pet_x', [aId, 0]);
  report(count === 1, `B sees exactly one pet on A (count=${count})`);
  report(remoteItem === localItem && localItem === PET_ITEM,
    `B pet itemid matches A (${remoteItem} == ${localItem})`);
  report(petX !== MISSING, `B pet x resolved (${petX} != ${MISSING})`);
  await driverB.screenshot('02-b-sees-pet');
  return count === 1 && remoteItem === localItem && localItem === PET_ITEM && petX !== MISSING;
}

// ---- 场景 2：登出重登 + 立刻 召唤→收回→再召唤 竞态（3 轮取多数） ----
const bSeesPet = async () => {
  if (await probe(driverB, 'mschar_pet_count', [aId]) !== 1) return false;
  const remoteItem = await probe(driverB, 'mschar_pet_itemid', [aId, 0]);
  return remoteItem === PET_ITEM && remoteItem === await probe(driverA, 'mspet_item_id');
};
async function summonRaceRound(round) {
  await ensureInGame(driverA, primary); // 状态护栏：不在 game 态时先拉回
  await logoutToLogin(driverA, { log: message => console.log(`INFO  [logout-a] ${message}`) });
  const freed = await waitForAccountLoggedOut(primary.account, { timeoutMs: 15000 });
  if (!freed.ok) throw new Error(`A account still loggedin=${freed.lastValue} in DB`);
  await enterGame(driverA, primary); // 进图后不再 settle，立即三连切换
  const point = await openCashPetSlot(driverA, note);
  await doubleClickUntil(driverA, point, () => petSummoned(driverA)); // summon
  await sleep(300);
  await doubleClickUntil(driverA, point,
    async () => await probe(driverA, 'mspet_active') === 0); // desummon（同一格再双击）
  await sleep(300);
  await doubleClickUntil(driverA, point, () => petSummoned(driverA)); // resummon
  await closeItemInventory(driverA);
  note(await petSummoned(driverA), `round ${round}: A pet 1→0→1 settled (itemid=${await probe(driverA, 'mspet_item_id')})`);
  const saw = await pollFor(bSeesPet, 10); // ≤10s 收敛
  await sleep(300);
  const stable = saw && await bSeesPet(); // 连续两拍（间隔 300ms）稳定
  await sleep(300);
  note(saw && stable && await bSeesPet(), `round ${round}: B converged to one matching pet`);
  await driverB.screenshot(`02-summon-race-r${round}`);
  return saw && stable;
}
async function scenarioSummonRace() {
  let ok = 0;
  for (let round = 1; round <= 3; round += 1) {
    try { if (await summonRaceRound(round)) ok += 1; }
    catch (error) { console.log(`WARN  summon round ${round} failed: ${error.message}`); }
  }
  report(ok >= 2, `summon race majority (${ok}/3 rounds)`);
  return ok >= 2;
}

// ---- 场景 3：A/B 并行进图 + A 立刻登出的幽灵竞态（3 轮取多数） ----
async function ghostRaceRound(round) {
  await ensureInGame(driverA, primary); // 上一轮失败可能停在 login/characters
  await ensureInGame(driverB, secondary);
  await logoutToLogin(driverA, { log: message => console.log(`INFO  [logout-a] ${message}`) });
  if (!(await waitForAccountLoggedOut(primary.account, { timeoutMs: 15000 })).ok)
    throw new Error('A account still logged in');
  await logoutToLogin(driverB, { log: message => console.log(`INFO  [logout-b] ${message}`) });
  if (!(await waitForAccountLoggedOut(secondary.account, { timeoutMs: 15000 })).ok)
    throw new Error('B account still logged in');
  await sleep(2500); // 等 channel 收割上一会话的角色对象，降低"半进图"概率
  // 并行：B 进图；A 进图后立即再登出（与 B 的地图加载竞速）。
  await Promise.all([
    enterGame(driverB, secondary),
    (async () => {
      await enterGame(driverA, primary);
      // 刚进图 UI 未解锁时 ESC 菜单打不开（首轮超时根因）：等地图+素材就绪再登出
      await pollFor(async () => await probe(driverA, 'msmap_id') === MAP_ID &&
        await probe(driverA, 'msasset_blocking') === 0, 8);
      await sleep(400);
      await logoutToLogin(driverA, { timeoutMs: 60000, attempts: 4,
        log: message => console.log(`INFO  [logout-a-quick] ${message}`) });
    })()
  ]);
  if (!(await waitForAccountLoggedOut(primary.account, { timeoutMs: 20000 })).ok)
    throw new Error('A account still logged in after ghost race');
  await waitFor('B back in game', async () => await probe(driverB, 'msui_state') === 5, 30);
  let clean = true, samples = 0; // 5s 连续采样：不得出现幽灵角色/幽灵宠物
  const deadline = Date.now() + 5000;
  while (Date.now() < deadline) {
    const visible = await probe(driverB, 'mschar_visible', [aId]);
    const petCount = await probe(driverB, 'mschar_pet_count', [aId]);
    const others = await probe(driverB, 'msother_character_count');
    if (samples % 8 === 0)
      console.log(`INFO  ghost sample r${round}: visible=${visible} pets=${petCount} others=${others}`);
    if (visible !== 0 || petCount !== -1) clean = false;
    samples += 1;
    await sleep(250);
  }
  note(clean, `round ${round}: no ghost char/pet on B across ${samples} samples`);
  await driverB.screenshot(round === 3 ? '03-no-ghost' : `03-no-ghost-r${round}`);
  return clean;
}
async function scenarioGhostRace(db) {
  let ok = 0;
  for (let round = 1; round <= 3; round += 1) {
    try { if (await ghostRaceRound(round)) ok += 1; }
    catch (error) {
      console.log(`WARN  ghost round ${round} failed: ${error.message}`);
      await Promise.allSettled([driverA?.screenshot(`03-fail-r${round}-a`),
        driverB?.screenshot(`03-fail-r${round}-b`)]);
      resetLogin(db, [primary.account, secondary.account]); // 清残留登录位并冷却
      await sleep(8000);
      // 快速重登边界偶发冻结/会话残留（changelog 观察窗）：重启隔离后重试下一轮
      await Promise.allSettled([restartDriver('a', primary), restartDriver('b', secondary)]);
    }
  }
  report(ok >= 2, `ghost race majority (${ok}/3 rounds clean)`);
  return ok >= 2;
}

// ---- 场景 4：A 回图 → 双端协作回归（coop-sync.mjs runCoopSync） ----
async function scenarioCoopRegression(db) {
  await ensureInGame(driverA, primary); // ghost 轮结束态不定，统一恢复
  if (!(await pollFor(async () =>
    await probe(driverA, 'mschar_visible', [bId]) === 1 &&
    await probe(driverB, 'mschar_visible', [aId]) === 1, 30))) {
    console.log('INFO  coop 前互见未恢复，重启双浏览器兜底');
    resetLogin(db, [primary.account, secondary.account]);
    await sleep(8000);
    await Promise.allSettled([restartDriver('a', primary), restartDriver('b', secondary)]);
  }
  await waitFor('mutual visibility restored', async () =>
    await probe(driverA, 'mschar_visible', [bId]) === 1 &&
    await probe(driverB, 'mschar_visible', [aId]) === 1, 60);
  await runCoopSync({ primaryDriver: driverA, secondaryDriver: driverB,
    primaryId: aId, secondaryId: bId, waitFor });
  const clean = !driverA.errors.length && !driverB.errors.length;
  report(clean, `no browser exceptions after coop regression (a=${driverA.errors.length}, b=${driverB.errors.length})`);
  await Promise.all([driverA.screenshot('04-coop-regression'), driverB.screenshot('04-coop-regression-b')]);
  return clean;
}

// ---- 调度：场景失败只在 body 内 report(false)，异常则截图+dump 后计失败 ----
async function runScenario(name, body) {
  try {
    if (await body()) { passed += 1; report(true, `scenario ${name} OK`); }
  } catch (error) {
    report(false, `scenario ${name} aborted: ${error.message}`);
    await dumpFailure(name);
  }
}
async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const db = openDb();
  const dbPrimaryId = characterId(db, primary.account, primary.character);
  const dbSecondaryId = characterId(db, secondary.account, secondary.character);
  if (!Number.isInteger(dbPrimaryId) || !Number.isInteger(dbSecondaryId))
    throw new Error(`character ids unresolved (A=${dbPrimaryId}, B=${dbSecondaryId})`);
  resetLogin(db, [primary.account, secondary.account]);
  const pet = injectPet(db, dbPrimaryId);
  report(true, `DB: pet injected for ${primary.character} (petid=${pet.petId}, cash slot=${pet.slot})`);
  mysqlRun(db, `UPDATE characters SET map=${MAP_ID}, spawnpoint=0 WHERE id IN (${dbPrimaryId}, ${dbSecondaryId});`);
  report(true, `DB: both characters pinned to map ${MAP_ID}`);
  await runScenario('base-visibility', () => scenarioBase(db, dbPrimaryId, dbSecondaryId));
  await runScenario('summon-race', scenarioSummonRace);
  await runScenario('ghost-race', () => scenarioGhostRace(db));
  await runScenario('coop-regression', () => scenarioCoopRegression(db));
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
    console.log(`SUMMARY remote-pet-desync-flow ${passed}/${TOTAL} scenarios OK`);
    process.exitCode = failed ? 1 : 0;
  });
