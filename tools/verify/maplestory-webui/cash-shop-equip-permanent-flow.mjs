// 现金商城「现金装备永久」浏览器端到端验收。
//
// 新契约（现金装备由 90 天租用改为永久，客户端+服务端双侧）：
//   1) 客户端商城模型把装备类商品（itemid/1000000==1，见 CashShopView.h
//      的 effective_period）period 归一为 0：左栏目录卡片 meta 与右下
//      「本期推荐」面板 meta 均显示「永久」，不再出现「90天」，也不挂
//      「限时」徽章（LIMITED 徽章仅 period<=30 才有）。
//   2) 服务端购买 EQUIP 现金商品不再写过期时间戳：购买 → 商城仓库取出
//      到 CASH 栏后，inventoryitems 新行 expiration=-1（永久，本脚本只
//      负责链路与取证，DB 终检由外部 mysql 步骤完成）。
//
// 本脚本覆盖：登录 test1 → 进商城（余额就绪）→ 校验 NX ≥ 3700 →
//   SN 20000016 搜索过滤唯一（1002000 帽子）→ 全页+卡片+推荐面板截图
//   （2x 裁剪图供 tesseract chi_sim OCR 「永久」）→ 双击卡片购买 →
//   确认框 OK → 仓库 +1 → 双击仓库格取出 → CASH 栏 +1 → ESC 退出回
//   游戏（触发服务端存档，patch 0006）。
// 探针语义沿用 cash-shop-take-equip-flow.mjs：mscashshop_* 系列 +
// msinv_slot_item(invtype,slot)（现金装备 1002000 属 CASH 栏=5）。
// 前置：账号 NX Credit ≥ 3700（不足则如实上报，不做 DB 造假）；角色离线。
// Run: CHROME_BIN=/usr/bin/chromium-browser node \
//   tools/verify/maplestory-webui/cash-shop-equip-permanent-flow.mjs

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const artifactDir = process.env.PERMANENT_ARTIFACT_DIR || '/tmp/cash-shop-permanent';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.PERMANENT_DEBUG_PORT || 9280);
const pageUrl = process.env.PERMANENT_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.PERMANENT_ACCOUNT || 'test1';
const password = process.env.PERMANENT_PASSWORD || 'test1';
// take-equip-flow 默认卡位 1；test1 当前只有 1 个角色（卡 0），因此默认
// 卡位进不去时会回退卡 0，保持与兄弟脚本同款坐标与语义。
const cardIndex = Number(process.env.PERMANENT_CARD || 1);
const itemId = Number(process.env.PERMANENT_ITEM_ID || 1002000);
const sn = process.env.PERMANENT_SN || '20000016';
const price = Number(process.env.PERMANENT_PRICE || 3700);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const inventoryTypeCash = 5;
// CashShopLayout.h：仓库右栏 7x3 格 (517,88)/39；卡片网格 GRID_POS{100,88}
// 步距 {101,148}、卡片 93x144（meta 文案在卡片内 (46,99)）；
// 「本期推荐」面板 FEATURED_PANEL_POS{508,396} 尺寸 288x196（meta 在
// (624,486)，标题在 (516,400)）。
const rightGrid = Object.freeze({ x: 517, y: 88, pitch: 39, hit: 18 });
const cardRect = Object.freeze({ x: 95, y: 78, w: 130, h: 172 });
const featuredRect = Object.freeze({ x: 504, y: 392, w: 296, h: 204 });
const charCard = index => ({ x: 105 + 120 * index + 25, y: 210 });

let failed = false;
const steps = [];
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
function step(name, ok) {
  steps.push({ name, ok });
  return report(ok, name);
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

// ---- 截图取证：全页 PNG + 按游戏坐标裁剪（clip.scale=2 便于 OCR） ----
async function canvasBox(driver) {
  const box = await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const b = canvas.getBoundingClientRect();
    return { left: b.left, top: b.top, width: b.width, height: b.height,
      gameW: canvas.width, gameH: canvas.height };
  })()`);
  fs.writeFileSync(`${artifactDir}/geometry.json`, JSON.stringify(box));
  return box;
}
async function shot(driver, name, gameRect = null, scale = 1) {
  const params = { format: 'png' };
  if (gameRect) {
    const box = await canvasBox(driver);
    const sx = box.width / box.gameW;
    const sy = box.height / box.gameH;
    params.clip = {
      x: box.left + gameRect.x * sx,
      y: box.top + gameRect.y * sy,
      width: gameRect.w * sx,
      height: gameRect.h * sy,
      scale
    };
  }
  const result = await driver.send('Page.captureScreenshot', params);
  fs.writeFileSync(`${artifactDir}/${name}.png`, Buffer.from(result.data, 'base64'));
}
async function settleShot(driver, name, gameRect = null, scale = 1) {
  // 截图前等按需加载屏退净再稳定 1.5s，避免半透明遮罩污染 OCR。
  if (!await waitFor(assetsIdle(driver), 90))
    throw new Error(`assets not idle before ${name}`);
  await sleep(1500);
  await shot(driver, name, gameRect, scale);
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
  let entered = false;
  for (const index of [...new Set([cardIndex, 0])]) {
    const center = charCard(index);
    for (let attempt = 0; attempt < 2 && !entered; attempt += 1) {
      await driver.doubleClick(center.x, center.y);
      for (let i = 0; i < 5 && !entered; i += 1) {
        entered = await uiStateIs(driver, uiState.game)();
        if (!entered) await sleep(1000);
      }
    }
    if (entered) break;
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
  await requireState('cash-shop balances ready', async () =>
    await probe(driver, 'mscashshop_balances_ready') === 1 &&
    await probe(driver, 'mscashshop_pending') === 0, 30);
}

async function searchSn(driver) {
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
    await probe(driver, 'mscashshop_highlighted_item') === itemId, 15);
}

async function buyCap(driver, before) {
  // 双击卡片：第一次单击选中（featured 面板切「试穿预览」），第二次单击
  // 弹出购买确认框；OK 按钮在确认框 (380,285)，分辨率>800x600 时按
  // pet-flow 同款居中偏移修正。
  await driver.click(155, 120);
  await requireState('catalog card selected', async () =>
    await probe(driver, 'mscashshop_selected_left') >= 0, 10);
  await sleep(600);
  await settleShot(driver, '05-card-selected-full');
  await settleShot(driver, '06-featured-selected-crop', featuredRect, 2);
  await driver.click(155, 120);
  await requireState('purchase confirmation visible', async () =>
    await probe(driver, 'msui_notice_active') === 1, 10);
  await sleep(500);
  await shot(driver, '07-purchase-confirm');
  const width = await probe(driver, 'msui_resolution_width');
  const height = await probe(driver, 'msui_resolution_height');
  const uiX = Math.max(0, Math.floor((width - 800) / 2));
  const uiY = Math.max(0, Math.floor((height - 600) / 2));
  await driver.click(380 + uiX, 285 + uiY);
  await requireState('purchase settles into locker', async () =>
    await probe(driver, 'mscashshop_pending') === 0 &&
    await probe(driver, 'mscashshop_nx_credit') === before.nx - price &&
    await probe(driver, 'mscashshop_locker_count') === before.locker + 1, 30);
}

async function takeCap(driver, before) {
  const row = await probe(driver, 'mscashshop_selected_right_row');
  if (row < 0) throw new Error('purchased locker row was not selected');
  const x = rightGrid.x + (row % 7) * rightGrid.pitch + rightGrid.hit;
  const y = rightGrid.y + Math.floor(row / 7) * rightGrid.pitch + rightGrid.hit;
  await driver.doubleClick(x, y);
  await requireState('double-click takes item exactly once', async () =>
    await probe(driver, 'mscashshop_pending') === 0 &&
    await probe(driver, 'mscashshop_locker_count') === before.locker &&
    await probe(driver, 'mscashshop_inventory_count') === before.inventory + 1, 30);
  let slot = 0;
  for (let candidate = 1; candidate <= 24; candidate += 1) {
    if (await probe(driver, 'msinv_slot_item', [inventoryTypeCash, candidate]) === itemId) {
      slot = candidate;
      break;
    }
  }
  step(`taken equip lands on the CASH tab (slot=${slot})`, slot >= 1);
}

async function exitCashShop(driver) {
  await driver.key('Escape', 'Escape', 27);
  await requireState('cash shop exits back to game', uiStateIs(driver, uiState.game), 30);
  await requireState('returned map assets ready', assetsIdle(driver), 60);
  await requireState('game UI enabled', async () =>
    await probe(driver, 'msui_enabled') === 1, 60);
  await sleep(2000); // patch 0006：退出商城即触发服务端存档，留出落库时间
}

async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await enterCashShop(driver);
    const before = {
      locker: await probe(driver, 'mscashshop_locker_count'),
      inventory: await probe(driver, 'mscashshop_inventory_count'),
      nx: await probe(driver, 'mscashshop_nx_credit')
    };
    step(`shop baselines captured (locker=${before.locker}, inventory=${before.inventory}, nx=${before.nx})`,
      before.locker >= 0 && before.inventory >= 0 && before.nx >= 0);
    if (before.nx < price) throw new Error(
      `NX credit ${before.nx} < price ${price}; refusing to fabricate balance`);
    console.log(`PASS  NX credit ${before.nx} covers price ${price}`);
    await settleShot(driver, '01-shop-entered-full');
    await searchSn(driver);
    await settleShot(driver, '02-sn-filtered-full');
    await settleShot(driver, '03-card-crop', cardRect, 2);
    await settleShot(driver, '04-featured-crop', featuredRect, 2);
    await buyCap(driver, before);
    await settleShot(driver, '08-purchased-locker-full');
    await takeCap(driver, before);
    await settleShot(driver, '09-taken-full');
    await exitCashShop(driver);
    await shot(driver, '10-exited-game-full');
    step('decorative cash-equip permanent contract holds end to end', !failed);
  } catch (error) {
    step(`cash shop equip-permanent flow aborted: ${error.message}`, false);
    console.error(error);
    try { await shot(driver, 'zz-failure'); } catch (shotError) {}
  } finally {
    if (driver.errors.length) {
      fs.writeFileSync(`${artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    }
    fs.writeFileSync(`${artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${artifactDir}`);
    await driver.stop();
  }
  console.log(JSON.stringify({ ok: !failed, steps }));
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  PERMANENT_CARD must be a card index 0..3, got ${process.env.PERMANENT_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
