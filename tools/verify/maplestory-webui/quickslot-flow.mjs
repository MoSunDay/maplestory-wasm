// 浏览器 E2E：状态栏快捷栏（post-Chaos mainBar/quickSlot）技能与物品绑定全流程。
//
// 覆盖链路：登录 → 进入角色 → 打开技能书 → 选中任意已学习非被动技能 →
// slot0 绑定/悬停/点击使用/右键清空 → slot7 拖拽绑定/右键清空 →
// Hero's Will 施法后的冷却倒计时显示 → 物品栏拖入 USE 物品、点击消耗、
// 右键清空。
// 两档分辨率均需通过：QS_PRESET=0（800x600，默认）与 QS_PRESET=1（1280x720）。
//
// 关键语义：每次 use/assign 之后、rightclick 之前必须等待
// probes.assetBlocking() === 0。施法（Combat::use_move）会触发 LazyFS
// 按需流式加载并阻塞输入，blocking 期间派发的右键会被整击吞掉。
//
// 用法（依赖本地 assets/ws/server 服务，页面默认探测 loopback 8000/8001，
// 可用 E2E_URL 覆盖）：
//   QS_PRESET=0 node tools/verify/maplestory-webui/quickslot-flow.mjs
//   QS_PRESET=1 node tools/verify/maplestory-webui/quickslot-flow.mjs
//
// 环境变量：
//   QS_PRESET            0=800x600（默认），1=1280x720
//   QS_ACCOUNT           登录账号（默认 test1，沿用仓库 E2E 惯例）
//   QS_PASSWORD          登录密码（必填，避免在代码中写入凭据）
//   QS_DEBUG_PORT        CDP 调试端口（默认 9335）
//   QS_ARTIFACT_DIR      截图/日志目录（默认 /tmp/qs-flow-<preset>）
//   QS_PLAYER_ID         登录角色 ID（默认 215，即 test1Hero）
//   QS_PLAYER_LEVEL      登录角色等级（默认 200）
//   QS_ITEM_ID           物品绑定用例的 USE 物品（默认 2000005 超级药水）
//   QS_SKIP_SKILLS=1     跳过前面的技能绑定段（用于独立跑冷却/物品段，避免
//                        SwiftShader 在多次技能书开关后偶发挂起）
//   QS_SKIP_COOLDOWN=1   跳过冷却段（用于独立验证物品绑定）
//   QS_SKIP_ITEMS=1      跳过物品绑定段（用于只验证 HUD 布局）
//   CHROME_BIN           Chromium 路径（默认 /usr/bin/chromium-browser）

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { resolvePageUrl, createRuntimeProbes } from './runtime-flow.mjs';
import { sampleLuminance } from './layout-pixels.mjs';

const preset = Number(process.env.QS_PRESET || 0);
if (preset !== 0 && preset !== 1) {
  throw new Error(`QS_PRESET must be 0 (800x600) or 1 (1280x720), got ${process.env.QS_PRESET}`);
}
const [viewW, viewH] = preset === 1 ? [1280, 720] : [800, 600];
const account = process.env.QS_ACCOUNT || 'test1';
const password = process.env.QS_PASSWORD;
if (!password) {
  throw new Error('QS_PASSWORD is required');
}
const expectedPlayerId = Number(process.env.QS_PLAYER_ID || 215);
const expectedPlayerLevel = Number(process.env.QS_PLAYER_LEVEL || 200);
const bindItemId = Number(process.env.QS_ITEM_ID || 2000005);
const skipSkills = process.env.QS_SKIP_SKILLS === '1';
const skipCooldown = process.env.QS_SKIP_COOLDOWN === '1';
const skipItems = process.env.QS_SKIP_ITEMS === '1';
const artifactDir = process.env.QS_ARTIFACT_DIR || `/tmp/qs-flow-${preset}`;

const driver = new BrowserDriver({
  binary: process.env.CHROME_BIN || '/usr/bin/chromium-browser',
  debugPort: Number(process.env.QS_DEBUG_PORT || 9335),
  artifactDir,
  profileDir: `${artifactDir}/profile`
});
const probes = createRuntimeProbes(driver);
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const EMPTY_SLOTS = [0, 0, 0, 0, 0, 0, 0, 0];
const state = () => probes.currentUiState();

function equal(name, actual, expected) {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(`${name}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
  console.log(`PASS  ${name} = ${JSON.stringify(actual)}`);
}

async function waitFor(name, predicate, seconds = 90) {
  for (let attempt = 0; attempt < seconds * 5; attempt += 1) {
    if (await predicate()) {
      console.log(`PASS  ${name}`);
      return;
    }
    await sleep(200);
  }
  throw new Error(`Timed out: ${name}`);
}

// LazyFS blocking 输入锁：流式加载期间鼠标/按键会被整击丢弃，
// 任何交互（打开技能书、右键清空）之前都必须等到 0。
const waitAssetsIdle = label =>
  waitFor(label, async () => await probes.assetBlocking() === 0, 60);

async function quickslots() {
  const values = [];
  for (let index = 0; index < 8; index += 1) {
    values.push(await probes.quickslotSkill(index));
  }
  return values;
}

// 只移动鼠标（悬停验证），把游戏坐标换算到 canvas 的 CSS 盒。
async function hover(x, y) {
  const point = await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const bounds = canvas.getBoundingClientRect();
    return {
      x: bounds.left + ${x} * bounds.width / canvas.width,
      y: bounds.top + ${y} * bounds.height / canvas.height
    };
  })()`);
  await driver.send('Input.dispatchMouseEvent', {
    type: 'mouseMoved', x: point.x, y: point.y, button: 'none'
  });
}

// post-Chaos 底部 HUD 布局（StatusBarPostChaos.h / QuickSlotModel.h）：
// StatusBar2.img mainBar，145x93 快捷栏面板右锚视口右下。NX origin
// (-143,143) 会让 panel.draw(origin) 再叠加一次原点，因此面板贴图必须
// 用 quickslot::texture_draw_position(origin, panel.get_origin()) 绘制，
// 才能让贴图落在命中框/图标/键帽同一个 top-left 上。
// CHAOS_SHORTCUT_ICON_TOP = -58（BtSkill 等 NORMAL 原点 y=58），
// CHAOS_QUICKSLOT_OFFSET_Y = -58 - 93 - 2 = -153：
// 面板顶边 viewH-163、底边 viewH-70，图标行顶边 viewH-68，间隙 2px。
// 技能书窗口是视口内固定坐标，与分辨率无关；状态栏技能按钮
//（mainBar/BtSkill，origin (-254,58)）随 HUD 右锚。
const panelX = viewW - 145;
const panelY = viewH - 163;
const shortcutRowTop = viewH - 68;
const slotCenter = index => ({
  x: panelX + [23, 56, 89, 122][index % 4],
  y: panelY + (index < 4 ? 31 : 64)
});
const skillbookOrigin = { x: 50, y: 150 };
const tabCenters = [13, 23, 34, 49, 66];
const skillIconCenter = row => ({
  x: skillbookOrigin.x + 29,
  y: skillbookOrigin.y + 110 + row * 40
});
// 状态栏技能按钮（mainBar/BtSkill 30x18，原点 (-254,58)，矩形
// x=viewW-34..viewW-4 / y=viewH-68..viewH-50）：
// 取按钮中心略偏左的稳定点，避免 canvas 缩放截断点击点。
const skillbookButton = { x: viewW - 24, y: viewH - 59 };

// Open the item inventory through the status bar Menu popup. The menu
// button is part of the right-anchored control cluster, so its viewport
// point follows the live resolution (711,574 at 800x600).
async function openItemInventory() {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (await probes.itemInventoryActive() === 1) return;
    await waitAssetsIdle('items menu assets idle');
    if (await probes.itemsMenuX() < 0) {
      await driver.click(viewW - 89, viewH - 26);
      await waitFor('items menu popup', async () => await probes.itemsMenuX() > 0, 8);
    }
    const menuX = await probes.itemsMenuX();
    const menuY = await probes.itemsMenuY();
    await driver.click(menuX, menuY);
    await sleep(500);
  }
  if (await probes.itemInventoryActive() !== 1) {
    throw new Error('item inventory did not open from the status bar menu');
  }
}

// Drag the configured USE item into quick slot 1, click it to consume one,
// then right-click to clear. The item grid origin/pitch is the post-Chaos
// UIWindow2 item inventory geometry (11,51) + 36x35.
async function verifyItemBinding() {
  await openItemInventory();
  // Opening the window constructs every slot icon and can start a LazyFS
  // chunk request after openItemInventory's last idle check. Retry the tab
  // click so a press dropped while input is blocked is not a hard failure.
  await waitAssetsIdle('item inventory open assets idle');
  const originX = await probes.itemInventoryX();
  const originY = await probes.itemInventoryY();
  const useTab = { x: originX + 42, y: originY + 28 };
  let inventoryTab = await probes.itemInventoryTab();
  for (let attempt = 0; attempt < 3 && inventoryTab !== 2; attempt += 1) {
    await waitAssetsIdle(`item inventory tab assets idle ${attempt}`);
    await driver.click(useTab.x, useTab.y);
    await sleep(350);
    inventoryTab = await probes.itemInventoryTab();
  }
  equal('item inventory USE tab', inventoryTab, 2);
  await waitAssetsIdle('use inventory assets idle');

  const itemSlot = await probes.inventorySlot(bindItemId);
  if (itemSlot <= 0) {
    throw new Error(`item ${bindItemId} is missing from the USE inventory`);
  }
  console.log(`INFO  item icon id=${await probes.itemInventoryIconItemId(itemSlot)}`);
  const first = await probes.itemInventoryFirstSlot();
  const last = await probes.itemInventoryLastSlot();
  if (itemSlot < first || itemSlot > last) {
    throw new Error(`item slot ${itemSlot} is outside the visible page [${first}..${last}]`);
  }
  const column = (itemSlot - first) % 4;
  const row = Math.floor((itemSlot - first) / 4);
  const itemX = originX + 29 + column * 36;
  const itemY = originY + 68 + row * 35;
  console.log(`INFO  item inventory origin=${originX},${originY} slot=${itemSlot} page=[${first}..${last}] point=${itemX},${itemY}`);
  equal('item grid hit test', await probes.itemInventorySlotAt(itemX, itemY), itemSlot);

  const target = slotCenter(1);
  let assigned = 0;
  for (let attempt = 0; attempt < 3 && assigned !== bindItemId; attempt += 1) {
    await waitAssetsIdle(`item drag assets idle ${attempt}`);
    // Settle the cursor on the destination before pressing on the source
    // icon; CDP drags are occasionally lost while the inventory settles.
    await hover(target.x, target.y);
    await sleep(250);
    await driver.drag(itemX, itemY, target.x, target.y);
    await sleep(700);
    assigned = await probes.quickslotItem(1);
    if (assigned !== bindItemId) {
      const slots = [];
      for (let index = 0; index < 8; index += 1) {
        slots.push(await probes.quickslotItem(index));
      }
      console.log(`INFO  item drag attempt=${attempt} slots=${JSON.stringify(slots)}`);
      const misplaced = slots.some((value, index) => value !== 0 && index !== 1);
      if (misplaced) {
        throw new Error(`item drag assigned to an unexpected slot: ${JSON.stringify(slots)}`);
      }
    }
  }
  await driver.screenshot('slot1-item-drop');
  equal('slot 1 item assigned', assigned, bindItemId);
  equal('slot 1 skill replaced', await probes.quickslotSkill(1), 0);
  await waitAssetsIdle('item assign assets idle');
  await driver.screenshot('slot1-item-assigned');

  const before = await probes.inventoryQuantity(bindItemId);
  await driver.click(target.x, target.y);
  await sleep(700);
  await waitFor('quick slot consumes item', async () =>
    await probes.inventoryQuantity(bindItemId) < before, 15);
  await waitAssetsIdle('item use assets idle');
  await driver.screenshot('slot1-item-used');

  await driver.rightClick(target.x, target.y);
  await sleep(500);
  equal('right click clears item slot', await probes.quickslotItem(1), 0);
  equal('item slot skill remains empty', await probes.quickslotSkill(1), 0);
  await driver.screenshot('slot1-item-cleared');
}

// UIStatusbar::Buttons 全量枚举（0..11）：聊天/商城/交易/弹层按钮与快捷图标行
// 共用同一张 post-Chaos mainBar 贴图，右边距固定，因此真实矩形随视宽右锚。
const statusbarControls = Object.freeze({
  whisper: 0,     // BT_WHISPER（mainBar/BtChat）
  callgm: 1,      // BT_CALLGM（mainBar/BtClaim）
  cashshop: 2,    // BT_CASHSHOP
  trade: 3,       // BT_TRADE（mainBar/BtMTS）
  menu: 4,        // BT_MENU（mainBar/BtMenu）
  system: 5,      // BT_OPTIONS（mainBar/BtSystem）
  character: 6,   // BT_CHARACTER
  stats: 7,       // BT_STATS
  quest: 8,       // BT_QUEST
  inventory: 9,   // BT_INVENTORY
  equips: 10,     // BT_EQUIPS
  skill: 11       // BT_SKILL
});
const shortcutControlNames = [
  'character', 'stats', 'quest', 'inventory', 'equips', 'skill'
];
const MISSING_COORDINATE = -32768;

// 严格矩形重叠：仅共边（a.right === b.left / a.bottom === b.top）不算重叠。
const overlaps = (a, b) =>
  a.left < b.right && b.left < a.right
  && a.top < b.bottom && b.top < a.bottom;
const showBounds = bounds =>
  `{${bounds.left},${bounds.top},${bounds.right},${bounds.bottom}}`;

// 读取真实按钮矩形；探针缺失时给出可诊断的错误而不是 -32768 参与比较。
async function controlBounds(name) {
  const id = statusbarControls[name];
  const bounds = await probes.statusbarControlBounds(id);
  if (Object.values(bounds).includes(MISSING_COORDINATE)) {
    throw new Error(`statusbar control ${name} (id=${id}) unavailable: ${showBounds(bounds)}`);
  }
  return bounds;
}

async function verifyQuickslotLayout() {
  const panel = await probes.quickslotPanel();
  // 面板期望值即真实绘制矩形：探针读取 quickslot::draw 里同一次
  // Texture::draw_bounds 的返回值，也就是 Texture::draw 使用的 destination。
  // 因此把 QuickSlotBar.cpp 改回 `panel.draw(origin)`（少一次原点补偿）
  // 会让这里的断言立即失败，而不是继续通过。
  equal('quickslot panel bounds', panel, {
    left: panelX,
    top: panelY,
    right: panelX + 145,
    bottom: panelY + 93
  });

  // 0) 探针越界 id 回归：65540 若未做范围校验会按 uint16_t 回绕成
  //    BT_MENU(4) 而返回真实矩形。四个边界都必须报缺失。
  const wrappedId = 65540;
  const wrapped = await probes.statusbarControlBounds(wrappedId);
  if (!Object.values(wrapped).every(value => value === MISSING_COORDINATE)) {
    throw new Error(`statusbar control id ${wrappedId} must be rejected, got`
      + ` ${showBounds(wrapped)} (out-of-range id wrapped to a real button)`);
  }
  console.log(`PASS  out-of-range control id ${wrappedId} rejected`);

  // 1) 快捷栏面板不得与任何真实状态栏控件矩形重叠（不使用硬编码矩形）。
  const controls = {};
  for (const name of Object.keys(statusbarControls)) {
    controls[name] = await controlBounds(name);
    if (overlaps(panel, controls[name])) {
      throw new Error(`quickslot panel ${showBounds(panel)} overlaps`
        + ` ${name} ${showBounds(controls[name])}`);
    }
  }
  console.log(`INFO  statusbar controls ${JSON.stringify(controls)}`);

  // 2) 快捷图标行与 SYSTEM/MENU 两行控件互不重叠。
  const shortcuts = shortcutControlNames.map(name => ({
    name,
    bounds: controls[name]
  }));
  for (const { name, bounds } of shortcuts) {
    if (overlaps(bounds, controls.system) || overlaps(bounds, controls.menu)) {
      throw new Error(`shortcut ${name} ${showBounds(bounds)} overlaps`
        + ` system ${showBounds(controls.system)}`
        + ` or menu ${showBounds(controls.menu)}`);
    }
  }

  // 3) 面板底边到图标行顶边的实测间隙：必须是 2px（硬断言，不是 INFO）。
  const earliestShortcutTop = Math.min(...shortcuts.map(entry => entry.bounds.top));
  const latestShortcutBottom = Math.max(...shortcuts.map(entry => entry.bounds.bottom));
  const gap = earliestShortcutTop - panel.bottom;
  if (earliestShortcutTop !== shortcutRowTop) {
    throw new Error(`shortcut row top measured ${earliestShortcutTop},`
      + ` expected ${shortcutRowTop} (viewH-68)`);
  }
  if (gap !== 2) {
    throw new Error(`quickslot panel ${showBounds(panel)} must clear the`
      + ` shortcut row by exactly 2px, measured ${gap}px`
      + ` (shortcut top=${earliestShortcutTop})`);
  }

  // 4) SYSTEM/MENU 必须严格位于快捷图标行下方（不共用图标行底边）。
  for (const name of ['system', 'menu']) {
    if (controls[name].top <= latestShortcutBottom) {
      throw new Error(`${name} ${showBounds(controls[name])} must be strictly`
        + ` below the shortcut icon row (latest bottom=${latestShortcutBottom})`);
    }
  }

  for (let index = 0; index < 8; index += 1) {
    const expected = slotCenter(index);
    const actual = {
      x: await probes.quickslotCenterX(index),
      y: await probes.quickslotCenterY(index)
    };
    if (actual.x !== expected.x || actual.y !== expected.y) {
      throw new Error(`quickslot ${index} center: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
  }
  console.log(`PASS  quickslot panel ${showBounds(panel)} clears shortcut row by ${gap}px`
    + ` (shortcut top=${earliestShortcutTop}, system top=${controls.system.top},`
    + ` menu top=${controls.menu.top})`);
}

async function login() {
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  // Tab 切到密码框，避免点击焦点丢失时把密码敲进账号框。
  await driver.key('Tab', 'Tab', 9);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
}

async function enterGame() {
  await waitFor('post-login UI', async () =>
    [uiState.world, uiState.characters, uiState.game].includes(await state()), 90);
  if (await state() === uiState.world) {
    await waitAssetsIdle('world select assets idle');
    // v83 世界选择：先选频道，确认按钮才可用。
    await driver.click(120, 172);
    await sleep(500);
    await driver.click(510, 216);
    await waitFor('character select', async () =>
      [uiState.characters, uiState.game].includes(await state()), 90);
  }
  if (await state() !== uiState.game) {
    await waitAssetsIdle('character select assets idle');
    await sleep(1000);
    // 双击第一张角色卡（中心 130,210）进入游戏，预览流未挂载时点击会被丢弃。
    for (let attempt = 0; attempt < 6 && await state() !== uiState.game; attempt += 1) {
      console.log(`INFO  character card attempt=${attempt} state=${await state()}`);
      await driver.doubleClick(130, 210);
      await sleep(2000);
    }
    if (await state() !== uiState.game) await driver.screenshot('charselect-stuck');
  }
  await waitFor('in game', async () => await state() === uiState.game, 90);
  await waitFor('map loaded', async () => await probes.mapId() > 0, 90);
  await waitFor('map assets idle', async () =>
    await probes.mapAssetLoading() === 0 && await probes.assetBlocking() === 0, 120);
}

// 底部状态栏回归断言（本轮两个修复点）：
//   1) 全宽覆盖 —— backgrnd 左缘贴 x=0，任意分辨率下底条左段都是条带
//      像素而非地图（旧实现 1280 下左缘从 viewW-800 起画，左侧漏地图）。
//      采样行 y=viewH-45..-42 在条带内部（条带 [viewH-95, viewH-10]）且
//      高于量条框架（viewH-43 起）、低于右侧图标行（viewH-46 起）之外
//      的纯 backgrnd 区域；参考段 x=100..160 在新旧实现里都是条带。
//   2) HP/MP 分隔符 —— 组串用 '/'（Charset 把 '\' 节点重映射为 '/' 查
//      找键），标签必须形如 [cur/max]。
async function verifyStatusBarChrome() {
  const hpLabel = await driver.evaluate(
    `Module.ccall('msstatusbar_stat_label', 'string', ['number'], [0])`);
  const mpLabel = await driver.evaluate(
    `Module.ccall('msstatusbar_stat_label', 'string', ['number'], [1])`);
  if (!/^\[\d+\/\d+\]$/.test(hpLabel) || !/^\[\d+\/\d+\]$/.test(mpLabel)) {
    throw new Error(`stat labels missing cur/max divider: hp="${hpLabel}" mp="${mpLabel}"`);
  }
  console.log(`PASS  stat divider hp=${hpLabel} mp=${mpLabel}`);

  // 输入区探针：条带覆盖区域 [0, viewW] x [viewH-95, viewH-10] 内任意点
  // 命中（bit1），条带上方一点不命中；修复前 1280 下左缘点 (4, viewH-45)
  // 落在旧 bounds [viewW-800, viewW] 之外。
  const hitBits = async (x, y) => Number(await driver.evaluate(
    `Module.ccall('msstatusbar_hit', 'number', ['number', 'number'], [${x}, ${y}])`));
  for (const [x, y] of [[4, viewH - 45], [viewW / 2, viewH - 45], [viewW - 4, viewH - 45], [4, viewH - 12]]) {
    const bits = await hitBits(x, y);
    if ((bits & 3) !== 3) {
      throw new Error(`strip point (${x}, ${y}) not in statusbar range: bits=${bits}`);
    }
  }
  // 避开聊天日志悬停列（x<500）与右侧快捷栏坞（x>viewW-145）。
  const above = await hitBits(viewW - 200, viewH - 105);
  if ((above & 2) !== 0) {
    throw new Error(`point above strip (${viewW - 200}, ${viewH - 105}) wrongly in range: bits=${above}`);
  }
  console.log('PASS  statusbar input range covers full view width');

  // 像素佐证：左段与正上方地图行亮度差 —— 修复前左段漏地图（两行同为
  // 地图、亮度接近），修复后左段是条带内容（等级块/渐变），差异显著。
  const means = await sampleLuminance(driver, [
    { name: 'stripLeft', x0: 2, y0: viewH - 48, x1: 62, y1: viewH - 42 },
    { name: 'stripMid', x0: viewW / 2 - 30, y0: viewH - 48, x1: viewW / 2 + 30, y1: viewH - 42 },
    { name: 'mapAbove', x0: 2, y0: viewH - 130, x1: 62, y1: viewH - 124 }
  ], { width: viewW, height: viewH });
  console.log(`INFO  strip means ${JSON.stringify(means)}`);
  if (Math.abs(means.stripLeft - means.mapAbove) < 35) {
    throw new Error(`bar left edge looks like map, not strip (left=${means.stripLeft} map=${means.mapAbove})`);
  }
  console.log('PASS  statusbar strip pixels cover left edge');
}

// 重试打开技能书：每次尝试前等 blocking 清零，失败时输出命中诊断
// （点击是否落在状态栏命中区与技能按钮上）。
async function openSkillbook(label) {
  for (let attempt = 0; attempt < 6; attempt += 1) {
    await waitAssetsIdle(`${label} input ready (attempt ${attempt})`);
    await driver.click(skillbookButton.x, skillbookButton.y);
    await sleep(1000);
    if (await probes.skillbookActive() === 1) return;
    const hit = await driver.evaluate(
      `Module.ccall('msstatusbar_hit','number',['number','number'],[${skillbookButton.x},${skillbookButton.y}])`);
    const button = await driver.evaluate(
      `Module.ccall('msstatusbar_button_at','number',['number','number'],[${skillbookButton.x},${skillbookButton.y}])`);
    console.log(`INFO  ${label} skillbook attempt=${attempt} active=${await probes.skillbookActive()} hit=${hit} button=${button}`);
  }
  throw new Error(`skillbook did not open (${label})`);
}

// 遍历页签与四行图标，选中任意已学习非被动技能。
// 不断言具体技能 ID（不同存档/职业的可学技能不同），返回真实选中值。
async function selectAnySkill() {
  for (let tab = 0; tab < 5; tab += 1) {
    await driver.click(skillbookOrigin.x + tabCenters[tab], skillbookOrigin.y + 33);
    await sleep(350);
    for (let row = 0; row < 4; row += 1) {
      const center = skillIconCenter(row);
      await driver.click(center.x, center.y);
      await sleep(300);
      const selected = await probes.skillbookSelected();
      if (selected > 0) {
        console.log(`PASS  skill selected tab=${tab} row=${row} id=${selected}`);
        return { id: selected, tab, row };
      }
    }
  }
  throw new Error('no learned non-passive skill could be selected');
}

// The fourth-job book has ten rows; scroll its 4-row window and click each
// visible icon until the requested skill (Hero's Will) is selected.
async function selectFourthJobSkill(target) {
  await driver.click(skillbookOrigin.x + tabCenters[4], skillbookOrigin.y + 33);
  await sleep(350);
  for (let scroll = 0; scroll <= 6; scroll += 1) {
    if (scroll > 0) {
      // Scrollbar down arrow: window-relative (154, 236), 12x12 texture.
      await driver.click(skillbookOrigin.x + 160, skillbookOrigin.y + 242);
      await sleep(250);
    }
    for (let row = 0; row < 4; row += 1) {
      const center = skillIconCenter(row);
      await driver.click(center.x, center.y);
      await sleep(250);
      if (await probes.skillbookSelected() === target) {
        console.log(`PASS  fourth-job skill selected scroll=${scroll} row=${row} id=${target}`);
        return;
      }
    }
  }
  throw new Error(`skill ${target} not found in fourth-job book`);
}

try {
  const url = await resolvePageUrl();
  await driver.start();
  await driver.setNetworkLatency(0);
  await driver.navigate(url);
  await waitFor('module ready', async () => await driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true"
    + " && Module.ccall('msui_state','number',[],[]) > 0"), 90);
  // 分辨率档位在切入 GAME 状态时从 localStorage 读取，须在进图前写入。
  await driver.evaluate(`localStorage.setItem('maplestory.display.resolution', '${preset}')`);
  await waitFor('login', async () => await state() === uiState.login, 60);
  await waitFor('loading screen hidden', async () => await driver.evaluate(
    "document.getElementById('loading-screen').classList.contains('is-hidden')"), 90);
  await waitFor('input enabled', async () => await driver.evaluate(
    "Module.ccall('msui_enabled','number',[],[])") === 1, 60);
  await waitAssetsIdle('login assets idle');

  await login();
  await enterGame();
  await verifyStatusBarChrome();
  await verifyQuickslotLayout();

  equal('player id', await probes.playerId(), expectedPlayerId);
  equal('player level', await driver.evaluate(
    "Module.ccall('msplayer_level','number',[],[])"), expectedPlayerLevel);
  equal(`runtime resolution ${viewW}x${viewH}`, await driver.evaluate(`(() => {
    const n = name => Module.ccall(name, 'number', [], []);
    return [n('msui_resolution_width'), n('msui_resolution_height')];
  })()`), [viewW, viewH]);
  console.log(`INFO  hp ${await probes.playerHp()}/${await probes.playerMaxHp()}`
    + ` mp ${await probes.playerMp()}/${await probes.playerMaxMp()}`);
  equal('initial quickslots', await quickslots(), EMPTY_SLOTS);
  await hover(400, 300);
  await sleep(300);
  equal('initial hovered', await probes.quickslotHovered(), -1);

  if (skipSkills) {
    console.log('SKIP  skill binding section (QS_SKIP_SKILLS=1)');
  } else {
    await driver.screenshot('initial-hud');
    await openSkillbook('initial');
    await driver.screenshot('skillbook');

    const firstSkill = await selectAnySkill();
    equal('selected before assign', await probes.skillbookSelected(), firstSkill.id);

    const first = slotCenter(0);
    await hover(first.x, first.y);
    await sleep(300);
    equal('hover slot 0', await probes.quickslotHovered(), 0);
    await driver.click(first.x, first.y);
    await sleep(600);
    equal('slot 0 assigned', (await quickslots())[0], firstSkill.id);
    equal('other slots empty', (await quickslots()).slice(1), EMPTY_SLOTS.slice(1));
    equal('selection cleared after assign', await probes.skillbookSelected(), 0);
    equal('hover owns assigned slot', await probes.quickslotHovered(), 0);
    await waitAssetsIdle('slot 0 assign assets idle');
    await driver.screenshot('slot0-assigned');

    await hover(400, 300);
    await sleep(300);
    equal('hover leaves quickslot', await probes.quickslotHovered(), -1);
    await hover(first.x, first.y);
    await sleep(300);
    equal('hover returns to assigned slot', await probes.quickslotHovered(), 0);
    await driver.screenshot('slot0-tooltip');

    // 点击已绑定槽位经 Combat::use_move 施法；施法后的 LazyFS 加载期间
    // 不能派发右键，故先等 blocking 清零。
    await driver.click(first.x, first.y);
    await sleep(500);
    equal('use keeps slot 0', (await quickslots())[0], firstSkill.id);
    equal('use keeps selection cleared', await probes.skillbookSelected(), 0);
    equal('state healthy after use', await state(), uiState.game);
    await waitAssetsIdle('slot 0 use assets idle');
    await driver.rightClick(first.x, first.y);
    await sleep(500);
    equal('right click clears slot 0', await quickslots(), EMPTY_SLOTS);
    await driver.screenshot('slot0-cleared');

    await openSkillbook('second');
    const secondSkill = await selectAnySkill();
    const secondIcon = skillIconCenter(secondSkill.row);
    const last = slotCenter(7);
    // Settle the cursor on the rightmost slot before dragging. Besides
    // proving the target is reachable, this dismisses any pending skill
    // tooltip that can otherwise consume the drag's initial press.
    await hover(last.x, last.y);
    await sleep(300);
    equal('slot 7 target hovered', await probes.quickslotHovered(), 7);
    await driver.drag(secondIcon.x, secondIcon.y, last.x, last.y);
    await sleep(600);
    equal('slot 7 drag assigned', (await quickslots())[7], secondSkill.id);
    equal('slot 7 selection cleared', await probes.skillbookSelected(), 0);
    await waitAssetsIdle('slot 7 assign assets idle');
    await driver.screenshot('slot7-assigned');
    await driver.rightClick(last.x, last.y);
    await sleep(500);
    equal('right click clears slot 7', await quickslots(), EMPTY_SLOTS);
    await driver.screenshot('slot7-cleared');
  }

  if (skipCooldown) {
    console.log('SKIP  cooldown section (QS_SKIP_COOLDOWN=1)');
  } else {
    // Hero's Will (1121011) starts a server-driven cooldown. The quickslot
    // panel must render the remaining seconds over the skill icon.
    await openSkillbook('cooldown');
    await selectFourthJobSkill(1121011);

    const cooldownSlot = slotCenter(0);
    await driver.click(cooldownSlot.x, cooldownSlot.y);
    await sleep(600);
    equal('cooldown skill assigned', (await quickslots())[0], 1121011);
    await driver.screenshot('slot0-cooldown-assigned');
    await driver.click(cooldownSlot.x, cooldownSlot.y);
    await sleep(500);
    await waitAssetsIdle('cooldown use assets idle');
    await waitFor('quickslot cooldown active',
      async () => await probes.quickslotCooldown(0) > 0, 20);
    const cooldownSeconds = await probes.quickslotCooldown(0);
    const cooldownLabelLength = await probes.quickslotCooldownLabel(0);
    console.log(`INFO  quickslot cooldown ${cooldownSeconds}s label=${cooldownLabelLength}`);
    if (cooldownLabelLength <= 0) {
      throw new Error('quickslot cooldown label was not populated');
    }
    // Move off the slot so the skill tooltip does not overlap the count.
    await hover(400, 300);
    await sleep(350);
    await driver.screenshot('slot0-cooldown');
    await driver.rightClick(cooldownSlot.x, cooldownSlot.y);
    await sleep(500);
    equal('right click clears cooldown slot', await quickslots(), EMPTY_SLOTS);
  }

  if (skipItems) {
    console.log('SKIP  item binding section (QS_SKIP_ITEMS=1)');
  } else {
    await verifyItemBinding();
  }

  equal('state final', await state(), uiState.game);
  if (driver.errors.length) throw new Error(`browser errors: ${driver.errors.join('\n')}`);
  await driver.screenshot('final');
  console.log(`DONE  preset=${preset} ${viewW}x${viewH}`);
} finally {
  fs.writeFileSync(`${artifactDir}/console.log`, driver.logs.join('\n'));
  fs.writeFileSync(`${artifactDir}/errors.log`, driver.errors.join('\n'));
  await driver.stop();
}
