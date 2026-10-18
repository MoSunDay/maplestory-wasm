import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chrome = process.env.CHROME_BIN || '/usr/bin/chromium';
const url = process.env.E2E_URL || 'http://127.0.0.1:8001/web/index.html';
const mode = process.env.SOCIAL_MODE || 'full';
const primary = {
  account: process.env.PRIMARY_ACCOUNT,
  password: process.env.PRIMARY_PASSWORD,
  name: process.env.PRIMARY_CHARACTER
};
const secondary = {
  account: process.env.SOCIAL_ACCOUNT,
  password: process.env.SOCIAL_PASSWORD,
  name: process.env.SOCIAL_CHARACTER
};
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });

function requireCredentials(credentials, label) {
  for (const [key, value] of Object.entries(credentials)) {
    if (!value) throw new Error(`${label} ${key} is required`);
  }
}

async function waitFor(label, predicate, seconds = 45) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    if (await predicate()) {
      console.log(`PASS  ${label}`);
      return;
    }
    await sleep(250);
  }
  throw new Error(`Timed out: ${label}`);
}

function probe(driver, name, args = [], argTypes = []) {
  return driver.evaluate(
    `Module.ccall(${JSON.stringify(name)}, 'number', ${JSON.stringify(argTypes)}, ${JSON.stringify(args)})`
  );
}

async function waitUi(driver, label, expected, seconds = 45) {
  await waitFor(label, () => probe(driver, 'msui_state').then(value => value === expected), seconds);
}

async function waitAssets(driver, label) {
  await waitFor(label, async () => {
    const hidden = await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
    return hidden && await probe(driver, 'msasset_blocking') === 0;
  }, 60);
}

async function boot(credentials, port) {
  const driver = new BrowserDriver({ binary: chrome, debugPort: port });
  await driver.start();
  await driver.navigate(url);
  await waitFor('WASM initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await waitUi(driver, 'login UI', uiState.login);
  await waitAssets(driver, 'login assets');
  await driver.click(340, 260);
  await driver.compose(credentials.account);
  await driver.click(340, 285);
  await driver.typeAscii(credentials.password);
  await driver.key('Enter', 'Enter', 13);
  await waitUi(driver, 'world UI', uiState.world);
  await waitAssets(driver, 'world assets');
  await driver.click(510, 216);
  await waitUi(driver, 'character UI', uiState.characters);
  await waitAssets(driver, 'character assets');
  await driver.click(130, 220);
  await driver.click(650, 400);
  await waitUi(driver, 'game UI', uiState.game, 60);
  await waitAssets(driver, 'game assets');
  return driver;
}

async function chat(driver, message) {
  await driver.key('Enter', 'Enter', 13);
  await sleep(100);
  await driver.compose(message);
  // The IME bridge intentionally suppresses Enter for 100 ms after a
  // composition commit so the browser cannot submit the same text twice.
  await sleep(150);
  await driver.key('Enter', 'Enter', 13);
}

async function buddyOrigin(driver) {
  return {
    x: await probe(driver, 'msbuddy_window_x'),
    y: await probe(driver, 'msbuddy_window_y')
  };
}

async function grindToTen() {
  requireCredentials(secondary, 'secondary');
  const driver = await boot(secondary, Number(process.env.E2E_DEBUG_PORT || 9261));
  try {
    let level = await probe(driver, 'msplayer_level');
    console.log(`secondary starts at level ${level}`);
    for (let round = 0; level < 10 && round < 240; round += 1) {
      const movingRight = round % 2 === 0;
      const key = movingRight ? 'ArrowRight' : 'ArrowLeft';
      const code = movingRight ? 'ArrowRight' : 'ArrowLeft';
      const virtual = movingRight ? 39 : 37;
      await driver.send('Input.dispatchKeyEvent', {
        type: 'keyDown', key, code, windowsVirtualKeyCode: virtual, nativeVirtualKeyCode: virtual
      });
      for (let attack = 0; attack < 16; attack += 1) {
        await driver.keyHold('Control', 'ControlLeft', 17, 100);
        await sleep(180);
      }
      await driver.send('Input.dispatchKeyEvent', {
        type: 'keyUp', key, code, windowsVirtualKeyCode: virtual, nativeVirtualKeyCode: virtual
      });
      level = await probe(driver, 'msplayer_level');
      if (round % 5 === 0) console.log(`grind round ${round + 1}, level ${level}`);
    }
    if (level < 10) throw new Error(`secondary only reached level ${level}`);
    await driver.screenshot('social-level-10');
    if (driver.errors.length) throw new Error(`browser exceptions: ${driver.errors.length}`);
    console.log('PASS  secondary reached level 10 through gameplay');
  } finally {
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
}

async function addBuddy(driver, name, openWindow = true) {
  if (openWindow) {
    await driver.key('r', 'KeyR', 82, 'r');
    await waitFor('buddy window opened', () =>
      probe(driver, 'msbuddy_window_active').then(value => value === 1));
  }
  const origin = await buddyOrigin(driver);
  await driver.screenshot('social-buddy-window-open');
  await driver.click(origin.x + 128, origin.y + 175);
  await driver.compose(name);
  await driver.screenshot('social-buddy-field-entry');
  await waitFor('buddy add field focused', () => driver.evaluate(
    `MapleWasmIME.active && document.getElementById('ime-input').value === ${JSON.stringify(name)}`));
  await driver.key('Enter', 'Enter', 13);
}

async function deleteFirstBuddy(driver, openWindow = true) {
  if (openWindow) {
    await driver.key('r', 'KeyR', 82, 'r');
    await waitFor('buddy window opened for deletion', () =>
      probe(driver, 'msbuddy_window_active').then(value => value === 1));
  }
  const origin = await buddyOrigin(driver);
  await driver.click(origin.x + 40, origin.y + 48);
  await sleep(100);
  await driver.screenshot('social-buddy-selected-delete');
  await driver.click(origin.x + 45, origin.y + 222);
}

async function clearExistingBuddies(driver) {
  let count = await probe(driver, 'msbuddy_count');
  if (count === 0) return;
  await driver.key('r', 'KeyR', 82, 'r');
  await waitFor('buddy cleanup window opened', () =>
    probe(driver, 'msbuddy_window_active').then(value => value === 1));
  await sleep(100);
  while (count > 0) {
    await deleteFirstBuddy(driver, false);
    const expected = count - 1;
    await waitFor('existing buddy removed through UI', () =>
      probe(driver, 'msbuddy_count').then(value => value === expected));
    count = expected;
  }
  await driver.key('r', 'KeyR', 82, 'r');
  await waitFor('buddy cleanup window closed', () =>
    probe(driver, 'msbuddy_window_active').then(value => value === 0));
}

async function cleanupBuddyState(credentials, label) {
  requireCredentials(credentials, label);
  const driver = await boot(credentials, Number(process.env.E2E_DEBUG_PORT || 9261));
  try {
    await clearExistingBuddies(driver);
    console.log(`PASS  ${label} buddy state cleaned through UI`);
  } finally {
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
}

async function fullSocialFlow() {
  requireCredentials(primary, 'primary');
  requireCredentials(secondary, 'secondary');
  const basePort = Number(process.env.E2E_DEBUG_PORT || 9261);
  const primaryDriver = await boot(primary, basePort);
  let secondaryDriver = await boot(secondary, basePort + 1);
  try {
    const primaryId = await probe(primaryDriver, 'msplayer_id');
    const secondaryId = await probe(secondaryDriver, 'msplayer_id');
    const primaryLevel = await probe(primaryDriver, 'msplayer_level');
    const secondaryLevel = await probe(secondaryDriver, 'msplayer_level');
    console.log(`social character ids primary=${primaryId} secondary=${secondaryId}`);
    if ((primaryLevel < 10) !== (secondaryLevel < 10)) {
      throw new Error('party participants must both be starters or both be level 10 or higher');
    }

    await waitFor('remote character visible for profile card', () =>
      probe(primaryDriver, 'mschar_visible', [secondaryId], ['number']).then(value => value === 1));
    const remotePoint = {
      x: await probe(primaryDriver, 'mschar_screen_x', [secondaryId], ['number']),
      y: await probe(primaryDriver, 'mschar_screen_y', [secondaryId], ['number']) - 32
    };
    await primaryDriver.doubleClick(remotePoint.x, remotePoint.y);
    await waitFor('remote character info card loaded', async () =>
      await probe(primaryDriver, 'msuserinfo_active') === 1 &&
      await probe(primaryDriver, 'msuserinfo_target_id') === secondaryId &&
      await probe(primaryDriver, 'msuserinfo_loaded') === 1);
    await primaryDriver.screenshot('social-remote-user-info');
    await primaryDriver.key('Escape', 'Escape', 27);
    await waitFor('remote character info card closed', () =>
      probe(primaryDriver, 'msuserinfo_active').then(value => value === 0));
    await primaryDriver.rightClick(remotePoint.x, remotePoint.y);
    await waitFor('remote right-click trade context retained', () =>
      probe(primaryDriver, 'msplayer_context_active').then(value => value === 1));
    await primaryDriver.key('Escape', 'Escape', 27);

    await clearExistingBuddies(primaryDriver);
    await clearExistingBuddies(secondaryDriver);

    await addBuddy(primaryDriver, secondary.name);
    await waitFor('buddy request delivered', () =>
      probe(secondaryDriver, 'msbuddy_pending_id').then(value => value === primaryId));
    await secondaryDriver.click(675, 487);
    await sleep(1000);
    await waitFor('requester buddy state synchronized', () =>
      probe(primaryDriver, 'msbuddy_contains', [secondaryId], ['number']).then(value => value === 1));
    await waitFor('receiver buddy state synchronized', () =>
      probe(secondaryDriver, 'msbuddy_contains', [primaryId], ['number']).then(value => value === 1));
    await primaryDriver.screenshot('social-buddy-accepted');
    await primaryDriver.key('r', 'KeyR', 82, 'r');
    await waitFor('buddy window closed before chat', () =>
      probe(primaryDriver, 'msbuddy_window_active').then(value => value === 0));

    const buddyMessages = await probe(secondaryDriver, 'msbuddy_message_count');
    await chat(primaryDriver, '/buddy buddy-e2e');
    await waitFor('buddy chat received', () =>
      probe(secondaryDriver, 'msbuddy_message_count').then(value => value > buddyMessages));

    await secondaryDriver.stop();
    await waitFor('buddy offline update received', () =>
      probe(primaryDriver, 'msbuddy_online_count').then(value => value === 0));
    secondaryDriver = await boot(secondary, basePort + 2);
    await waitFor('buddy online update received', () =>
      probe(primaryDriver, 'msbuddy_online_count').then(value => value === 1));

    await chat(primaryDriver, '/party create');
    await waitFor('party created with complete snapshot', async () =>
      await probe(primaryDriver, 'msparty_id') > 0 &&
      await probe(primaryDriver, 'msparty_leader_id') === primaryId &&
      await probe(primaryDriver, 'msparty_member_count') === 1);
    await chat(primaryDriver, `/party invite ${secondary.name}`);
    await waitFor('party invite delivered', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value > 0));
    await chat(secondaryDriver, '/party deny');
    await waitFor('party invite denied', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value === -1));

    await chat(primaryDriver, `/party invite ${secondary.name}`);
    await waitFor('party reinvite delivered', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value > 0));
    await chat(secondaryDriver, '/party accept');
    await waitFor('party member snapshots synchronized', async () =>
      await probe(primaryDriver, 'msparty_member_count') === 2 &&
      await probe(secondaryDriver, 'msparty_member_count') === 2 &&
      await probe(primaryDriver, 'msparty_has_member', [secondaryId], ['number']) === 1);
    await primaryDriver.screenshot('social-party-members');

    const partyMessages = await probe(secondaryDriver, 'msparty_message_count');
    await chat(primaryDriver, '/pt party-e2e');
    await waitFor('party chat received', () =>
      probe(secondaryDriver, 'msparty_message_count').then(value => value > partyMessages));
    await waitFor('party HP synchronized', () =>
      probe(primaryDriver, 'msparty_member_hp', [secondaryId], ['number']).then(value => value >= 0));

    await chat(primaryDriver, `/party leader ${secondaryId}`);
    await waitFor('party leadership transferred', async () =>
      await probe(primaryDriver, 'msparty_leader_id') === secondaryId &&
      await probe(secondaryDriver, 'msparty_leader_id') === secondaryId);
    await chat(secondaryDriver, `/party expel ${primaryId}`);
    await waitFor('party member expelled', async () =>
      await probe(primaryDriver, 'msparty_id') === -1 &&
      await probe(secondaryDriver, 'msparty_member_count') === 1);
    await chat(secondaryDriver, '/party leave');
    await waitFor('party disbanded', () =>
      probe(secondaryDriver, 'msparty_id').then(value => value === -1));

    await deleteFirstBuddy(secondaryDriver);
    await waitFor('buddy deletion applied to secondary', () =>
      probe(secondaryDriver, 'msbuddy_count').then(value => value === 0));
    await deleteFirstBuddy(primaryDriver);
    await waitFor('buddy deletion applied to primary', () =>
      probe(primaryDriver, 'msbuddy_count').then(value => value === 0));

    await addBuddy(primaryDriver, secondary.name, false);
    await waitFor('buddy request delivered for rejection', () =>
      probe(secondaryDriver, 'msbuddy_pending_id').then(value => value === primaryId));
    await secondaryDriver.screenshot('social-buddy-request-decline');
    await secondaryDriver.click(745, 487);
    await waitFor('buddy request declined', () =>
      probe(secondaryDriver, 'msbuddy_pending_id').then(value => value === -1));
    await sleep(1000);
    await deleteFirstBuddy(primaryDriver, false);
    await waitFor('requester removed declined buddy', () =>
      probe(primaryDriver, 'msbuddy_count').then(value => value === 0));
    await primaryDriver.screenshot('social-flow-complete');
    if (primaryDriver.errors.length || secondaryDriver.errors.length) {
      throw new Error(`browser exceptions: ${primaryDriver.errors.length + secondaryDriver.errors.length}`);
    }
    console.log('PASS  buddy and party dual-account flow completed');
  } finally {
    for (const driver of [primaryDriver, secondaryDriver]) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
      fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
      console.log(`artifacts: ${driver.artifactDir}`);
      await driver.stop();
    }
  }
}

if (mode === 'grind') await grindToTen();
else if (mode === 'cleanup') await cleanupBuddyState(primary, 'primary');
else if (mode === 'cleanup-secondary') await cleanupBuddyState(secondary, 'secondary');
else await fullSocialFlow();
