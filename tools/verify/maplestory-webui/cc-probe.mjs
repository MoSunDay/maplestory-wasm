// 建卡 OK 按钮定位探针：名字录入后点击候选坐标，观察 msui_character_creation_
// customizing 翻位 + 服务器 NameCharPacket，确定 pre-Chaos BtYes 的真实命中区。
// 背景：run.mjs 建卡流程点击 (520,310) 从未让 customizing 置位且服务器无
// NameCharPacket —— 像素亮块显示 OK 艺术在 x≈385..465, y≈298..346，
// (520,310) 落在取消键区域。用法：
//   CHROME_BIN=/usr/bin/chromium-browser node cc-probe.mjs
import { BrowserDriver, sleep } from './cdp.mjs';
const binary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const port = Number(process.env.CC_PROBE_DEBUG_PORT || 9261);
const url = process.env.CC_PROBE_URL || 'http://127.0.0.1:8000/web/index.html';
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, creation: 4, game: 5 });
const ccall = (d, n) => d.evaluate(`Module.ccall('${n}','number',[],[])`);
async function waitFor(predicate, seconds, label) {
  for (let i = 0; i < seconds; i += 1) { if (await predicate()) return true; await sleep(1000); }
  throw new Error(`Timed out: ${label}`);
}
const driver = new BrowserDriver({ binary, debugPort: port });
try {
  await driver.start();
  await driver.navigate(url);
  await waitFor(async () => driver.evaluate("typeof Module!=='undefined'&&Module.calledRun===true") &&
    (await ccall(driver, 'msui_state')) === uiState.login, 90, 'login');
  await driver.clickField(340, 260);
  await driver.typeAscii('test1');
  await driver.clickField(340, 285);
  await driver.typeAscii('test1');
  await driver.key('Enter', 'Enter', 13);
  await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.world, 45, 'world');
  await driver.click(510, 216);
  await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.characters, 45, 'charselect');
  for (let i = 0; i < 60 && await ccall(driver, 'mscharlist_remaining_bytes') > 0; i += 1) await sleep(1000);
  for (let attempt = 0; attempt < 5; attempt += 1) {
    if ((await ccall(driver, 'msui_state')) === uiState.creation) break;
    await driver.click(250, 515);
    for (let i = 0; i < 4; i += 1) {
      if ((await ccall(driver, 'msui_state')) === uiState.creation) break;
      await sleep(1000);
    }
  }
  await waitFor(async () => (await ccall(driver, 'msui_state')) === uiState.creation, 5, 'creation UI');
  await sleep(10000);
  await driver.click(530, 230);
  await driver.compose('测试一');
  await driver.screenshot('cp0-name-entered');
  for (const [x, y] of [[425, 322], [469, 318], [447, 320], [462, 318], [520, 310]]) {
    if ((await ccall(driver, 'msui_character_creation_customizing')) === 1) break;
    console.log(`[try] click (${x},${y})`);
    await driver.click(x, y);
    let ok = false;
    for (let i = 0; i < 6 && !ok; i += 1) {
      ok = (await ccall(driver, 'msui_character_creation_customizing')) === 1;
      if (!ok) await sleep(1000);
    }
    console.log(`[try] (${x},${y}) customizing=${ok ? 1 : 0} state=${await ccall(driver, 'msui_state')}`);
    if (ok) { await driver.screenshot('cp1-customizing'); break; }
    if ((await ccall(driver, 'msui_state')) !== uiState.creation) {
      console.log('[abort] left creation screen, state=' + await ccall(driver, 'msui_state'));
      break;
    }
  }
  console.log('PASS  cc probe finished');
} finally { await driver.stop(); }
