// In-game display resolution acceptance flow.
//
// Verifies the real render-surface switch between the 800x600 default and
// 1280x720: boot-time localStorage restore, Game Options menu wiring, cancel
// semantics, canvas backing size, GL viewport, centered design-canvas origin,
// persistence, and the 800x600 round trip.
//
// Run: node tools/verify/maplestory-webui/resolution-flow.mjs
// Env:
//   CHROME_BIN / RESOLUTION_CHROME_BIN   browser (default /usr/bin/chromium-browser)
//   RESOLUTION_URL                       page (default :8000/web/index.html)
//   RESOLUTION_DEBUG_PORT                CDP port (default 9251)
//   RESOLUTION_ACCOUNT / RESOLUTION_PASSWORD  login (default test1/test1)
//   RESOLUTION_CARD                      character card index 0..3 (default 1)

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.RESOLUTION_CHROME_BIN
  || process.env.CHROME_BIN
  || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.RESOLUTION_DEBUG_PORT || 9251);
const pageUrl = process.env.RESOLUTION_URL
  || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.RESOLUTION_ACCOUNT || 'test1';
const password = process.env.RESOLUTION_PASSWORD || 'test1';
const cardIndex = Number(process.env.RESOLUTION_CARD || 1);
const storageKey = 'maplestory.display.resolution';
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = index => ({ x: 105 + 120 * index + 25, y: 210 });
const designNext = Object.freeze({ x: 505, y: 308 });
const designOk = Object.freeze({ x: 342, y: 372 });
const wideOrigin = Object.freeze({ x: 240, y: 60 });

let failed = false;
let originalStored = null;
let storageCaptured = false;

function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}

async function waitFor(predicate, seconds = 45) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    try {
      if (await predicate()) return true;
    } catch (error) {
      // The runtime may still be navigating.
    }
    await sleep(250);
  }
  return false;
}

function probe(driver, name) {
  return driver.evaluate(
    `Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
}

async function imeState(driver) {
  return await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas').getBoundingClientRect();
    const input = document.getElementById('ime-input');
    const rect = input.getBoundingClientRect();
    return {
      active: !!(window.MapleWasmIME && window.MapleWasmIME.active),
      resolution: window.MapleWasmUI
        ? window.MapleWasmUI.getResolution() : null,
      canvas: {
        left: canvas.left, top: canvas.top,
        width: canvas.width, height: canvas.height
      },
      rect: {
        left: rect.left, top: rect.top,
        width: rect.width, height: rect.height
      }
    };
  })()`);
}

function imeFieldMatches(ime, origin, field) {
  if (!ime.active || !ime.resolution) return false;
  const scaleX = ime.canvas.width / ime.resolution.width;
  const scaleY = ime.canvas.height / ime.resolution.height;
  const expectedLeft = ime.canvas.left + (field.x + origin.x) * scaleX;
  const expectedTop = ime.canvas.top + (field.y + origin.y) * scaleY;
  const expectedWidth = Math.max(40, field.width * scaleX);
  const expectedHeight = Math.max(24, field.height * scaleY);
  return Math.abs(ime.rect.left - expectedLeft) <= 1
    && Math.abs(ime.rect.top - expectedTop) <= 1
    && Math.abs(ime.rect.width - expectedWidth) <= 1
    && Math.abs(ime.rect.height - expectedHeight) <= 1;
}

function uiStateIs(driver, expected) {
  return async () => await probe(driver, 'msui_state') === expected;
}

function assetsIdle(driver) {
  return async () => await probe(driver, 'msstage_loading') === 0
    && await probe(driver, 'msasset_blocking') === 0
    && await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

async function waitForModule(driver) {
  return await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
}

async function setStoredResolution(driver, value) {
  await driver.evaluate(value === null
    ? `localStorage.removeItem(${JSON.stringify(storageKey)})`
    : `localStorage.setItem(${JSON.stringify(storageKey)}, ${JSON.stringify(value)})`);
  await driver.navigate(`${pageUrl}?resolution-flow=${Date.now()}`);
  if (!await waitForModule(driver))
    throw new Error('WASM module did not initialize after reload');
}

async function resolutionState(driver) {
  return await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const rect = canvas.getBoundingClientRect();
    const ui = window.MapleWasmUI;
    const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
    return {
      cppWidth: Module.ccall('msui_resolution_width', 'number', [], []),
      cppHeight: Module.ccall('msui_resolution_height', 'number', [], []),
      width: canvas.width,
      height: canvas.height,
      rect: { left: rect.left, top: rect.top, width: rect.width, height: rect.height },
      viewport: gl ? Array.from(gl.getParameter(gl.VIEWPORT)) : null,
      ui: ui ? ui.getResolution() : null,
      origin: ui ? ui.getDesignOrigin() : null,
      viewportSize: {
        width: visualViewport ? visualViewport.width : innerWidth,
        height: visualViewport ? visualViewport.height : innerHeight,
        offsetLeft: visualViewport && Number.isFinite(visualViewport.offsetLeft)
          ? visualViewport.offsetLeft : 0,
        offsetTop: visualViewport && Number.isFinite(visualViewport.offsetTop)
          ? visualViewport.offsetTop : 0
      },
      loadingHidden: document.getElementById('loading-screen')
        .classList.contains('is-hidden'),
      stored: localStorage.getItem(${JSON.stringify(storageKey)})
    };
  })()`);
}

function isResolution(state, width, height) {
  return state.cppWidth === width && state.cppHeight === height
    && state.width === width && state.height === height;
}

function fitSize(viewWidth, viewHeight, gameWidth, gameHeight) {
  const validGame = Number.isFinite(gameWidth) && gameWidth > 0
    && Number.isFinite(gameHeight) && gameHeight > 0;
  const designWidth = validGame ? gameWidth : 800;
  const designHeight = validGame ? gameHeight : 600;
  if (viewWidth * designHeight <= viewHeight * designWidth) {
    const fittedWidth = Math.max(1, Math.floor(viewWidth));
    return {
      width: fittedWidth,
      height: Math.max(1, Math.floor(fittedWidth * designHeight / designWidth))
    };
  }
  const fittedHeight = Math.max(1, Math.floor(viewHeight));
  return {
    width: Math.max(1, Math.floor(fittedHeight * designWidth / designHeight)),
    height: fittedHeight
  };
}

function centeredCss(state) {
  const view = state.viewportSize;
  const ui = state.ui || {};
  const fitted = fitSize(view.width, view.height,
    Number.isFinite(ui.width) ? ui.width : 800,
    Number.isFinite(ui.height) ? ui.height : 600);
  const offsetLeft = Number.isFinite(view.offsetLeft) ? view.offsetLeft : 0;
  const offsetTop = Number.isFinite(view.offsetTop) ? view.offsetTop : 0;
  // #canvas keeps translateX(-50%), so its left edge sits at the fitted
  // center; the visual viewport offset shifts that center on pinch zoom.
  const center = offsetLeft + view.width / 2;
  return Math.abs(state.rect.width - fitted.width) <= 1
    && Math.abs(state.rect.height - fitted.height) <= 1
    && Math.abs((state.rect.left + state.rect.width / 2) - center) <= 1
    && Math.abs(state.rect.top - offsetTop) <= 1;
}

async function assertBootResolution(driver, width, height, origin) {
  if (!await waitFor(async () => {
    const state = await resolutionState(driver);
    return isResolution(state, width, height)
      && state.viewport && state.viewport[2] === width
      && state.viewport[3] === height;
  }, 30))
    throw new Error(`boot resolution ${width}x${height} never became ready`);
  const state = await resolutionState(driver);
  report(isResolution(state, width, height),
    `boot restored ${width}x${height} C++ and canvas backing (${JSON.stringify(state)})`);
  report(state.ui && state.ui.width === width && state.ui.height === height,
    `browser controller reports ${width}x${height}`);
  report(state.origin && state.origin.x === origin.x && state.origin.y === origin.y,
    `design origin is ${JSON.stringify(origin)}`);
  report(state.viewport && state.viewport[2] === width && state.viewport[3] === height,
    `GL viewport is ${width}x${height}`);
  report(centeredCss(state),
    `CSS presentation stays centered and top-aligned (${JSON.stringify(state.rect)})`);
}

async function loginAndEnterGame(driver) {
  if (!await waitFor(uiStateIs(driver, uiState.login), 30))
    throw new Error('login UI did not activate');
  if (!await waitFor(() => driver.evaluate(
    "document.getElementById('loading-screen').classList.contains('is-hidden')"), 30))
    throw new Error('loading screen did not dismiss');
  await sleep(500);
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 45))
    throw new Error('world-select UI did not activate; check credentials');
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 45))
    throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60))
    throw new Error('character-select assets never reached idle');

  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered)
    throw new Error(`character card ${cardIndex} did not enter the game`);
  if (!await waitFor(assetsIdle(driver), 90))
    throw new Error('in-game assets never reached idle');
}

async function openSystemMenu(driver) {
  await driver.click(400, 300);
  let point = { x: -32768, y: -32768 };
  for (let attempt = 0; attempt < 5 && point.x < 0; attempt += 1) {
    await driver.key('Escape', 'Escape', 27);
    await sleep(350);
    point = {
      x: await probe(driver, 'msgame_options_menu_x'),
      y: await probe(driver, 'msgame_options_menu_y')
    };
  }
  if (!Number.isFinite(point.x) || point.x < 0)
    throw new Error(`Game Options menu point unavailable: ${JSON.stringify(point)}`);
  return point;
}

async function openGameOptions(driver) {
  const point = await openSystemMenu(driver);
  await driver.click(point.x, point.y);
  if (!await waitFor(async () => await probe(driver, 'msgame_options_active') === 1, 10))
    throw new Error('Game Options window did not activate');
  await sleep(250);
  // The menu probe only resolves while the popup is open, so hand the point
  // captured before the click back to the caller for anchoring checks.
  return point;
}

function nextButton(driver) {
  return driver.evaluate(`({
    x: Module.ccall('msgame_options_next_x', 'number', [], []),
    y: Module.ccall('msgame_options_next_y', 'number', [], [])
  })`);
}

function okButton(driver) {
  return driver.evaluate(`({
    x: Module.ccall('msgame_options_ok_x', 'number', [], []),
    y: Module.ccall('msgame_options_ok_y', 'number', [], [])
  })`);
}

function pointMatches(point, expected) {
  return Number.isFinite(point.x) && Number.isFinite(point.y)
    && Math.abs(point.x - expected.x) <= 1
    && Math.abs(point.y - expected.y) <= 1;
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    originalStored = await driver.evaluate(
      `localStorage.getItem(${JSON.stringify(storageKey)})`);
    storageCaptured = true;

    // 1. Persisted 1280x720 is applied before GLFW/UI construction.
    await setStoredResolution(driver, '1');
    await assertBootResolution(driver, 1280, 720, wideOrigin);
    report((await resolutionState(driver)).stored === '1',
      'boot read the persisted preset index');

    // The login account field is focused by the UILogin constructor. Its DOM
    // IME anchor must already include the 1280x720 design origin and the
    // VIEWYOFFSET scene shift, not just the field's local bounds.
    if (!await waitFor(() => driver.evaluate(
      "!!(window.MapleWasmIME && window.MapleWasmIME.active)"), 10))
      throw new Error('login IME field did not activate');
    const loginIme = await imeState(driver);
    report(imeFieldMatches(loginIme, wideOrigin, {
      x: 315, y: 249 + 10, width: 150, height: 24
    }), `login IME anchor follows the 1280x720 origin (${JSON.stringify(loginIme)})`);

    // 2. Clear it and boot the default before exercising the in-game flow.
    await setStoredResolution(driver, null);
    await assertBootResolution(driver, 800, 600, { x: 0, y: 0 });
    report((await resolutionState(driver)).stored === null,
      'missing stored value falls back to 800x600');

    await loginAndEnterGame(driver);
    const narrowMenu = await openGameOptions(driver);
    report(await probe(driver, 'msgame_options_index') === 0,
      'Game Options opens on the active 800x600 preset');
    report(narrowMenu.x > 0 && narrowMenu.x < 800 && narrowMenu.y > 0,
      `800x600 status bar menu is right-anchored (${JSON.stringify(narrowMenu)})`);

    const narrowNext = await nextButton(driver);
    const narrowOk = await okButton(driver);
    report(pointMatches(narrowNext, designNext),
      `800x600 next control at ${JSON.stringify(narrowNext)}`);
    report(pointMatches(narrowOk, designOk),
      `800x600 OK control at ${JSON.stringify(narrowOk)}`);

    // 3. Cancel is a pure preview: Escape closes without applying.
    await driver.click(narrowNext.x, narrowNext.y);
    await sleep(200);
    report(await probe(driver, 'msgame_options_index') === 1,
      'next cycles to 1280x720');
    await driver.click(narrowNext.x, narrowNext.y);
    await sleep(200);
    report(await probe(driver, 'msgame_options_index') === 0,
      'cycling wraps back to 800x600');
    await driver.click(narrowNext.x, narrowNext.y);
    await sleep(200);
    await driver.key('Escape', 'Escape', 27);
    if (!await waitFor(async () => await probe(driver, 'msgame_options_active') === 0, 5))
      throw new Error('Game Options did not close after Escape');
    const cancelled = await resolutionState(driver);
    report(isResolution(cancelled, 800, 600),
      'Escape cancels without changing the render surface');
    report(cancelled.stored === null,
      'Escape does not persist a pending resolution');

    // 4. Apply 1280x720 and verify every presentation surface.
    await openGameOptions(driver);
    await driver.click(narrowNext.x, narrowNext.y);
    await sleep(200);
    const wideOk = await okButton(driver);
    report(pointMatches(wideOk, designOk),
      'preview uses the current 800x600 layout until OK');
    await driver.click(wideOk.x, wideOk.y);
    if (!await waitFor(async () => await probe(driver, 'msgame_options_active') === 0, 10))
      throw new Error('Game Options did not close after OK');
    if (!await waitFor(async () => {
      const state = await resolutionState(driver);
      return isResolution(state, 1280, 720);
    }, 10))
      throw new Error('runtime resolution did not switch to 1280x720');

    const wide = await resolutionState(driver);
    report(wide.ui && wide.ui.width === 1280 && wide.ui.height === 720,
      'browser controller switched to 1280x720');
    report(wide.origin && wide.origin.x === wideOrigin.x && wide.origin.y === wideOrigin.y,
      `design canvas origin is ${JSON.stringify(wideOrigin)}`);
    report(wide.viewport && wide.viewport[2] === 1280 && wide.viewport[3] === 720,
      `GL viewport updated to ${JSON.stringify(wide.viewport)}`);
    report(centeredCss(wide),
      `1280x720 CSS surface is centered and top-aligned (${JSON.stringify(wide.rect)})`);
    report(wide.stored === '1',
      `localStorage persisted ${JSON.stringify(storageKey)}=1`);

    // 5. Reopen at 1280: the settings panel and menu coordinates must follow
    // the centered 800x600 design canvas while the status bar stays full-width.
    const wideMenu = await openGameOptions(driver);
    const wideNext = await nextButton(driver);
    const wideOkButton = await okButton(driver);
    report(pointMatches(wideNext, {
      x: designNext.x + wideOrigin.x,
      y: designNext.y + wideOrigin.y
    }), `1280x720 next control is design-origin shifted (${JSON.stringify(wideNext)})`);
    report(pointMatches(wideOkButton, {
      x: designOk.x + wideOrigin.x,
      y: designOk.y + wideOrigin.y
    }), `1280x720 OK control is design-origin shifted (${JSON.stringify(wideOkButton)})`);
    report(wideMenu.x > 0 && wideMenu.x < 1280 && wideMenu.y > 0,
      `status bar menu remains visible at 1280x720 (${JSON.stringify(wideMenu)})`);
    await driver.screenshot('20-resolution-1280x720-options');
    await driver.click(wideOkButton.x, wideOkButton.y);
    if (!await waitFor(async () => await probe(driver, 'msgame_options_active') === 0, 10))
      throw new Error('Game Options did not close after OK on the already-active preset');
    report(isResolution(await resolutionState(driver), 1280, 720),
      'OK on the already-active preset keeps the render surface stable');

    // 6. Switch back to 800x600 at runtime and verify the reverse transition.
    await openGameOptions(driver);
    await driver.click(wideNext.x, wideNext.y);
    await sleep(200);
    report(await probe(driver, 'msgame_options_index') === 0,
      '1280x720 preview cycles back to 800x600');
    await driver.click(wideOkButton.x, wideOkButton.y);
    if (!await waitFor(async () => {
      const state = await resolutionState(driver);
      return isResolution(state, 800, 600);
    }, 10))
      throw new Error('runtime resolution did not return to 800x600');
    const restored = await resolutionState(driver);
    report(restored.viewport && restored.viewport[2] === 800
      && restored.viewport[3] === 600,
      `GL viewport returned to ${JSON.stringify(restored.viewport)}`);
    report(restored.ui && restored.ui.width === 800 && restored.ui.height === 600,
      'browser controller returned to 800x600');
    report(restored.origin && restored.origin.x === 0 && restored.origin.y === 0,
      'design origin returned to 0,0');
    report(restored.stored === '0',
      `localStorage persisted ${JSON.stringify(storageKey)}=0`);
    await driver.screenshot('21-resolution-800x600-restored');
  } catch (error) {
    report(false, `resolution flow aborted: ${error.message}`);
  } finally {
    if (storageCaptured) {
      try {
        await driver.evaluate(originalStored === null
          ? `localStorage.removeItem(${JSON.stringify(storageKey)})`
          : `localStorage.setItem(${JSON.stringify(storageKey)}, ${JSON.stringify(originalStored)})`);
      } catch (error) {
        // The page may be navigating or the browser may have exited.
      }
    }
    if (driver.errors.length)
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`,
        `${driver.errors.join('\n')}\n`);
    // Runtime.exceptionThrown entries have no prefix; transport failures are
    // logged with one and must not fail the flow.
    const exceptions = driver.errors.filter(
      entry => !entry.startsWith('Network load failed:'));
    if (exceptions.length)
      report(false, `browser runtime exception: ${exceptions[0]}`);
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`,
      `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  RESOLUTION_CARD must be a card index 0..3, got ${process.env.RESOLUTION_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
