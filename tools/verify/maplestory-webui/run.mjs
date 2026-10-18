import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { verifyImeChatFlow } from './ime-chat-flow.mjs';
import { verifyNpcShopFlow } from './npc-shop-flow.mjs';
import { verifyPetAutomationFlow } from './pet-automation-flow.mjs';
import { verifyUserInfoFlow } from './user-info-flow.mjs';
import {
  createAssetIdleWaiter,
  createRuntimeProbes,
  enterTutorialLootMap,
  enterTutorialMobMap,
  resolvePageUrl
} from './runtime-flow.mjs';

const browserBinary = process.env.CHROME_BIN || (fs.existsSync('/usr/bin/chromium') ? '/usr/bin/chromium' : 'google-chrome');
const debugPort = Number(process.env.E2E_DEBUG_PORT || 9231);
const retryCharacterName = process.env.E2E_RETRY_CHARACTER || '';
const stopAfterCharacterCreate = process.env.E2E_STOP_AFTER_CHARACTER_CREATE === '1';
const createCharacter = stopAfterCharacterCreate || retryCharacterName !== '' || process.env.E2E_CREATE_CHARACTER !== '0';
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
const charSelectLatencyMs = Number(process.env.E2E_CHAR_SELECT_LATENCY_MS || 0);
const stopAtCharSelect = process.env.E2E_STOP_AT_CHAR_SELECT === '1';
const stopAtGame = process.env.E2E_STOP_AT_GAME === '1';
const verifyCashShop = process.env.E2E_CASH_SHOP === '1';
const verifyNpcShop = process.env.E2E_NPC_SHOP === '1';
const verifyImeChat = process.env.E2E_IME_CHAT === '1';
const logoutToLogin = process.env.E2E_LOGOUT_TO_LOGIN === '1';
const requireMobs = process.env.E2E_REQUIRE_MOBS === '1';
const routeTutorialMobMap = process.env.E2E_ROUTE_TUTORIAL_MOB_MAP === '1';
const captureAttackEffects = process.env.E2E_CAPTURE_ATTACK_EFFECTS === '1';
const petOptionsMode = process.env.E2E_PET_OPTIONS || '';
const verifyPetOptions = ['1', 'configure', 'runtime', 'pickup', 'follow'].includes(petOptionsMode);
const verifyCharacterInfo = process.env.E2E_CHARACTER_INFO === '1';
const attackSamples = Number(process.env.E2E_ATTACK_SAMPLES || 20);
const attackSampleGapMs = Number(process.env.E2E_ATTACK_SAMPLE_GAP_MS || 1200);

const uiState = Object.freeze({ login: 1, worldSelect: 2, charSelect: 3, charCreation: 4, game: 5, cashShop: 6 });

const driver = new BrowserDriver({
  binary: browserBinary,
  debugPort,
  artifactDir: process.env.E2E_ARTIFACT_DIR || undefined,
  profileDir: process.env.E2E_PROFILE_DIR || undefined
});
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

const probes = createRuntimeProbes(driver);
const {
  currentUiState, loginNoticeActive, loginNoticeMessage,
  characterCreationCustomizing, noticeActive, mapAssetLoading,
  assetBlocking, mapId, mobCount, playerX, playerY, petActive, petX, petY,
  petStance, petHasLootTarget, petOptionsActive, petOptionsHpItem,
  petOptionsMpItem, petOptionsMenuX, petOptionsMenuY, itemsMenuX, itemsMenuY,
  inventorySlot, cashShopProbe
} = probes;

function requireUiState(name, expected, timeoutSeconds = 30) {
  return requireState(name, async () => await currentUiState() === expected, timeoutSeconds);
}

const requireAssetIdle = createAssetIdleWaiter(driver, requireState);

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
    await driver.evaluate(`document.fonts.status === 'loaded' &&
      getComputedStyle(document.body).fontFamily.includes('Noto Sans CJK SC')`));
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
	await requireState('Map.nx registered', async () => {
		try {
			return await driver.evaluate(
				"Boolean(Module.LazyFS?.files?.get('Map.nx')?.size)"
			);
		} catch (error) {
			return false;
		}
	}, 90);
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
    lazy.prefetchFileRange = function(filename, ...args) {
      if (filename !== '__overlay-test__.nx')
        return window.assetTestOriginalPrefetch.call(this, filename, ...args);
      return new Promise((resolve, reject) => {
        window.assetTestAttempts++;
        window.assetTestResolve = resolve;
        window.assetTestReject = reject;
      });
    };
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
    await driver.evaluate("document.getElementById('loading-screen').classList.contains('is-hidden')"), 60);
  if (await driver.evaluate('document.title') !== '冒险岛online') {
    throw new Error('Unexpected WebUI document title');
  }
  console.log('PASS  Chinese WebUI title');
  await requireUiState('login UI active', uiState.login, 30);
  await requireAssetIdle('login assets ready');
  const loginCanvasLayout = await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const rect = canvas.getBoundingClientRect();
    const viewport = window.visualViewport
      ? { width: window.visualViewport.width, height: window.visualViewport.height }
      : { width: document.documentElement.clientWidth, height: document.documentElement.clientHeight };
    return {
      top: rect.top,
      left: rect.left,
      width: rect.width,
      height: rect.height,
      viewportWidth: viewport.width,
      viewportHeight: viewport.height
    };
  })()`);
  if (Math.abs(loginCanvasLayout.top) > 1) {
    throw new Error(`Canvas is not top-aligned: ${JSON.stringify(loginCanvasLayout)}`);
  }
  console.log(`PASS  canvas top aligned ${JSON.stringify(loginCanvasLayout)}`);
  await driver.screenshot('01-login');

  if (useRegistration) {
    await driver.click(360, 345);
    await requireAssetIdle('registration assets ready');
    await driver.screenshot('01b-registration');
    await driver.click(340, 250);
    await driver.compose(account);
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
      await driver.clickField(340, 260);
      await driver.typeAscii(account);
      await driver.clickField(340, 285);
      await driver.typeAscii(password);
      await driver.key('Enter', 'Enter', 13);
    }
  } else {
    await driver.click(340, 260);
    if (!await driver.waitFieldFocused()) throw new Error('username field focus race');
    await driver.typeAscii(account);
    await driver.click(340, 285);
    if (!await driver.waitFieldFocused()) throw new Error('password field focus race');
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
  }
  // The web build also routes canvas clicks through world_select_input.js
  // (DOM-level GO-button bridge), so the client can advance past world
  // select before this 1 Hz poll observes state 2. Accept either state;
  // the later 'go' retryClick self-skips when char select is already up.
  await requireState('world-select UI active', async () => {
    const state = await currentUiState();
    return state === uiState.worldSelect || state === uiState.charSelect;
  }, 45);
  await requireAssetIdle('world-select assets ready');
  await driver.screenshot('02-world-select');

  await driver.screenshot('03-channel-one-only');
  await driver.evaluate(`(() => {
    const lazy = Module.LazyFS;
    const state = () => Module.ccall('msui_state', 'number', [], []);
    const probe = name => Module.ccall(name, 'number', [], []);
    const snapshot = () => ({
      state: state(),
      uiEnabled: probe('msui_enabled'),
      phase: probe('mscharlist_phase'),
      characterIndex: probe('mscharlist_character_index'),
      equipId: probe('mscharlist_equip_id'),
      remainingBytes: probe('mscharlist_remaining_bytes'),
      previewReady: probe('mscharlist_preview_ready'),
      previewTotal: probe('mscharlist_preview_total'),
      at: performance.now()
    });
    const intercepted = [
      'readFileData',
      'prefetchFileRange',
      'requestPriorityFileRange',
      'requestForegroundFileRange'
    ];
    const originals = {};
    const evidence = {
      active: true,
      states: [snapshot()],
      characterRequests: []
    };
    for (const kind of intercepted) {
      const original = lazy[kind];
      if (typeof original !== 'function') continue;
      originals[kind] = original;
      lazy[kind] = function(filepath, offset, length) {
        if (evidence.active && filepath === 'Character.nx') {
          evidence.characterRequests.push({
            kind, filepath, offset, length, ...snapshot()
          });
        }
        return original.apply(this, arguments);
      };
    }
    const sample = () => {
      if (!evidence.active) return;
      evidence.states.push(snapshot());
      requestAnimationFrame(sample);
    };
    evidence.restore = () => {
      evidence.active = false;
      for (const [kind, original] of Object.entries(originals)) {
        lazy[kind] = original;
      }
    };
    window.charSelectTransitionEvidence = evidence;
    requestAnimationFrame(sample);
  })()`);
  if (!Number.isFinite(charSelectLatencyMs) || charSelectLatencyMs < 0) {
    throw new Error(`Invalid E2E_CHAR_SELECT_LATENCY_MS: ${charSelectLatencyMs}`);
  }
  if (charSelectLatencyMs > 0) await driver.setNetworkLatency(charSelectLatencyMs);
  if (worldSelectAction === 'channel') {
    await driver.doubleClick(255, 260);
  } else if (worldSelectAction === 'dom-go') {
    await driver.domClick(510, 216);
  } else if (worldSelectAction === 'dom-channel') {
    await driver.domClick(255, 260);
    await driver.domClick(255, 260);
  } else if (worldSelectAction === 'go') {
    // The normal-state bitmap occupies x=445..576 and y=201..232.
    await driver.retryClick(510, 216, async () => await currentUiState() === uiState.charSelect);
  } else {
    throw new Error(`Unsupported E2E_WORLD_SELECT_ACTION: ${worldSelectAction}`);
  }
  await requireUiState('character-select UI active', uiState.charSelect, 45);
  // Preview streaming is not part of the blocking overlay; wait for the
  // lazy charlook wave explicitly so the transition readback below cannot
  // cut the window while previews are still loading (larger rosters stream
  // more cold Character.nx chunks than the overlay tracks).
  await requireState('character previews settled', async () => {
    const ready = await driver.evaluate("Module.ccall('mscharlist_preview_ready', 'number', [], [])");
    const total = await driver.evaluate("Module.ccall('mscharlist_preview_total', 'number', [], [])");
    return total > 0 && ready === total;
  }, 90);
  await requireAssetIdle('character-select assets ready');
  if (charSelectLatencyMs > 0) await driver.setNetworkLatency(0);
  const charSelectTransition = await driver.evaluate(`(() => {
    const evidence = window.charSelectTransitionEvidence;
    evidence.states.push({
      state: Module.ccall('msui_state', 'number', [], []),
      uiEnabled: Module.ccall('msui_enabled', 'number', [], []),
      phase: Module.ccall('mscharlist_phase', 'number', [], []),
      characterIndex: Module.ccall('mscharlist_character_index', 'number', [], []),
      equipId: Module.ccall('mscharlist_equip_id', 'number', [], []),
      remainingBytes: Module.ccall('mscharlist_remaining_bytes', 'number', [], []),
      previewReady: Module.ccall('mscharlist_preview_ready', 'number', [], []),
      previewTotal: Module.ccall('mscharlist_preview_total', 'number', [], []),
      at: performance.now()
    });
    evidence.restore();
    return {
      states: evidence.states,
      characterRequests: evidence.characterRequests
    };
  })()`);
  fs.writeFileSync(
    `${driver.artifactDir}/char-select-transition.json`,
    `${JSON.stringify(charSelectTransition, null, 2)}\n`
  );
  const transitionStates = charSelectTransition.states.map(sample => sample.state);
  if (transitionStates[0] !== uiState.worldSelect
      || transitionStates[transitionStates.length - 1] !== uiState.charSelect
      || transitionStates.includes(0)) {
    throw new Error(`Character-select transition crossed an invalid UI state: ${JSON.stringify(transitionStates)}`);
  }
  const mountedOrLater = charSelectTransition.states.some(
    sample => sample.phase >= 5 && sample.phase <= 8
  );
  if (!mountedOrLater) {
    throw new Error(`Character-select UI never reached the mounted phase: ${JSON.stringify(charSelectTransition.states)}`);
  }
  const finalPreview = charSelectTransition.states.at(-1);
  if (finalPreview.phase !== 8
      || finalPreview.previewReady !== finalPreview.previewTotal
      || finalPreview.uiEnabled !== 1) {
    throw new Error(`Character previews did not settle: ${JSON.stringify(finalPreview)}`);
  }
  const enabledWhileLoading = charSelectTransition.states.filter(
    sample => sample.phase >= 5 && sample.phase < 8 && sample.uiEnabled !== 0
  );
  if (enabledWhileLoading.length > 0) {
    throw new Error(`Character-select input unlocked before previews settled: ${JSON.stringify(enabledWhileLoading)}`);
  }
  const earlyCharacterRequests = charSelectTransition.characterRequests.filter(
    request => request.state !== uiState.charSelect
  );
  if (earlyCharacterRequests.length > 0) {
    throw new Error(`Character assets were read before CHARSELECT installation: ${JSON.stringify(earlyCharacterRequests)}`);
  }
  console.log(`PASS  character-select transition remained mounted ${JSON.stringify({
    samples: transitionStates.length,
    characterRequests: charSelectTransition.characterRequests.length,
    finalPhase: charSelectTransition.states.at(-1).phase,
    previews: `${charSelectTransition.states.at(-1).previewReady}/${charSelectTransition.states.at(-1).previewTotal}`
  })}`);
  await driver.screenshot('04-character-select-before');
  if (stopAtCharSelect) throw new Error('__CHAR_SELECT_COMPLETE__');

  if (createCharacter) {
    if (!await driver.retryClick(250, 515, async () => await currentUiState() === uiState.charCreation)) {
      throw new Error('character-creation UI did not activate');
    }
    await requireUiState('character-creation UI active', uiState.charCreation);
    // Character creation references a wider cold set of UI and avatar
    // bitmaps than selection; allow the frame-budgeted queue to request them.
    await sleep(10000);
    await requireAssetIdle('character-creation assets ready');
    await driver.click(530, 230);
    const firstName = retryCharacterName ? 'ab' : characterName;
    await driver.compose(firstName);
    await requireState('character name entered', async () =>
      await driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(firstName)}`));
    await driver.screenshot('05-character-name');
    await driver.click(520, 310);

    if (retryCharacterName) {
      await requireState('illegal character-name notice active', async () =>
        await loginNoticeActive() === 1, 5);
      const illegalNameNotice = await loginNoticeMessage();
      if (illegalNameNotice !== 10) {
        throw new Error(`Expected ILLEGAL_NAME notice (10), got ${illegalNameNotice}`);
      }
      // Drawing the newly constructed modal schedules its cold bitmap ranges;
      // wait for that second foreground wave before clicking the small button.
      await sleep(5000);
      await requireAssetIdle('illegal character-name notice assets ready');
      await driver.screenshot('05b-character-name-rejected');
      // The NX origin places the 50x23 visible button at (392, 311); click
      // its center so the assertion does not depend on edge rounding.
      await driver.click(417, 322);
      await sleep(250);
      await driver.screenshot('05c-character-name-notice-dismissed');
      await requireState('illegal character-name notice dismissed', async () =>
        await loginNoticeActive() === 0, 5);
      await driver.click(530, 230);
      await requireState('rejected name remains editable', async () =>
        await driver.evaluate(
          `MapleWasmIME.active && document.getElementById('ime-input').value === ${JSON.stringify(firstName)}`
        ));
      await driver.compose(retryCharacterName);
      await driver.click(520, 310);
    }

    // The name submit schedules a second cold-asset wave (customization
    // previews); CDP clicks keep getting dropped while those chunks stream.
    if (!await driver.retryClick(520, 310, async () =>
      await characterCreationCustomizing() === 1, 8, 4000)) {
      throw new Error('accepted name never entered customization');
    }
    await requireState('accepted name enters customization', async () =>
      await characterCreationCustomizing() === 1, 15);
    await requireAssetIdle('character-customization assets ready');
    await driver.screenshot('06-character-customize');

    if (retryCharacterName) {
      if (stopAfterCharacterCreate) {
        await driver.retryClick(525, 465, async () => await currentUiState() === uiState.charSelect);
        await requireUiState('created character returned to selection', uiState.charSelect, 45);
      } else {
        // Verify availability without creating persistent test data.
        await driver.click(600, 470);
        await requireState('customization returns to name entry', async () =>
          await characterCreationCustomizing() === 0, 5);
        await driver.screenshot('06b-character-name-restored');
        await driver.click(600, 322);
        await sleep(250);
        await driver.screenshot('06c-character-creation-cancelled');
        await requireUiState('character creation cancels back to selection', uiState.charSelect);
      }
      await driver.screenshot('07-character-select-after');
    } else {
      await driver.retryClick(525, 465, async () => await currentUiState() === uiState.charSelect);
      await requireUiState('character creation returned to selection', uiState.charSelect, 45);
    }
  }
  if (!retryCharacterName) await driver.screenshot('07-character-select-after');

  if (!retryCharacterName && !stopAfterCharacterCreate) {
    await driver.setNetworkLatency(500);
    await driver.evaluate(`(() => {
      const lazy = Module.LazyFS;
      const screen = document.getElementById('asset-loading-screen');
      const original = lazy.requestForegroundFileRange;
      let evictedMapChunks = 0;
      for (const key of Array.from(lazy.chunkCache.keys())) {
        if (key.startsWith('Map.nx:')) {
          lazy.chunkCache.delete(key);
          evictedMapChunks += 1;
        }
      }
      const evidence = {
        totalCalls: 0,
        missingCalls: 0,
        sampleCalls: [],
        overlayShown: !screen.classList.contains('is-hidden'),
        evictedMapChunks,
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
    // Double-click the first character card (card 0 center) to enter the game.
    // Works for both pre- and post-Chaos layouts since card positions are
    // derived from the AreaButton grid, and double-click is layout-agnostic.
    // Retry the synthetic double-click: the browser can drop the second
    // press, and re-issuing it on an already-selected card is harmless.
    for (let attempt = 0; attempt < 5; attempt += 1) {
      if ((await driver.evaluate("Module.ccall('msui_state','number',[],[])")) !== uiState.charSelect) break;
      await driver.doubleClick(130, 210);
      for (let i = 0; i < 4; i += 1) {
        if ((await driver.evaluate("Module.ccall('msui_state','number',[],[])")) !== uiState.charSelect) break;
        await sleep(1000);
      }
    }
    const selectionLock = await driver.evaluate(`({
      state: Module.ccall('msui_state', 'number', [], []),
      uiEnabled: Module.ccall('msui_enabled', 'number', [], [])
    })`);
    if (selectionLock.state === uiState.charSelect && selectionLock.uiEnabled !== 0) {
      throw new Error(`Character-select input unlocked while selection was pending: ${JSON.stringify(selectionLock)}`);
    }
    console.log(`PASS  character selection keeps input locked ${JSON.stringify(selectionLock)}`);
    await requireUiState('in-game UI active', uiState.game, 60);
    await requireState('map field initialized', async () => await mapId() > 0, 60);
    // Ariant (npc-shop flow) streams a much larger cold-asset set; give the
    // map gates more headroom there.
    const mapGateSeconds = verifyNpcShop ? 300 : 60;
    await requireState('map asset safety gate released', async () =>
      await mapAssetLoading() === 0, mapGateSeconds);
    await requireState('all blocking assets released', async () =>
      await assetBlocking() === 0, mapGateSeconds);
    await requireAssetIdle('in-game foreground assets ready');
    if (requireMobs) {
      const initialMapId = await mapId();
      const initialMobCount = await mobCount();
      console.log(`INFO  mob runtime entry map=${initialMapId} count=${initialMobCount}`);
      if (initialMobCount === 0 && routeTutorialMobMap && initialMapId === 10000)
        await enterTutorialMobMap(driver, {
          mapId, playerX, playerY, mapAssetLoading
        }, requireState);
      if (petOptionsMode === 'pickup' && await mapId() === 40000)
        await enterTutorialLootMap(driver, {
          mapId, playerX, playerY, mapAssetLoading
        }, requireState);
      await requireState('map monsters instantiated', async () =>
        await mobCount() > 0, 30);
      console.log(`PASS  map monster runtime count ${await mobCount()}`);
    }
    const naturalAssetEvidence = await driver.evaluate(`(() => {
      const evidence = window.naturalAssetEvidence;
      evidence.restore();
      return {
        totalCalls: evidence.totalCalls,
        missingCalls: evidence.missingCalls,
        sampleCalls: evidence.sampleCalls,
        overlayShown: evidence.overlayShown,
        evictedMapChunks: evidence.evictedMapChunks,
        elapsedMs: performance.now() - evidence.startedAt
      };
    })()`);
    await driver.setNetworkLatency(0);
    fs.writeFileSync(
      `${driver.artifactDir}/natural-asset-loading.json`,
      `${JSON.stringify(naturalAssetEvidence, null, 2)}\n`
    );
    if (!naturalAssetEvidence.overlayShown || naturalAssetEvidence.evictedMapChunks === 0
        || naturalAssetEvidence.missingCalls === 0) {
      throw new Error(`Natural asset loading gate failed: ${JSON.stringify(naturalAssetEvidence)}`);
    }
    console.log(`PASS  natural in-game asset loading ${JSON.stringify({
      calls: naturalAssetEvidence.totalCalls,
      missingCalls: naturalAssetEvidence.missingCalls,
      overlayShown: naturalAssetEvidence.overlayShown
    })}`);
    await driver.screenshot('08-in-game');

    if (verifyNpcShop) {
      await verifyNpcShopFlow(driver);
      throw new Error('__NPC_SHOP_COMPLETE__');
    }

    if (verifyCashShop) {
      // StatusBar2 BtCashShop occupies x=569..625, y=554..589 at 800x600.
      // Use the visible button because account keymaps may rebind the shortcut.
      await driver.click(597, 571);
      await requireUiState('cash-shop UI active', uiState.cashShop, 30);
      await requireAssetIdle('cash-shop assets ready');
      const initial = {
        locker: await cashShopProbe('mscashshop_locker_count'),
        inventory: await cashShopProbe('mscashshop_inventory_count'),
        nxCredit: await cashShopProbe('mscashshop_nx_credit')
      };
      if (initial.locker < 0 || initial.inventory < 0 || initial.nxCredit < 1) {
        throw new Error(`Invalid initial cash-shop state: ${JSON.stringify(initial)}`);
      }
      await driver.screenshot('10-cash-shop-modern');

      // Modern CashShop fills the 800x600 canvas; the search field occupies
      // (109,34)-(493,59) in canvas coordinates.
      await driver.click(300, 50);
      await requireState('cash-shop search focused', async () =>
        await driver.evaluate('MapleWasmIME.active === true'));
      await driver.compose('80000002');
      await requireState('cash-shop SN entered', async () =>
        await driver.evaluate("document.getElementById('ime-input').value === '80000002'"));
      await sleep(250);
      await driver.key('Enter', 'Enter', 13);
      await sleep(500);
      // First catalog card sits at (109,86) with size 93x144; the icon is
      // centred at (155,120).
      await driver.click(155, 120);
      await sleep(250);
      await driver.screenshot('11-cash-shop-selected');
      await driver.click(155, 120);
      await requireState('cash purchase confirmation visible', async () =>
        await noticeActive() === 1);
      await sleep(500);
      await driver.screenshot('12-cash-shop-confirm');
      await driver.click(380, 285);
      await requireState('cash item purchase persisted in UI model', async () =>
        await cashShopProbe('mscashshop_pending') === 0 &&
        await cashShopProbe('mscashshop_nx_credit') === initial.nxCredit - 1 &&
        await cashShopProbe('mscashshop_locker_count') === initial.locker + 1, 30);
      await driver.screenshot('13-cash-shop-purchased');

      const lockerRow = await cashShopProbe('mscashshop_selected_right_row');
      if (lockerRow < 0) {
        throw new Error('Purchased locker item was not selected');
      }
      // Locker slots form a 7x3 grid from (504,102) with 39px steps.
      await driver.click(504 + (lockerRow % 7) * 39 + 18, 102 + Math.floor(lockerRow / 7) * 39 + 18);
      await requireState('cash item taken into character inventory', async () =>
        await cashShopProbe('mscashshop_pending') === 0 &&
        await cashShopProbe('mscashshop_locker_count') === initial.locker &&
        await cashShopProbe('mscashshop_inventory_count') === initial.inventory + 1, 30);
      await driver.screenshot('14-cash-shop-taken');

      const inventoryRow = await cashShopProbe('mscashshop_selected_right_row');
      if (inventoryRow < 0) {
        throw new Error('Taken cash item was not selected');
      }
      await driver.click(504 + (inventoryRow % 7) * 39 + 18, 102 + Math.floor(inventoryRow / 7) * 39 + 18);
      await requireState('cash item returned to locker', async () =>
        await cashShopProbe('mscashshop_pending') === 0 &&
        await cashShopProbe('mscashshop_locker_count') === initial.locker + 1 &&
        await cashShopProbe('mscashshop_inventory_count') === initial.inventory, 30);
      await driver.screenshot('15-cash-shop-returned');
      await driver.key('Escape', 'Escape', 27);
      await requireUiState('cash-shop exits back to game', uiState.game, 30);
      await requireAssetIdle('returned map assets ready');
      await driver.screenshot('16-cash-shop-exited');
      throw new Error('__CASH_SHOP_COMPLETE__');
    }

    if (logoutToLogin) {
      await driver.key('Escape', 'Escape', 27);
      await sleep(500);
      await driver.screenshot('08e-system-logout-selected');
      await driver.key('Enter', 'Enter', 13);
      await requireUiState('logout returns to login UI', uiState.login, 30);
      throw new Error('__LOGOUT_COMPLETE__');
    }

    if (captureAttackEffects) {
      await driver.click(400, 300);
      await sleep(1000);
      // Leave the spawn ledge and approach the map's live mobs so the
      // screencast proves movement and hit feedback, not only the attack pose.
      await driver.keyHold('ArrowRight', 'ArrowRight', 39, 3500);
      await sleep(1000);
      await driver.screenshot('08a-attack-position');
      await driver.evaluate(`(() => {
        const screen = document.getElementById('asset-loading-screen');
        const lazy = Module.LazyFS;
        const originalForeground = lazy.requestForegroundFileRange;
        const evidence = {
          active: true,
          blockingSeen: false,
          overlaySeen: false,
          gpuSeen: false,
          mapSeen: false,
          foregroundKeys: [],
          rangeCalls: [],
          currentSample: 0,
          attacks: []
        };
        lazy.requestForegroundFileRange = function (filepath, offset, length) {
          if (evidence.active && evidence.rangeCalls.length < 100) {
            evidence.rangeCalls.push({
              filepath, offset, length,
              residentBeforeRequest: this.isFileRangeResident(filepath, offset, length)
            });
          }
          return originalForeground.call(this, filepath, offset, length);
        };
        evidence.restore = () => { lazy.requestForegroundFileRange = originalForeground; };
        const sample = () => {
          if (!evidence.active) return;
          evidence.overlaySeen ||= !screen.classList.contains('is-hidden');
          evidence.blockingSeen ||= Module.ccall('msasset_blocking', 'number', [], []) !== 0;
          evidence.gpuSeen ||= window.MapleAssetLoading.has('gpu');
          evidence.mapSeen ||= window.MapleAssetLoading.has('map');
          if (evidence.foregroundKeys.length === 0 && lazy.foregroundRequests.size > 0) {
            evidence.foregroundKeys = Array.from(lazy.foregroundRequests.keys());
          }
          const stance = Module.ccall('msattack_stance', 'number', [], []);
          if ([15, 16, 23, 24, 25].includes(stance)) {
            const attack = evidence.attacks.find(entry => entry.sample === evidence.currentSample);
            if (attack) {
              if (!attack.stances.includes(stance)) attack.stances.push(stance);
              const frame = Module.ccall('msattack_frame', 'number', [], []);
              if (!attack.frames.includes(frame)) attack.frames.push(frame);
              attack.presented ||= Module.ccall(
                'msattack_afterimage_presented', 'number', [], []) !== 0;
            }
          }
          requestAnimationFrame(sample);
        };
        window.attackVisualEvidence = evidence;
        requestAnimationFrame(sample);
      })()`);
      const screencasts = [];
      for (let sample = 0; sample < attackSamples; sample += 1) {
        await driver.evaluate(`(() => {
          window.attackVisualEvidence.currentSample = ${sample + 1};
          window.attackVisualEvidence.attacks.push({
            sample: ${sample + 1}, stances: [], frames: [], presented: false
          });
        })()`);
        const prefix = `08d-attack-${String(sample + 1).padStart(2, '0')}`;
        await driver.startScreencast(prefix);
        // Hold the key across multiple game ticks so input observation does
        // not depend on two back-to-back CDP events landing in one tick.
        await driver.keyHold('Control', 'ControlLeft', 17, 50);
        await sleep(900);
        const capture = await driver.stopScreencast();
        if (capture.frameCount < 3) {
          throw new Error(`Attack screencast ${sample + 1} captured only ${capture.frameCount} frames`);
        }
        screencasts.push({ sample: sample + 1, frames: capture.frameCount });
        await requireAssetIdle(`attack sample ${sample + 1} assets ready`);
        await sleep(attackSampleGapMs);
      }
      const attackVisualEvidence = await driver.evaluate(`(() => {
        window.attackVisualEvidence.active = false;
        window.attackVisualEvidence.restore();
        return {
          active: false,
          blockingSeen: window.attackVisualEvidence.blockingSeen,
          overlaySeen: window.attackVisualEvidence.overlaySeen,
          gpuSeen: window.attackVisualEvidence.gpuSeen,
          mapSeen: window.attackVisualEvidence.mapSeen,
          foregroundKeys: window.attackVisualEvidence.foregroundKeys,
          rangeCalls: window.attackVisualEvidence.rangeCalls,
          attacks: window.attackVisualEvidence.attacks
        };
      })()`);
      fs.writeFileSync(
        `${driver.artifactDir}/attack-visual-evidence.json`,
        `${JSON.stringify({ ...attackVisualEvidence, screencasts }, null, 2)}\n`
      );
      if (attackVisualEvidence.blockingSeen || attackVisualEvidence.overlaySeen) {
        throw new Error(`Attack unexpectedly blocked asset loading: ${JSON.stringify(attackVisualEvidence)}`);
      }
      const observedAttacks = attackVisualEvidence.attacks.filter(attack => attack.stances.length > 0);
      const missingPresentation = observedAttacks.filter(attack => !attack.presented);
      const stabSeen = observedAttacks.some(attack => attack.stances.some(stance => [15, 16].includes(stance)));
      const swingSeen = observedAttacks.some(attack => attack.stances.some(stance => [23, 24, 25].includes(stance)));
      if (missingPresentation.length > 0 || !stabSeen || !swingSeen) {
        throw new Error(`Attack afterimage presentation failed: ${JSON.stringify({
          missingPresentation, stabSeen, swingSeen, observedAttacks
        })}`);
      }
      console.log(`PASS  ${observedAttacks.length} attacks presented afterimages (stab and swing)`);
    }

    if (verifyCharacterInfo) {
      await verifyUserInfoFlow(driver, probes, requireState);
      if (!verifyPetOptions) throw new Error('__CHARACTER_INFO_COMPLETE__');
    }

    if (verifyPetOptions) {
      await verifyPetAutomationFlow(
        driver, probes, requireState, petOptionsMode === '1' ? 'options' : petOptionsMode);
      throw new Error('__PET_OPTIONS_COMPLETE__');
    }

    if (stopAtGame) throw new Error('__GAME_COMPLETE__');

    if (verifyImeChat) {
      await verifyImeChatFlow(driver, chatMessage);
      throw new Error('__IME_CHAT_COMPLETE__');
    }

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

    await verifyImeChatFlow(driver, chatMessage);
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
  } else if (verifyCashShop && error.message === '__CASH_SHOP_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else if (verifyNpcShop && error.message === '__NPC_SHOP_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else if (verifyImeChat && error.message === '__IME_CHAT_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else if (verifyPetOptions && error.message === '__PET_OPTIONS_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else if (verifyCharacterInfo && error.message === '__CHARACTER_INFO_COMPLETE__') {
    assertRuntimeHealthy();
    passed = true;
  } else if (logoutToLogin && error.message === '__LOGOUT_COMPLETE__') {
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
