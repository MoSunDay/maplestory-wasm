// NPC 对话内嵌图片宏 #f<path># 端到端验证（妮娜 9270052, FM hub 910000000）。
//
// 登录 → 卡0 进游戏 → 走到 x≈34 妮娜处按 Space 开对话,然后验证:
//  1. msnpc_prompt 不再泄漏 UI/、Effect/ 等资源路径子串(修复前整段路径当文本输出);
//  2. prompt 里出现成段空格占位(#f 精灵按宽度替换出的 placeholder);
//  3. 页面内 WebGL readPixels 采样: 四方印分隔行所在水平带 chroma(通道极差)
//     显著高于灰阶文本参照带,且横向多列被染色 → 图标真实渲染成彩色位图;
//  4. Escape 关闭对话后同带 chroma 大幅回落 → 采样确实来自对话窗口;
//  5. 浏览器 console 无新错误,无 "[npc] unresolved inline image" 告警。
//
// 运行: node tools/verify/maplestory-webui/npc-embed-icons.mjs
//   (可用 NPC_EMBED_ACCOUNT/PASSWORD/CARD/URL/CHROME_BIN 覆盖默认值)
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const account = process.env.NPC_EMBED_ACCOUNT || 'fmflow1';
const password = process.env.NPC_EMBED_PASSWORD || 'fmflow1';
const cardIndex = Number(process.env.NPC_EMBED_CARD ?? 0);
const npcX = Number(process.env.NPC_EMBED_X ?? 34);
const uiState = { login: 1, world: 2, characters: 3, game: 5 };
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });

const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const probeStr = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'string', [], [])`);

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
}
function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}
const uiStateIs = (driver, state) => async () => await probe(driver, 'msui_state') === state;

async function walkTowards(driver, targetX) {
  for (let iter = 0; iter < 40; iter += 1) {
    const x = await probe(driver, 'msplayer_x');
    if (Math.abs(x - targetX) < 8) return x;
    const ms = Math.abs(targetX - x) > 300 ? 420 : 160;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(80);
  }
  return (await probe(driver, 'msplayer_x'));
}

// In-page sampler (pattern from layout-pixels.mjs): reads the live WebGL
// drawing buffer after the game render pass. For each game-coord rect it
// returns mean luminance and mean chroma (max-min channel spread, i.e. how
// "colored" vs grey the pixels are).
async function sampleLuminanceAndChroma(driver, rects) {
  const config = JSON.stringify({ rects });
  const result = await driver.send('Runtime.evaluate', {
    expression: `(() => new Promise(resolve => {
      requestAnimationFrame(() => {
        const config = ${config};
        const canvas = document.getElementById('canvas');
        const gl = canvas.getContext('webgl2') || canvas.getContext('webgl')
          || canvas.getContext('experimental-webgl');
        if (!gl) { resolve({ error: 'game canvas exposes no WebGL context' }); return; }
        const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
        const mapX = v => Math.round(v * scaleX), mapY = v => Math.round(v * scaleY);
        const x0 = Math.min(...config.rects.map(r => mapX(r.x0)));
        const x1 = Math.max(...config.rects.map(r => mapX(r.x1)));
        const y0 = Math.min(...config.rects.map(r => mapY(r.y0)));
        const y1 = Math.max(...config.rects.map(r => mapY(r.y1)));
        const width = x1 - x0 + 1, height = y1 - y0 + 1;
        const pixels = new Uint8Array(width * height * 4);
        const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.readPixels(x0, canvas.height - 1 - y1, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
        const stats = (x, y) => {
          const offset = ((height - 1 - (mapY(y) - y0)) * width + (mapX(x) - x0)) * 4;
          const r = pixels[offset], g = pixels[offset + 1], b = pixels[offset + 2];
          return [(r + g + b) / 3, Math.max(r, g, b) - Math.min(r, g, b)];
        };
        const out = {};
        for (const rect of config.rects) {
          let lum = 0, chroma = 0, n = 0;
          for (let y = rect.y0; y <= rect.y1; y += 1)
            for (let x = rect.x0; x <= rect.x1; x += 1) {
              const s = stats(x, y); lum += s[0]; chroma += s[1]; n += 1;
            }
          out[rect.name] = {
            lum: Math.round(lum / n * 100) / 100,
            chroma: Math.round(chroma / n * 100) / 100
          };
        }
        resolve(out);
      });
    }))()`,
    awaitPromise: true,
    returnByValue: true
  });
  if (result.exceptionDetails) {
    throw new Error(`page evaluation failed: ${JSON.stringify(result.exceptionDetails)}`);
  }
  const value = result.result.value;
  if (!value || value.error) throw new Error(`sampling failed: ${JSON.stringify(value)}`);
  return value;
}

function bandRects(x0, x1, y0, y1, step, label) {
  const rects = [];
  for (let y = y0; y <= y1; y += step) {
    rects.push({ name: `${label}_${y}`, x0, x1, y0: y, y1: Math.min(y + step - 1, y1) });
  }
  return rects;
}

const failures = [];
function requireThat(condition, message) {
  if (!condition) failures.push(message);
  return condition;
}

const driver = new BrowserDriver({ debugPort: 9265, binary: process.env.CHROME_BIN || '/usr/bin/chromium-browser' });
try {
  await driver.start();
  await driver.navigate(process.env.NPC_EMBED_URL || 'http://127.0.0.1:8000/web/index.html');
  await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await waitFor(uiStateIs(driver, uiState.login), 30);
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  await waitFor(uiStateIs(driver, uiState.world), 45);
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  await waitFor(assetsIdle(driver), 60);
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)(); if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error('game UI did not activate');
  await waitFor(assetsIdle(driver), 120);
  await sleep(1500);

  // ---- open Nina's dialogue -------------------------------------------------
  const x = await walkTowards(driver, npcX);
  await sleep(400);
  await driver.key('Space', 'Space', 32);
  const opened = await waitFor(async () => await probe(driver, 'msnpc_active') === 1, 8);
  if (!requireThat(opened, `Nina dialogue did not open (walked to x=${x})`)) throw new Error('dialogue did not open');

  const prompt = await probeStr(driver, 'msnpc_prompt');
  console.log(`PROMPT>> ${JSON.stringify(prompt)}`);

  // 1. no resource-path leakage
  for (const leak of ['UI/', 'Effect/', 'GuildMark', 'CharacterEff', '.nx', 'QuestIcon']) {
    requireThat(!prompt.includes(leak), `prompt leaks resource path fragment: ${leak}`);
  }
  // 2. placeholder space runs from resolved sprites
  const spaceRuns = prompt.match(/ {4,}/g) || [];
  const longestRun = spaceRuns.reduce((max, run) => Math.max(max, run.length), 0);
  requireThat(spaceRuns.length >= 1 && longestRun >= 12,
    `expected icon placeholder space runs, got ${spaceRuns.length} runs (longest ${longestRun})`);

  // 3. locate the colored icon band inside the dialogue text column
  //    (icons stream in via LazyFS, so retry until the band turns colorful)
  const TEXT_SCAN = { x0: 300, x1: 620, y0: 215, y1: 290, step: 5 };
  let openBands = null;
  const bandsReady = await waitFor(async () => {
    const bands = await sampleLuminanceAndChroma(driver, bandRects(
      TEXT_SCAN.x0, TEXT_SCAN.x1, TEXT_SCAN.y0, TEXT_SCAN.y1, TEXT_SCAN.step, 'band'));
    const best = Math.max(...Object.values(bands).map(s => s.chroma));
    if (best >= 20) { openBands = bands; return true; }
    return false;
  }, 20);
  requireThat(bandsReady, 'inline image band never became colorful (LazyFS textures missing)');
  if (!openBands) throw new Error('icon band not found');
  let iconBand = null;
  const bandEntries = Object.entries(openBands);
  for (const [name, s] of bandEntries) {
    if (!iconBand || s.chroma > openBands[iconBand].chroma) iconBand = name;
  }
  const iconY = Number(iconBand.split('_')[1]);
  console.log(`icon band: ${iconBand} chroma=${openBands[iconBand].chroma} lum=${openBands[iconBand].lum}`);
  // grey text reference: mean chroma of all other bands
  const greyReference = bandEntries.filter(([n]) => n !== iconBand)
    .reduce((sum, [, s]) => sum + s.chroma, 0) / (bandEntries.length - 1);
  requireThat(openBands[iconBand].chroma - greyReference >= 8,
    `icon band chroma ${openBands[iconBand].chroma} not above grey text reference ${greyReference.toFixed(2)}`);

  // horizontal coverage: several separate icon columns inside the band
  const bandY1 = Math.min(iconY + TEXT_SCAN.step - 1, TEXT_SCAN.y1);
  // 17 icon placeholders sit consecutively on the wrapped line: x~298..562
  // (pitch 15). Sample 12 columns across that strip.
  const columnRects = [];
  for (let col = 0; col < 12; col += 1) {
    const cx0 = 300 + Math.round(col * (260 / 12));
    columnRects.push({ name: `col_${col}`, x0: cx0, x1: cx0 + Math.round(260 / 12) - 1, y0: iconY, y1: bandY1 });
  }
  const columns = await sampleLuminanceAndChroma(driver, columnRects);
  const coloredColumns = Object.values(columns).filter(s => s.chroma >= 10).length;
  requireThat(coloredColumns >= 8,
    `icon row covers only ${coloredColumns}/12 columns: ${JSON.stringify(columns)}`);

  await driver.screenshot('nina-embed-icons-open');

  // 4. close dialogue, same band must lose its color (differential reference)
  await driver.key('Escape', 'Escape', 27);
  await waitFor(async () => await probe(driver, 'msnpc_active') === 0, 8);
  await sleep(600);
  const closedBand = await sampleLuminanceAndChroma(driver, [
    { name: 'iconBandClosed', x0: TEXT_SCAN.x0, x1: TEXT_SCAN.x1, y0: iconY, y1: bandY1 }
  ]);
  await driver.screenshot('nina-embed-icons-closed');
  // open-state reference: the band scan sampled while the dialogue was visible
  const openBandChroma = openBands[iconBand].chroma;
  requireThat(openBandChroma - closedBand.iconBandClosed.chroma >= 10,
    `closing dialogue did not drop band chroma (${openBandChroma} -> ${closedBand.iconBandClosed.chroma})`);

  // 5. console hygiene
  const unresolvedWarnings = driver.logs.filter(line => line.includes('unresolved inline image'));
  requireThat(unresolvedWarnings.length === 0,
    `unresolved inline image warnings: ${JSON.stringify(unresolvedWarnings.slice(0, 5))}`);
  requireThat(driver.errors.length === 0, `browser errors: ${JSON.stringify(driver.errors.slice(0, 5))}`);

  const evidence = {
    account, characterCard: cardIndex, npcX: x,
    prompt, spaceRuns: spaceRuns.length, longestRun,
    iconBand: { y: iconY, ...openBands[iconBand], greyReference: Math.round(greyReference * 100) / 100 },
    coloredColumns,
    bandChromaOpen: openBands[iconBand].chroma,
    bandChromaClosed: closedBand.iconBandClosed.chroma,
    consoleErrors: driver.errors.length,
    unresolvedInlineImageWarnings: unresolvedWarnings.length
  };
  fs.writeFileSync(`${driver.artifactDir}/npc-embed-icons.json`, `${JSON.stringify(evidence, null, 2)}\n`);
  if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);

  if (failures.length > 0) {
    throw new Error(`npc embed icon verification failed:\n  - ${failures.join('\n  - ')}`);
  }
  console.log(`PASS  npc #f inline images: prompt clean, ${spaceRuns.length} placeholder run(s), ` +
    `icon band chroma ${openBands[iconBand].chroma} vs grey ${greyReference.toFixed(2)}, ` +
    `${coloredColumns}/12 columns colored, closed-dialogue chroma ${closedBand.iconBandClosed.chroma}`);
  console.log(`artifacts: ${driver.artifactDir}`);
} finally {
  await driver.stop();
}
