// PQ e2e harness —— 对话框交互层：选项行悬停扫描（ORANGE 重绘采样）与
// YES/OK 按钮视觉定位（png-probe 纯像素解码）（拆自 pq-harness.mjs）。
import fs from 'fs';
import path from 'path';
import { sleep } from './cdp.mjs';
import { decodePng, findDialogButtons } from './png-probe.mjs';
import { mapId, probe, promptText, waitFor } from './pq-harness-game.mjs';

// —— 悬停扫描 ————————————————————————————————————————————————
// 页面内按行统计橙色像素（ORANGE = 255,128,0；抗锯齿边缘会向米色对话底
// (≈240,220,180) 混合，阈值 r>180、g∈[90,200]、b<90 只收 alpha>0.5 的字芯）。
// 返回 [[gameY, orangePixelCount], ...]（仅非零行）。
export async function sampleOrangeRows(driver, region) {
  const result = await driver.send('Runtime.evaluate', {
    expression: `(() => new Promise(resolve => {
      requestAnimationFrame(() => {
        const region = ${JSON.stringify(region)};
        const canvas = document.getElementById('canvas');
        const gl = canvas.getContext('webgl2') || canvas.getContext('webgl')
          || canvas.getContext('experimental-webgl');
        if (!gl) { resolve({ error: 'game canvas exposes no WebGL context' }); return; }
        const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
        const x0 = Math.round(region.x0 * scaleX), x1 = Math.round(region.x1 * scaleX);
        const y0 = Math.round(region.y0 * scaleY), y1 = Math.round(region.y1 * scaleY);
        const width = x1 - x0 + 1, height = y1 - y0 + 1;
        const pixels = new Uint8Array(width * height * 4);
        const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.readPixels(x0, canvas.height - 1 - y1, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
        const rows = [];
        for (let ry = 0; ry < height; ry += 1) {
          let orange = 0;
          for (let x = 0; x < width; x += 1) {
            const o = (ry * width + x) * 4;
            if (pixels[o] > 180 && pixels[o + 1] >= 90 && pixels[o + 1] <= 200 && pixels[o + 2] < 90)
              orange += 1;
          }
          if (orange > 0) rows.push([region.y1 - ry, orange]);
        }
        resolve(rows);
      });
    }))()`,
    awaitPromise: true,
    returnByValue: true,
  });
  const value = result.result.value;
  if (!value || value.error) throw new Error(`orange sampling failed: ${JSON.stringify(value)}`);
  return value;
}

export function orangeBands(rows, minHeight = 8) {
  const bands = [];
  let run = null;
  for (const [y] of [...rows].sort((a, b) => a[0] - b[0])) {
    if (run && y - run.bottom <= 1) run.bottom = y;
    else {
      if (run && run.bottom - run.top + 1 >= minHeight) bands.push(run);
      run = { top: y, bottom: y };
    }
  }
  if (run && run.bottom - run.top + 1 >= minHeight) bands.push(run);
  return bands.map(b => ({
    top: b.top, bottom: b.bottom, center: Math.round((b.top + b.bottom) / 2),
  }));
}

// 沿对话列移动光标（x=340 在选项矩形 x∈[292,612] 内），每次移动后采样，
// 只接受“覆盖当前光标 y”的橙色行带（因果过滤掉对话框外的地图底色）。
// 单次采样可能读到悬停重绘之前的帧，因此无命中时补采一次；第一相位没
// 扫够 stopAfterRows 行时再补一个错位相位取并集。返回自上而下排序的
// 选项行带 = 选项 0..N。
export async function mapOptionRows(driver, {
  // step=7：行带高 ~16px、间距 ~19px，7px 步进每带仍命中 2 次以上；
  // step=5 时全程 77 次悬停采样 ~45s，是 LudiPQ 等 115s 预算的瓶颈之一。
  x = 340, fromY = 115, toY = 500, step = 7, stopAfterRows = Infinity,
  region = { x0: 300, x1: 480, y0: 100, y1: 510 },
} = {}) {
  if (await probe(driver, 'msnpc_active') !== 1)
    throw new Error('选项行扫描要求 NPC 对话处于打开状态');
  const found = new Map();
  const hoverAndSample = async y => {
    await driver.move(x, y);
    await sleep(80);
    let bands = orangeBands(await sampleOrangeRows(driver, region))
      .filter(band => band.top - 2 <= y && y <= band.bottom + 2);
    if (bands.length === 0) {
      await sleep(150);
      bands = orangeBands(await sampleOrangeRows(driver, region))
        .filter(band => band.top - 2 <= y && y <= band.bottom + 2);
    }
    for (const band of bands) found.set(band.top, band);
  };
  const scanPass = async offset => {
    for (let y = fromY + offset; y <= toY; y += step) {
      await hoverAndSample(y);
      if (found.size >= stopAfterRows) return true;
    }
    return false;
  };
  if (!await scanPass(0)) await scanPass(Math.max(2, step - 2));
  await driver.move(700, 60); // 光标移出对话，避免悬停态残留
  return [...found.values()].sort((a, b) => a.top - b.top);
}

// 悬停扫描得到的行带是“画布坐标”（UI 场景整体下移 VIEWYOFFSET=10px 绘
// 制），而点击命中矩形在未偏移的 UI 坐标系里——实测点击 band.top-10 恰
// 好命中所选行（191→选项0），band 中心反而会点到下一行。取 top-6 落在
// 命中矩形中部（矩形约 [top-10, top+5]）。
// 另：招募菜单的正文行也会被悬停扫描成行带（KerningPQ 实测：需求说明
// 3 行 + 真选项 3 行 + 底部区域 3 行＝9 带，rows[0] 落在正文上，点击无效
// 且无任何封包）。因此点击后校验「对话推进」（换提示/关框/换图），无效
// 则从该带起继续向下尝试最多 6 个候选带。
export async function clickOptionRow(driver, rows, index, label = '') {
  const row = rows[index];
  if (!row) throw new Error(`选项行 ${index} 未找到（${label}；共 ${rows.length} 行）`);
  const promptBefore = await promptText(driver).catch(() => '');
  const mapBefore = await mapId(driver).catch(() => 0);
  const advanced = async () => {
    if ((await probe(driver, 'msnpc_active')) === 0) return true;
    if ((await mapId(driver)) !== mapBefore) return true;
    return (await promptText(driver)) !== promptBefore;
  };
  for (let i = index; i < rows.length && i < index + 12; i += 1) {
    await driver.click(340, rows[i].top - 6);
    if (await waitFor(advanced, 2)) return rows[i];
  }
  return row;
}

// —— YES / OK 按钮 ————————————————————————————————————————————
// 点击对话底部按钮并等待效果（对话关闭 / 地图变化 / 提示变化）。x 取自
// UINpcTalk/WZ 常量推导的列，y 未命中时沿 footer 网格扫描；该列的误点只会
// 落在对话空白处，不会触发其它按钮。
// 点击对话底部按钮并等待效果（对话关闭 / 地图变化 / 提示变化）。
// 截图像素实证（CWKPQ-created.png vs fm-pq-teleport-0）：对话按钮槽位于
// x≈545..590（Yes 在 570 命中）；sendOk 的 OK 同槽，旧 x=518 偏左落空，
// 且按钮精灵实际绘制在 y≈477..494（旧扫描上界 454 也够不到）。因此
// 每种按钮给多个 x 候选 ×（浅+深）y 网格全扫。该列的误点只会落在对
// 话空白处，不会触发其它按钮。
export const dialogButtonXs = Object.freeze({
  yes: [570],
  no: [622, 610],
  // 620：CWKPQ 远征系列对话框的黄底 OK（实测按钮块 x598~641、cy≈481，
  // 位于白盒下方，视觉定位的蓝底/白底判据都覆盖不到）。放在首位：固定
  // 网格每个 x 要扫 ~30 个 y（每个 5s 生效等待），旧顺序会先空扫 10 分钟。
  ok: [620, 566, 572, 598, 518, 575],
});

function footerScanYs(center = 394) {
  const ys = [];
  for (let delta = 0; delta <= 60; delta += 4) ys.push(center + delta, center - delta);
  for (let y = 462; y <= 496; y += 4) ys.push(y);   // 按钮精灵实际行（截图实证）
  return [...new Set(ys)].filter(y => y > 160 && y < 520);
}

// 按钮实测为「蓝色底带 + 绿色字素」：结束(左) 确认/否(右) 是(中右)。
// 字素 y 随文本高度浮动（同一 OK 出现过 y311 与 y425+），故先视觉定位，
// 旧的固定网格只作兜底。kind → 候选选择：ok/no 取最右，yes 取右二。
function pickVisionButtons(buttons, kind) {
  const sorted = [...buttons].sort((a, b) => a.x - b.x);
  if (!sorted.length) return [];
  const rightmost = sorted[sorted.length - 1];
  if (kind === 'yes' && sorted.length >= 2) return [sorted[sorted.length - 2], rightmost];
  if (kind === 'yes') return [rightmost];
  return [rightmost, ...sorted.slice(0, -1).reverse()];
}

// 探针直读 UINpcTalk 按钮真实几何（新客户端导出 msnpctalk_button_field/
// msnpctalk_element_field）。视觉/网格坐标曾因 v83 与 post-Chaos 的
// UtilDlgEx 宽度差（529 vs 519）整体偏移 ~88px 而全部点空；探针给出按钮
// 相对坐标 + 元素原点，换算即游戏内坐标。which: 0=YES 1=NO 2=END 3=OK。
const PROBE_WHICH = Object.freeze({ yes: 0, no: 1, end: 2, ok: 3 });
async function probeDialogButton(driver, kind) {
  const which = PROBE_WHICH[kind];
  if (which === undefined) return null;
  try {
    const cc = (fn, ...args) => driver.evaluate(
      `Module.ccall('${fn}','number',[${args.map(() => "'number'").join(',')}],[${args.join(',')}])`);
    const px = Number(await cc('msnpctalk_element_field', 0));
    const py = Number(await cc('msnpctalk_element_field', 1));
    if (px <= -9000 || py <= -9000) return null;
    const bx = Number(await cc('msnpctalk_button_field', which, 0));
    const by = Number(await cc('msnpctalk_button_field', which, 1));
    const bw = Number(await cc('msnpctalk_button_field', which, 2));
    const bh = Number(await cc('msnpctalk_button_field', which, 3));
    const active = Number(await cc('msnpctalk_button_field', which, 4));
    if ([bx, by, bw, bh, active].some(v => v <= -9000) || !active || bw <= 0 || bh <= 0)
      return null;
    return { x: px + bx + (bw >> 1), y: py + by + (bh >> 1), probed: true };
  } catch {
    return null;
  }
}

// 截图坐标 → 游戏内部坐标：canvas 实际显示尺寸≠800×600（如 662×497、
// 左缘 79），driver.click 收的是内部坐标并自行做正向映射，故这里必须
// 先做逆向换算，否则视觉定位的按钮会被点偏约 5%。
export async function visionDialogButtons(driver) {
  try {
    const scale = await driver.evaluate(`(() => {
      const c = document.getElementById('canvas');
      const b = c.getBoundingClientRect();
      return { left: b.left, top: b.top, sw: b.width / c.width, sh: b.height / c.height };
    })()`);
    return findDialogButtons(decodePng(await driver.capture()))
      .map(b => ({
        ...b,
        x: Math.round((b.x - scale.left) / scale.sw),
        y: Math.round((b.y - scale.top) / scale.sh),
      }));
  } catch {
    return [];
  }
}

export async function clickDialogButton(driver, kind, effect, effectSeconds = 3) {
  const probed = await probeDialogButton(driver, kind);
  if (probed) {
    console.log(`INFO  对话按钮探针定位 ${kind}: (${probed.x},${probed.y})`);
    await driver.click(probed.x, probed.y);
    if (await waitFor(effect, effectSeconds)) return probed.y;
    console.log(`INFO  探针坐标未生效，退回视觉/网格`);
  }
  const seen = await visionDialogButtons(driver);
  if (seen.length)
    console.log(`INFO  对话按钮视觉定位 ${kind}: ${seen.map(b => `(${b.x},${b.y})`).join(' ')}`);
  else
    console.log(`INFO  对话按钮视觉定位 ${kind}: 无命中，退回固定网格`);
  for (const b of pickVisionButtons(seen, kind)) {
    await driver.click(b.x, b.y);
    if (await waitFor(effect, effectSeconds)) return b.y;
  }
  const xs = dialogButtonXs[kind];
  for (const x of xs) {
    for (const y of footerScanYs()) {
      await driver.click(x, y);
      if (await waitFor(effect, effectSeconds)) return y;
    }
  }
  throw new Error(`${kind} button click had no effect`);
}

export function dialogAdvanced(driver, { promptBefore, mapBefore }) {
  return async () => {
    if (await probe(driver, 'msnpc_active') === 0) return true;
    const mapNow = await mapId(driver);
    if (mapBefore !== undefined && mapNow !== mapBefore) return true;
    const promptNow = await promptText(driver);
    return promptBefore !== undefined && promptNow !== promptBefore;
  };
}

export function writeArtifacts(driver, dir, extra = {}) {
  fs.mkdirSync(dir, { recursive: true });
  if (driver.errors.length)
    fs.writeFileSync(path.join(dir, 'browser-errors.log'), `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(path.join(dir, 'browser-console.log'), `${driver.logs.join('\n')}\n`);
  for (const [name, text] of Object.entries(extra))
    fs.writeFileSync(path.join(dir, name), text);
}
