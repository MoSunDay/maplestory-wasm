// 纯函数式 PNG 解码 + NPC 对话框按钮定位。
// 动机：UINpcTalk 的 OK/YES/NO 按钮从对话框右缘布局，
// y 随文本高度变化（实证：同一 OK 按钮在不同对话框出现在 y310 / y394+），
// 固定网格点击会长时间落空。这里直接解码截图找按钮。
import zlib from 'node:zlib';

export function parseChunks(buffer) {
  const view = Buffer.isBuffer(buffer) ? buffer : Buffer.from(buffer);
  const chunks = [];
  let off = 8;
  while (off + 8 <= view.length) {
    const len = view.readUInt32BE(off);
    const type = view.toString('ascii', off + 4, off + 8);
    const data = view.subarray(off + 8, off + 8 + len);
    chunks.push({ type, data });
    off += 12 + len;
    if (type === 'IEND') break;
  }
  return chunks;
}

function paeth(a, b, c) {
  const p = a + b - c;
  const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

function unfilter(raw, width, height, bpp) {
  const stride = width * bpp;
  const out = new Uint8Array(stride * height);
  let src = 0;
  for (let y = 0; y < height; y++) {
    const filter = raw[src++];
    const row = y * stride;
    for (let x = 0; x < stride; x++) {
      const v = raw[src++];
      const left = x >= bpp ? out[row + x - bpp] : 0;
      const up = y > 0 ? out[row - stride + x] : 0;
      const ul = y > 0 && x >= bpp ? out[row - stride + x - bpp] : 0;
      let px;
      switch (filter) {
        case 0: px = v; break;
        case 1: px = v + left; break;
        case 2: px = v + up; break;
        case 3: px = v + ((left + up) >> 1); break;
        case 4: px = v + paeth(left, up, ul); break;
        default: throw new Error(`PNG filter ${filter} unsupported`);
      }
      out[row + x] = px & 0xff;
    }
  }
  return out;
}

// 返回 { width, height, rgb: Uint8Array(width*height*3) }
export function decodePng(buffer) {
  const chunks = parseChunks(buffer);
  const ihdr = chunks.find(c => c.type === 'IHDR').data;
  const width = ihdr.readUInt32BE(0), height = ihdr.readUInt32BE(4);
  const bitDepth = ihdr[8], colorType = ihdr[9], interlace = ihdr[12];
  if (bitDepth !== 8) throw new Error(`PNG bit depth ${bitDepth} unsupported`);
  if (interlace !== 0) throw new Error('PNG interlace unsupported');
  const srcBpp = colorType === 6 ? 4 : colorType === 2 ? 3 : null;
  if (!srcBpp) throw new Error(`PNG color type ${colorType} unsupported`);
  const raw = zlib.inflateSync(Buffer.concat(chunks.filter(c => c.type === 'IDAT').map(c => c.data)));
  const px = unfilter(raw, width, height, srcBpp);
  const rgb = new Uint8Array(width * height * 3);
  for (let i = 0, j = 0; i < rgb.length; i += 3, j += srcBpp) {
    rgb[i] = px[j]; rgb[i + 1] = px[j + 1]; rgb[i + 2] = px[j + 2];
  }
  return { width, height, rgb };
}

const isLight = (r, g, b) => r > 205 && g > 205 && b > 195;
const isBlue = (r, g, b) => b > 120 && b > r + 35 && b >= g + 18 && r < 130;
// 按钮字素（OK/是/否等）实测为绿-黄绿（如 187,238,0 / 102,170,0）
const isGlyph = (r, g, b) => g > 130 && g > b + 40 && g > r + 20;

export const pixelAt = (img, x, y) => {
  const i = (y * img.width + x) * 3;
  return [img.rgb[i], img.rgb[i + 1], img.rgb[i + 2]];
};

// 白色对话框盒子：统计每行“浅色”像素比例，找连续浅色行带。
export function findDialogBox(img) {
  const rows = [];
  for (let y = 0; y < img.height; y++) {
    let light = 0;
    for (let x = 0; x < img.width; x += 2) {
      const [r, g, b] = pixelAt(img, x, y);
      if (isLight(r, g, b)) light++;
    }
    rows.push(light / (img.width >> 1));
  }
  const band = rows
    .map((ratio, y) => ({ ratio, y }))
    .filter(r => r.ratio > 0.22);
  if (!band.length) return null;
  const top = band[0].y, bottom = band[band.length - 1].y;
  // 盒子的水平范围：取带内最亮几行的浅色列并集
  let minX = img.width, maxX = 0;
  for (const { y } of band) {
    for (let x = 0; x < img.width; x++) {
      const [r, g, b] = pixelAt(img, x, y);
      if (isLight(r, g, b)) { if (x < minX) minX = x; if (x > maxX) maxX = x; }
    }
  }
  return { top, bottom, left: minX, right: maxX, height: bottom - top + 1 };
}

// 对话框按钮：以绿色字素（OK/是/否/结束等按钮文字）为锚点，
// 向两侧沿蓝色底扩张得到按钮矩形。实测布局为「整条蓝色底带 +
// 左 END 右 OK」或「右 NO 左 YES」，字素即点击目标。
export function findDialogButtons(img) {
  // 按钮必须在对话框盒子内部：用盒子底边代替旧固定 y<=445 截断
  // （Kerning/CWKPQ 实测按钮行 y452~468，旧截断会整行丢弃）。
  const box = findDialogBox(img);
  const boxBottom = box ? box.bottom - 2 : 445;
  const xMin = 150, xMax = Math.min(img.width - 1, 760);
  const yMin = 170, yMax = Math.min(img.height - 2, 500);
  const hits = [];
  for (let y = yMin; y <= yMax; y++) {
    let runStart = -1, run = 0;
    for (let x = xMin; x <= xMax; x++) {
      const [r, g, b] = pixelAt(img, x, y);
      if (isGlyph(r, g, b)) {
        if (run === 0) runStart = x;
        run++;
      } else {
        if (run >= 2) hits.push({ y, x: runStart, len: run });
        run = 0;
      }
    }
    if (run >= 2) hits.push({ y, x: runStart, len: run });
  }
  if (!hits.length) return [];
  // 行聚类成字素块：与任意「底部未封闭且 x 相邻」的块合并（并排按钮互不吸收）
  const blocks = [];
  for (const h of hits) {
    const open = blocks.filter(
      // 合并间距阈值 12px：CJK 文本字距 ~4-6px 仍会聚成整行（后被宽度
      // 上限剔除），而「是/否」并排按钮实测间距 25px（Kerning/CWKPQ 盒底
      // 无蓝底按钮对），30px 阈值会把两钮并成 168px 宽块再被宽度上限误杀。
      bk => h.y - bk.bottom <= 2 && h.x < bk.right + 12 && h.x + h.len > bk.left - 12
    );
    if (open.length) {
      for (const bk of open) {
        bk.bottom = h.y;
        bk.left = Math.min(bk.left, h.x);
        bk.right = Math.max(bk.right, h.x + h.len - 1);
      }
    } else {
      blocks.push({ top: h.y, bottom: h.y, left: h.x, right: h.x + h.len - 1 });
    }
  }
  // 统计一个字素块区域的属性（字素/蓝底像素计数 + 盒内浅底比例）。
  const describeBlock = bk => {
    const cy = Math.round((bk.top + bk.bottom) / 2);
    let left = bk.left, right = bk.right;
    while (left - 1 >= xMin && isBlue(...pixelAt(img, left - 1, cy))) left--;
    while (right + 1 <= xMax && isBlue(...pixelAt(img, right + 1, cy))) right++;
    let glyph = 0, blue = 0, light = 0;
    for (let y = bk.top; y <= bk.bottom; y++)
      for (let x = left; x <= right; x++) {
        const [cr, cg, cb] = pixelAt(img, x, y);
        if (isGlyph(cr, cg, cb)) glyph++;
        if (isBlue(cr, cg, cb)) blue++;
      }
    const sampleY = box
      ? Math.min(bk.bottom + 6, boxBottom - 2)
      : Math.min(bk.bottom + 6, img.height - 1);
    for (let x = left; x <= right; x++) {
      const [cr, cg, cb] = pixelAt(img, x, sampleY);
      if (isLight(cr, cg, cb)) light++;
    }
    const lightRatio = light / Math.max(1, right - left + 1);
    return {
      x: Math.round((bk.left + bk.right) / 2), y: cy, bottom: bk.bottom,
      left, right, top: bk.top, width: right - left + 1,
      height: bk.bottom - bk.top + 1, glyph, blue, lightRatio,
    };
  };
  // 无蓝底的宽块（盒底「是/否」并排按钮实测间距 <12px 会聚成 168px）按
  // 字素列占空拆分；有蓝底的宽块维持整块丢弃（蓝底文本带会被拆成多个
  // 带 blue 的小假钮，必须整体拒绝）。
  const splitWideBlock = bk => {
    const cols = [];
    for (let x = bk.left; x <= bk.right; x++) {
      let any = false;
      for (let y = bk.top; y <= bk.bottom; y++) {
        if (isGlyph(...pixelAt(img, x, y))) { any = true; break; }
      }
      cols.push({ x, any });
    }
    const parts = [];
    for (const c of cols) {
      const last = parts[parts.length - 1];
      if (c.any && last && c.x - last.right <= 1) last.right = c.x;
      else if (c.any) parts.push({ left: c.x, right: c.x });
    }
    return parts
      .filter(p => p.right - p.left >= 8)
      .map(p => ({ top: bk.top, bottom: bk.bottom, left: p.left, right: p.right }));
  };
  return blocks
    .filter(bk => bk.bottom - bk.top >= 8)
    .map(describeBlock)
    .flatMap(b => (b.width > 140 && b.blue < 60 * Math.max(1, b.height))
      ? splitWideBlock(b).map(describeBlock)
      : [b])
    .filter(b => b.glyph >= 10 && b.width <= 140
      && (b.blue >= 60 || (b.lightRatio >= 0.45 && b.bottom >= boxBottom - 40)))
    // 同一按钮可能被拆成重复块，按中心去重
    .filter((b, i, all) => all.findIndex(o => o.x === b.x && o.y === b.y) === i);
}
