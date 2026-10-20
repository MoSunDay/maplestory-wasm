// 一次性端到端实测：Hero 斗气 (Combo Attack) → Slash Blast 蓄气 → Coma 消耗。
// 复用 cdp.mjs BrowserDriver 与 runtime-flow.mjs 探针；登录链沿用 skill-anim-flow.mjs。
// DB 预检按 verify_all.sh setup 把 test1Hero 放到 40001，并从 keymap 解析技能键
// （Combo Attack 是 serverBuff，缺失时补一个空闲字母键，report 标 provisioned）。
// 用法: node .../hero-combo-e2e.mjs [--headed] [--port 9320]；产物 /tmp/hero-combo-e2e。
import fs from 'fs';
import path from 'path';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';
import { createRuntimeProbes, resolvePageUrl } from './runtime-flow.mjs';

const BATTLE_MAP = 40001, MISSING = -32768;
const UI = Object.freeze({ login: 1, characters: 3, game: 5 });
const SKILLS = Object.freeze([
  { id: 1001005, name: 'Slash Blast', role: 'slash' },
  { id: 1111002, name: 'Combo Attack', role: 'combo', provisionIfMissing: true },
  { id: 1111005, name: 'Coma: Sword', role: 'finisher' },
  { id: 1111003, name: 'Panic: Sword', role: 'panic', provisionIfMissing: true },
  { id: 1100002, name: 'Combo Attack (passive)', role: 'passive', passive: true }
]);
// Maple 键位索引 -> GLFW 字母键（Keyboard.cpp Keytable / LETTER_KEYS 同源）。
const LETTER_INDEX = Object.freeze({
  Q: 16, W: 17, E: 18, R: 19, T: 20, Y: 21, U: 22, I: 23, O: 24, P: 25,
  A: 30, S: 31, D: 32, F: 33, G: 34, H: 35, J: 36, K: 37, L: 38,
  Z: 44, X: 45, C: 46, V: 47, B: 48, N: 49, M: 50
});
const KEY_EVENT = new Map(Object.entries(LETTER_INDEX).map(([letter, index]) =>
  [index, { key: letter.toLowerCase(), code: `Key${letter}`, vk: letter.charCodeAt(0) }]));
const FREE_KEYS = [35, 36, 37, 38, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 44, 45, 46, 47, 48, 49, 50];

function parseArgs(argv) {
  const parsed = {};
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--headed') parsed.headed = true;
    else if (arg === '--port') parsed.port = Number(argv[++i]);
    else if (arg.startsWith('--port=')) parsed.port = Number(arg.slice(7));
    else if (arg === '--help') parsed.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return parsed;
}

let args;
try { args = parseArgs(process.argv.slice(2)); }
catch (error) { console.log(`FAIL  ${error.message}`); process.exit(64); }
if (args.help) { console.log('usage: node hero-combo-e2e.mjs [--headed] [--port 9320]'); process.exit(0); }
const debugPort = Number(args.port || process.env.E2E_DEBUG_PORT || 9320);
if (!Number.isInteger(debugPort) || debugPort <= 0) {
  console.log(`FAIL  invalid port: ${args.port || process.env.E2E_DEBUG_PORT}`);
  process.exit(1);
}
const account = process.env.E2E_ACCOUNT || 'test1', password = process.env.E2E_PASSWORD || 'test1';
const characterName = process.env.E2E_CHAR || 'test1Hero', cardIndex = Number(process.env.E2E_CARD || 0);
const artifactDir = process.env.E2E_ARTIFACT_DIR || '/tmp/hero-combo-e2e';
const configPath = process.env.MS_SERVER_CONFIG || '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const chromeBinary = process.env.CHROME_BIN || (fs.existsSync('/usr/bin/chromium-browser') ? '/usr/bin/chromium-browser' : '/usr/bin/chromium');
const headed = args.headed === true || process.env.E2E_HEADED === '1';

function readDbConfig(file) {
  const values = Object.fromEntries(fs.readFileSync(file, 'utf8').split(/\r?\n/)
    .filter(line => line.indexOf('=') > 0)
    .map(line => [line.slice(0, line.indexOf('=')).trim(), line.slice(line.indexOf('=') + 1).trim()]));
  const match = /\/\/([^/:]+)(?::(\d+))?\/([^?]+)/.exec(values.URL || '');
  return { host: match?.[1] || '127.0.0.1', port: match?.[2] || '3306',
    database: match?.[3] || 'maplestory', user: values.DB_USER || 'root', password: values.DB_PASS || '' };
}

function mysqlRun(db, sql) {
  return execFileSync('mysql', ['-h', db.host, '-P', db.port, '-u', db.user, db.database,
    '--batch', '--skip-column-names', '-e', sql],
  { encoding: 'utf8', env: { ...process.env, MYSQL_PWD: db.password }, stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}

const sqlString = value => `'${String(value).replace(/'/g, "''")}'`;

// 读角色/键位；只补 Combo Attack，其余目标技能缺键视为 setup 未跑。
function prepareCharacter(db) {
  const row = mysqlRun(db, `SELECT id, map, world FROM characters WHERE name=${sqlString(characterName)} LIMIT 1;`);
  if (!row) throw new Error(`character ${characterName} not found in DB`);
  const [characterId, originalMap, world] = row.split('\t').map(Number);
  mysqlRun(db, `UPDATE accounts SET loggedin=0 WHERE name=${sqlString(account)};`);
  const stored = mysqlRun(db, `SELECT \`key\`, type, action FROM keymap WHERE characterid=${characterId} ORDER BY \`key\`;`);
  const rows = (stored ? stored.split('\n') : []).map(line => {
    const [key, type, action] = line.split('\t').map(Number);
    return { key, type, action };
  });
  const usedKeys = new Set(rows.map(entry => entry.key)), bindings = {}, provisioned = [];
  for (const skill of SKILLS) {
    if (skill.passive) { bindings[skill.role] = { id: skill.id, name: skill.name, source: 'passive' }; continue; }
    let binding = rows.find(entry => entry.type === 1 && entry.action === skill.id), source = 'db';
    if (!binding && skill.provisionIfMissing) {
      const key = FREE_KEYS.find(candidate => !usedKeys.has(candidate));
      if (key === undefined) throw new Error(`no free letter key for skill ${skill.id}`);
      mysqlRun(db, `INSERT INTO keymap (characterid, \`key\`, type, action) VALUES (${characterId}, ${key}, 1, ${skill.id});`);
      binding = { key, type: 1, action: skill.id }; rows.push(binding); usedKeys.add(key);
      provisioned.push({ id: skill.id, name: skill.name, key, keyIndex: key }); source = 'provisioned';
    }
    if (!binding) throw new Error(`DB keymap has no key for ${skill.name} (${skill.id}); run skill-keymap-prep.py`);
    const event = KEY_EVENT.get(binding.key);
    if (!event) throw new Error(`${skill.name} bound to unsupported key index ${binding.key}`);
    bindings[skill.role] = { id: skill.id, name: skill.name, keyIndex: binding.key,
      key: event.key.toUpperCase(), event, source };
  }
  const mapMoved = originalMap !== BATTLE_MAP;
  if (mapMoved) mysqlRun(db, `UPDATE characters SET map=${BATTLE_MAP} WHERE id=${characterId};`);
  return { characterId, originalMap, world, mapMoved, rows, bindings, provisioned };
}

async function waitFor(predicate, seconds = 45) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return await predicate();
}

async function assetsIdle(driver, probes) {
  return await probes.assetBlocking() === 0 && await driver.evaluate(
    "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

// 与 skill-anim-flow.mjs 相同：资产空闲后输入，Tab 切密码框，GO(510,216)。
async function loginAndEnterGame(driver, probes) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90)) {
    throw new Error('WASM module did not reach calledRun within 90s');
  }
  if (!await waitFor(async () => await probes.currentUiState() === UI.login, 30)) throw new Error('login UI did not activate');
  if (!await waitFor(() => assetsIdle(driver, probes), 60)) throw new Error('login assets never idle');
  let advanced = false;
  for (let attempt = 0; attempt < 3 && !advanced; attempt += 1) {
    await driver.clickField(340, 260); await driver.typeAscii(account);
    await driver.key('Tab', 'Tab', 9); await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () => [2, 3].includes(await probes.currentUiState()), 20);
    if (!advanced && await probes.loginNoticeActive() === 1) {
      await driver.key('Enter', 'Enter', 13); await sleep(3000);
    }
  }
  if (!advanced) throw new Error('login never advanced past the login UI (3 attempts)');
  console.log('INFO  login accepted; entering character select');
  await driver.retryClick(510, 216, async () => await probes.currentUiState() === UI.characters);
  if (!await waitFor(async () => await probes.currentUiState() === UI.characters, 45)) throw new Error('character-select UI did not activate');
  const previewsReady = async () => {
    const total = await probes.cashShopProbe('mscharlist_preview_total');
    return total > 0 && await probes.cashShopProbe('mscharlist_preview_ready') === total;
  };
  if (!await waitFor(previewsReady, 90)) throw new Error('character previews never settled');
  if (!await waitFor(() => assetsIdle(driver, probes), 60)) throw new Error('char-select assets never idle');
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(130 + 120 * cardIndex, 210);
    entered = await waitFor(async () => await probes.currentUiState() === UI.game, 5);
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(async () => await probes.mapId() === BATTLE_MAP && await probes.mapAssetLoading() === 0, 60)) {
    throw new Error(`spawn map is ${await probes.mapId()}, expected ${BATTLE_MAP}`);
  }
  if (!await waitFor(() => assetsIdle(driver, probes), 60)) throw new Error('game assets stayed blocking');
}

const clamp = (value, low, high) => Math.min(Math.max(Math.round(value), low), high);

async function gameState(p) {
  const [map, mobs, playerX, playerY, screenX, screenY, nearestMobX, nearestMobY] = await Promise.all([
    p.mapId(), p.mobCount(), p.playerX(), p.playerY(), p.playerScreenX(), p.playerScreenY(),
    p.nearestMobX(), p.nearestMobY()
  ]);
  return { map, mobs, playerX, playerY, screenX, screenY, nearestMobX, nearestMobY };
}

async function approachNearestMob(driver, probes) {
  const state = await gameState(probes);
  if (state.nearestMobX === MISSING) return false;
  await driver.click(
    clamp(state.nearestMobX + state.screenX - state.playerX, 30, 770),
    clamp(state.nearestMobY + state.screenY - state.playerY - 50, 40, 375));
  await sleep(900);
  return true;
}

// 至少 1 只活怪：有怪就贴近；无怪左右走位并空挥 Slash Blast 触发刷新。
async function ensureMobs(driver, probes, slashEvent) {
  for (let attempt = 0; attempt < 30; attempt += 1) {
    if (await probes.mobCount() > 0) { await approachNearestMob(driver, probes); return await probes.mobCount(); }
    if (attempt % 4 === 3) await driver.keyHold(slashEvent.key, slashEvent.code, slashEvent.vk, 100);
    else {
      const right = attempt % 8 < 4;
      await driver.keyHold(right ? 'ArrowRight' : 'ArrowLeft', right ? 'ArrowRight' : 'ArrowLeft', right ? 39 : 37, 350);
    }
    await sleep(500);
  }
  return await probes.mobCount();
}

const pressSkill = (driver, event) => driver.keyHold(event.key, event.code, event.vk, 120);
const attackState = async p => ({ stance: await p.cashShopProbe('msattack_stance'), frame: await p.cashShopProbe('msattack_frame'),
  afterimage: await p.cashShopProbe('msattack_afterimage_presented') });

async function capture(ctx, label, note = '') {
  await ctx.driver.screenshot(label);
  const entry = {
    name: label, path: path.join(ctx.artifactDir, `${label}.png`),
    note, atMs: Date.now() - ctx.startedAt
  };
  ctx.shots.push(entry);
  return entry;
}

function beginPhase(ctx, name, skill = null) {
  console.log(`INFO  phase ${name}${skill ? `: ${skill.name} (${skill.key})` : ''}`);
  return { name, skill: skill ? { id: skill.id, name: skill.name, key: skill.key, source: skill.source } : null,
    startedAtMs: Date.now() - ctx.startedAt, logFrom: ctx.driver.logs.length, screenshots: [], logs: [] };
}

function endPhase(ctx, phase) {
  phase.endedAtMs = Date.now() - ctx.startedAt;
  phase.logs = ctx.driver.logs.slice(phase.logFrom).filter(line => /\[FA\]|\[CB\]|\[ATK\]/.test(line));
  ctx.phases[phase.name] = phase; return phase;
}

async function phaseA(ctx) {
  const phase = beginPhase(ctx, 'A'); phase.state = await gameState(ctx.probes);
  await capture(ctx, 'A1-baseline', 'baseline HUD before combo'); return endPhase(ctx, phase);
}

async function phaseB(ctx) {
  const skill = ctx.bindings.combo, phase = beginPhase(ctx, 'B', skill);
  phase.state = await gameState(ctx.probes);
  await pressSkill(ctx.driver, skill.event); await capture(ctx, 'B1-combo-after-press', 'Combo Attack key pressed');
  await sleep(300); await capture(ctx, 'B2-combo-after-300ms', 'Combo Attack +300ms');
  phase.stateAfter = await gameState(ctx.probes); return endPhase(ctx, phase);
}

async function phaseC(ctx) {
  const skill = ctx.bindings.slash;
  const phase = beginPhase(ctx, 'C', skill);
  phase.lines = [];
  for (let i = 1; i <= 12; i += 1) {
    await ensureMobs(ctx.driver, ctx.probes, skill.event);
    const from = ctx.driver.logs.length, started = Date.now();
    await pressSkill(ctx.driver, skill.event);
    await sleep(250);
    await capture(ctx, `C${String(i).padStart(2, '0')}-slash-250ms`, `Slash Blast #${i} +250ms`);
    phase.attacks ??= []; const attack = { press: i, ...await attackState(ctx.probes) }; phase.attacks.push(attack);
    if (!attack.afterimage) throw new Error(`Slash Blast #${i} did not start an attack animation`);
    await sleep(250);
    await capture(ctx, `C${String(i).padStart(2, '0')}-slash-500ms`, `Slash Blast #${i} +500ms`);
    const remaining = 1200 - (Date.now() - started); if (remaining > 0) await sleep(remaining);
    phase.lines.push(...ctx.driver.logs.slice(from)
      .filter(line => /\[FA\]|\[CB\]/.test(line)).map(line => `#${i} ${line}`));
  }
  phase.stateAfter = await gameState(ctx.probes); return endPhase(ctx, phase);
}

async function phaseD(ctx) {
  const skill = ctx.bindings.finisher, phase = beginPhase(ctx, 'D', skill);
  phase.mobsBefore = await ensureMobs(ctx.driver, ctx.probes, ctx.bindings.slash.event);
  await pressSkill(ctx.driver, skill.event);
  await capture(ctx, 'D1-coma-immediate', 'Coma pressed with charged orbs');
  await sleep(400);
  await capture(ctx, 'D2-coma-400ms', 'Coma +400ms');
  phase.stateAfter = await gameState(ctx.probes); return endPhase(ctx, phase);
}

async function phaseE(ctx) {
  const skill = ctx.bindings.finisher, phase = beginPhase(ctx, 'E', skill);
  // Coma 冷却 4s；等冷却结束再按，避免把“冷却中”误当成“斗气已消耗”。
  phase.cooldownWaitMs = 4300; await sleep(phase.cooldownWaitMs);
  await pressSkill(ctx.driver, skill.event);
  await capture(ctx, 'E1-coma-no-combo', 'Coma re-pressed; orbs already consumed');
  phase.stateAfter = await gameState(ctx.probes); return endPhase(ctx, phase);
}

async function phaseF(ctx) {
  const slash = ctx.bindings.slash, finisher = ctx.bindings.finisher, phase = beginPhase(ctx, 'F', finisher);
  await ensureMobs(ctx.driver, ctx.probes, slash.event);
  for (let i = 1; i <= 3; i += 1) {
    const started = Date.now();
    await pressSkill(ctx.driver, slash.event);
    await sleep(500);
    await capture(ctx, `F${i}-slash-recharge`, `Slash Blast rebuild orb #${i}`);
    const remaining = 1200 - (Date.now() - started); if (remaining > 0) await sleep(remaining);
  }
  await sleep(800);
  await ensureMobs(ctx.driver, ctx.probes, slash.event);
  await pressSkill(ctx.driver, finisher.event);
  await capture(ctx, 'F4-coma-recharged', 'Coma pressed after recharge');
  await sleep(400);
  await capture(ctx, 'F5-coma-recharged-400ms', 'Coma +400ms after recharge');
  phase.stateAfter = await gameState(ctx.probes); return endPhase(ctx, phase);
}

async function phaseG(ctx) {
  const slash = ctx.bindings.slash, panic = ctx.bindings.panic, phase = beginPhase(ctx, 'G', panic);
  for (let i = 1; i <= 2; i += 1) {
    await ensureMobs(ctx.driver, ctx.probes, slash.event);
    await pressSkill(ctx.driver, slash.event);
    await sleep(1000);
  }
  await ensureMobs(ctx.driver, ctx.probes, slash.event);
  await pressSkill(ctx.driver, panic.event);
  await capture(ctx, 'G1-panic-immediate', 'Panic pressed with charged orbs');
  await sleep(400);
  await capture(ctx, 'G2-panic-400ms', 'Panic +400ms');
  phase.stateAfter = await gameState(ctx.probes); return endPhase(ctx, phase);
}

function summaryBindings(bindings) {
  return Object.fromEntries(Object.entries(bindings).map(([role, binding]) => [role, {
    id: binding.id, name: binding.name, key: binding.key ?? null,
    keyIndex: binding.keyIndex ?? null, source: binding.source
  }]));
}

function writeReport(ctx) {
  const logs = ctx.driver?.logs || [];
  const fa = logs.filter(line => line.includes('[FA]'));
  const cb = logs.filter(line => line.includes('[CB]'));
  const report = {
    ok: ctx.error === null, startedAt: new Date(ctx.startedAt).toISOString(),
    durationMs: Date.now() - ctx.startedAt, account, character: characterName,
    cardIndex, pageUrl: ctx.pageUrl, battleMap: BATTLE_MAP, debugPort, headed,
    keymap: ctx.prepared ? {
      characterId: ctx.prepared.characterId, world: ctx.prepared.world,
      originalMap: ctx.prepared.originalMap,
      mapMovedTo: ctx.prepared.mapMoved ? BATTLE_MAP : null,
      rows: ctx.prepared.rows, bindings: summaryBindings(ctx.prepared.bindings),
      provisioned: ctx.prepared.provisioned
    } : null,
    phases: ctx.phases, screenshots: ctx.shots,
    logs: {
      faCount: fa.length, cbCount: cb.length, fa, cb,
      interesting: logs.filter(line =>
        /\[FA\]|\[CB\]/.test(line) || /\berror\b|exception|failed/i.test(line)),
      browserErrors: ctx.driver?.errors || [], last20: logs.slice(-20)
    },
    error: ctx.error
  };
  fs.mkdirSync(ctx.artifactDir, { recursive: true });
  ctx.reportPath = path.join(ctx.artifactDir, 'report.json');
  fs.writeFileSync(ctx.reportPath, `${JSON.stringify(report, null, 2)}\n`);
}

function printFailure(ctx) {
  console.log('INFO  screenshot files:');
  for (const shot of ctx.shots) console.log(`  ${shot.path}`);
  console.log('INFO  last 20 console lines:');
  for (const line of (ctx.driver?.logs || []).slice(-20)) console.log(`  ${line}`);
}

async function main() {
  const ctx = {
    startedAt: Date.now(), artifactDir, shots: [], phases: {}, error: null,
    prepared: null, pageUrl: null, driver: null, probes: null, bindings: {}
  };
  fs.mkdirSync(artifactDir, { recursive: true });
  fs.rmSync(path.join(artifactDir, 'chrome-profile'), { recursive: true, force: true });
  try {
    const db = readDbConfig(configPath);
    ctx.prepared = prepareCharacter(db);
    ctx.bindings = ctx.prepared.bindings;
    console.log(`INFO  ${account}/${characterName}: map ${ctx.prepared.originalMap} -> ${BATTLE_MAP}`
      + `${ctx.prepared.mapMoved ? ' (DB update)' : ' (already there)'}`);
    for (const [role, binding] of Object.entries(ctx.bindings)) {
      console.log(`INFO  ${role}: ${binding.name} (${binding.id})`
        + `${binding.key ? ` -> key ${binding.key} index ${binding.keyIndex}` : ' -> passive, no key'} [${binding.source}]`);
    }
    ctx.pageUrl = await resolvePageUrl();
    console.log(`INFO  WebUI endpoint: ${ctx.pageUrl}`);
    ctx.driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir, headed });
    await ctx.driver.start();
    await ctx.driver.navigate(ctx.pageUrl);
    ctx.probes = createRuntimeProbes(ctx.driver);
    await loginAndEnterGame(ctx.driver, ctx.probes);
    await capture(ctx, '00-hud-entry', 'game HUD after login');
    const mobs = await ensureMobs(ctx.driver, ctx.probes, ctx.bindings.slash.event);
    if (mobs < 1) throw new Error(`no live mob on map ${BATTLE_MAP} after 60s`);
    console.log(`PASS  entered map ${BATTLE_MAP} with ${mobs} live mob(s)`);
    await phaseA(ctx);
    await phaseB(ctx);
    await ensureMobs(ctx.driver, ctx.probes, ctx.bindings.slash.event);
    await phaseC(ctx);
    await phaseD(ctx);
    await phaseE(ctx);
    await phaseF(ctx);
    await phaseG(ctx);
    console.log(`SUMMARY  ${ctx.shots.length} screenshots, `
      + `${Object.values(ctx.phases).reduce((sum, phase) => sum + phase.logs.length, 0)} [FA]/[CB]/[ATK] lines, `
      + `${ctx.driver.errors.length} browser error(s)`);
  } catch (error) {
    ctx.error = error.message;
    process.exitCode = 1;
    console.log(`FAIL  ${error.message}`);
    if (ctx.driver) {
      try {
        await ctx.driver.screenshot('Z-failure');
        ctx.shots.push({ name: 'Z-failure', path: path.join(artifactDir, 'Z-failure.png'),
          note: 'failure state', atMs: Date.now() - ctx.startedAt });
      } catch (screenshotError) {
        console.log(`WARN  failure screenshot unavailable: ${screenshotError.message}`);
      }
    }
  } finally {
    if (ctx.driver) {
      try { await ctx.driver.stop(); } catch (error) { console.log(`WARN  driver stop: ${error.message}`); }
    }
    writeReport(ctx);
    console.log(`INFO  report: ${ctx.reportPath}`);
    console.log(`INFO  screenshots: ${ctx.artifactDir}`);
    if (ctx.error !== null) printFailure(ctx);
  }
}

await main();
