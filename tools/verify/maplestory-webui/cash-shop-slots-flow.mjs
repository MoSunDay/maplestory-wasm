// 现金商城「购买背包格子」面板浏览器专项验收。
//
// 新契约（DETAILS 物品详情面板删除后，原位新增 SLOTBUY 面板）：
//   1) 面板位于右栏 (508,294) 288x94，2x2 pill（x=516/656 宽132，
//      y=322/358 高30），按钮中心可由 mscashshop_slotbuy_x/y 探针取得；
//   2) 点击 pill（0..3 → EQUIP/USE/SETUP/ETC）弹 UIYesNo 确认框，
//      确认后发送 CASHSHOP_OPERATION 0x06（mode 0，+4 格，4000 币）；
//   3) 服务端回 0x60（showBoughtInventorySlots）→ 客户端 set_slotmax
//      + 结算 pending；0x144 推送余额。断言 slotmax 恰好 +4、
//      nx_credit 恰好 -4000、pending 归零、退出商城正常。
//
// 测试会真实消耗 4000 NX 并永久扩展一格栏容量，运行前需角色离线。
// Run: CHROME_BIN=/usr/bin/chromium-browser node \
//   tools/verify/maplestory-webui/cash-shop-slots-flow.mjs

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.SLOTS_DEBUG_PORT || 9252);
const pageUrl = process.env.SLOTS_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.SLOTS_ACCOUNT || 'test1';
const password = process.env.SLOTS_PASSWORD || 'test1';
const cardIndex = Number(process.env.SLOTS_CARD || 1);
const slotPrice = Number(process.env.SLOTS_PRICE || 4000);
const slotStep = 4;
// pill 0..3 → 服务端 gainSlots 的 type（1=EQUIP..4=ETC）。
const pillIndex = Number(process.env.SLOTS_PILL || 0);
const inventoryType = pillIndex + 1;
const slotMaxLimit = 128;

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const cashShopEntry = Object.freeze([[597, 571], [562, 583]]);
const confirmOk = Object.freeze({ x: 380, y: 285 });
const pillArea = Object.freeze({ left: 516, right: 788, top: 322, bottom: 388 });
const charCard = index => ({ x: 105 + 120 * index + 25, y: 210 });

let failed = false;
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
function probe(driver, name, args = []) {
  return driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', ${JSON.stringify(args.map(() => 'number'))}, ${JSON.stringify(args)})`);
}
function uiStateIs(driver, expected) {
  return () => probe(driver, 'msui_state').then(value => value === expected);
}
function assetsIdle(driver) {
  let hiddenSince = 0;
  return async () => {
    const hidden = await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
    if (!hidden || await probe(driver, 'msasset_blocking') !== 0) {
      hiddenSince = 0;
      return false;
    }
    if (!hiddenSince) hiddenSince = Date.now();
    return Date.now() - hiddenSince >= 1000;
  };
}
async function waitFor(predicate, seconds = 30) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}
async function requireState(name, predicate, seconds = 30) {
  if (!await waitFor(predicate, seconds)) throw new Error(`Timed out: ${name}`);
  console.log(`PASS  ${name}`);
}

async function loginAndEnterGame(driver) {
  await requireState('WASM module initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await requireState('login UI active', uiStateIs(driver, uiState.login), 30);
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  await requireState('world-select UI active', uiStateIs(driver, uiState.world), 45);
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  await requireState('character-select UI active', uiStateIs(driver, uiState.characters), 45);
  await requireState('character-select assets ready', assetsIdle(driver), 60);
  const card = charCard(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(card.x, card.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error('game field did not activate');
  await requireState('game assets ready', assetsIdle(driver), 90);
}

async function enterCashShop(driver) {
  // 商城入口按钮随 HUD 世代移动（新版 (597,571)，传统 v83 (562,583)）。
  let active = false;
  for (const [x, y] of cashShopEntry) {
    await driver.click(x, y);
    active = await waitFor(uiStateIs(driver, uiState.cashShop), 15);
    if (active) break;
  }
  if (!active) throw new Error('cash-shop UI did not activate');
  console.log('PASS  cash-shop UI active');
  await requireState('cash-shop assets ready', assetsIdle(driver), 60);
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await enterCashShop(driver);

    // 基线：目标栏 slotmax、NX 余额、四个 pill 的按钮中心。
    const before = {
      slotmax: await probe(driver, 'mscashshop_slotmax', [inventoryType]),
      nx: await probe(driver, 'mscashshop_nx_credit'),
      pills: []
    };
    for (let index = 0; index < 4; index += 1) {
      before.pills.push([
        await probe(driver, 'mscashshop_slotbuy_x', [index]),
        await probe(driver, 'mscashshop_slotbuy_y', [index])
      ]);
    }
    report(before.slotmax > 0 && before.slotmax + slotStep <= slotMaxLimit,
      `target tab has room to expand (slotmax=${before.slotmax})`);
    report(before.nx >= slotPrice, `balance covers the price (nx=${before.nx})`);
    report(before.pills.every(([x, y]) => x >= pillArea.left && x <= pillArea.right &&
      y >= pillArea.top && y <= pillArea.bottom),
      `four pills centered inside the panel area (${JSON.stringify(before.pills)})`);
    await driver.screenshot('10-slotbuy-panel');

    // 点击目标 pill → UIYesNo 确认 → 服务端 0x60 结算。
    const [pillX, pillY] = before.pills[pillIndex];
    await driver.click(pillX, pillY);
    await requireState('slot purchase confirmation visible', async () =>
      await probe(driver, 'msui_notice_active') === 1, 15);
    await sleep(500);
    await driver.screenshot('11-slotbuy-confirm');
    await driver.click(confirmOk.x, confirmOk.y);
    await requireState('slot purchase settles', async () =>
      await probe(driver, 'mscashshop_pending') === 0 &&
      await probe(driver, 'mscashshop_slotmax', [inventoryType]) === before.slotmax + slotStep &&
      await probe(driver, 'mscashshop_nx_credit') === before.nx - slotPrice, 30);
    report(true, `slotmax grew by ${slotStep} for type ${inventoryType} ` +
      `(${before.slotmax} -> ${before.slotmax + slotStep})`);
    report(true, `nx credit decreased by ${slotPrice} ` +
      `(${before.nx} -> ${before.nx - slotPrice})`);
    await driver.screenshot('12-slotbuy-settled');

    // 其他栏不受影响；退出商城回图仍正常。
    for (let other = 1; other <= 4; other += 1) {
      if (other === inventoryType) continue;
      const slotmax = await probe(driver, 'mscashshop_slotmax', [other]);
      report(slotmax > 0 && slotmax !== before.slotmax + slotStep,
        `other tab ${other} unaffected (slotmax=${slotmax})`);
    }
    await driver.key('Escape', 'Escape', 27);
    await requireState('cash shop exits back to game', uiStateIs(driver, uiState.game), 30);
    await requireState('returned map assets ready', assetsIdle(driver), 60);
    const after = await probe(driver, 'mscashshop_slotmax', [inventoryType]);
    report(after === before.slotmax + slotStep,
      `expanded slotmax persists after leaving (slotmax=${after})`);
    report(true, 'slot-expansion panel contract holds end to end');
  } catch (error) {
    report(false, `cash shop slots flow aborted: ${error.message}`);
    console.error(error);
  } finally {
    if (driver.errors.length) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    }
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  SLOTS_CARD must be a card index 0..3, got ${process.env.SLOTS_CARD}`);
  process.exitCode = 1;
} else if (pillIndex < 0 || pillIndex > 3) {
  console.log(`FAIL  SLOTS_PILL must be a pill index 0..3, got ${process.env.SLOTS_PILL}`);
  process.exitCode = 1;
} else {
  await main();
}
