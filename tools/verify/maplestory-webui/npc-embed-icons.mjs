// NPC 对话内嵌图片宏 #f<path># 端到端验证（妮娜 9270052, FM hub 910000000）。
//
// 登录 → 卡0 进游戏 → 走到 x≈34 妮娜处按 Space 开对话,然后验证:
//  1. msnpc_prompt 不再泄漏 UI/、Effect/ 等资源路径子串(修复前整段路径当文本输出);
//  2. case 0 已删除 21 个 GuildMark 四方印拼成的分隔行, prompt 正文里不应再出现
//     分隔行级别的成段空格(旧的 21 图标 ≈ 105 空格); 剩下的 3 个菜单爱心
//     (#f Effect/CharacterEff/1062114/1/0#) 挂在 #L0#..#l 选项标签上, 客户端把选项
//     文本放进 selection_labels 而不是 msnpc_prompt, 所以正文里本来就没有 #f 占位;
//  3. 页面内 WebGL readPixels 采样: 菜单爱心图标所在水平带(3 条选项行的行首图标列,
//     canvas x≈296..308)chroma(通道极差)显著高于灰阶文本参照带 → 爱心真实渲染成彩色位图;
//     分隔行删除后不再有整行彩色带, 因此只要求该图标列的 3 个窄列里至少 1 列被染色;
//  4. Escape 关闭对话后同带亮度大幅回落(白色对话面板消失) → 采样确实来自对话窗口;
//     自由市场地图底色比白色面板更"彩色", 所以差异判据用亮度而不是 chroma 回落;
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
  // 2. divider row is gone: no divider-sized space run may survive in the prompt.
  //    The deleted body line was 21 GuildMark stamps in a row (~105 spaces, far
  //    above the 12-space floor this test used to require). The 3 remaining menu
  //    hearts ride on #L<n># option labels, and the client keeps option text in
  //    selection_labels instead of prompttext (UINpcTalk.cpp, SELECTION branch),
  //    so the body legitimately holds zero placeholders now.
  const spaceRuns = prompt.match(/ {3,}/g) || [];
  const longestRun = spaceRuns.reduce((max, run) => Math.max(max, run.length), 0);
  const DIVIDER_RUN = 12; // one heart sprite (~3..6 spaces) stays well below this
  requireThat(longestRun < DIVIDER_RUN,
    `prompt still carries a GuildMark divider-sized placeholder run ` +
    `(${spaceRuns.length} run(s), longest ${longestRun})`);

  // 3. locate the menu-heart band inside the dialogue text column
  //    (heart sprites stream in via LazyFS, so retry until the band turns colorful).
  //    The strip covers only the option-row icon cell: the dialogue text column starts
  //    at panel_x + DIALOG_TEXT_X (156) ~= canvas x296, the heart sprite is 11x10 and
  //    the option text itself starts ~x308. The y window spans the 3 option lines that
  //    now sit where the deleted divider row used to push them down (the text block
  //    stays vertically centred, so option rows move up by half a line).
  const TEXT_SCAN = { x0: 294, x1: 308, y0: 258, y1: 350, step: 5 };
  let openBands = null;
  const bandsReady = await waitFor(async () => {
    const bands = await sampleLuminanceAndChroma(driver, bandRects(
      TEXT_SCAN.x0, TEXT_SCAN.x1, TEXT_SCAN.y0, TEXT_SCAN.y1, TEXT_SCAN.step, 'band'));
    const best = Math.max(...Object.values(bands).map(s => s.chroma));
    if (best >= 20) { openBands = bands; return true; }
    return false;
  }, 20);
  requireThat(bandsReady, 'menu-heart band never became colorful (LazyFS heart texture missing)');
  if (!openBands) throw new Error('icon band not found');
  let iconBand = null;
  const bandEntries = Object.entries(openBands);
  for (const [name, s] of bandEntries) {
    if (!iconBand || s.chroma > openBands[iconBand].chroma) iconBand = name;
  }
  const iconY = Number(iconBand.split('_')[1]);
  console.log(`icon band: ${iconBand} chroma=${openBands[iconBand].chroma} lum=${openBands[iconBand].lum}`);
  // grey band reference: mean chroma of all other bands (blank panel + plain text)
  const greyReference = bandEntries.filter(([n]) => n !== iconBand)
    .reduce((sum, [, s]) => sum + s.chroma, 0) / (bandEntries.length - 1);
  requireThat(openBands[iconBand].chroma - greyReference >= 8,
    `icon band chroma ${openBands[iconBand].chroma} not above grey band reference ${greyReference.toFixed(2)}`);

  // horizontal evidence: the 3 menu hearts are the only inline sprites left, and each
  // sits at the very start of an option label (heart sprite 11x10 at canvas x~296..308;
  // the option text itself starts ~x308). Sample 3 narrow columns inside that heart
  // strip and require at least one colored column — the deleted 21-stamp GuildMark
  // divider row used to cover x=300..562 continuously, and that whole-row coverage is
  // deliberately no longer asserted.
  const bandY1 = Math.min(iconY + TEXT_SCAN.step - 1, TEXT_SCAN.y1);
  const ICON_COLUMNS = { x0: 296, x1: 308, count: 3 };
  const columnWidth = Math.round((ICON_COLUMNS.x1 - ICON_COLUMNS.x0) / ICON_COLUMNS.count);
  const columnRects = [];
  for (let col = 0; col < ICON_COLUMNS.count; col += 1) {
    const cx0 = ICON_COLUMNS.x0 + col * columnWidth;
    columnRects.push({ name: `col_${col}`, x0: cx0, x1: cx0 + columnWidth - 1, y0: iconY, y1: bandY1 });
  }
  const columns = await sampleLuminanceAndChroma(driver, columnRects);
  const coloredColumns = Object.values(columns).filter(s => s.chroma >= 10).length;
  requireThat(coloredColumns >= 1,
    `no colored menu-heart icon column in x=${ICON_COLUMNS.x0}..${ICON_COLUMNS.x1}: ` +
    `${JSON.stringify(columns)}`);

  await driver.screenshot('nina-embed-icons-open');

  // 4. close dialogue, same band must stop showing the dialogue panel
  //    (differential reference). The panel is a bright, low-chroma surface while the
  //    FM map behind it is darker and more colorful, so the panel disappearing shows
  //    up as a luminance drop (chroma can actually rise once the map is revealed).
  await driver.key('Escape', 'Escape', 27);
  await waitFor(async () => await probe(driver, 'msnpc_active') === 0, 8);
  await sleep(600);
  const closedBand = await sampleLuminanceAndChroma(driver, [
    { name: 'iconBandClosed', x0: TEXT_SCAN.x0, x1: TEXT_SCAN.x1, y0: iconY, y1: bandY1 }
  ]);
  await driver.screenshot('nina-embed-icons-closed');
  // open-state reference: the band scan sampled while the dialogue was visible.
  // Closing the window removes the bright dialogue panel, so the same band must change:
  // luminance drops hard (the FM map behind is darker) while chroma can even rise (the
  // map is far more colorful than the white panel). Either signal proves the sample came
  // from the dialogue window, so the assertion accepts both signals.
  const openBandChroma = openBands[iconBand].chroma;
  const openBandLum = openBands[iconBand].lum;
  const closedChroma = closedBand.iconBandClosed.chroma;
  const closedLum = closedBand.iconBandClosed.lum;
  const chromaDrop = openBandChroma - closedChroma;
  const lumDrop = openBandLum - closedLum;
  requireThat(chromaDrop >= 10 || lumDrop >= 30,
    `closing dialogue did not change the sampled band ` +
    `(chroma ${openBandChroma} -> ${closedChroma}, lum ${openBandLum} -> ${closedLum})`);

  // 5. console hygiene
  const unresolvedWarnings = driver.logs.filter(line => line.includes('unresolved inline image'));
  requireThat(unresolvedWarnings.length === 0,
    `unresolved inline image warnings: ${JSON.stringify(unresolvedWarnings.slice(0, 5))}`);
  requireThat(driver.errors.length === 0, `browser errors: ${JSON.stringify(driver.errors.slice(0, 5))}`);

  const evidence = {
    account, characterCard: cardIndex, npcX: x,
    prompt, spaceRuns: spaceRuns.length, longestRun, dividerRun: DIVIDER_RUN,
    iconBand: { y: iconY, ...openBands[iconBand], greyReference: Math.round(greyReference * 100) / 100 },
    iconColumnStrip: { x0: ICON_COLUMNS.x0, x1: ICON_COLUMNS.x1, columns: ICON_COLUMNS.count },
    coloredColumns,
    bandLumOpen: openBandLum,
    bandLumClosed: closedLum,
    bandChromaOpen: openBandChroma,
    bandChromaClosed: closedChroma,
    bandLumDrop: Math.round(lumDrop * 100) / 100,
    bandChromaDrop: Math.round(chromaDrop * 100) / 100,
    consoleErrors: driver.errors.length,
    unresolvedInlineImageWarnings: unresolvedWarnings.length
  };
  fs.writeFileSync(`${driver.artifactDir}/npc-embed-icons.json`, `${JSON.stringify(evidence, null, 2)}\n`);
  if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);

  if (failures.length > 0) {
    throw new Error(`npc embed icon verification failed:\n  - ${failures.join('\n  - ')}`);
  }
  console.log(`PASS  npc #f menu-heart icons: prompt clean (${spaceRuns.length} space run(s), ` +
    `longest ${longestRun} < divider ${DIVIDER_RUN}), ` +
    `${coloredColumns}/${ICON_COLUMNS.count} heart columns colored in x=${ICON_COLUMNS.x0}..${ICON_COLUMNS.x1}, ` +
    `band chroma ${openBandChroma} vs grey ${greyReference.toFixed(2)}, ` +
    `closed-dialogue lum ${closedLum} (open ${openBandLum}, drop ${Math.round(lumDrop)}), ` +
    `chroma ${closedChroma} (open ${openBandChroma}, drop ${Math.round(chromaDrop)})`);
  console.log(`artifacts: ${driver.artifactDir}`);
} finally {
  await driver.stop();
}
