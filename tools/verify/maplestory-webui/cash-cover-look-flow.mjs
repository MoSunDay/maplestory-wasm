// 现金覆盖装备「穿戴 → 重登外观保持 → 卸下」浏览器端到端验收。
//
// 背景（bug 现场）：现金覆盖装备（如爵士礼帽 1003052）穿戴后库存链路本就正常
// （MOVE_ITEM dst=-(100+eqslot)，落 EQUIP 隐藏槽 -100-x，服务端外观封包把它放进
// 第一段“可见列表”），但客户端 CharLook 构造函数曾把服务端外观封包第二段
// （maskedequips：被现金件覆盖而隐藏的普通装备）后应用，把第一段里同一部位的
// 现金外观覆盖掉——重登/他人视角看到的帽子从现金帽退回普通帽。
// 修复一（数据）：src/client/Character/Look/CharLook.cpp 构造时跳过键已存在于
// 第一段 equips 的 maskedequips 条目（仅无可见对应者——如现金武器 -111——照常应用）。
// 修复二（渲染）：现金帽（如 1003052）全部正面姿势帧 z=capOverHair，归入
// Clothing::CAP_OVER_HAIR 层；而 vslot CpH1H5 分类为 HALFCOVER 的绘制分支
// 原先只画 Layer::CAP——现金帽一像素都不画（look 数据对、画面没帽子）。
// 现在 HALFCOVER/FULLCOVER 分支在 CAP 之后补画 CAP_OVER_HAIR。
//
// 本脚本对 test1Warr（test1/test1，cardIndex 0）按四个阶段实测断言：
//   1) 登录进游戏（SET_FIELD 外观路径）：角色戴普通帽 1002047（EQUIPPED -1），
//      CASH 栏 slot 1 有现金帽 1003052（未穿）→ 基线 look=1002047、cover=0；
//   2) 物品栏 CASH 标签双击现金帽 → 覆盖穿戴：msinventory_cover_equipped(1)
//      ==1003052（隐藏槽）且 msinventory_equipped(1)==1003052（look 探针，
//      读 CharLook.get_equip）；
//   3) 重登（logoutToLogin 干净登出 → 重新登录进游戏）后
//      msinventory_equipped(1) 必须仍为 1003052（修复前这里是 1002047，
//      即 bug 现场回归断言）；隐藏槽 cover 探针同步保持 1003052；
//   4) 装备窗口双击 CAP 部位卸下 → look 回到 1002047、cover=0、现金帽回到
//      CASH 栏（msinv_slot_item 扫描可见）。
//
// 探针（客户端只读，src/client/Gameplay/Probes/）：
//   msui_state / msasset_blocking / msinventory_equipped（CharLook look）/
//   msinventory_cover_equipped（隐藏槽 100+slot）/ msinv_slot_item /
//   msiteminventory_active/_x/_y/_tab/_first_slot/_last_slot/_slot_at/
//   _tab_x/_tab_y / msstatusbar_control_bounds（状态栏快捷钮）/ msui_front_at
//
// DB fixture（角色 test1Warr = characters.id 243，属账号 test1）：EQUIPPED(-1)
// 槽 position=-1 放普通帽 1002047（reqLevel 35，故 characters.level 需 ≥35）；
// CASH(5) 槽 position=1 放现金帽 1003052（两行都要带 inventoryequipment 明细，
// 注意 owner 列 NOT NULL 无默认值）。characters.world 必须等于服务端公告的
// 最后一个世界（world.ini worlds=N → 世界 N-1；当前 N=5 → world 4）：
// 客户端 GO 按钮固定请求 worlds.back() 那个世界，角色不在那里就会得到空
// 选人列表（preview_total=0）并卡在 previews settled 阶段。
// 重复运行无需重置：脚本以“穿戴→重登→卸下”自愈回到 fixture 态。
//
// 环境变量：CHROME_BIN（默认 /usr/bin/chromium-browser）、COVER_LOOK_URL
// （默认 http://127.0.0.1:8000/web/index.html）、COVER_LOOK_ACCOUNT/
// COVER_LOOK_PASSWORD（默认 test1/test1）、COVER_LOOK_CARD（默认 0）、
// COVER_LOOK_DEBUG_PORT（默认 9265）。
// 用法: node tools/verify/maplestory-webui/cash-cover-look-flow.mjs [--headed]

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { logoutToLogin, UI_STATE } from './logout.mjs';

const parseArgs = argv => {
  const parsed = {};
  for (const arg of argv) {
    if (arg === '--headed') parsed.headed = true;
    else if (arg === '--help') parsed.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return parsed;
};
let args;
try { args = parseArgs(process.argv.slice(2)); }
catch (error) { console.log(`FAIL  ${error.message}`); process.exit(64); }
if (args.help) {
  console.log('usage: node tools/verify/maplestory-webui/cash-cover-look-flow.mjs [--headed]');
  process.exit(0);
}

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.COVER_LOOK_DEBUG_PORT || 9265);
const pageUrl = process.env.COVER_LOOK_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.COVER_LOOK_ACCOUNT || 'test1';
const password = process.env.COVER_LOOK_PASSWORD || 'test1';
const cardIndex = Number(process.env.COVER_LOOK_CARD ?? 0);

const cashHatId = 1003052;      // 爵士礼帽（CASH 栏现金帽）
const ordinaryCapId = 1002047;  // 普通帽（真实 EQUIP 槽）
const capSlot = 1;              // Equipslot::CAP
const uiState = Object.freeze({
  login: UI_STATE.login, world: UI_STATE.world,
  characters: UI_STATE.characters, game: UI_STATE.game
});
const inventoryTypeEquip = 1;  // InventoryType::EQUIP
const inventoryTypeCash = 5;   // InventoryType::CASH（物品栏 CASH 标签同源）
const elementType = Object.freeze({ equipInventory: 18 });       // UIElement::Type
const statusbarButton = Object.freeze({ inventory: 9, equips: 10 }); // 快捷钮 id
// 物品栏格心相对窗口原点的偏移：post-Chaos 网格 (11,51) 36/35 间距，
// 传统 v83 为 (8,50) 36/34 间距；先 msiteminventory_slot_at 校验命中再双击。
const slotOffsets = Object.freeze([
  { dx: 11 + 18, dy: 51 + 17, pitchX: 36, pitchY: 35 },
  { dx: 8 + 18, dy: 50 + 17, pitchX: 36, pitchY: 34 }
]);
// 装备窗口默认位 PosEQINV (250,150)，CAP 插槽 post-Chaos (43,25)、
// v83 (38,35)，32px 格取中心；坐标已含窗口原点偏移。
const capSocketCandidates = Object.freeze([[309, 191], [304, 201]]);
const charCard = index => ({ x: 130 + 120 * index + 25, y: 210 });

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
// 扫描一个物品栏的可见页（1..24），返回 itemId 所在 slot（0=未找到）。
async function findSlotInTab(driver, invtype, wanted) {
  for (let slot = 1; slot <= 24; slot += 1) {
    if (await probe(driver, 'msinv_slot_item', [invtype, slot]) === wanted) return slot;
  }
  return 0;
}
// 状态栏快捷钮中心：msstatusbar_control_bounds 四边（l/t/r/b）取中点。
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
  await requireState('login UI active', uiStateIs(driver, uiState.login), 45);
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  await requireState('world-select UI active', uiStateIs(driver, uiState.world), 45);
  // GO 按钮固定请求服务端公告的最后一个世界（worlds.back()，见头注释的
  // fixture 世界要求；不要在此处点击世界按钮——与选人预览流竞争会触发
  // asyncify 卡死）。
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  await requireState('character-select UI active', uiStateIs(driver, uiState.characters), 45);
  await requireState('character-select assets ready', assetsIdle(driver), 60);
  await requireState('character previews settled', async () => {
    const total = await probe(driver, 'mscharlist_preview_total');
    return total > 0 && await probe(driver, 'mscharlist_preview_ready') === total;
  }, 90);
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

// ---- 断言 1：登录基线（SET_FIELD 外观路径）----
async function assertLoginBaseline(driver) {
  const equipped = await probe(driver, 'msinventory_equipped', [capSlot]);
  const cover = await probe(driver, 'msinventory_cover_equipped', [capSlot]);
  const cashSlot = await findSlotInTab(driver, inventoryTypeCash, cashHatId);
  const equipSlot = await findSlotInTab(driver, inventoryTypeEquip, cashHatId);
  report(equipped === ordinaryCapId, `login look wears the ordinary cap ${ordinaryCapId} (got ${equipped})`);
  report(cover === 0, `no cover in the hidden slot at login (cover=${cover})`);
  report(cashSlot >= 1 && cashSlot <= 24,
    `cash hat sits in the CASH tab unworn (slot=${cashSlot}, fixture expects 1)`);
  report(equipSlot === 0, `cash hat absent from the EQUIP tab at login (slot=${equipSlot})`);
  await driver.screenshot('10-login-baseline');
}

// 用状态栏 INVENTORY 快捷钮（id 9）打开物品栏。
async function openItemInventory(driver) {
  if (await probe(driver, 'msiteminventory_active') === 1) return true;
  const center = await statusbarButtonCenter(driver, statusbarButton.inventory);
  if (!center) return false;
  await driver.click(center.x, center.y);
  return waitFor(async () => await probe(driver, 'msiteminventory_active') === 1, 10);
}
// 同一快捷钮再点一次收起物品栏（默认位 (300,150) 会盖住装备窗口 CAP 插槽）。
async function closeItemInventory(driver) {
  if (await probe(driver, 'msiteminventory_active') === 0) return true;
  const center = await statusbarButtonCenter(driver, statusbarButton.inventory);
  if (!center) return false;
  await driver.click(center.x, center.y);
  return waitFor(async () => await probe(driver, 'msiteminventory_active') === 0, 10);
}

// 切到目标标签：tab 按钮中心由 msiteminventory_tab_x/_y(itemid) 解出，
// msiteminventory_tab 断言到位（CASH=5）。
async function switchItemTab(driver, targetTab) {
  const tabX = await probe(driver, 'msiteminventory_tab_x', [cashHatId]);
  const tabY = await probe(driver, 'msiteminventory_tab_y', [cashHatId]);
  report(tabX > 0 && tabY > 0, `CASH tab button resolved from item id (${tabX},${tabY})`);
  await driver.retryClick(tabX, tabY, async () =>
    await probe(driver, 'msiteminventory_tab') === targetTab, 4, 2000);
  await requireState('item inventory shows the CASH tab', async () =>
    await probe(driver, 'msiteminventory_tab') === targetTab, 10);
}

// 解出物品栏 slot 的格心坐标：两代网格候选逐一试，_slot_at 命中即用。
async function resolveSlotPoint(driver, slot, first) {
  const originX = await probe(driver, 'msiteminventory_x');
  const originY = await probe(driver, 'msiteminventory_y');
  const column = (slot - first) % 4;
  const row = Math.floor((slot - first) / 4);
  let point = { x: 0, y: 0, hit: -1 };
  for (const offset of slotOffsets) {
    const x = originX + offset.dx + column * offset.pitchX;
    const y = originY + offset.dy + row * offset.pitchY;
    const hit = await probe(driver, 'msiteminventory_slot_at', [x, y]);
    point = { x, y, hit };
    if (hit === slot) break;
  }
  return point;
}

// ---- 断言 2：UI 双击 CASH 栏穿戴现金帽 ----
async function wearCashCover(driver) {
  await requireState('item inventory opened via the statusbar shortcut',
    () => openItemInventory(driver), 30);
  await requireState('item inventory assets ready', assetsIdle(driver), 45);
  await switchItemTab(driver, inventoryTypeCash);

  const slot = await findSlotInTab(driver, inventoryTypeCash, cashHatId);
  report(slot >= 1 && slot <= 24, `cash hat located in the CASH tab (slot=${slot})`);
  if (slot < 1) throw new Error('cash hat not found in the CASH tab');
  const first = await probe(driver, 'msiteminventory_first_slot');
  const last = await probe(driver, 'msiteminventory_last_slot');
  report(slot >= first && slot <= last,
    `cash slot ${slot} is on the visible page [${first}..${last}]`);
  const point = await resolveSlotPoint(driver, slot, first);
  report(point.hit === slot,
    `grid hit test resolves slot ${slot} (got ${point.hit} at ${point.x},${point.y})`);

  await driver.doubleClick(point.x, point.y);
  await requireState('double-click wears the cash cover', async () =>
    await probe(driver, 'msinventory_cover_equipped', [capSlot]) === cashHatId, 30);
  report(await probe(driver, 'msinv_slot_item', [inventoryTypeCash, slot]) === 0,
    `worn cover left its CASH slot empty (slot=${slot})`);
  const equipped = await probe(driver, 'msinventory_equipped', [capSlot]);
  report(equipped === cashHatId,
    `look shows the cash hat while worn (${equipped}, expected ${cashHatId})`);
  await driver.screenshot('20-cover-worn');
}

// ---- 断言 3：重登后 look 仍为现金帽（bug 现场回归断言）----
async function reloginAndVerifyLook(driver) {
  const logoutState = await logoutToLogin(driver, { log: console.log });
  report(logoutState === uiState.login,
    `clean logout back to the login page (msui_state=${logoutState})`);
  // 服务端清 accounts.loggedin 是异步的，稍作沉淀再登录。
  await sleep(3000);
  await loginAndEnterGame(driver);

  const equipped = await probe(driver, 'msinventory_equipped', [capSlot]);
  const cover = await probe(driver, 'msinventory_cover_equipped', [capSlot]);
  report(equipped === cashHatId,
    `look keeps the cash hat after relogin (${equipped}, expected ${cashHatId}; ` +
    `${ordinaryCapId} here is the pre-fix regression)`);
  report(cover === cashHatId, `hidden cover slot persists after relogin (cover=${cover})`);
  await driver.screenshot('30-after-relogin');
}

// ---- 断言 4：装备窗口双击 CAP 部位卸下现金帽 ----
async function unequipCashCover(driver) {
  await requireState('item inventory closed before the equip window opens',
    () => closeItemInventory(driver), 15);
  const equipsCenter = await statusbarButtonCenter(driver, statusbarButton.equips);
  report(equipsCenter !== null,
    `statusbar EQUIPS shortcut located (${JSON.stringify(equipsCenter)})`);
  if (!equipsCenter) throw new Error('statusbar EQUIPS shortcut missing');
  await driver.retryClick(equipsCenter.x, equipsCenter.y, async () => {
    for (const [x, y] of capSocketCandidates) {
      if (await probe(driver, 'msui_front_at', [x, y]) === elementType.equipInventory) return true;
    }
    return false;
  }, 4, 2000);
  await requireState('equip window opened over the CAP socket', async () => {
    for (const [x, y] of capSocketCandidates) {
      if (await probe(driver, 'msui_front_at', [x, y]) === elementType.equipInventory) return true;
    }
    return false;
  }, 15);

  // 两代 CAP 插槽坐标候选都试，以 cover 探针归零判定成功。
  let removed = false;
  for (const [x, y] of capSocketCandidates) {
    await driver.doubleClick(x, y);
    removed = await waitFor(async () =>
      await probe(driver, 'msinventory_cover_equipped', [capSlot]) === 0, 8);
    if (removed) break;
  }
  report(removed, 'double-click on the CAP socket takes the cover off');

  const cover = await probe(driver, 'msinventory_cover_equipped', [capSlot]);
  const equipped = await probe(driver, 'msinventory_equipped', [capSlot]);
  const cashSlot = await findSlotInTab(driver, inventoryTypeCash, cashHatId);
  report(cover === 0, `hidden cover slot emptied (cover=${cover})`);
  report(equipped === ordinaryCapId,
    `look falls back to the ordinary cap (got ${equipped}, expected ${ordinaryCapId})`);
  report(cashSlot >= 1 && cashSlot <= 24,
    `cash hat returned to the CASH tab (slot=${cashSlot})`);
  await driver.screenshot('40-cover-removed');
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, headed: args.headed });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await assertLoginBaseline(driver);
    await wearCashCover(driver);
    await reloginAndVerifyLook(driver);
    await unequipCashCover(driver);
  } catch (error) {
    report(false, `cash cover look flow aborted: ${error.message}`);
    console.error(error);
  } finally {
    if (driver.errors.length) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    }
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) {
    console.log('FAIL cash-cover-look-flow');
    process.exitCode = 1;
  } else {
    console.log('PASS cash-cover-look-flow');
  }
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  COVER_LOOK_CARD must be a card index 0..3, got ${process.env.COVER_LOOK_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
