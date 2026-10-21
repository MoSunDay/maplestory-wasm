// 现金商城试穿流程：浮层自动收起、试穿集累计（同槽覆盖）、重置按钮还原。
// 依赖只读探针：mscashshop_equip_menu_open / mscashshop_equip_sub /
// mscashshop_filtered_count / mscashshop_highlighted_item /
// mscashshop_selected_left / mscashshop_tryon_count / mscashshop_preview_cap。
import { BrowserDriver, sleep } from './cdp.mjs';
import fs from 'fs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.TRYON_FLOW_DEBUG_PORT || 9250);
const pageUrl = process.env.TRYON_FLOW_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.TRYON_FLOW_ACCOUNT || process.env.CASH_EQUIP_ACCOUNT || 'test1';
const password = process.env.TRYON_FLOW_PASSWORD || process.env.CASH_EQUIP_PASSWORD || 'test1';
const cardIndex = Number(process.env.TRYON_FLOW_CARD ?? process.env.CASH_EQUIP_CARD ?? 1);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
// game 800x600 设计坐标。
const charCard = index => ({ x: 105 + 120 * index + 25, y: 210 });
const rail = Object.freeze({ equip: { x: 49, y: 40 + 34 * 1 + 12 } });
const subRow = sub => ({ x: 148, y: 106 + 22 * sub + 11 });
const gridCard = index => ({
  x: 109 + (index % 4) * 97 + 46,
  y: 86 + Math.floor(index / 4) * 148 + 72
});
const resetButton = Object.freeze({ x: 528, y: 564 });
const shopEntry = Object.freeze({ x: 597, y: 571 });
// 试穿人像采样带（avatar frame 内部）。
const avatarBand = Object.freeze({ x0: 514, x1: 586, y0: 458, y1: 544 });

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

// 人像带亮度均值/标准差（readPixels，保持 FRAMEBUFFER 绑定）。
function avatarStats(driver, refGrid) {
  return evaluateAwait(driver, `(() => new Promise(resolve => {
    requestAnimationFrame(() => {
      const band = ${JSON.stringify(avatarBand)};
      const canvas = document.getElementById('canvas');
      const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
      const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
      const x0 = Math.round(band.x0 * scaleX), x1 = Math.round(band.x1 * scaleX);
      const y0 = Math.round(band.y0 * scaleY), y1 = Math.round(band.y1 * scaleY);
      const width = x1 - x0, height = y1 - y0;
      const pixels = new Uint8Array(width * height * 4);
      const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.readPixels(x0, canvas.height - y1, width, height,
        gl.RGBA, gl.UNSIGNED_BYTE, pixels);
      gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
      let sum = 0;
      for (let i = 0; i < width * height; i += 1) {
        sum += pixels[i * 4] + pixels[i * 4 + 1] + pixels[i * 4 + 2];
      }
      const mean = sum / (3 * width * height);
      let dev = 0;
      for (let i = 0; i < width * height; i += 1) {
        dev += Math.abs(pixels[i * 4] + pixels[i * 4 + 1] +
          pixels[i * 4 + 2] - mean);
      }
      dev /= 3 * width * height;
      const grid = [];
      for (let gy = 0; gy < height; gy += 3)
        for (let gx = 0; gx < width; gx += 3) {
          const i = (gy * width + gx) * 4;
          grid.push(pixels[i], pixels[i + 1], pixels[i + 2]);
        }
      let changed = -1;
      if (${refGrid ? 'true' : 'false'}) {
        const ref = ${JSON.stringify(refGrid || [])};
        let diff = 0, cells = 0;
        for (let c = 0; c + 2 < Math.min(grid.length, ref.length); c += 3) {
          cells += 1;
          if (Math.abs(grid[c] - ref[c]) > 12 ||
              Math.abs(grid[c + 1] - ref[c + 1]) > 12 ||
              Math.abs(grid[c + 2] - ref[c + 2]) > 12)
            diff += 1;
        }
        changed = cells ? Math.round(diff / cells * 10000) / 10000 : 0;
      }
      resolve({ mean: Math.round(mean * 100) / 100, dev: Math.round(dev * 100) / 100,
        changed, grid });
    });
  }))()`);
}

async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90)) {
    throw new Error('WASM module did not initialize');
  }
  if (!await waitFor(uiStateIs(driver, uiState.login), 30)) {
    throw new Error('login UI did not activate');
  }
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 45)) {
    throw new Error('world-select UI did not activate');
  }
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 45)) {
    throw new Error('character-select UI did not activate');
  }
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('character-select assets never reached idle');
  }
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
  if (!await waitFor(assetsIdle(driver), 90)) {
    throw new Error('game assets never reached idle');
  }
}

async function enterCashShop(driver) {
  for (let attempt = 0; attempt < 5 &&
    !await uiStateIs(driver, uiState.cashShop)(); attempt += 1) {
    await driver.click(shopEntry.x, shopEntry.y);
    await sleep(2000);
  }
  if (!await waitFor(uiStateIs(driver, uiState.cashShop), 20)) {
    throw new Error('cash shop did not activate');
  }
  if (!await waitFor(assetsIdle(driver), 60)) {
    throw new Error('cash shop assets never reached idle');
  }
}

async function selectEquipSub(driver, sub) {
  await driver.click(rail.equip.x, rail.equip.y);
  await sleep(400);
  await driver.click(subRow(sub).x, subRow(sub).y);
  await sleep(600);
}

async function verifyTryon(driver) {
  // 1. 装备→帽子：选中后浮层自动收起，目录只剩帽子。
  await selectEquipSub(driver, 1);
  report(await probe(driver, 'mscashshop_equip_menu_open') === 0,
    '子分类选中后浮层自动收起');
  report(await probe(driver, 'mscashshop_equip_sub') === 1,
    '帽子子分类生效');
  const hatCount = await probe(driver, 'mscashshop_filtered_count');
  report(hatCount > 0, `帽子目录非空 (filtered=${hatCount})`);

  // 2. 基线：试穿集为空，人像为玩家自身外观。
  const playerCap = await probe(driver, 'mscashshop_preview_cap');
  const firstHat = await probe(driver, 'mscashshop_highlighted_item');
  report(await probe(driver, 'mscashshop_tryon_count') === 0,
    `试穿集初始为空 (player cap=${playerCap}, first hat=${firstHat})`);
  const baseline = await avatarStats(driver);
  await driver.screenshot('20-tryon-baseline');

  // 3. 试穿第一顶帽子：入集、预览立即切换。
  const hat0 = await probe(driver, 'mscashshop_highlighted_item');
  await driver.click(gridCard(0).x, gridCard(0).y);
  await sleep(700);
  report(await probe(driver, 'mscashshop_tryon_count') === 1,
    '点击帽子卡入试穿集 (count=1)');
  report(await probe(driver, 'mscashshop_preview_cap') === hat0,
    `预览帽子切换为选中件 (cap=${await probe(driver, 'mscashshop_preview_cap')} === ${hat0})`);
  const afterHat0 = await avatarStats(driver);
  report(Math.abs(afterHat0.mean - baseline.mean) >= 3,
    `人像带亮度变化 (mean ${baseline.mean} -> ${afterHat0.mean})`);
  await driver.screenshot('21-tryon-first-hat');

  // 4. 同槽第二顶：覆盖第一顶，集大小不变。
  await driver.click(gridCard(1).x, gridCard(1).y);
  await sleep(700);
  const hat1 = await probe(driver, 'mscashshop_highlighted_item');
  report(await probe(driver, 'mscashshop_tryon_count') === 1,
    `同槽第二顶覆盖第一顶 (count=1, cap=${await probe(driver, 'mscashshop_preview_cap')})`);
  report(await probe(driver, 'mscashshop_preview_cap') === hat1 &&
    hat1 !== hat0, `预览帽子换为 ${hat1} (原 ${hat0})`);
  const afterHat1 = await avatarStats(driver);
  report(Math.abs(afterHat1.mean - afterHat0.mean) >= 3,
    `人像带亮度再次变化 (mean ${afterHat0.mean} -> ${afterHat1.mean})`);
  await driver.screenshot('22-tryon-same-slot-overwrite');

  // 5. 换部位（上衣）：累计，帽子保持。
  await selectEquipSub(driver, 5);
  report(await probe(driver, 'mscashshop_equip_sub') === 5, '上衣子分类生效');
  const capBefore = await probe(driver, 'mscashshop_preview_cap');
  await driver.click(gridCard(0).x, gridCard(0).y);
  await sleep(700);
  report(await probe(driver, 'mscashshop_tryon_count') === 2,
    `不同部位累计试穿 (count=2)`);
  report(await probe(driver, 'mscashshop_preview_cap') === capBefore,
    `帽子预览保持 (${capBefore})`);
  const afterTop = await avatarStats(driver, afterHat1.grid);
  report(afterTop.changed >= 0.02,
    `人像像素随上衣变化 (${Math.round(afterTop.changed * 100)}% cells, mean ${afterHat1.mean} -> ${afterTop.mean})`);
  await driver.screenshot('23-tryon-accumulated');

  // 6. 重置：集清空、人像还原玩家外观。
  await driver.click(resetButton.x, resetButton.y);
  await sleep(700);
  report(await probe(driver, 'mscashshop_tryon_count') === 0,
    '重置按钮清空试穿集');
  report(await probe(driver, 'mscashshop_preview_cap') === playerCap,
    `预览还原玩家外观 (cap=${await probe(driver, 'mscashshop_preview_cap')} === ${playerCap})`);
  const afterReset = await avatarStats(driver);
  report(Math.abs(afterReset.mean - baseline.mean) < 1.5,
    `人像带亮度回到基线 (mean ${afterReset.mean} ≈ ${baseline.mean})`);
  await driver.screenshot('24-tryon-reset');
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await enterCashShop(driver);
    await verifyTryon(driver);
    await driver.key('Escape', 'Escape', 27);
    report(await waitFor(uiStateIs(driver, uiState.game), 30),
      'cash shop exits back to game');
    await driver.screenshot('25-tryon-flow-exited');
  } catch (error) {
    report(false, `cash shop try-on flow aborted: ${error.message}`);
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
  console.log(`FAIL  TRYON_FLOW_CARD must be a card index 0..3, got ${process.env.TRYON_FLOW_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
