// 圣甲术施放节流回归（Combat::BUFF_CAST_INTERVAL_MS=1000）：
// 无动作 buff 技能（WZ 无 action 节点）attacking 恒 false，按住/连按会在每个
// keydown 重复施放（狂按技能键疯狂触发 bug）。本脚本用探针
// mscombat_last_buffcast(skillid) 断言：
//   A. 冷却窗内（<1s）连按 N 次 → 时间戳不变（全部被节流）
//   B. 冷却窗过后（1.1s）再按 → 时间戳前进（节流恢复，非永久封锁）
// 前置（一次性，见 tools/verify/maplestory-webui/index.md 圣甲术章节）：
//   keymap: (test1Hero, key=23/I, type=1, action=1001003)、map=910000000、
//   accounts.loggedin=0（运行前 UPDATE maplestory.accounts SET loggedin=0）
// 用法: CHROME_BIN=/usr/bin/chromium-browser node iron-body-cast-throttle.mjs
// exit 0=PASS 1=FAIL 2=HARNESS
import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';

const binary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const port = Number(process.env.IB_DEBUG_PORT || 9275);
const url = process.env.IB_URL || 'http://127.0.0.1:8000/web/index.html';
const artifactDir = process.env.IB_ARTIFACT_DIR || '/tmp/iron-body-throttle';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || 'test1';
const SKILL_ID = 1001003;
const uiState = Object.freeze({ login: 1, worldSelect: 2, charSelect: 3, charCreation: 4, game: 5 });

const ccall = (d, n) => d.evaluate(`Module.ccall('${n}','number',[],[])`);
const probe = (d) => d.evaluate(
  `Module.ccall('mscombat_last_buffcast','number',['number'],[${SKILL_ID}])`);
async function waitFor(predicate, seconds, label) {
  for (let i = 0; i < seconds * 5; i += 1) { if (await predicate()) return; await sleep(200); }
  throw new Error(`Timed out: ${label}`);
}
async function step(label, fn, timeoutMs = 12000, retries = 3) {
  for (let i = 0; ; i += 1) {
    try {
      return await Promise.race([
        fn(),
        sleep(timeoutMs).then(() => { throw new Error(`step timeout: ${label}`); }),
      ]);
    } catch (e) {
      console.log(`[step-retry ${i + 1}] ${label}: ${e.message}`);
      if (i >= retries) throw e;
    }
  }
}
const fail = (msg, code) => { console.error(msg); process.exit(code); };

try {
  fs.mkdirSync(artifactDir, { recursive: true });
  const driver = new BrowserDriver({ binary, debugPort: port, artifactDir });
  await driver.start();
  try {
    await driver.navigate(url);
    await waitFor(async () => driver.evaluate("typeof Module!=='undefined'&&Module.calledRun===true") &&
      (await ccall(driver, 'msui_state')) === uiState.login, 90, 'login UI');

    await step('click user', () => driver.clickField(340, 260));
    await step('type user', () => driver.typeAscii(account));
    await step('click pass', () => driver.clickField(340, 285));
    await step('type pass', () => driver.typeAscii(password));
    await step('enter', () => driver.key('Enter', 'Enter', 13));
    await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.worldSelect, 45, 'world select');

    for (let i = 0; i < 6; i += 1) {
      if ((await ccall(driver, 'msui_state')) === uiState.charSelect) break;
      await step('world enter', () => driver.click(510, 216));
      await sleep(1200);
    }
    await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.charSelect, 30, 'char select');
    for (let i = 0; i < 60 && (await ccall(driver, 'mscharlist_remaining_bytes')) > 0; i += 1) await sleep(1000);

    for (let attempt = 0; attempt < 5; attempt += 1) {
      if ((await ccall(driver, 'msui_state')) !== uiState.charSelect) break;
      await step('char card 0', () => driver.doubleClick(130, 210)); // test1Hero
      for (let i = 0; i < 4; i += 1) {
        if ((await ccall(driver, 'msui_state')) !== uiState.charSelect) break;
        await sleep(1000);
      }
    }
    await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.game, 60, 'game entry');
    await waitFor(async () => (await ccall(driver, 'msstage_loading')) === 0, 90, 'map load');
    await waitFor(async () => (await ccall(driver, 'msasset_blocking')) === 0, 90, 'assets idle');
    console.log('IN GAME, map', await ccall(driver, 'msmap_id'));
    await sleep(1500);

    // A0: fresh client has never cast
    if ((await probe(driver)) !== 0) fail('HARNESS: probe nonzero before any cast (stale client state?)', 2);

    // A1: first cast records a timestamp
    await step('first cast', () => driver.key('I', 'KeyI', 73));
    await waitFor(async () => (await probe(driver)) > 0, 10, 'first cast recorded');
    const ts0 = await probe(driver);
    console.log(`cast #1 recorded at ${ts0}`);

    // A2: hammer inside the 1s cooldown window — timestamp must not move
    let taps = 0;
    const t0 = Date.now();
    while (Date.now() - t0 < 750) {
      await driver.key('I', 'KeyI', 73);
      taps += 1;
      await sleep(40);
    }
    const tsAfterHammer = await probe(driver);
    console.log(`hammered ${taps} taps in ${Date.now() - t0}ms, probe ${ts0} -> ${tsAfterHammer}`);
    if (taps < 5) fail(`HARNESS: only ${taps} taps delivered, test invalid`, 2);
    if (tsAfterHammer !== ts0) {
      fail(`FAIL: ${taps} taps within ${Date.now() - t0}ms (<1s cooldown) changed last-cast ${ts0} -> ${tsAfterHammer}: recast not throttled`, 1);
    }
    console.log(`PASS(A): ${taps} rapid taps fully suppressed inside cooldown`);

    // B: past the cooldown, a tap must cast again (throttle, not a block)
    await sleep(1100);
    await step('post-cooldown cast', () => driver.key('I', 'KeyI', 73));
    await waitFor(async () => (await probe(driver)) > ts0, 10, 'post-cooldown cast recorded');
    const ts1 = await probe(driver);
    if (ts1 <= ts0) fail('FAIL: cast after cooldown window did not record (gate stuck closed)', 1);
    console.log(`PASS(B): cast after cooldown recorded at ${ts1} (delta ${ts1 - ts0}ms >= 1000)`);
    console.log('PASS: Iron Body recast throttled to 1s interval');
    process.exit(0);
  } finally {
    try { await driver.stop(); } catch (e) { /* already stopped */ }
  }
} catch (e) {
  fail(`HARNESS ERROR: ${e.message}`, 2);
}
