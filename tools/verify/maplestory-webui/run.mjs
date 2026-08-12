import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const browserBinary = process.env.CHROME_BIN || (fs.existsSync('/usr/bin/chromium') ? '/usr/bin/chromium' : 'google-chrome');
const debugPort = Number(process.env.E2E_DEBUG_PORT || 9231);
const retryCharacterName = process.env.E2E_RETRY_CHARACTER || '';
const createCharacter = retryCharacterName !== '' || process.env.E2E_CREATE_CHARACTER !== '0';
const useRegistration = process.env.E2E_REGISTER === '1';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || 'test1';
const characterName = process.env.E2E_CHARACTER || '测试一';
const chatMessage = process.env.E2E_CHAT || '中文聊天测试';

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

try {
  const pageUrl = await resolvePageUrl();
  console.log(`WebUI endpoint: ${pageUrl}`);
  await driver.start();
  // Delay the dynamically injected client briefly so the pre-WASM loading
  // state is observable and screenshot validation cannot race the first frame.
  await driver.setNetworkLatency(500);
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
  if (!driver.logs.some(line => line.includes('[loading] assets'))) {
    throw new Error('NX asset loading phase was not reported');
  }
  console.log('PASS  NX asset loading phase reported');
  await requireState('loading screen dismissed after first frame', async () =>
    await driver.evaluate("document.getElementById('loading-screen').classList.contains('is-hidden')"));
  if (await driver.evaluate('document.title') !== '冒险岛online') {
    throw new Error('Unexpected WebUI document title');
  }
  console.log('PASS  Chinese WebUI title');
  await requireUiState('login UI active', uiState.login, 30);
  await driver.screenshot('01-login');

  if (useRegistration) {
    await driver.click(360, 345);
    await sleep(500);
    await driver.screenshot('01b-registration');
    await driver.typeAscii(account);
    await driver.click(340, 287);
    await driver.typeAscii(password);
    await driver.click(340, 324);
    await driver.typeAscii(password);
    await driver.screenshot('01c-registration-filled');
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
  } else {
    await driver.click(340, 260);
    await driver.typeAscii(account);
    await driver.click(340, 285);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
  }
  await requireUiState('world-select UI active', uiState.worldSelect, 45);
  await driver.screenshot('02-world-select');

  await driver.screenshot('03-channel-one-only');
  await driver.doubleClick(255, 260);
  await requireUiState('character-select UI active', uiState.charSelect, 45);
  await driver.screenshot('04-character-select-before');

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
    await driver.click(650, 400);
    await requireUiState('in-game UI active', uiState.game, 60);
    await driver.screenshot('08-in-game');

    await driver.key('Enter', 'Enter', 13);
    await sleep(500);
    await driver.compose(chatMessage);
    await requireState('Chinese chat text entered', async () =>
      await driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(chatMessage)}`));
    await driver.key('Enter', 'Enter', 13);
    await sleep(1500);
    await driver.screenshot('09-chinese-chat');
  }

  const fatalLogs = driver.logs.filter(line => /abort|runtimeerror|exception|\berror\b|failed to|offline/i.test(line));
  if (driver.errors.length || fatalLogs.length) {
    throw new Error(`Browser errors: ${driver.errors.length}; fatal logs: ${fatalLogs.length}`);
  }
  passed = true;
} finally {
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  console.log(`artifacts: ${driver.artifactDir}`);
  console.log(`console lines: ${driver.logs.length}; browser exceptions: ${driver.errors.length}`);
  await driver.stop();
}

if (!passed) process.exit(1);
console.log('PASS  MapleStory WebUI flow completed');
