// 现金商城「仓库取出 → CASH 栏 → 外观覆盖穿戴/卸下」浏览器专项验收。
//
// 新契约（现金商城装备纯装饰化，服务端 patch 0005）：
//   1) 商城仓库「取出」（CASHSHOP_OPERATION 0x68/0x0D）的装备落入角色
//      CASH 物品栏（InventoryType id=5，物品栏 CASH 标签），不再进 EQUIP
//      标签；双击仓库格仍只触发一次取出，成功后的自动选中不得把装备存回。
//   2) 物品栏切到 CASH 标签后双击该装备格：以「外观覆盖」方式戴上
//      （MOVE_ITEM dst=-(100+eqslot)，落 EQUIP 隐藏槽 100+eqslot），
//      不占真实装备槽、不顶掉普通装备、不改任何属性总值。
//   3) 装备窗口双击对应部位：覆盖卸下（MOVE_ITEM src=-(100+eqslot)、
//      dst=CASH 空格），装备回到 CASH 栏，属性总值仍不变。
//
// 断言使用客户端只读探针（注意语义：msinventory_slot/quantity 按
// InventoryType::by_item_id 定栏——现金装备 1002000 属 CASH 栏；
// msinv_slot_item(invtype, slot) 直接按栏读格，本脚本用它扫描）：
//   mscashshop_locker_count / _inventory_count / _pending / _nx_credit /
//   _selected_right_row / _filtered_count / _highlighted_item / _selected_left
//   msinv_slot_item / msinventory_equipped / _equipped_marker / _cover_equipped
//   msiteminventory_active / _x / _y / _tab / _slot_at / _tab_x / _tab_y
//   msstatusbar_control_bounds（状态栏 INVENTORY/EQUIPS 快捷钮中心）
//   msui_front_at（装备窗口命中）msstat_total（STR..JUMP 属性快照）
//
// 前置：测试商品在售且可被当前性别购买；CASH 栏有空位；角色离线。
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
const elementType = Object.freeze({ itemInventory: 17, equipInventory: 18 });
const statusbarButton = Object.freeze({ inventory: 9, equips: 10 });
const inventoryTypeEquip = 1;
const inventoryTypeCash = 5;
const statIds = Array.from({ length: 15 }, (unused, i) => i);
const rightGrid = Object.freeze({ x: 517, y: 88, pitch: 39, hit: 18 });
// 物品栏格心相对窗口原点的偏移：post-Chaos 网格 (11,51) 36/35 间距，
// 传统 v83 为 (8,50) 36/34 间距；用 msiteminventory_slot_at 校验命中。
const slotOffsets = Object.freeze([
  { dx: 11 + 18, dy: 51 + 17, pitchX: 36, pitchY: 35 },
  { dx: 8 + 18, dy: 50 + 17, pitchX: 36, pitchY: 34 }
]);
// 装备窗口默认位 PosEQINV (250,150)，CAP 插槽 post-Chaos (43,25)、
// v83 (38,35)，32px 格取中心；两个世代候选均落在彼此的 CAP 格内。
const capSocketCandidates = Object.freeze([[309, 191], [304, 201]]);
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
async function statsSnapshot(driver) {
  const values = [];
  for (const stat of statIds) values.push(await probe(driver, 'msstat_total', [stat]));
  return values;
}
async function assertStatsUnchanged(driver, baseline, label) {
  const current = await statsSnapshot(driver);
  report(JSON.stringify(current) === JSON.stringify(baseline),
    `${label}: stat totals unchanged (${baseline.join(',')} -> ${current.join(',')})`);
}
// 扫描一个物品栏的可见页（1..24），返回 itemId 所在 slot（0=未找到）。
async function findSlotInTab(driver, invtype, wanted) {
  for (let slot = 1; slot <= 24; slot += 1) {
    if (await probe(driver, 'msinv_slot_item', [invtype, slot]) === wanted) return slot;
  }
  return 0;
}
// 返回 itemId 在该栏占用的全部 slot（历史遗留的同类装备可能已存在）。
async function findSlotsInTab(driver, invtype, wanted) {
  const slots = [];
  for (let slot = 1; slot <= 24; slot += 1) {
    if (await probe(driver, 'msinv_slot_item', [invtype, slot]) === wanted) slots.push(slot);
  }
  return slots;
}
async function statusbarButtonCenter(driver, id) {
  const edges = [];
  for (let edge = 0; edge < 4; edge += 1) {
    edges.push(await probe(driver, 'msstatusbar_control_bounds', [id, edge]));
  }
  if (edges.some(edge => edge < 0)) return null;
  return {
    x: Math.round((edges[0] + edges[2]) / 2),
    y: Math.round((edges[1] + edges[3]) / 2)
  };
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
  // 商城入口按钮随 HUD 世代移动（新版 (597,571)，传统 v83 (562,583)）。
  let active = false;
  for (const [x, y] of [[597, 571], [562, 583]]) {
    await driver.click(x, y);
    active = await waitFor(uiStateIs(driver, uiState.cashShop), 15);
    if (active) break;
  }
  if (!active) throw new Error('cash-shop UI did not activate');
  console.log('PASS  cash-shop UI active');
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
  if (row < 0) throw new Error('purchased locker row was not selected');
  const x = rightGrid.x + (row % 7) * rightGrid.pitch + rightGrid.hit;
  const y = rightGrid.y + Math.floor(row / 7) * rightGrid.pitch + rightGrid.hit;
  await driver.doubleClick(x, y);
  await requireState('double-click takes item exactly once', async () =>
    await probe(driver, 'mscashshop_pending') === 0 &&
    await probe(driver, 'mscashshop_locker_count') === before.locker &&
    await probe(driver, 'mscashshop_inventory_count') === before.inventory + 1, 30);

  const slot = await findSlotInTab(driver, inventoryTypeCash, itemId);
  report(slot >= 1 && slot <= 24, `taken equip lands on the CASH tab (slot=${slot})`);
  const equipSlotsAfter = await findSlotsInTab(driver, inventoryTypeEquip, itemId);
  report(JSON.stringify(equipSlotsAfter) === JSON.stringify(before.equipSlots),
    `EQUIP tab keeps only pre-existing copies (${JSON.stringify(before.equipSlots)} -> ${JSON.stringify(equipSlotsAfter)})`);
  const equipped = await probe(driver, 'msinventory_equipped', [capEquipSlot]);
  const marker = await probe(driver, 'msinventory_equipped_marker', [capEquipSlot]);
  const cover = await probe(driver, 'msinventory_cover_equipped', [capEquipSlot]);
  report(equipped === before.equipped && marker === before.marker && cover === before.cover,
    `take does not touch equipment slots (equipped ${before.equipped}->${equipped}, ` +
    `marker ${before.marker}->${marker}, cover ${before.cover}->${cover})`);
  await assertStatsUnchanged(driver, before.stats, 'take keeps stat totals');
  await driver.screenshot('11-taken-into-cash-tab');
  return slot;
}

async function exitCashShop(driver) {
  await driver.key('Escape', 'Escape', 27);
  await requireState('cash shop exits back to game', uiStateIs(driver, uiState.game), 30);
  await requireState('returned map assets ready', assetsIdle(driver), 60);
  await requireState('game UI enabled', async () =>
    await probe(driver, 'msui_enabled') === 1, 60);
}

async function openItemInventory(driver) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (await probe(driver, 'msiteminventory_active') === 1) return true;
    if (await probe(driver, 'msitems_menu_x') < 0) {
      await requireState('map assets idle before menu', assetsIdle(driver), 45);
      // 物品菜单快捷图标随 HUD 世代移动：StatusBar2 在 (711,574)，
      // 传统 v83 状态栏 InvenKey 图标在 (537,544)。
      for (const [x, y] of [[711, 574], [537, 544]]) {
        await driver.click(x, y);
        if (await waitFor(async () => await probe(driver, 'msitems_menu_x') > 0, 5)) break;
      }
      if (await probe(driver, 'msitems_menu_x') < 0) continue;
    }
    const menuX = await probe(driver, 'msitems_menu_x');
    const menuY = await probe(driver, 'msitems_menu_y');
    await driver.click(menuX, menuY);
    if (await waitFor(async () => await probe(driver, 'msiteminventory_active') === 1, 5)) return true;
  }
  return false;
}

// 换到目标标签并返回（校验用）格坐标：优先用探针给出的 tab 按钮中心。
async function switchItemTab(driver, targetTab) {
  const tabX = await probe(driver, 'msiteminventory_tab_x', [itemId]);
  const tabY = await probe(driver, 'msiteminventory_tab_y', [itemId]);
  report(tabX > 0 && tabY > 0, `CASH tab button resolved from item id (${tabX},${tabY})`);
  await driver.retryClick(tabX, tabY, async () =>
    await probe(driver, 'msiteminventory_tab') === targetTab, 4, 2000);
  await requireState('item inventory shows the CASH tab', async () =>
    await probe(driver, 'msiteminventory_tab') === targetTab, 10);
}

async function equipCover(driver, slot, before) {
  await requireState('item inventory opened', () => openItemInventory(driver), 30);
  await requireState('item inventory assets ready', assetsIdle(driver), 45);
  await switchItemTab(driver, inventoryTypeCash);
  const first = await probe(driver, 'msiteminventory_first_slot');
  const last = await probe(driver, 'msiteminventory_last_slot');
  report(slot >= first && slot <= last,
    `taken slot ${slot} is on the visible page [${first}..${last}]`);

  const originX = await probe(driver, 'msiteminventory_x');
  const originY = await probe(driver, 'msiteminventory_y');
  const column = (slot - first) % 4;
  const row = Math.floor((slot - first) / 4);
  let x = 0;
  let y = 0;
  let hit = -1;
  for (const offset of slotOffsets) {
    x = originX + offset.dx + column * offset.pitchX;
    y = originY + offset.dy + row * offset.pitchY;
    hit = await probe(driver, 'msiteminventory_slot_at', [x, y]);
    if (hit === slot) break;
  }
  report(hit === slot, `grid hit test resolves slot ${slot} (got ${hit} at ${x},${y})`);

  await driver.doubleClick(x, y);
  await requireState('double-click wears the cash cover', async () =>
    await probe(driver, 'msinventory_cover_equipped', [capEquipSlot]) === itemId, 30);
  report(await probe(driver, 'msinv_slot_item', [inventoryTypeCash, slot]) === 0,
    `worn cover left its CASH slot empty (slot=${slot})`);
  const equipped = await probe(driver, 'msinventory_equipped', [capEquipSlot]);
  const marker = await probe(driver, 'msinventory_equipped_marker', [capEquipSlot]);
  report(equipped === itemId,
    `cover look is shown while worn (${before.equipped} -> ${equipped}, cover=${itemId})`);
  report(marker === before.marker,
    `equipped cash-id marker unchanged (${before.marker} -> ${marker})`);
  await assertStatsUnchanged(driver, before.stats, 'wearing the cover keeps stat totals');
  await driver.screenshot('12-cover-worn');
}

async function unequipCover(driver, before) {
  // 覆盖穿戴完成后物品栏窗口仍开着，且默认位 (300,150) 会盖住装备窗口
  // (250,150) 的 CAP 插槽：先点状态栏 INVENTORY 快捷钮把它收起。
  const invenCenter = await statusbarButtonCenter(driver, statusbarButton.inventory);
  report(invenCenter !== null, `statusbar INVENTORY shortcut located (${JSON.stringify(invenCenter)})`);
  if (invenCenter) {
    await driver.retryClick(invenCenter.x, invenCenter.y, async () =>
      await probe(driver, 'msiteminventory_active') === 0, 4, 1500);
  }
  await requireState('item inventory closed', async () =>
    await probe(driver, 'msiteminventory_active') === 0, 15);

  const equipsCenter = await statusbarButtonCenter(driver, statusbarButton.equips);
  report(equipsCenter !== null, `statusbar EQUIPS shortcut located (${JSON.stringify(equipsCenter)})`);
  if (!equipsCenter) throw new Error('statusbar EQUIPS shortcut missing');
  await driver.retryClick(equipsCenter.x, equipsCenter.y, async () => {
    for (const [x, y] of capSocketCandidates) {
      if (await probe(driver, 'msui_front_at', [x, y]) === elementType.equipInventory) return true;
    }
    return false;
  }, 4, 2000);
  await requireState('equip inventory window opened over the CAP socket', async () => {
    for (const [x, y] of capSocketCandidates) {
      if (await probe(driver, 'msui_front_at', [x, y]) === elementType.equipInventory) return true;
    }
    return false;
  }, 15);

  const [x, y] = capSocketCandidates[0];
  await driver.doubleClick(x, y);
  await requireState('double-click takes the cover off', async () =>
    await probe(driver, 'msinventory_cover_equipped', [capEquipSlot]) === 0, 30);
  const slot = await findSlotInTab(driver, inventoryTypeCash, itemId);
  report(slot >= 1 && slot <= 24, `cover returned to the CASH tab (slot=${slot})`);
  const equipped = await probe(driver, 'msinventory_equipped', [capEquipSlot]);
  const marker = await probe(driver, 'msinventory_equipped_marker', [capEquipSlot]);
  report(equipped === before.equipped && marker === before.marker,
    `normal equip untouched after the cover comes off (${before.equipped}/${before.marker} -> ${equipped}/${marker})`);
  await assertStatsUnchanged(driver, before.stats, 'removing the cover keeps stat totals');
  await driver.screenshot('13-cover-removed');
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    // Equipment baselines are readable in-game, but shop probes (locker /
    // inventory / nx) only answer once the cash-shop UI exists, so capture
    // them after entering the shop (-1 otherwise).
    const before = {
      equipped: await probe(driver, 'msinventory_equipped', [capEquipSlot]),
      marker: await probe(driver, 'msinventory_equipped_marker', [capEquipSlot]),
      cover: await probe(driver, 'msinventory_cover_equipped', [capEquipSlot]),
      equipSlots: await findSlotsInTab(driver, inventoryTypeEquip, itemId),
      stats: await statsSnapshot(driver)
    };
    await enterCashShop(driver);
    before.locker = await probe(driver, 'mscashshop_locker_count');
    before.inventory = await probe(driver, 'mscashshop_inventory_count');
    before.nx = await probe(driver, 'mscashshop_nx_credit');
    report(before.locker >= 0 && before.inventory >= 0 && before.nx >= 0,
      `shop baselines captured (locker=${before.locker}, inventory=${before.inventory}, nx=${before.nx})`);
    await buyCap(driver, before);
    const slot = await takeCap(driver, before);
    await exitCashShop(driver);
    await equipCover(driver, slot, before);
    await unequipCover(driver, before);
    report(true, 'decorative cash-equip contract holds end to end');
  } catch (error) {
    report(false, `cash shop take/cover flow aborted: ${error.message}`);
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
