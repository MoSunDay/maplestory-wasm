// Pixel-level layout assertions for the login-chain screens. These lock in
// the commit 49e4dc6 layout - world-select Nova pinned to the sidebar top
// slot and the char-select single-row roster - with measured baselines:
// Nova top slot beats the old mid-low slot / sidebar background by ~36/~46
// mean luminance, char-select arrows beat their background references by
// ~40/~86 and the slot-0 nametag beats the left background by ~45.
// Sampling runs fully in page on the live WebGL drawing buffer
// (canvas #canvas) via requestAnimationFrame + gl.readPixels, with the
// backing-buffer scale canvas.width/800, a Y-flip, and FRAMEBUFFER binding
// save/restore - the proven pattern from gm-shop-flow.mjs.

async function evaluateAwait(driver, expression) {
  const result = await driver.send('Runtime.evaluate', {
    expression, awaitPromise: true, returnByValue: true
  });
  if (result.exceptionDetails) {
    throw new Error(`page evaluation failed: ${JSON.stringify(result.exceptionDetails)}`);
  }
  return result.result.value;
}

// In-page sampler: the requestAnimationFrame callback is ordered after the
// game render pass and before compositing clears the drawing buffer. One
// readPixels covers the bounding box of all requested rects; the game GL
// state is left untouched. Rects are game-coord {name, x0, y0, x1, y1} in
// the 800x600 space and resolve to mean luminance (0-255).
function samplerExpression(rects) {
  const config = JSON.stringify({ rects });
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
      const x0 = Math.min(...config.rects.map(rect => mapX(rect.x0)));
      const x1 = Math.max(...config.rects.map(rect => mapX(rect.x1)));
      const y0 = Math.min(...config.rects.map(rect => mapY(rect.y0)));
      const y1 = Math.max(...config.rects.map(rect => mapY(rect.y1)));
      const width = x1 - x0 + 1, height = y1 - y0 + 1;
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
      const regionMean = rect => {
        let total = 0, samples = 0;
        for (let y = rect.y0; y <= rect.y1; y += 1)
          for (let x = rect.x0; x <= rect.x1; x += 1) {
            total += luminance(x, y);
            samples += 1;
          }
        return round(total / samples);
      };
      const means = {};
      for (const rect of config.rects) means[rect.name] = regionMean(rect);
      resolve({ means });
    });
  }))()`;
}

// Generic sampler: evaluates the in-page expression for a list of game-coord
// rects and returns a map name -> mean luminance (0-255).
export async function sampleLuminance(driver, rects) {
  const sample = await evaluateAwait(driver, samplerExpression(rects));
  if (!sample || !sample.means) {
    throw new Error(`layout pixel sampling failed: ${JSON.stringify(sample)}`);
  }
  return sample.means;
}

// World-select: the Nova button bitmap lands at (664,59)-(760,85) in the top
// sidebar slot; the pre-49e4dc6 hardcoded anchor plus baked origin left it
// in the mid-low area that must now be plain background.
const worldSelectRects = Object.freeze([
  { name: 'novaTop', x0: 665, y0: 60, x1: 759, y1: 84 },
  { name: 'oldSlot', x0: 665, y0: 101, x1: 759, y1: 129 },
  { name: 'sidebarBg', x0: 665, y0: 141, x1: 759, y1: 169 }
]);

function requireMargin(failures, label, value, reference, minimum) {
  const margin = Math.round((value - reference) * 100) / 100;
  if (margin < minimum) {
    failures.push(`${label}: ${value} - ${reference} = ${margin} (need >= ${minimum})`);
  }
}

export async function assertWorldSelectNovaTopSlot(driver) {
  const means = await sampleLuminance(driver, worldSelectRects);
  const failures = [];
  requireMargin(failures, 'novaTop-oldSlot', means.novaTop, means.oldSlot, 20);
  requireMargin(failures, 'novaTop-sidebarBg', means.novaTop, means.sidebarBg, 20);
  if (failures.length > 0) {
    throw new Error(
      `world-select Nova not in sidebar top slot ${JSON.stringify(means)}: ${failures.join('; ')}`);
  }
  console.log(`PASS  world-select Nova pinned to sidebar top slot ${JSON.stringify(means)}`);
}

// Char-select: page arrows sit at (46,240)/(533,240), occupied slots ride
// disc platforms at char_slot_pos(i) = (130+120i, 270) with their white
// nametags just below at y 282-290. bgAbove samples the strip between the
// card tops and the right arrow; the old bgR rect (590,241)-(620,287)
// overlapped the charInfo board and read high (146.9 vs 44.9).
const charSelectRects = Object.freeze([
  { name: 'arrowL', x0: 47, y0: 241, x1: 83, y1: 287 },
  { name: 'arrowR', x0: 534, y0: 241, x1: 571, y1: 287 },
  { name: 'bgAbove', x0: 536, y0: 196, x1: 568, y1: 226 },
  { name: 'bgL', x0: 90, y0: 241, x1: 120, y1: 287 },
  { name: 'nametag0', x0: 110, y0: 282, x1: 150, y1: 290 }
]);

export async function assertCharSelectModernLayout(driver) {
  const means = await sampleLuminance(driver, charSelectRects);
  const failures = [];
  requireMargin(failures, 'arrowL-bgL', means.arrowL, means.bgL, 20);
  requireMargin(failures, 'arrowR-bgAbove', means.arrowR, means.bgAbove, 20);
  requireMargin(failures, 'nametag0-bgL', means.nametag0, means.bgL, 25);
  // The lower screen is the textured pier deck of backdrop back/11 (band
  // means 242.7 vs 167.4 across x), so old-grid second-row sprites are not
  // luminance-separable; nametag y + page-arrow band + PAGESIZE lock single-row.
  const summary = JSON.stringify({
    arrowL: means.arrowL, arrowR: means.arrowR, bgL: means.bgL,
    bgAbove: means.bgAbove, nametag0: means.nametag0
  });
  if (failures.length > 0) {
    throw new Error(
      `char-select not in single-row modern layout ${summary}: ${failures.join('; ')}`);
  }
  console.log(`PASS  char-select single-row modern layout ${summary}`);
}
