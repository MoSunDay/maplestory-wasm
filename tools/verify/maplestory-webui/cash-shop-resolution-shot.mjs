// 商城分辨率取证：分别在 800x600 与 1280x720 下登录并截图商城，
// 输出 canvas 元素几何（rect JSON）与商城状态探针，供离线像素比对。
// 用法: SHOP_RES=1|none [SHOP_CDP_PORT=9268] node cash-shop-resolution-shot.mjs
// 1280x720 下 800x600 设计画布按 1.2 倍缩放并居中；两档都应以
// filtered=1984 完成，且左栏/商品区/右栏不重叠。
import fs from 'node:fs';
import path from 'node:path';
import { BrowserDriver, sleep } from './cdp.mjs';

const res = process.env.SHOP_RES ?? '1';
const debugPort = Number(process.env.SHOP_CDP_PORT || 9268);
const driver = new BrowserDriver({ binary: '/usr/bin/chromium-browser', debugPort });
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const state = () => driver.evaluate("Module.ccall('msui_state','number',[],[])");
const stateName = s => Object.keys(uiState).find(k => uiState[k] === s) ?? `?${s}`;
async function waitFor(pred, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await pred()) return true; await sleep(250); }
  return false;
}
async function main() {
  await driver.start();
  await driver.navigate('http://127.0.0.1:8000/web/index.html');
  await driver.evaluate(res === 'none'
    ? "localStorage.removeItem('maplestory.display.resolution')"
    : `localStorage.setItem('maplestory.display.resolution', '${res}')`);
  await driver.navigate(`http://127.0.0.1:8000/web/index.html?shot=${Date.now()}`);
  if (!await waitFor(() => driver.evaluate("typeof Module !== 'undefined' && Module.calledRun === true"), 60))
    throw new Error('module never ran');
  if (!await waitFor(async () => (await state()) === uiState.login)) throw new Error('login never shown');
  await driver.clickField(340, 260);
  await driver.typeAscii('test1');
  await driver.clickField(340, 285);
  await driver.typeAscii('test1');
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => (await state()) === uiState.world))
    throw new Error(`world never shown (state=${stateName(await state())})`);
  if (!await driver.retryClick(510, 216, async () => (await state()) === uiState.characters))
    throw new Error(`characters never shown (state=${stateName(await state())})`);
  await sleep(3000);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(250, 210);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = (await state()) === uiState.game; if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error(`game never entered (state=${stateName(await state())})`);
  // The status bar only accepts input after the initial asset burst finishes;
  // clicking during the loading overlay silently drops the event.
  if (!await waitFor(() => driver.evaluate(
    "Module.ccall('msasset_blocking','number',[],[]) === 0"
    + " && document.getElementById('asset-loading-screen').classList.contains('is-hidden')"), 45))
    throw new Error('game assets never became idle');
  await sleep(1000);
  const wide = await driver.evaluate("Module.ccall('msui_resolution_width','number',[],[])") > 800;
  // 双候选坐标：StatusBar2 的 BtCashShop 随右锚移动。1280x720 时元素
  // 锚点 x=992、按钮相对锚点 (57..113, -46..-11)，对应中心 (1077, 691)；
  // 800x600 时中心为 (597, 571)，并保留 v83 旧栏候选。
  const cands = wide ? [[1077, 691], [1055, 680]] : [[597, 571], [562, 583]];
  for (const [x, y] of cands) {
    await driver.click(x, y);
    await sleep(2500);
    if ((await state()) === uiState.cashShop) break;
  }
  if ((await state()) !== uiState.cashShop) throw new Error('cash shop did not activate');
  await sleep(3000);
  await driver.screenshot(`shop-${res}`);
  const rect = await driver.evaluate(
    "(() => { const c = document.getElementById('canvas'); const b = c.getBoundingClientRect();"
    + " return {left: b.left, top: b.top, width: b.width, height: b.height,"
    + " cw: c.width, ch: c.height, pw: innerWidth, ph: innerHeight}; })()");
  fs.writeFileSync(path.join(driver.artifactDir, `shop-${res}-rect.json`), JSON.stringify(rect));
  const probe = name => driver.evaluate(`Module.ccall('${name}','number',[],[])`);
  console.log(`done res=${res}`,
    'filtered=', await probe('mscashshop_filtered_count'),
    'sel_left=', await probe('mscashshop_selected_left'),
    'locker=', await probe('mscashshop_locker_count'),
    'inv=', await probe('mscashshop_inventory_count'));
  await driver.stop();
}
main().catch(e => { console.error(e); process.exit(1); });
