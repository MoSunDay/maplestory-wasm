// Char-select 079MAX4 bitmap chrome. The 079 client paints this screen from
// baked bitmaps (window frame, roster card, world banner, footer buttons,
// page arrows) instead of the flat procedural panels, so the contract below
// compares colour, not just luminance: an orange frame band, a blue card
// body, and pink plates all read as similar mid-tone luminance and would
// slip through a luminance-only check even when the art is mis-anchored.
// Rects are canvas rows in the 800x600 game space with the design
// VIEWYOFFSET=+10 already baked in (same convention as
// assertCharSelectModernLayout). Sampling shares the one readPixels pass of
// layout-pixels.mjs (sampleChannels); luminance margins are derived from
// the same mean channels, so the whole contract reads a single frame.
import { requireMargin, sampleChannels } from './layout-pixels.mjs';

// Frozen baseline rects with the measured mean values from a passing run:
//   frameTop   x200-600 y12-22   (255,221,34)  top orange edge
//   frameLeft  x2-11    y100-400 (255,203,34)  left orange edge
//   frameRight x789-797 y100-400 (255,199,34)  right orange edge
//   frameBottom x60-740 y550-570 (~233,167,48) warm footer band, R-B ~150
//   cardHeader x600-720 y266-272 (70,136,200)  card title band
//   cardFooter x600-720 y437-438 (68,136,203)  card bottom band
//   rankCaption x600-720 y394-397 (187,180,162, lum 176)
//   rankValue   x600-720 y415-429 (226,223,213, lum 221)
//   bannerBoard x510-650 y95-135  (lum 70)     world banner board
//   bannerAbove x510-650 y60-80   (lum 46)     dark sky strip above it
//   startBtn  x415-465 y505-523 (220,163,190)  pink plate
//   createBtn x175-225 y505-523 (221,180,201)  pink plate
//   deleteBtn x295-345 y505-523 (219,179,199)  pink plate
//   arrowL    x73-87   y480-496 (188,212,73)  left page-arrow kernel
//   arrowR    x673-687 y480-496 (188,212,73)  right page-arrow kernel
//   frameBand x60-740  y550-575 (B-G negative) pink-plate reference band
export const charSelectBitmapRects = Object.freeze([
  { name: 'frameTop', x0: 200, y0: 12, x1: 600, y1: 22 },
  { name: 'frameLeft', x0: 2, y0: 100, x1: 11, y1: 400 },
  { name: 'frameRight', x0: 789, y0: 100, x1: 797, y1: 400 },
  { name: 'frameBottom', x0: 60, y0: 550, x1: 740, y1: 570 },
  { name: 'cardHeader', x0: 600, y0: 266, x1: 720, y1: 272 },
  { name: 'cardFooter', x0: 600, y0: 437, x1: 720, y1: 438 },
  { name: 'rankCaption', x0: 600, y0: 394, x1: 720, y1: 397 },
  { name: 'rankValue', x0: 600, y0: 415, x1: 720, y1: 429 },
  { name: 'bannerBoard', x0: 510, y0: 95, x1: 650, y1: 135 },
  { name: 'bannerAbove', x0: 510, y0: 60, x1: 650, y1: 80 },
  { name: 'startBtn', x0: 415, y0: 505, x1: 465, y1: 523 },
  { name: 'createBtn', x0: 175, y0: 505, x1: 225, y1: 523 },
  { name: 'deleteBtn', x0: 295, y0: 505, x1: 345, y1: 523 },
  { name: 'arrowL', x0: 73, y0: 480, x1: 87, y1: 496 },
  { name: 'arrowR', x0: 673, y0: 480, x1: 687, y1: 496 },
  { name: 'frameBand', x0: 60, y0: 550, x1: 740, y1: 575 }
]);

const round = value => Math.round(value * 100) / 100;

// Mean of the three channel means equals the mean luminance, so the
// luminance checks below reuse the colour readback instead of a second
// sampling pass (and a second animation frame).
const luminanceOf = ([red, green, blue]) => round((red + green + blue) / 3);

// [text, satisfied] pair consumed by requireChannel; the text carries the
// measured value so a failure explains itself without another lookup.
const atLeast = (name, actual, minimum) =>
  [`${name} >= ${minimum} (got ${round(actual)})`, actual >= minimum];

// Channel gate: `requirements` holds every [text, satisfied] pair that must
// hold for one rect's mean [r, g, b]; the rect is reported once with all
// unmet clauses, keeping a broken screen to a single readable failure line.
function requireChannel(failures, label, rgb, requirements) {
  const unmet = requirements.filter(([, satisfied]) => !satisfied).map(([text]) => text);
  if (unmet.length === 0) return;
  failures.push(`${label}: rgb=[${rgb.join(', ')}] need ${unmet.join('; ')}`);
}

// 079 bitmap contract. Every check is occupancy-independent (no roster
// content is sampled), so no firstSlotOccupied argument is needed.
//   1. frameTop/frameLeft/frameRight are the straight 079 window edges: a
//      saturated orange (R >= 225, R-B >= 120, G-B >= 110). A mis-anchored
//      or fallback-drawn screen loses the yellow cast and fails all three.
//   2. frameBottom is the warmer orange/red footer band under the buttons
//      (R >= 190, R-B >= 80) - it reads redder than the straight edges.
//   3. cardHeader/cardFooter are the card's baked blue bands (B >= 170,
//      B-R >= 80); they prove the roster card landed on its 079 box.
//   4. rankValue - rankCaption >= 20 locks the two stacked value bands of
//      the card to the measured rows (caption 176, value 221).
//   5. bannerBoard - bannerAbove >= 10 proves the world banner board sits at
//      the 079 (578,112) anchor above the dark sky strip.
//   6. startBtn/createBtn/deleteBtn are the pink footer plates: R >= 195
//      and B-G >= 12, and each must out-pink the neutral-orange frameBand
//      by >= 40 in B-G - the band below them is the reference that turns
//      "pinkish" into an actual 079 plate check.
//   7. arrowL/arrowR sample the yellow kernels of the two page arrows
//      (G >= 190, G-B >= 90). The 079 arrows are two-state bitmaps whose
//      bright state is a saturated yellow; backdrop pixels behind a
//      missing arrow keep B high and land far below the G-B gate. A
//      luminance margin against the parallax backdrop was measured first
//      and rejected - the right-hand sky strip drifts within ~5 lum of
//      the arrow, so a colour kernel is the stable signature.
export async function assertCharSelectBitmapLayout(driver) {
  const channels = await sampleChannels(driver, charSelectBitmapRects);
  const luminance = name => luminanceOf(channels[name]);
  const failures = [];

  // 1. Straight orange window edges.
  for (const name of ['frameTop', 'frameLeft', 'frameRight']) {
    const [red, green, blue] = channels[name];
    requireChannel(failures, name, channels[name], [
      atLeast('R', red, 225),
      atLeast('R-B', red - blue, 120),
      atLeast('G-B', green - blue, 110)
    ]);
  }

  // 2. Warm footer band under the pink plates.
  const [footerRed, , footerBlue] = channels.frameBottom;
  requireChannel(failures, 'frameBottom', channels.frameBottom, [
    atLeast('R', footerRed, 190),
    atLeast('R-B', footerRed - footerBlue, 80)
  ]);

  // 3. Blue baked bands of the roster card.
  for (const name of ['cardHeader', 'cardFooter']) {
    const [red, , blue] = channels[name];
    requireChannel(failures, name, channels[name], [
      atLeast('B', blue, 170),
      atLeast('B-R', blue - red, 80)
    ]);
  }

  // 4./5. Luminance margins of the stacked card bands and the banner stack.
  requireMargin(failures, 'rankValue-rankCaption',
    luminance('rankValue'), luminance('rankCaption'), 20);
  requireMargin(failures, 'bannerBoard-bannerAbove',
    luminance('bannerBoard'), luminance('bannerAbove'), 10);

  // 6. Pink plates against the recorded orange frame band below them.
  const frameBandBlueMinusGreen = channels.frameBand[2] - channels.frameBand[1];
  for (const name of ['startBtn', 'createBtn', 'deleteBtn']) {
    const [red, green, blue] = channels[name];
    requireChannel(failures, name, channels[name], [
      atLeast('R', red, 195),
      atLeast('B-G', blue - green, 12),
      atLeast(`B-G over frameBand B-G (${round(frameBandBlueMinusGreen)})`,
        blue - green - frameBandBlueMinusGreen, 40)
    ]);
  }

  // 7. Page-arrow kernels stay saturated yellow (backdrop is blue-green).
  for (const name of ['arrowL', 'arrowR']) {
    const [, green, blue] = channels[name];
    requireChannel(failures, name, channels[name], [
      atLeast('G', green, 190),
      atLeast('G-B', green - blue, 90)
    ]);
  }

  const summary = JSON.stringify({
    frameTop: channels.frameTop, frameBottom: channels.frameBottom,
    cardHeader: channels.cardHeader, cardFooter: channels.cardFooter,
    frameBand: channels.frameBand,
    startBtn: channels.startBtn, createBtn: channels.createBtn,
    deleteBtn: channels.deleteBtn,
    rankCaption: luminance('rankCaption'), rankValue: luminance('rankValue'),
    bannerAbove: luminance('bannerAbove'), bannerBoard: luminance('bannerBoard'),
    arrowL: channels.arrowL, arrowR: channels.arrowR
  });
  if (failures.length > 0) {
    throw new Error(
      `char-select not in 079 bitmap layout ${summary}: ${failures.join('; ')}`);
  }
  console.log(`PASS  char-select 079 bitmap layout ${summary}`);
}
