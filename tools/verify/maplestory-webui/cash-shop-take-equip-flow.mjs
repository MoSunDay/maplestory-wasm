// 现金商城「仓库取出 → 物品栏穿戴」浏览器专项验收。
//
// 修复的两个回归点：
//   1) 双击商城仓库槽位应只发起一次「取出」；成功后的第二下点击不得因
//      右栏自动选中而再次触发「存回」，否则装备会从背包消失。
//   2) 取出的装备进入角色 EQUIP 物品栏后，打开物品栏双击该格应正常穿戴，
//      并从 EQUIP 栏移除、更新已装备外观与属性。
//
// 断言使用客户端只读探针：
//   mscashshop_locker_count / mscashshop_inventory_count / mscashshop_pending
//   msinventory_quantity / msinventory_slot / msinventory_equipped /
//   msinventory_equipped_marker
//   msiteminventory_active / _x / _y / _tab / _first_slot / _last_slot / _slot_at
//
// 前置：角色至少有一个空 EQUIP 槽位；测试商品在售且可被当前性别购买。
// Run: CHROME_BIN=/usr/bin/chromium-browser node \
//   tools/verify/maplestory-webui/cash-shop-take-equip-flow.mjs

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.TAKE_EQUIP_DEBUG_PORT || 9251);
const pageUrl = process.env.TAKE_EQUIP_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.TAKE_EQUIP_ACCOUNT || 'test1';
const password = process.env.TAKE_EQUIP_PASSWORD || 'test1';
const cardIndex = Number(process.env.TAKE_EQUIP_CARD || 1);
const itemId = Number(process.env.TAKE_EQUIP_ITEM_ID || 1002000);
const sn = process.env.TAKE_EQUIP_SN || '20000016';
const price = Number(process.env.TAKE_EQUIP_PRICE || 3700);
const capEquipSlot = Number(process.env.TAKE_EQUIP_CAP_SLOT || 1);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const inventoryTypeEquip = 1;
const rightGrid = Object.freeze({ x: 504, y: 102, pitch: 39, hit: 18 });
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
  const center = charCard(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error('game field did not activate');
  await requireState('game assets ready', assetsIdle(driver), 90);
}

async function enterCashShop(driver) {
  await driver.click(597, 571);
  await requireState('cash-shop UI active', uiStateIs(driver, uiState.cashShop), 30);
  await requireState('cash-shop assets ready', assetsIdle(driver), 60);
}

async function buyCap(driver, before) {
  await driver.click(300, 50);
  await requireState('cash-shop search focused', () =>
    driver.evaluate('MapleWasmIME.active === true'), 10);
  await driver.compose(sn);
  await requireState('cash-shop SN entered', () =>
    driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(sn)}`), 10);
  await sleep(250);
  await driver.key('Enter', 'Enter', 13);
  await requireState('search filters target item', async () =>
    await probe(driver, 'mscashshop_filtered_count') === 1 &&
    await probe(driver, 'mscashshop_highlighted_item') === itemId, 10);

  await driver.click(155, 120);
  await requireState('catalog card selected', async () =>
    await probe(driver, 'mscashshop_selected_left') >= 0, 10);
  await driver.click(155, 120);
  await requireState('purchase confirmation visible', async () =>
    await probe(driver, 'msui_notice_active') === 1, 10);
  await sleep(500);
  await driver.screenshot('10-purchase-confirm');
  await driver.click(380, 285);
  await requireState('purchase settles into locker', async () =>
    await probe(driver, 'mscashshop_pending') === 0 &&
    await probe(driver, 'mscashshop_nx_credit') === before.nx - price &&
    await probe(driver, 'mscashshop_locker_count') === before.locker + 1, 30);
}

async function takeCap(driver, before) {
  const row = await probe(driver, 'mscashshop_selected_right_row');
  report(row >= 0, `purchased item selected in locker (row=${row})`);
  if (row < 0) return 0;
  const x = rightGrid.x + (row % 7) * rightGrid.pitch + rightGrid.hit;
  const y = rightGrid.y + Math.floor(row / 7) * rightGrid.pitch + rightGrid.hit;
  await driver.doubleClick(x, y);
  await requireState('double-click takes item exactly once', async () =>
    await probe(driver, 'mscashshop_pending') === 0 &&
    await probe(driver, 'mscashshop_locker_count') === before.locker &&
    await probe(driver, 'mscashshop_inventory_count') === before.inventory + 1, 30);
  const slot = await probe(driver, 'msinventory_slot', [itemId]);
  report(slot >= 1 && slot <= 24, `taken item is in EQUIP inventory (slot=${slot})`);
  const quantity = await probe(driver, 'msinventory_quantity', [itemId]);
  report(quantity === before.quantity + 1,
    `taken item count increased by one (${before.quantity} -> ${quantity})`);
  await driver.screenshot('11-taken-into-inventory');
  return slot;
}

async function openItemInventory(driver) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (await probe(driver, 'msiteminventory_active') === 1) return true;
    if (await probe(driver, 'msitems_menu_x') < 0) {
      await requireState('map assets idle before menu', assetsIdle(driver), 45);
      await driver.click(711, 574);
      if (!await waitFor(async () => await probe(driver, 'msitems_menu_x') > 0, 5)) continue;
    }
    const menuX = await probe(driver, 'msitems_menu_x');
    const menuY = await probe(driver, 'msitems_menu_y');
    await driver.click(menuX, menuY);
    if (await waitFor(async () => await probe(driver, 'msiteminventory_active') === 1, 5)) return true;
  }
  return false;
}

async function equipTakenCap(driver, slot) {
  await requireState('item inventory opened', () => openItemInventory(driver), 30);
  await requireState('item inventory assets ready', assetsIdle(driver), 45);
  const tab = await probe(driver, 'msiteminventory_tab');
  report(tab === inventoryTypeEquip, `item inventory shows EQUIP tab (tab=${tab})`);
  const first = await probe(driver, 'msiteminventory_first_slot');
  const last = await probe(driver, 'msiteminventory_last_slot');
  report(slot >= first && slot <= last,
    `taken slot ${slot} is on the visible page [${first}..${last}]`);

  const originX = await probe(driver, 'msiteminventory_x');
  const originY = await probe(driver, 'msiteminventory_y');
  const column = (slot - first) % 4;
  const row = Math.floor((slot - first) / 4);
  const x = originX + 29 + column * 36;
  const y = originY + 68 + row * 35;
  const hit = await probe(driver, 'msiteminventory_slot_at', [x, y]);
  report(hit === slot, `grid hit test resolves slot ${slot} (got ${hit} at ${x},${y})`);

  const equippedBefore = await probe(driver, 'msinventory_equipped', [capEquipSlot]);
  const markerBefore = await probe(driver, 'msinventory_equipped_marker', [capEquipSlot]);
  const quantityBefore = await probe(driver, 'msinventory_quantity', [itemId]);
  const expectedQuantity = equippedBefore === itemId ? quantityBefore : quantityBefore - 1;
  await driver.doubleClick(x, y);
  await requireState('double-click equips taken cap', async () =>
    await probe(driver, 'msinventory_equipped', [capEquipSlot]) === itemId &&
    await probe(driver, 'msinventory_equipped_marker', [capEquipSlot]) !== markerBefore &&
    await probe(driver, 'msinventory_quantity', [itemId]) === expectedQuantity, 30);
  await driver.screenshot('12-cap-equipped');
  console.log(`equipped cap ${equippedBefore} -> ${await probe(driver, 'msinventory_equipped', [capEquipSlot])}, ` +
    `marker ${markerBefore} -> ${await probe(driver, 'msinventory_equipped_marker', [capEquipSlot])}`);
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await enterCashShop(driver);
    const before = {
      locker: await probe(driver, 'mscashshop_locker_count'),
      inventory: await probe(driver, 'mscashshop_inventory_count'),
      nx: await probe(driver, 'mscashshop_nx_credit'),
      quantity: await probe(driver, 'msinventory_quantity', [itemId])
    };
    await buyCap(driver, before);
    const slot = await takeCap(driver, before);
    await driver.key('Escape', 'Escape', 27);
    await requireState('cash shop exits back to game', uiStateIs(driver, uiState.game), 30);
    await requireState('returned map assets ready', assetsIdle(driver), 60);
    await requireState('game UI enabled', async () =>
      await probe(driver, 'msui_enabled') === 1, 60);
    await equipTakenCap(driver, slot);
  } catch (error) {
    report(false, `cash shop take/equip flow aborted: ${error.message}`);
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
  console.log(`FAIL  TAKE_EQUIP_CARD must be a card index 0..3, got ${process.env.TAKE_EQUIP_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
