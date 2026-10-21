// 现金商城「装备」分类二级菜单（装备子分类浮层）的浏览器专项验收。
//
// 用途：
//   验证新加的装备分类菜单：点击左侧分类栏「装备」展开浮层（全部装备/帽子/
//   脸饰/眼饰/耳环/上衣/套装/裤子/鞋子/手套/盾牌/披风/戒指/坠饰/腰带/勋章/
//   武器），选择子分类后目录按物品 ID 槽位过滤、数量摘要变化；「×」或再次
//   点击「装备」收起；切换回「全部」分类时菜单收起且子分类复位。
//
//   断言分两层：
//   1) 只读状态探针（Module.ccall）：mscashshop_equip_menu_open /
//      mscashshop_equip_sub / mscashshop_filtered_count 驱动全部状态转换；
//   2) 页面内 WebGL readPixels 目检：菜单展开时选中行是暖橙色高亮
//      （R 明显高于 B），收起后同一带不再出现橙色高亮。
//
// 运行前置：
//   1. 客户端已构建：./scripts/build_wasm.sh
//   2. Web 栈运行中（web-server :8000；ws-proxy :8080；assets-server :8765）
//      且游戏服务器可登录。
//
// Run:   node tools/verify/maplestory-webui/cash-shop-equip-flow.mjs
// Env:
//   CHROME_BIN              浏览器（默认 /usr/bin/chromium-browser）
//   CASH_EQUIP_URL          页面（默认 http://127.0.0.1:8000/web/index.html）
//   CASH_EQUIP_DEBUG_PORT   调试端口（默认 9249）
//   CASH_EQUIP_ACCOUNT / CASH_EQUIP_PASSWORD   登录账号（默认 test1/test1）
//   CASH_EQUIP_CARD         角色卡索引 0..3（默认 1）
//
// Exit code 0 = all checks passed.

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.CASH_EQUIP_DEBUG_PORT || 9249);
const account = process.env.CASH_EQUIP_ACCOUNT || 'test1';
const password = process.env.CASH_EQUIP_PASSWORD || 'test1';
const cardIndex = Number(process.env.CASH_EQUIP_CARD || 1);
const pageUrl = process.env.CASH_EQUIP_URL || 'http://127.0.0.1:8000/web/index.html';

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });

// 角色卡中心 = game(105 + 120*i + 25, 210)。
const cardCenter = index => ({ x: 105 + 120 * index + 25, y: 210 });

// StatusBar2 BtCashShop：x=569..625, y=554..589（与 run.mjs 现金商城流程一致）。
const cashShopButton = Object.freeze({ x: 597, y: 571 });

// 左侧分类栏条目：x=4..94, 第 i 项 y = 40 + 34*i, 高 24。
const railEntry = index => ({ x: 49, y: 40 + 34 * index + 12 });
const rail = Object.freeze({ all: railEntry(0), equip: railEntry(1) });

// 装备子分类浮层：x=100..196, 表头 y=86..106（「×」在 x178..192 y88..104），
// 条目区从 y=106 起每行 22px，第 sub 行中心 (148, 106 + 22*sub + 11)。
const equipSub = sub => ({ x: 148, y: 106 + 22 * sub + 11 });
const subCategories = Object.freeze({
  all: equipSub(0), hat: equipSub(1), weapon: equipSub(16)
});
const equipClose = Object.freeze({ x: 185, y: 96 });

// 选中行暖橙高亮 ≈ (0.88, 0.52, 0.16)（≈RGB 224,133,41）：整行均值应满足
// R > G > B 且 R-B 足够大；文字为黑色只会拉低均值、不破坏色序。
const warmRow = Object.freeze({ x0: 110, x1: 186, minRedBlueDelta: 40, minRed: 120 });
const selectedRowBand = sub => ({ x0: warmRow.x0, x1: warmRow.x1, y0: 106 + 22 * sub + 2, y1: 106 + 22 * sub + 20 });
// readPixels 采样区需覆盖以上所有区域（game 坐标，排他右/下边界）。
const sampleRect = Object.freeze({ x0: 96, x1: 200, y0: 82, y1: 484 });
// 中性点击点：浮层与分类栏之外的商品卡第 3 列空白，用于采样前挪走光标精灵。
const clickAwayPoint = Object.freeze({ x: 450, y: 300 });

let failed = false;

function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}

async function waitFor(predicate, seconds = 45) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}

async function clickAway(driver) {
  await driver.click(clickAwayPoint.x, clickAwayPoint.y);
  await sleep(400);
}

function probe(driver, name) {
  return driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
}

function uiStateIs(driver, expected) {
  return () => probe(driver, 'msui_state').then(value => value === expected);
}

function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

async function evaluateAwait(driver, expression) {
  const result = await driver.send('Runtime.evaluate', {
    expression, awaitPromise: true, returnByValue: true
  });
  if (result.exceptionDetails) {
    throw new Error(`page evaluation failed: ${result.exceptionDetails.text}`);
  }
  return result.result.value;
}

// 采样浮层区域内若干行的均值 RGB；保持并恢复 FRAMEBUFFER 绑定。
function samplerExpression(bands) {
  const config = JSON.stringify({ rect: sampleRect, bands });
  return `(() => new Promise(resolve => {
    requestAnimationFrame(() => {
      const config = ${config};
      const canvas = document.getElementById('canvas');
      const gl = canvas.getContext('webgl2') || canvas.getContext('webgl')
        || canvas.getContext('experimental-webgl');
      if (!gl) { resolve({ error: 'game canvas exposes no WebGL context' }); return; }
      const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
      const mapX = value => Math.round(value * scaleX);
      const mapY = value => Math.round(value * scaleY);
      const bounds = config.rect;
      const x0 = mapX(bounds.x0), x1 = mapX(bounds.x1);
      const y0 = mapY(bounds.y0), y1 = mapY(bounds.y1);
      const width = x1 - x0, height = y1 - y0;
      const pixels = new Uint8Array(width * height * 4);
      const boundFrameBuffer = gl.getParameter(gl.FRAMEBUFFER_BINDING);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.readPixels(x0, canvas.height - y1, width, height,
        gl.RGBA, gl.UNSIGNED_BYTE, pixels);
      gl.bindFramebuffer(gl.FRAMEBUFFER, boundFrameBuffer);
      const round = value => Math.round(value * 100) / 100;
      const rows = {};
      for (const band of config.bands) {
        let r = 0, g = 0, b = 0, n = 0;
        for (let y = band.y0; y <= band.y1; y += 1)
          for (let x = band.x0; x <= band.x1; x += 1) {
            const offset = ((height - 1 - (mapY(y) - y0)) * width +
              (mapX(x) - x0)) * 4;
            r += pixels[offset]; g += pixels[offset + 1]; b += pixels[offset + 2];
            n += 1;
          }
        rows[band.name] = { r: round(r / n), g: round(g / n), b: round(b / n) };
      }
      resolve({ rows });
    });
  }))()`;
}

function sampleRows(driver, bands) {
  return evaluateAwait(driver, samplerExpression(bands));
}

function isWarmHighlight(row) {
  return row && row.r > row.g && row.g > row.b &&
    row.r - row.b > warmRow.minRedBlueDelta && row.r > warmRow.minRed;
}

async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90)) {
    throw new Error('WASM module did not initialize');
  }
  if (!await waitFor(uiStateIs(driver, uiState.login), 30)) {
    throw new Error('login UI did not activate');
  }
  console.log('PASS  login UI active');
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 45)) {
    throw new Error('world-select UI did not activate; check CASH_EQUIP_ACCOUNT/CASH_EQUIP_PASSWORD');
  }
  console.log('PASS  world-select UI active');
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 45)) {
    throw new Error('character-select UI did not activate');
  }
  console.log('PASS  character-select UI active');
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('character-select assets never reached idle');
  }
  const center = cardCenter(cardIndex);
  await driver.screenshot('10-equip-flow-character-card');
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) {
    throw new Error('game field did not activate; check CASH_EQUIP_CARD points at a valid character');
  }
  console.log('PASS  game field active');
  if (!await waitFor(assetsIdle(driver), 90)) {
    throw new Error('in-game assets never reached idle');
  }
}

async function enterCashShop(driver) {
  await driver.click(cashShopButton.x, cashShopButton.y);
  if (!await waitFor(uiStateIs(driver, uiState.cashShop), 30)) {
    throw new Error('cash-shop UI did not activate');
  }
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('cash-shop assets never reached idle');
  }
  console.log('PASS  cash-shop UI active');
}

// 等待菜单探针进入期望状态（展开/收起/子分类索引）。
function menuStateIs(driver, { open, sub }) {
  return async () => await probe(driver, 'mscashshop_equip_menu_open') === open &&
    await probe(driver, 'mscashshop_equip_sub') === sub;
}

async function verifyEquipMenu(driver) {
  const filteredCount = () => probe(driver, 'mscashshop_filtered_count');
  const initial = await filteredCount();
  report(initial > 0, `catalog has on-sale items before filtering (filtered=${initial})`);
  report(await probe(driver, 'mscashshop_equip_menu_open') === 0,
    'equipment menu closed initially');

  // 1. 点击「装备」：选中装备类并展开浮层。
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(500);
  if (!await waitFor(menuStateIs(driver, { open: 1, sub: 0 }), 10)) {
    report(false, 'equipment menu did not open with sub=全部装备');
    return;
  }
  const equipAll = await filteredCount();
  report(equipAll > 0 && equipAll <= initial,
    `装备 category selected, menu open (equip-all=${equipAll} of ${initial})`);
  await clickAway(driver);
  await driver.screenshot('11-equip-menu-open');

  // 选中行「全部装备」应为暖橙高亮。
  const openSample = await sampleRows(driver,
    [{ name: 'selected', ...selectedRowBand(0) }, { name: 'other', ...selectedRowBand(2) }]);
  if (openSample.error) {
    report(false, `pixel sample failed: ${openSample.error}`);
  } else {
    report(isWarmHighlight(openSample.rows.selected),
      `selected row is warm highlight (rgb=${JSON.stringify(openSample.rows.selected)}); ` +
        `unselected row rgb=${JSON.stringify(openSample.rows.other)}`);
  }

  // 2. 选择「武器」（最后一个子分类）：选中后浮层自动收起。
  await driver.click(subCategories.weapon.x, subCategories.weapon.y);
  await sleep(500);
  if (!await waitFor(menuStateIs(driver, { open: 0, sub: 16 }), 10)) {
    report(false, 'weapon sub-category selected and flyout auto-closed');
    return;
  }
  const weaponCount = await filteredCount();
  report(weaponCount >= 0 && weaponCount <= equipAll,
    `weapon filter applied, flyout folded (weapons=${weaponCount} <= equip-all=${equipAll})`);
  await driver.screenshot('12-equip-weapon-selected');

  // 3. 重新展开后选择「帽子」：同样自动收起。
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(400);
  await driver.click(subCategories.hat.x, subCategories.hat.y);
  await sleep(500);
  if (!await waitFor(menuStateIs(driver, { open: 0, sub: 1 }), 10)) {
    report(false, 'hat sub-category selected and flyout auto-closed');
    return;
  }
  const hatCount = await filteredCount();
  report(hatCount >= 0 && hatCount <= equipAll,
    `hat filter applied, flyout folded (hats=${hatCount} <= equip-all=${equipAll})`);
  await driver.screenshot('13-equip-hat-selected');

  // 4. 重新展开后回到「全部装备」：数量应恢复、浮层收起。
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(400);
  await driver.click(subCategories.all.x, subCategories.all.y);
  await sleep(500);
  const restored = await filteredCount();
  report(await waitFor(menuStateIs(driver, { open: 0, sub: 0 }), 10) &&
    restored === equipAll,
    `全部装备 restored, flyout folded (filtered=${restored} === equip-all=${equipAll})`);

  // 5. 展开后用「×」收起：过滤保持，菜单关闭。
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(400);
  await driver.click(equipClose.x, equipClose.y);
  await sleep(500);
  const closedCount = await filteredCount();
  report(await waitFor(menuStateIs(driver, { open: 0, sub: 0 }), 10) &&
    closedCount === equipAll,
    `close button collapses menu, filter kept (filtered=${closedCount})`);
  await clickAway(driver);
  await driver.screenshot('14-equip-menu-closed');
  const closedSample = await sampleRows(driver,
    [{ name: 'weaponBand', ...selectedRowBand(16) }]);
  if (closedSample.error) {
    report(false, `closed-state pixel sample failed: ${closedSample.error}`);
  } else {
    report(!isWarmHighlight(closedSample.rows.weaponBand),
      `weapon band no longer warm-highlight when closed (rgb=${JSON.stringify(closedSample.rows.weaponBand)})`);
  }

  // 6. 分类栏「装备」可重新展开，再次点击可收起（toggle）。
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(400);
  const reopened = await waitFor(menuStateIs(driver, { open: 1, sub: 0 }), 10);
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(400);
  const collapsed = await waitFor(menuStateIs(driver, { open: 0, sub: 0 }), 10);
  report(reopened && collapsed, 'rail 装备 toggles the menu open and closed');

  // 7. 切回「全部」分类：菜单收起、子分类复位、目录数量恢复初始值。
  await driver.click(rail.all.x, rail.all.y);
  await sleep(500);
  const backToAll = await filteredCount();
  report(await waitFor(menuStateIs(driver, { open: 0, sub: 0 }), 10) &&
    backToAll === initial,
    `全部 category restores catalog (filtered=${backToAll} === initial=${initial})`);
  await driver.screenshot('15-equip-flow-all-restored');
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await enterCashShop(driver);
    await verifyEquipMenu(driver);
    // Escape 触发商城退出（与 run.mjs 现金商城流程一致）。
    await driver.key('Escape', 'Escape', 27);
    report(await waitFor(uiStateIs(driver, uiState.game), 30),
      'cash shop exits back to game');
    await driver.screenshot('16-equip-flow-exited');
  } catch (error) {
    report(false, `cash shop equip flow aborted: ${error.message}`);
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
  console.log(`FAIL  CASH_EQUIP_CARD must be a card index 0..3, got ${process.env.CASH_EQUIP_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
