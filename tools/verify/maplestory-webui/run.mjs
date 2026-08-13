import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const browserBinary = process.env.CHROME_BIN || (fs.existsSync('/usr/bin/chromium') ? '/usr/bin/chromium' : 'google-chrome');
const debugPort = Number(process.env.E2E_DEBUG_PORT || 9231);
const retryCharacterName = process.env.E2E_RETRY_CHARACTER || '';
const createCharacter = retryCharacterName !== '' || process.env.E2E_CREATE_CHARACTER !== '0';
const useRegistration = process.env.E2E_REGISTER === '1';
const submitRegistration = process.env.E2E_REGISTER_SUBMIT === '1';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || 'test1';
const characterName = process.env.E2E_CHARACTER || '测试一';
const chatMessage = process.env.E2E_CHAT || '中文聊天测试';
// A 30 Hz frame is 33.333... ms; 34 ms permits one scheduled frame without
// admitting the next (50 ms) cadence tier.
const maxP95FrameMs = Number(process.env.E2E_MAX_P95_FRAME_MS || 34);
const maxLongTaskMs = Number(process.env.E2E_MAX_LONG_TASK_MS || 100);
const worldSelectAction = process.env.E2E_WORLD_SELECT_ACTION || 'go';
const stopAtCharSelect = process.env.E2E_STOP_AT_CHAR_SELECT === '1';
const stopAtGame = process.env.E2E_STOP_AT_GAME === '1';

const uiState = Object.freeze({ login: 1, worldSelect: 2, charSelect: 3, charCreation: 4, game: 5 });

const driver = new BrowserDriver({ binary: browserBinary, debugPort });
let passed = false;

async function requireState(name, predicate, timeoutSeconds = 30) {
  for (let second = 0; second < timeoutSeconds; second += 1) {
    if (await predicate()) {
      console.log(`PASS  ${name}`);
      return;
    }
    await sleep(1000);
  }
  throw new Error(`Timed out: ${name}`);
}

async function isReachable(url) {
  try {
    const response = await fetch(url, { signal: AbortSignal.timeout(3000) });
    return response.ok;
  } catch (error) {
    return false;
  }
}

async function resolvePageUrl() {
  if (process.env.E2E_URL) {
    if (!await isReachable(process.env.E2E_URL)) {
      throw new Error(`E2E_URL is not reachable: ${process.env.E2E_URL}`);
    }
    return process.env.E2E_URL;
  }

  const candidates = [
    'http://127.0.0.1:8000/web/index.html',
    'http://127.0.0.1:8001/web/index.html'
  ];
  for (const candidate of candidates) {
    if (await isReachable(candidate)) return candidate;
  }
  throw new Error(`No loopback WebUI endpoint; checked: ${candidates.join(', ')}. Set E2E_URL for a custom bind.`);
}

function currentUiState() {
  return driver.evaluate("Module.ccall('msui_state', 'number', [], [])");
}

function requireUiState(name, expected, timeoutSeconds = 30) {
  return requireState(name, async () => await currentUiState() === expected, timeoutSeconds);
}

async function requireAssetIdle(name = 'foreground assets ready') {
  await sleep(250);
  return requireState(name, async () => await driver.evaluate(
    "document.getElementById('asset-loading-screen').classList.contains('is-hidden')"), 45);
}

function fatalRuntimeLogs() {
  return driver.logs.filter(line =>
    /abort|runtimeerror|exception|\berror\b|failed to|offline/i.test(line) &&
    !line.includes('simulated asset failure'));
}

function assertRuntimeHealthy() {
  const fatalLogs = fatalRuntimeLogs();
  if (driver.errors.length || fatalLogs.length) {
    throw new Error(`Browser errors: ${driver.errors.length}; fatal logs: ${fatalLogs.length}`);
  }
}

try {
  const pageUrl = await resolvePageUrl();
  console.log(`WebUI endpoint: ${pageUrl}`);
  await driver.start();
  const webgl2 = await driver.evaluate(`(() => {
    const canvas = document.createElement('canvas');
    const context = canvas.getContext('webgl2');
    return context && {
      version: context.getParameter(context.VERSION),
      renderer: context.getParameter(context.RENDERER)
    };
  })()`);
  if (!webgl2) {
    throw new Error(`Browser does not provide WebGL2: ${browserBinary}. Set CHROME_BIN to a compatible Chromium.`);
  }
  console.log(`PASS  WebGL2 available ${JSON.stringify(webgl2)}`);
  // Delay the dynamically injected client briefly so the pre-WASM loading
  // state is observable and screenshot validation cannot race the first frame.
  await driver.setNetworkLatency(process.env.E2E_ASSET_ONLY === '1' ? 0 : 500);
  await driver.navigate(pageUrl);
  await requireState('loading font ready', async () =>
    await driver.evaluate("document.fonts.check('16px Maple CJK')"));
  const loadingScreen = await driver.evaluate(`(() => {
    const screen = document.getElementById('loading-screen');
    return screen && {
      phase: screen.dataset.phase,
      visible: !screen.classList.contains('is-hidden'),
      title: document.getElementById('loading-title').textContent
    };
  })()`);
  if (!loadingScreen || !loadingScreen.visible || !loadingScreen.title) {
    throw new Error('Loading screen was not visible while the client initialized');
  }
  console.log('PASS  loading screen visible');
  await driver.screenshot('00-loading');
  await driver.setNetworkLatency(0);
  await requireState('WASM module initialized', async () => {
    try {
      return await driver.evaluate(
        "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"
      );
    } catch (error) {
      return false;
    }
  }, 90);
	await requireState('NX asset loading phase reported', async () =>
		driver.logs.some(line => line.includes('[loading] assets')), 45);
  await driver.evaluate(`(() => {
    const startup = document.getElementById('loading-screen');
    window.assetTestStartupDisplay = startup.style.display;
    startup.style.display = 'none';
  })()`);
  await driver.setNetworkLatency(500);
  const realAssetRequestStarted = await driver.evaluate(`(() => {
    const lazy = Module.LazyFS;
    const file = lazy.files.get('Map.nx');
    if (!file || !file.size) return false;
    const screen = document.getElementById('asset-loading-screen');
    window.realAssetOverlayShown = !screen.classList.contains('is-hidden');
    window.realAssetOverlayObserver = new MutationObserver(() => {
      if (!screen.classList.contains('is-hidden')) window.realAssetOverlayShown = true;
    });
    window.realAssetOverlayObserver.observe(screen, { attributes: true, attributeFilter: ['class'] });
    const offset = file.size - 1;
    const chunk = Math.floor(offset / lazy.CHUNK_SIZE);
    lazy.chunkCache.delete(lazy.getChunkCacheKey('Map.nx', lazy.getFileVersionTag(file), chunk));
    window.realAssetRequest = lazy.requestForegroundFileRange('Map.nx', offset, 1);
    return true;
  })()`);
  if (!realAssetRequestStarted) {
    throw new Error('Map.nx was unavailable for real foreground asset verification');
  }
  await requireState('real NX miss shows asset overlay', async () =>
    await driver.evaluate('window.realAssetOverlayShown === true'));
  await driver.screenshot('00b-asset-loading-real');
  await driver.evaluate('window.realAssetRequest');
  await requireState('real NX request closes asset overlay', async () =>
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')"));
  await driver.evaluate('window.realAssetOverlayObserver.disconnect()');
  await driver.setNetworkLatency(0);
  await driver.evaluate(`(() => {
    const lazy = Module.LazyFS;
    window.assetTestOriginalPrefetch = lazy.prefetchFileRange;
    window.assetTestAttempts = 0;
    lazy.prefetchFileRange = () => new Promise((resolve, reject) => {
      window.assetTestAttempts++;
      window.assetTestResolve = resolve;
      window.assetTestReject = reject;
    });
    lazy.requestForegroundFileRange('__overlay-test__.nx', 0, 1);
  })()`);
  const assetOverlayLoading = await driver.evaluate(`(() => {
    const screen = document.getElementById('asset-loading-screen');
    return {
      visible: !screen.classList.contains('is-hidden'),
      inert: document.getElementById('container').inert,
      message: document.getElementById('asset-loading-message').textContent,
      attempts: window.assetTestAttempts
    };
  })()`);
  if (!assetOverlayLoading.visible || !assetOverlayLoading.inert ||
      assetOverlayLoading.message !== '素材加载中...' || assetOverlayLoading.attempts !== 1) {
    throw new Error('Foreground LazyFS request did not show the asset loading overlay');
  }
  await driver.screenshot('00c-asset-loading');
  await driver.evaluate("window.assetTestReject(new Error('simulated asset failure'))");
  await requireState('asset failure offers retry', async () =>
    await driver.evaluate("document.getElementById('asset-loading-message').textContent.includes('失败')"));
  await driver.screenshot('00d-asset-loading-failed');
  await driver.evaluate("document.getElementById('asset-loading-retry').click()");
  await requireState('asset retry starts a second request', async () =>
    await driver.evaluate('window.assetTestAttempts === 2'));
  await driver.evaluate('window.assetTestResolve()');
  await requireState('asset overlay closes after retry succeeds', async () =>
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')"));
  const assetOverlayClosed = await driver.evaluate(`(() => {
    Module.LazyFS.prefetchFileRange = window.assetTestOriginalPrefetch;
    document.getElementById('loading-screen').style.display = window.assetTestStartupDisplay;
    return {
      hidden: document.getElementById('asset-loading-screen').classList.contains('is-hidden'),
      inert: document.getElementById('container').inert,
      attempts: window.assetTestAttempts
    };
  })()`);
  if (!assetOverlayClosed.hidden || assetOverlayClosed.inert || assetOverlayClosed.attempts !== 2) {
    throw new Error('Asset loading overlay did not restore interaction');
  }
  console.log('PASS  foreground asset loading failure and retry');
  if (process.env.E2E_ASSET_ONLY === '1') {
    await requireState('client window initialized', async () => {
      assertRuntimeHealthy();
      return await driver.evaluate(
        "document.getElementById('loading-screen').classList.contains('is-hidden')");
    }, 90);
    assertRuntimeHealthy();
    throw new Error('__ASSET_ONLY_COMPLETE__');
  }
  await requireState('loading screen dismissed after first frame', async () =>
    await driver.evaluate("document.getElementById('loading-screen').classList.contains('is-hidden')"));
  if (await driver.evaluate('document.title') !== '冒险岛online') {
    throw new Error('Unexpected WebUI document title');
  }
  console.log('PASS  Chinese WebUI title');
  await requireUiState('login UI active', uiState.login, 30);
  await requireAssetIdle('login assets ready');
  await driver.screenshot('01-login');

  if (useRegistration) {
    await driver.click(360, 345);
    await requireAssetIdle('registration assets ready');
    await driver.screenshot('01b-registration');
    await driver.typeAscii(account);
    await driver.click(340, 287);
    await driver.typeAscii(password);
    await driver.click(340, 324);
    await driver.typeAscii(password);
    await driver.screenshot('01c-registration-filled');
    if (submitRegistration) {
      await driver.key('Enter', 'Enter', 13);
    } else {
      await driver.key('Escape', 'Escape', 27);
      await requireUiState('registration closes back to login', uiState.login);
      // Reload before the login flow so the registration-form IME state cannot
      // leak into the independent credential verification.
      await driver.navigate(pageUrl);
      await requireState('WASM module reinitialized after registration check', async () => {
        try {
          return await driver.evaluate(
            "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"
          );
        } catch (error) {
          return false;
        }
      }, 90);
      await requireUiState('login UI active after registration check', uiState.login, 30);
      await driver.click(340, 260);
      await driver.typeAscii(account);
      await driver.click(340, 285);
      await driver.typeAscii(password);
      await driver.key('Enter', 'Enter', 13);
    }
  } else {
    await driver.click(340, 260);
    await driver.typeAscii(account);
    await driver.click(340, 285);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
  }
  await requireUiState('world-select UI active', uiState.worldSelect, 45);
  await requireAssetIdle('world-select assets ready');
  await driver.screenshot('02-world-select');

  await driver.screenshot('03-channel-one-only');
  if (worldSelectAction === 'channel') {
    await driver.doubleClick(255, 260);
  } else if (worldSelectAction === 'dom-go') {
    await driver.domClick(510, 216);
  } else if (worldSelectAction === 'dom-channel') {
    await driver.domClick(255, 260);
    await driver.domClick(255, 260);
  } else if (worldSelectAction === 'go') {
    // The normal-state bitmap occupies x=445..576 and y=201..232.
    await driver.click(510, 216);
  } else {
    throw new Error(`Unsupported E2E_WORLD_SELECT_ACTION: ${worldSelectAction}`);
  }
  await requireUiState('character-select UI active', uiState.charSelect, 45);
  await requireAssetIdle('character-select assets ready');
  await driver.screenshot('04-character-select-before');
  if (stopAtCharSelect) throw new Error('__CHAR_SELECT_COMPLETE__');

  if (createCharacter) {
    await driver.click(250, 515);
    await requireUiState('character-creation UI active', uiState.charCreation);
    await driver.click(530, 230);
    const firstName = retryCharacterName ? 'ab' : characterName;
    await driver.compose(firstName);
    await requireState('character name entered', async () =>
      await driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(firstName)}`));
    await driver.screenshot('05-character-name');
    await driver.click(520, 310);

    if (retryCharacterName) {
      await requireState('rejected name remains editable', async () =>
        await driver.evaluate(
          `MapleWasmIME.active && document.getElementById('ime-input').value === ${JSON.stringify(firstName)}`
        ));
      await driver.screenshot('05b-character-name-rejected');
      await driver.click(392, 300);
      await sleep(250);
      await driver.compose(retryCharacterName);
      await driver.click(520, 310);
    }

    if (retryCharacterName) {
      // A pending request also blurs the field. Wait past the recovery timeout
      // so an inactive IME proves that the server accepted the second name.
      await sleep(8500);
    }
    await requireState('accepted name enters customization', async () =>
      await driver.evaluate('MapleWasmIME.active === false'));
    await driver.screenshot('06-character-customize');

    if (retryCharacterName) {
      // Verify availability without creating persistent test data.
      await driver.click(565, 465);
      await driver.click(560, 310);
      await requireUiState('character creation cancels back to selection', uiState.charSelect);
      await driver.screenshot('07-character-select-after');
    } else {
      await driver.click(525, 465);
      await requireUiState('character creation returned to selection', uiState.charSelect, 45);
    }
  }
  if (!retryCharacterName) await driver.screenshot('07-character-select-after');

  if (!retryCharacterName) {
    await driver.click(130, 220);
    await driver.setNetworkLatency(500);
    await driver.evaluate(`(() => {
      const lazy = Module.LazyFS;
      const screen = document.getElementById('asset-loading-screen');
      const original = lazy.requestForegroundFileRange;
      const evidence = {
        totalCalls: 0,
        missingCalls: 0,
        sampleCalls: [],
        overlayShown: !screen.classList.contains('is-hidden'),
        startedAt: performance.now()
      };
      lazy.requestForegroundFileRange = function (filepath, offset, length) {
        const residentBeforeRequest = this.isFileRangeResident(filepath, offset, length);
        evidence.totalCalls += 1;
        if (!residentBeforeRequest) evidence.missingCalls += 1;
        if (evidence.sampleCalls.length < 100) {
          evidence.sampleCalls.push({ filepath, offset, length, residentBeforeRequest, at: performance.now() });
        }
        return original.call(this, filepath, offset, length);
      };
      evidence.observer = new MutationObserver(() => {
        if (!screen.classList.contains('is-hidden')) evidence.overlayShown = true;
      });
      evidence.observer.observe(screen, { attributes: true, attributeFilter: ['class'] });
      evidence.restore = () => {
        evidence.observer.disconnect();
        lazy.requestForegroundFileRange = original;
      };
      window.naturalAssetEvidence = evidence;
    })()`);
    await driver.click(650, 400);
    await requireUiState('in-game UI active', uiState.game, 60);
    await requireAssetIdle('in-game foreground assets ready');
    const naturalAssetEvidence = await driver.evaluate(`(() => {
      const evidence = window.naturalAssetEvidence;
      evidence.restore();
      return {
        totalCalls: evidence.totalCalls,
        missingCalls: evidence.missingCalls,
        sampleCalls: evidence.sampleCalls,
        overlayShown: evidence.overlayShown,
        elapsedMs: performance.now() - evidence.startedAt
      };
    })()`);
    await driver.setNetworkLatency(0);
    fs.writeFileSync(
      `${driver.artifactDir}/natural-asset-loading.json`,
      `${JSON.stringify(naturalAssetEvidence, null, 2)}\n`
    );
    if (!naturalAssetEvidence.overlayShown || naturalAssetEvidence.missingCalls === 0) {
      throw new Error(`Natural asset loading gate failed: ${JSON.stringify(naturalAssetEvidence)}`);
    }
    console.log(`PASS  natural in-game asset loading ${JSON.stringify({
      calls: naturalAssetEvidence.totalCalls,
      missingCalls: naturalAssetEvidence.missingCalls,
      overlayShown: naturalAssetEvidence.overlayShown
    })}`);
    await driver.screenshot('08-in-game');
    if (stopAtGame) throw new Error('__GAME_COMPLETE__');

    await driver.evaluate(`(() => {
      const startedAt = performance.now();
      const sample = { frames: [], longTasks: [], previous: startedAt, startedAt, active: true };
      const tick = now => {
        if (!sample.active) return;
        sample.frames.push(now - sample.previous);
        sample.previous = now;
        requestAnimationFrame(tick);
      };
      if (typeof PerformanceObserver === 'function') {
        try {
          sample.observer = new PerformanceObserver(list => {
            for (const entry of list.getEntries()) {
              if (entry.startTime >= sample.startedAt) sample.longTasks.push(entry.duration);
            }
          });
          sample.observer.observe({ type: 'longtask' });
        } catch (_) {}
      }
      window.assetPerformanceSample = sample;
      requestAnimationFrame(tick);
    })()`);
    await driver.click(400, 300);
    await driver.key('i', 'KeyI', 73, 'i');
    await sleep(1500);
    await driver.screenshot('08b-item-inventory-cold');
    await driver.key('i', 'KeyI', 73, 'i');
    await driver.key('k', 'KeyK', 75, 'k');
    await sleep(1500);
    await driver.screenshot('08c-skill-book-cold');
    await sleep(1000);
    const performanceSample = await driver.evaluate(`(() => {
      const sample = window.assetPerformanceSample;
      sample.active = false;
      sample.observer?.disconnect();
      const frames = sample.frames.slice(2).sort((a, b) => a - b);
      const percentile = frames.length
        ? frames[Math.min(frames.length - 1, Math.ceil(frames.length * 0.95) - 1)]
        : Infinity;
      return {
        frameCount: frames.length,
        p95FrameMs: percentile,
        maxFrameMs: frames.length ? frames[frames.length - 1] : Infinity,
        maxLongTaskMs: sample.longTasks.length ? Math.max(...sample.longTasks) : 0,
        longTaskCount: sample.longTasks.length
      };
    })()`);
    fs.writeFileSync(
      `${driver.artifactDir}/asset-performance.json`,
      `${JSON.stringify(performanceSample, null, 2)}\n`
    );
    if (performanceSample.frameCount < 60 || performanceSample.p95FrameMs > maxP95FrameMs ||
        performanceSample.maxLongTaskMs > maxLongTaskMs) {
      throw new Error(`Asset performance gate failed: ${JSON.stringify(performanceSample)}`);
    }
    console.log(`PASS  asset performance ${JSON.stringify(performanceSample)}`);
    await driver.key('k', 'KeyK', 75, 'k');

    await driver.key('Enter', 'Enter', 13);
    await sleep(500);
    await driver.compose(chatMessage);
    await requireState('Chinese chat text entered', async () =>
      await driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(chatMessage)}`));
    await driver.key('Enter', 'Enter', 13);
    await sleep(1500);
    await driver.screenshot('09-chinese-chat');
  }

  assertRuntimeHealthy();
  passed = true;
} catch (error) {
  if (process.env.E2E_ASSET_ONLY === '1' && error.message === '__ASSET_ONLY_COMPLETE__') {
    passed = true;
  } else if (stopAtCharSelect && error.message === '__CHAR_SELECT_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else if (stopAtGame && error.message === '__GAME_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else {
    throw error;
  }
} finally {
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  console.log(`artifacts: ${driver.artifactDir}`);
  console.log(`console lines: ${driver.logs.length}; browser exceptions: ${driver.errors.length}`);
  await driver.stop();
}

if (!passed) process.exit(1);
console.log('PASS  MapleStory WebUI flow completed');
