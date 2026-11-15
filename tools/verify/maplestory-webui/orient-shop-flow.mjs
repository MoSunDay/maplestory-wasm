// 东方神州（上海）与泰国六家商店 NPC 的商店 UI E2E（shops/shopitems 数据
// 由服务端 sql/custom_orient_shops.sql 提供，shopid==npcid；数据生成与双侧
// 物品校验见 tools/build_orient_shops_sql.py）。
//
// 六家（NPC x 取自 wz Map life，商店分类由 079 模板映射）：
//   上海 701000000：
//     金女士 9310003 杂货 x=-128   街面 y=316
//     张老板 9310002 防具 x=-1380  街面 y=316
//     李女士 9310001 武器 x=-1556  遮阳棚 y=233（街面跳上 fh72 y=273 后，
//       在 fh152 跨度(-1591..-1513)左侧的 -1610 再跳右一段；fromX 不能越过
//       fh72 左缘 -1621，否则走回街面）
//   泰国 500000000：
//     诺伊 9250009 防具 x=-1953    栈道 y=68（底部 y=152 起跳 84px 直达）
//     丹 9250007 武器 x=1539       东栈道 y=68（x=1600 向左跳上 fh34 右缘）
//     帕恩 9250008 杂货 x=-3059    桅顶 y=9（fh57 y=91 → 西栈道 68 → fh18）
//   起跳实测：跳跃高度 ~82px（底部 152→68=84px 借边缘可落）；玩家与 NPC
//   对话要求 |dx|<=120 且 |dy|<=80，各 NPC 的 cy 见上面 y 值。
//
// 断言：Space 触发后 msshop_active=1；msshop_buy_item 前 8 行至少 1 行
// 非零（首屏 5 行：服务端 ORDER BY position DESC，首行是 position 最大
// 的商品）；EXIT 按钮坐标非零；Escape 关闭。每家一次独立登录（换图需要）。
//
// 注意：服务端登出会把 spawnpoint 重置回 0，所以每轮登录前都要重写
// （prepCharacter 已覆盖）；上海用 sp3（-496,253 → 落到街面），泰国用
// sp0（-293,10 → 土丘滑坡到底部 y=152）。
//
// 运行（凭据不入仓库；并发批次时换专用账号）：
//   ORIENT_SHOP_ACCOUNT=orient1 ORIENT_SHOP_PASSWORD=orient1 \
//   ORIENT_SHOP_CHAR_ID=289 node tools/verify/maplestory-webui/orient-shop-flow.mjs
//   # 只跑其中一家：ORIENT_SHOP_ONLY=李女士
import { sleep } from './cdp.mjs';
import { probe, waitFor } from './pq-harness-game.mjs';
import { walkTowards } from './pq-harness-move.mjs';
import { prepCharacter, waitAccountFree } from './pq-harness-db.mjs';
import { startSession } from './pq-harness-login.mjs';

const account = process.env.ORIENT_SHOP_ACCOUNT || 'orient1';
const password = process.env.ORIENT_SHOP_PASSWORD || 'orient1';
const charId = Number(process.env.ORIENT_SHOP_CHAR_ID || 289);
const only = process.env.ORIENT_SHOP_ONLY || '';
const debugPort = Number(process.env.ORIENT_SHOP_DEBUG_PORT || 9292);

// stages: 依次「站在 fromX、向 dir 跳上 y」的平台换层序列
const ROUTES = [
  { label: '金女士9310003', map: 701000000, spawnpoint: 3, stages: [], x: -128 },
  { label: '张老板9310002', map: 701000000, spawnpoint: 3, stages: [], x: -1380 },
  { label: '李女士9310001', map: 701000000, spawnpoint: 3,
    stages: [{ from: -1660, y: 273, dir: 1 }, { from: -1610, y: 233, dir: 1 }], x: -1556 },
  { label: '诺伊9250009', map: 500000000, spawnpoint: 0,
    stages: [{ from: -1950, y: 68, dir: -1 }], x: -1953 },
  { label: '丹9250007', map: 500000000, spawnpoint: 0,
    stages: [{ from: 1600, y: 68, dir: -1 }], x: 1539 },
  { label: '帕恩9250008', map: 500000000, spawnpoint: 0,
    stages: [{ from: -2580, y: 91, dir: -1 }, { from: -2780, y: 68, dir: 1 },
      { from: -3110, y: 9, dir: 1 }], x: -3059 },
];

let failed = false;
const report = (ok, msg) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${msg}`); if (!ok) failed = true; };
const playerY = async driver => probe(driver, 'msplayer_y');

async function keyDown(driver, key, code, vk) {
  await driver.send('Input.dispatchKeyEvent', {
    type: 'keyDown', key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk });
}
async function keyUp(driver, key, code, vk) {
  await driver.send('Input.dispatchKeyEvent', {
    type: 'keyUp', key, code, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk });
}

// 边走边跳：先按住方向获得水平速度，起跳后继续漂移（起跳前若无水平速度，
// 起跳后再按方向的水平位移不足，无法落到平台边缘上）
async function runningJump(driver, dir, preMs, airMs) {
  const key = dir > 0 ? 'ArrowRight' : 'ArrowLeft';
  const vk = dir > 0 ? 39 : 37;
  await keyDown(driver, key, key, vk);
  await sleep(preMs);
  await keyDown(driver, 'Alt', 'AltLeft', 18);
  await sleep(90);
  await keyUp(driver, 'Alt', 'AltLeft', 18);
  await sleep(airMs);
  await keyUp(driver, key, key, vk);
  await sleep(400);
}

async function jumpToLedge(driver, label, fromX, wantY, dir, tries = 10) {
  // 走位失败（如中途掉层卡住）不致命：保持原位直接开始起跳尝试
  try { await walkTowards(driver, fromX, 90); } catch { /* 见上 */ }
  for (let i = 0; i < tries; i += 1) {
    await runningJump(driver, dir, 60 + (i % 3) * 30, 240 + (i % 3) * 40);
    const y = await playerY(driver);
    if (Math.abs(y - wantY) <= 6) {
      report(true, `${label}: 跳上平台 y=${y}`);
      return true;
    }
    try { await walkTowards(driver, fromX, 90); } catch { /* 同上 */ }
  }
  report(false, `${label}: 跳上平台失败（y=${await playerY(driver)} 期望 ${wantY}）`);
  return false;
}

async function openAndVerifyShop(driver, label) {
  for (let i = 0; i < 4; i += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msshop_active') === 1, 6)) break;
  }
  if (!(await probe(driver, 'msshop_active') === 1)) {
    report(false, `${label}: 商店 UI 打开`);
    return;
  }
  report(true, `${label}: 商店 UI 打开`);
  await sleep(5000); // 等 UI.nx 图标流式加载稳定
  const rows = [];
  for (let r = 0; r < 8; r += 1) {
    rows.push(await driver.evaluate(
      `Module.ccall('msshop_buy_item', 'number', ['number'], [${r}])`));
  }
  report(rows.filter(id => id > 0).length > 0,
    `${label}: 商品行渲染（rows=${JSON.stringify(rows)}）`);
  const bx = await driver.evaluate(
    "Module.ccall('msshop_button_x', 'number', ['number'], [2])");
  const by = await driver.evaluate(
    "Module.ccall('msshop_button_y', 'number', ['number'], [2])");
  report(bx > 0 && by > 0, `${label}: EXIT 按钮坐标 (${bx},${by})`);
  await driver.screenshot(`orient-shop-${label}`);
  await driver.key('Escape', 'Escape', 27);
  report(await waitFor(async () => await probe(driver, 'msshop_active') === 0, 8),
    `${label}: Escape 关闭商店`);
}

async function runRoute(route) {
  if (!(await waitAccountFree(account, 60))) {
    report(false, `${route.label}: 账号 ${account} 登录态未释放`);
    return;
  }
  prepCharacter({ account, charId, map: route.map, spawnpoint: route.spawnpoint });
  let driver = null;
  try {
    driver = await startSession({
      binary: process.env.CHROME_BIN || '/usr/bin/chromium-browser',
      debugPort, artifactDir: `/tmp/orient-shop-flow-${route.label}`,
      account, password, cardIndex: 0,
      pageUrl: process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html',
    });
    // 冷启动首次登录时 assetsIdle 可能早于地图字段初始化（msmap_id=0），
    // 先等地图就绪再比对，避免把竞态误报成登录失败
    await waitFor(async () => (await probe(driver, 'msmap_id')) !== 0, 120);
    const map = await probe(driver, 'msmap_id');
    report(map === route.map, `${route.label}: 登录地图 = ${route.map}（实测 ${map}）`);
    if (map !== route.map) return;
    await sleep(2000);
    for (const stage of route.stages) {
      if (!(await jumpToLedge(driver, route.label, stage.from, stage.y, stage.dir))) return;
    }
    const x = await walkTowards(driver, route.x, 90);
    report(Math.abs(x - route.x) <= 15,
      `${route.label}: 到位 x=${x} y=${await playerY(driver)}`);
    await openAndVerifyShop(driver, route.label);
  } catch (error) {
    report(false, `${route.label}: flow aborted: ${error.message}`);
  } finally {
    if (driver) await driver.stop().catch(() => {});
  }
}

for (const route of ROUTES) {
  if (only && !route.label.includes(only)) continue;
  await runRoute(route);
}
if (failed) process.exitCode = 1;
