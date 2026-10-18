// GM 聊天命令开商店的几何目检流程。
//
// 用途：
//   `npc-shop-flow.mjs`（走位 + 双击 NPC 开店）对环境敏感：走位完成后商店可能
//   不弹出（NPC 对话框正常）。本脚本改用 GM 聊天命令 `!gmshop`（服务端
//   HeavenMS GmShopCommand，gm2 级，打开 shop 1337）稳定开店，再用页面内
//   WebGL readPixels 做纯几何断言：商店窗口左边缘竖线、USE/EQUIP 页签互变、
//   售物行暗像素。全程不移动角色，也不依赖 NPC 寻路。
//
// 用法示例：
//   GM_SHOP_ACCOUNT=test1 GM_SHOP_PASSWORD=test1 GM_SHOP_CARD=1 \
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/gm-shop-flow.mjs
//
// 前置条件：
//   * 所选角色的 characters.gm 必须 ≥ 2（注意是 characters 表的 gm 列，而不是
//     accounts 表）。提权：MySQL> update characters set gm=2 where id=<该角色id>;
//     验证完成后还原：update characters set gm=0 where id=<该角色id>;
//   * 账号 test1/test1 实测登录进的是账号 test1test1(id 26)，其角色列表为
//     card0=e2efix1 card1=wt1210 card2=验证员一，与直觉的账号 25 不同；选卡前
//     先核对 GM_SHOP_CARD 指向的角色（看 10-gm-shop-character-card.png 截图）。
//   * 页面服务需在 http://127.0.0.1:8000/web/index.html（GM_SHOP_URL 可覆盖）。
//
// 几何依据（800×600 游戏坐标系；页面 canvas 位于 page(79,0) 尺寸 662×497，页面
// 尺寸 820×497，例如边缘 game x174 对应 page x = 79+174*662/800；本脚本直接对
// 800×600 canvas backing buffer 做 readPixels，坐标即游戏坐标）：
//   * 商店窗口：UIShop 以 backgrnd(465×328) 居中 → 窗口原点 game(168,76)；
//     backgrnd3(451×254, origin(-7,-64)) 实际画面 game(175,140)-(626,394)，左
//     边缘竖线落在 game x≈174/175。
//   * 售物行槽：window-local y=115+42k（k=0..4）→ game y 191/233/275/317/359/401
//     为槽线，42 步进；行命中框 game x410..610。
//   * USE 页签（第 2 个 TabSell，index 1）TwoSpriteButton bounds = 窗口原点 +
//     (-43,-9) - 位图 origin(-327,-100) = window-local(284,91)，尺寸 42×19 →
//     中心 game(473,176)；EQUIP 页签中心 game(430,176)。
//   * 实测校准：EQUIP 页签选中/未选中两套位图均值差仅 ~1.5，双区均值差 > 3 的
//     强判据在真实渲染上不成立，页签判定退回可逆切换验证（USE 页签名均值先显
//     著变化再点回 EQUIP 复原）；空行槽框最暗像素约 119、物品描边接近 0，物品
//     暗像素阈值取 100。
//   * 实测校准：游戏内光标精灵绘制在点击点右下方（约 +0..+50/+14..+34px），
//     点 USE 页签 (473,176) 后光标恰好落入售物行采样区，空 USE 栏也会被误判出
//     纯黑“物品”像素；任何点击后必须先 clickAway 把光标挪到中性点再采样，
//     否则行区/页签采样被光标污染。

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.GM_SHOP_DEBUG_PORT || 9248);
const account = process.env.GM_SHOP_ACCOUNT || 'test1';
const password = process.env.GM_SHOP_PASSWORD || 'test1';
const cardIndex = Number(process.env.GM_SHOP_CARD || 1);
const pageUrl = process.env.GM_SHOP_URL || 'http://127.0.0.1:8000/web/index.html';

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });

// 角色卡中心 = game(105 + 120*i + 25, 210)：card0(130,210) card1(250,210)
// card2(370,210) card3(490,210)。
const cardCenter = index => ({ x: 105 + 120 * index + 25, y: 210 });

// 商店左边缘竖线：候选列 game x174±3，参照列两侧背景 x165 / x183（183 已在
// 窗口内部平面上，用作对照）；行覆盖 game y200..380。边缘列的相邻列亮度差分
// 均值应 > 40，且显著高于参照列。
const shopEdge = Object.freeze({
  candidates: [171, 172, 173, 174, 175, 176, 177],
  references: [165, 183],
  y0: 200,
  y1: 380,
  minDiff: 40,
  marginOverReference: 20
});
const useTab = Object.freeze({
  x0: 452, x1: 494, y0: 167, y1: 186,
  center: { x: 473, y: 176 }
});
const equipTab = Object.freeze({ x0: 410, x1: 451, y0: 167, y1: 186, center: { x: 430, y: 176 } });
// 实测空行槽框最暗像素约 119，物品图标描边接近 0；取 100 才能区分“有物”与
// “空栏”（任务原定 140 会把空槽框误判为物品像素）。
const sellRow = Object.freeze({ x0: 410, x1: 610, y0: 196, y1: 232, darkBelow: 100 });
// readPixels 采样区需覆盖以上所有区域（game 坐标）。
const sampleRect = Object.freeze({ x0: 160, x1: 620, y0: 164, y1: 390 });
// 中性点击点 game(700,300)：在商店窗口（game x175..626）与采样区 sampleRect
// 之外，也不在底部状态栏；光标精灵（绘制在点击点右下方约 +50/+34px 内）停留
// 在此点不会进入任何采样区域，用于每次采样前固定光标位置。
const clickAwayPoint = Object.freeze({ x: 700, y: 300 });
const shopOpenTimeoutMs = 6000;

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

// 点击中性点把游戏内光标精灵挪出所有采样区（光标绘制在点击点右下方，残留
// 在页签/售物行上的光标会污染亮度统计），并等待 ~500ms 让光标与页签状态稳定。
async function clickAway(driver) {
  await driver.click(clickAwayPoint.x, clickAwayPoint.y);
  await sleep(500);
}

function probe(driver, name) {
  return driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
}

function uiStateIs(driver, expected) {
  return () => probe(driver, 'msui_state').then(value => value === expected);
}

// msasset_blocking 是 MapleAssetLoading::isBlocking() 的导出探针；叠加 DOM 遮罩
// 判断可避免刚卸下 C++ 侧阻塞、遮罩尚未隐藏的瞬间误判。
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
    throw new Error(`page evaluation failed: ${JSON.stringify(result.exceptionDetails)}`);
  }
  return result.result.value;
}

// 页面内采样器：在 requestAnimationFrame 回调里读取 WebGL drawing buffer（注册
// 顺序保证回调排在游戏渲染之后、合成清除之前），对固定 game 坐标区域计算亮度
// 统计。保持并恢复 FRAMEBUFFER 绑定，不污染游戏 GL 状态。
function samplerExpression() {
  const config = JSON.stringify({
    rect: sampleRect,
    edge: {
      candidates: shopEdge.candidates,
      references: shopEdge.references,
      y0: shopEdge.y0,
      y1: shopEdge.y1
    },
    useTab,
    equipTab,
    sellRow
  });
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
      const luminance = (x, y) => {
        const column = mapX(x) - x0, row = mapY(y) - y0;
        const offset = ((height - 1 - row) * width + column) * 4;
        return (pixels[offset] + pixels[offset + 1] + pixels[offset + 2]) / 3;
      };
      const round = value => Math.round(value * 100) / 100;
      const columnDiff = column => {
        let total = 0, rows = 0;
        for (let y = config.edge.y0; y < config.edge.y1; y += 1) {
          total += Math.abs(luminance(column + 1, y) - luminance(column, y));
          rows += 1;
        }
        return round(total / rows);
      };
      const regionMean = region => {
        let total = 0, samples = 0;
        for (let y = region.y0; y <= region.y1; y += 1)
          for (let x = region.x0; x <= region.x1; x += 1) {
            total += luminance(x, y);
            samples += 1;
          }
        return round(total / samples);
      };
      const regionMin = region => {
        let minimum = 255;
        for (let y = region.y0; y <= region.y1; y += 1)
          for (let x = region.x0; x <= region.x1; x += 1)
            minimum = Math.min(minimum, luminance(x, y));
        return round(minimum);
      };
      const columns = {};
      for (const column of config.edge.candidates.concat(config.edge.references))
        columns[column] = columnDiff(column);
      resolve({
        edgeColumns: columns,
        useTabMean: regionMean(config.useTab),
        equipTabMean: regionMean(config.equipTab),
        sellRowMin: regionMin(config.sellRow)
      });
    });
  }))()`;
}

function sampleShopPixels(driver) {
  return evaluateAwait(driver, samplerExpression());
}

function edgeVerdict(sample) {
  const edgeMax = Math.max(...shopEdge.candidates.map(column => sample.edgeColumns[column]));
  const referenceMax = Math.max(
    ...shopEdge.references.map(column => sample.edgeColumns[column]));
  return {
    edgeMax,
    referenceMax,
    ok: edgeMax > shopEdge.minDiff && edgeMax > referenceMax + shopEdge.marginOverReference
  };
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
  await driver.click(340, 260);
  await driver.typeAscii(account);
  await driver.click(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 45)) {
    throw new Error('world-select UI did not activate; check GM_SHOP_ACCOUNT/GM_SHOP_PASSWORD');
  }
  console.log('PASS  world-select UI active');
  await driver.click(510, 216);
  if (!await waitFor(uiStateIs(driver, uiState.characters), 45)) {
    throw new Error('character-select UI did not activate');
  }
  console.log('PASS  character-select UI active');
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('character-select assets never reached idle');
  }
  const center = cardCenter(cardIndex);
  await driver.screenshot('10-gm-shop-character-card');
  await driver.doubleClick(center.x, center.y);
  if (!await waitFor(uiStateIs(driver, uiState.game), 60)) {
    throw new Error(`game UI did not activate after double-clicking card ${cardIndex}`);
  }
  console.log('PASS  in-game UI active');
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('in-game assets never reached idle');
  }
  console.log('PASS  in-game assets unblocked');
}

async function openGmShop(driver) {
  await sleep(2000);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(() => driver.evaluate(
    "typeof MapleWasmIME !== 'undefined' && MapleWasmIME.active === true && " +
    "document.activeElement && document.activeElement.id === 'ime-input'"), 5)) {
    throw new Error('chat field did not take IME focus after Enter');
  }
  // 让游戏侧聊天框先完成打开：紧跟 Enter 的 compose 会与字段初始化竞争，
  // 同步文本可能被丢弃（实测立即 compose 时命令不生效）。
  await sleep(500);
  await driver.compose('!gmshop');
  if (!await waitFor(() => driver.evaluate(
    `document.getElementById('ime-input').value === ${JSON.stringify('!gmshop')}`), 5)) {
    throw new Error('gmshop command did not reach the chat input');
  }
  await sleep(400);
  await driver.key('Enter', 'Enter', 13);
  await sleep(2500);
  await driver.screenshot('shop-open');
}

async function verifyShopOpen(driver) {
  // 角色卡双击后光标可能停在卡片区（GM_SHOP_CARD=0 时精灵可延伸至 x<=180，
  // 擦到边缘候选列），统一先移到中性点再开始采样。
  await clickAway(driver);
  const deadline = Date.now() + shopOpenTimeoutMs;
  let lastSample = null;
  while (Date.now() < deadline) {
    const sample = await sampleShopPixels(driver);
    if (!sample.error) {
      lastSample = sample;
      const verdict = edgeVerdict(sample);
      if (verdict.ok) {
        console.log(`PASS  shop window left edge visible at game x~174 ` +
          `(edgeDiff=${verdict.edgeMax} > ${shopEdge.minDiff}, refDiff=${verdict.referenceMax})`);
        return true;
      }
    }
    await sleep(500);
  }
  const shopActive = await probe(driver, 'msshop_active').catch(() => -1);
  const detail = lastSample === null
    ? 'no pixel sample succeeded'
    : `best edgeDiff=${edgeVerdict(lastSample).edgeMax} (need > ${shopEdge.minDiff}), ` +
      `refDiff=${edgeVerdict(lastSample).referenceMax}`;
  report(false, `shop window not detected within ${shopOpenTimeoutMs / 1000}s ` +
    `(msshop_active=${shopActive}; ${detail})`);
  console.log('      角色 gm 等级需 ≥2（MySQL: update characters set gm=2 where id=<该角色id>; 验后还原 0）');
  return false;
}

async function verifyUseTab(driver) {
  // 所有采样（before/after/restored）前都先把光标固定在 (700,300)：光标精灵
  // 绘制在点击点右下方，选卡/点页签残留的光标会污染页签与售物行统计。
  await clickAway(driver);
  const before = await sampleShopPixels(driver);
  if (before.error) {
    report(false, `tab-state sample before click failed: ${before.error}`);
    return;
  }
  await driver.click(useTab.center.x, useTab.center.y);
  await sleep(900);
  await clickAway(driver);
  await driver.screenshot('shop-use-tab');
  const after = await sampleShopPixels(driver);
  if (after.error) {
    report(false, `tab-state sample after click failed: ${after.error}`);
    return;
  }
  if (after.sellRowMin < sellRow.darkBelow) {
    report(true, `USE sell rows show item pixels ` +
      `(minLum=${after.sellRowMin} < ${sellRow.darkBelow} in game x${sellRow.x0}..${sellRow.x1} y${sellRow.y0}..${sellRow.y1})`);
  } else {
    // 空 USE 栏不算失败：行内容为条件式目检（几何断言仍保持硬失败语义）。
    console.log(`WARN  USE sell rows appear empty ` +
      `(minLum=${after.sellRowMin} >= ${sellRow.darkBelow}); ` +
      `row-content check skipped — add USE items to the character to exercise this path`);
  }
  const useChange = Math.abs(after.useTabMean - before.useTabMean);
  const equipChange = Math.abs(after.equipTabMean - before.equipTabMean);
  if (useChange > 3 && equipChange > 3) {
    report(true, `USE tab click changed both tab faces ` +
      `(useTab delta=${useChange}, equipTab delta=${equipChange}; need > 3)`);
    return;
  }
  // 实测 EQUIP 页签选中/未选中两套位图的均值差只有 ~1.5，双区 > 3 的强判据不
  // 成立；退回“互变”验证：点击 USE 后再点回 EQUIP，要求 USE 页签名均值先显著
  // 变化又复原（选中态可逆），以此证明页签状态真实切换。
  await driver.click(equipTab.center.x, equipTab.center.y);
  await sleep(900);
  await clickAway(driver);
  await driver.screenshot('shop-equip-tab-restored');
  const restored = await sampleShopPixels(driver);
  if (restored.error) {
    report(false, `tab-state sample after restoring EQUIP failed: ${restored.error}`);
    return;
  }
  const useRestored = Math.abs(restored.useTabMean - before.useTabMean);
  const rowChange = Math.abs(after.sellRowMin - before.sellRowMin);
  report(useChange > 3 && useRestored <= 3,
    `USE tab state toggled and restored (useTab delta=${useChange} > 3, restore delta=${useRestored} <= 3; ` +
      `equipTab delta=${equipChange}, sellRow min delta=${rowChange})`);
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await openGmShop(driver);
    if (await verifyShopOpen(driver)) await verifyUseTab(driver);
  } catch (error) {
    report(false, `gm shop flow aborted: ${error.message}`);
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
  console.log(`FAIL  GM_SHOP_CARD must be a card index 0..3, got ${process.env.GM_SHOP_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
