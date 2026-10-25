// 圣甲术（Iron Body, 1001003）客户端生效验证：真实 WASM 客户端登录战士号，
// 按 I 键施放，断言右上角 buff 图标槽（UIBuffList 绘制于 {width-50,40}，32x32）
// 在施放后出现新像素。前置自检：施放前同区域连拍两帧必须一致（背景静态），
// 否则判 inconclusive，避免背景动画误报。
// 前置（一次性）：
//   keymap: (test1Hero, key=23/I, type=1, action=1001003)
//   角色传送到静态室内图 910000000；accounts.loggedin=0
// 用法: CHROME_BIN=/usr/bin/chromium-browser node iron-body-client-flow.mjs
// exit 0=PASS 1=FAIL 2=HARNESS 3=INCONCLUSIVE
import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';

const binary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const port = Number(process.env.IB_DEBUG_PORT || 9273);
const url = process.env.IB_URL || 'http://127.0.0.1:8000/web/index.html';
const artifactDir = process.env.IB_ARTIFACT_DIR || '/tmp/iron-body-client';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || 'test1';
const GAME_W = 800, GAME_H = 600;
const uiState = Object.freeze({ login: 1, worldSelect: 2, charSelect: 3, charCreation: 4, game: 5 });

const ccall = (d, n) => d.evaluate(`Module.ccall('${n}','number',[],[])`);
async function waitFor(predicate, seconds, label) {
  for (let i = 0; i < seconds * 5; i += 1) { if (await predicate()) return; await sleep(200); }
  throw new Error(`Timed out: ${label}`);
}
// cdp.mjs 的 send 无超时：偶发丢消息会永久悬挂，这里统一加护栏。
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

// 游戏坐标 → 页面 CSS 坐标映射（canvas 可能有缩放）
async function canvasRect(driver) {
  return JSON.parse(await driver.evaluate(`(() => {
    const c = document.querySelector('canvas');
    const r = c.getBoundingClientRect();
    return JSON.stringify({ left: r.left, top: r.top, w: r.width, h: r.height });
  })()`));
}
async function regionShot(driver, rect, { x, y, w, h }) {
  const sx = rect.left + (x / GAME_W) * rect.w;
  const sy = rect.top + (y / GAME_H) * rect.h;
  const sw = (w / GAME_W) * rect.w;
  const sh = (h / GAME_H) * rect.h;
  const r = await driver.send('Page.captureScreenshot', {
    format: 'png',
    clip: { x: sx, y: sy, width: sw, height: sh, scale: 1 },
  });
  return r.data;
}

const driver = new BrowserDriver({ binary, debugPort: port, artifactDir });
try {
  fs.mkdirSync(artifactDir, { recursive: true });
  await driver.start();
  await driver.navigate(url);
  await waitFor(async () => driver.evaluate("typeof Module!=='undefined'&&Module.calledRun===true") &&
    (await ccall(driver, 'msui_state')) === uiState.login, 90, 'login UI');
  console.log('login UI reached');

  await step('click user', () => driver.clickField(340, 260));
  await step('type user', () => driver.typeAscii(account));
  await step('click pass', () => driver.clickField(340, 285));
  await step('type pass', () => driver.typeAscii(password));
  await step('enter', () => driver.key('Enter', 'Enter', 13));
  await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.worldSelect, 45, 'world select');
  console.log('world select reached');

  for (let i = 0; i < 6; i += 1) {
    if ((await ccall(driver, 'msui_state')) === uiState.charSelect) break;
    await step('world enter', () => driver.click(510, 216));
    await sleep(1200);
  }
  await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.charSelect, 30, 'char select');
  console.log('char select reached');
  for (let i = 0; i < 60 && (await ccall(driver, 'mscharlist_remaining_bytes')) > 0; i += 1) await sleep(1000);

  for (let attempt = 0; attempt < 5; attempt += 1) {
    if ((await ccall(driver, 'msui_state')) !== uiState.charSelect) break;
    await step('char card 0', () => driver.doubleClick(130, 210)); // char card 0 = test1Hero
    for (let i = 0; i < 4; i += 1) {
      if ((await ccall(driver, 'msui_state')) !== uiState.charSelect) break;
      await sleep(1000);
    }
  }
  await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.game, 60, 'game entry');
  await waitFor(async () => (await ccall(driver, 'msstage_loading')) === 0, 90, 'map load');
  await waitFor(async () => (await ccall(driver, 'msasset_blocking')) === 0, 90, 'assets idle');
  console.log('IN GAME, map', await ccall(driver, 'msmap_id'), 'ui', await ccall(driver, 'msui_state'));
  await sleep(1500);

  const rect = await canvasRect(driver);
  const strip = { x: 744, y: 32, w: 48, h: 52 }; // buff icon cell (750,40)+32
  const pre1 = await regionShot(driver, rect, strip);
  await sleep(400);
  const pre2 = await regionShot(driver, rect, strip);
  fs.writeFileSync(path.join(artifactDir, 'pre1.png'), Buffer.from(pre1, 'base64'));
  fs.writeFileSync(path.join(artifactDir, 'pre2.png'), Buffer.from(pre2, 'base64'));
  if (pre1 !== pre2) fail('INCONCLUSIVE: buff strip not static before cast (animated background?)', 3);

  await step('cast key I', () => driver.key('I', 'KeyI', 73), 12000, 1); // keymap 23 → SKILL 1001003
  console.log('KEY I pressed (cast Iron Body)');
  await sleep(2500);

  const post = await regionShot(driver, rect, strip);
  fs.writeFileSync(path.join(artifactDir, 'post.png'), Buffer.from(post, 'base64'));
  await driver.screenshot('full-after-cast');

  if (post === pre1) fail('FAIL: buff icon strip unchanged after casting Iron Body', 1);
  console.log('PASS: Iron Body buff icon appeared in the client buff strip (region changed after cast)');
  process.exit(0);
} catch (e) {
  fail(`HARNESS ERROR: ${e.message}`, 2);
} finally {
  try { await driver.stop(); } catch (e) { /* already stopped */ }
}
