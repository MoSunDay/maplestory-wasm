import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { runCoopSync } from './coop-sync.mjs';

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
const primaryCard = Number(process.env.PRIMARY_CARD || 0);
const secondaryCard = Number(process.env.SOCIAL_CARD || 0);
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

async function enterCharacter(driver, cardIndex) {
  const x = 105 + 120 * cardIndex + 25;
  for (let attempt = 0; attempt < 4; attempt += 1) {
    await driver.doubleClick(x, 210);
    for (let poll = 0; poll < 10; poll += 1) {
      if (await probe(driver, 'msui_state') === uiState.game) return;
      await sleep(500);
    }
  }
  throw new Error(`game UI timed out for character card ${cardIndex}`);
}

async function boot(credentials, port, cardIndex) {
  const driver = new BrowserDriver({ binary: chrome, debugPort: port });
  await driver.start();
  await driver.navigate(url);
  await waitFor('WASM initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await waitUi(driver, 'login UI', uiState.login);
  await waitAssets(driver, 'login assets');
  await driver.clickField(340, 260);
  if (/^[\x20-\x7e]*$/.test(credentials.account)) {
    await driver.typeAscii(credentials.account);
  } else {
    await driver.compose(credentials.account);
  }
  await driver.clickField(340, 285);
  await driver.typeAscii(credentials.password);
  await driver.key('Enter', 'Enter', 13);
  try {
    await waitUi(driver, 'world UI', uiState.world);
  } catch (error) {
    console.log('login state', {
      notice: await probe(driver, 'msui_login_notice_active'),
      message: await probe(driver, 'msui_login_notice_message'),
      focused: await probe(driver, 'msime_field_focused')
    });
    await driver.screenshot('world-timeout');
    throw error;
  }
  await waitAssets(driver, 'world assets');
  await sleep(2500);
  await driver.retryClick(510, 216, () =>
    probe(driver, 'msui_state').then(value => value === uiState.characters));
  await waitUi(driver, 'character UI', uiState.characters);
  await waitAssets(driver, 'character assets');
  try {
    await waitFor('character list ready', async () =>
      await probe(driver, 'mscharlist_phase') === 8 &&
      await probe(driver, 'mscharlist_character_index') >= 0);
  } catch (error) {
    console.log('character list state', {
      phase: await probe(driver, 'mscharlist_phase'),
      index: await probe(driver, 'mscharlist_character_index'),
      previewReady: await probe(driver, 'mscharlist_preview_ready'),
      previewTotal: await probe(driver, 'mscharlist_preview_total'),
      remainingBytes: await probe(driver, 'mscharlist_remaining_bytes')
    });
    await driver.screenshot('character-list-timeout');
    throw error;
  }
  await enterCharacter(driver, cardIndex);
  await waitFor('game UI', () => probe(driver, 'msui_state').then(
    value => value === uiState.game), 60);
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
  await driver.click(origin.x + 260, origin.y + 180);
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

async function coopFlow() {
  requireCredentials(primary, 'primary');
  requireCredentials(secondary, 'secondary');
  const basePort = Number(process.env.E2E_DEBUG_PORT || 9261);
  const primaryDriver = await boot(primary, basePort, primaryCard);
  const secondaryDriver = await boot(secondary, basePort + 1, secondaryCard);
  try {
    const primaryId = await probe(primaryDriver, 'msplayer_id');
    const secondaryId = await probe(secondaryDriver, 'msplayer_id');
    await waitFor('same map for co-op', async () =>
      await probe(primaryDriver, 'msmap_id') === await probe(secondaryDriver, 'msmap_id'), 15);
    await runCoopSync({
      primaryDriver,
      secondaryDriver,
      primaryId,
      secondaryId,
      waitFor
    });
    if (primaryDriver.errors.length || secondaryDriver.errors.length) {
      throw new Error(`browser exceptions: ${primaryDriver.errors.length + secondaryDriver.errors.length}`);
    }
    console.log('PASS  dual-account co-op UI acceptance completed');
  } finally {
    for (const driver of [primaryDriver, secondaryDriver]) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
      fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
      console.log(`artifacts: ${driver.artifactDir}`);
      await driver.stop();
    }
  }
}

async function actionSyncFlow() {
  requireCredentials(primary, 'primary');
  requireCredentials(secondary, 'secondary');
  const basePort = Number(process.env.E2E_DEBUG_PORT || 9261);
  const primaryDriver = await boot(primary, basePort, primaryCard);
  const secondaryDriver = await boot(secondary, basePort + 1, secondaryCard);
  try {
    const secondaryId = await probe(secondaryDriver, 'msplayer_id');
    await waitFor('remote character visible for actions', () =>
      probe(primaryDriver, 'mschar_visible', [secondaryId], ['number'])
        .then(value => value === 1));
    await verifyRemoteActions(primaryDriver, secondaryDriver, secondaryId);
    if (primaryDriver.errors.length || secondaryDriver.errors.length) {
      throw new Error(`browser exceptions: ${primaryDriver.errors.length + secondaryDriver.errors.length}`);
    }
    console.log('PASS  remote face/attack action flow completed');
  } finally {
    for (const driver of [primaryDriver, secondaryDriver]) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
      fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
      console.log(`artifacts: ${driver.artifactDir}`);
      await driver.stop();
    }
  }
}

// Atomic mob snapshot: one evaluate() with no awaits, so every field of every
// mob comes from the same game loop instant. Per-field CDP probes interleave
// with rendering and produce false position drift between the two clients.
function mobSnapshot(driver) {
  return driver.evaluate(`(() => {
    const count = Module.ccall('msmob_count', 'number', [], []);
    const rows = [];
    for (let index = 0; index < count; index += 1) {
      const row = [];
      for (let field = 0; field < 10; field += 1) {
        row.push(Module.ccall('msmob_state', 'number', ['number', 'number'], [index, field]));
      }
      rows.push(row);
    }
    rows.sort((a, b) => a[0] - b[0]);
    return rows;
  })()`);
}

const MOB_POSITION_TOLERANCE = 32;
const MOB_WORST_TOLERANCE = 48;

async function mobSyncFlow() {
  requireCredentials(primary, 'primary');
  requireCredentials(secondary, 'secondary');
  const basePort = Number(process.env.E2E_DEBUG_PORT || 9261);
  const primaryDriver = await boot(primary, basePort, primaryCard);
  const secondaryDriver = await boot(secondary, basePort + 1, secondaryCard);
  try {
    const primaryId = await probe(primaryDriver, 'msplayer_id');
    const secondaryId = await probe(secondaryDriver, 'msplayer_id');
    await waitFor('secondary visible from primary', () =>
      probe(primaryDriver, 'mschar_visible', [secondaryId], ['number'])
        .then(value => value === 1));
    await waitFor('primary visible from secondary', () =>
      probe(secondaryDriver, 'mschar_visible', [primaryId], ['number'])
        .then(value => value === 1));
    await waitFor('mobs spawned on both clients', async () =>
      (await mobSnapshot(primaryDriver)).length > 0 &&
      (await mobSnapshot(secondaryDriver)).length > 0, 30);
    await primaryDriver.screenshot('mobs-sync-before-primary');
    await secondaryDriver.screenshot('mobs-sync-before-secondary');

    const failures = [];
    let worstDelta = 0;
    async function sample(label) {
      const [rowsA, rowsB] = await Promise.all([
        mobSnapshot(primaryDriver), mobSnapshot(secondaryDriver)]);
      const mapA = new Map(rowsA.map(row => [row[0], row]));
      const mapB = new Map(rowsB.map(row => [row[0], row]));
      const missing = [...mapA.keys()].filter(oid => !mapB.has(oid)).length +
        [...mapB.keys()].filter(oid => !mapA.has(oid)).length;
      let maxDelta = 0;
      let positional = 0;
      let action = 0;
      for (const [oid, rowA] of mapA) {
        const rowB = mapB.get(oid);
        if (!rowB) continue;
        const dx = Math.abs(rowA[1] - rowB[1]);
        const dy = Math.abs(rowA[2] - rowB[2]);
        maxDelta = Math.max(maxDelta, dx, dy);
        if (dx > MOB_POSITION_TOLERANCE || dy > MOB_POSITION_TOLERANCE) positional += 1;
        if (rowA[7] !== rowB[7] || rowA[8] !== rowB[8]) action += 1;
      }
      const controlledA = rowsA.filter(row => row[4] === 1).length;
      const controlledB = rowsB.filter(row => row[4] === 1).length;
      console.log(`MOBS ${label} n=${rowsA.length}/${rowsB.length} ` +
        `maxDelta=${maxDelta} pos>${MOB_POSITION_TOLERANCE}=${positional} ` +
        `action=${action} missing=${missing} ctrl=${controlledA}/${controlledB}`);
      worstDelta = Math.max(worstDelta, maxDelta);
      if (missing > 0) failures.push(`${label}: oid set mismatch (${missing})`);
      if (positional > 0) failures.push(`${label}: ${positional} mobs >${MOB_POSITION_TOLERANCE}px`);
      if (action > 0) failures.push(`${label}: ${action} action mismatches`);
      return { maxDelta, missing, positional, action };
    }

    async function walk(key, code, virtualKeyCode, samples) {
      const dispatch = type => primaryDriver.send('Input.dispatchKeyEvent', {
        type, key, code, windowsVirtualKeyCode: virtualKeyCode, nativeVirtualKeyCode: virtualKeyCode
      });
      await dispatch('keyDown');
      for (let index = 0; index < samples; index += 1) {
        await sample(`${key}-${index}`);
        await sleep(250);
      }
      await dispatch('keyUp');
      await sleep(500);
    }

    for (let index = 0; index < 3; index += 1) {
      await sample(`idle-${index}`);
      await sleep(250);
    }
    await walk('ArrowLeft', 'ArrowLeft', 37, 10);
    await walk('ArrowRight', 'ArrowRight', 39, 10);
    for (let index = 0; index < 3; index += 1) {
      await sample(`settle-${index}`);
      await sleep(250);
    }

    await primaryDriver.screenshot('mobs-sync-after-primary');
    await secondaryDriver.screenshot('mobs-sync-after-secondary');
    console.log(`mob sync worst maxDelta=${worstDelta}px (limit ${MOB_WORST_TOLERANCE}px)`);
    if (failures.length) {
      throw new Error(`mob sync mismatch: ${failures.slice(0, 5).join('; ')}`);
    }
    if (worstDelta > MOB_WORST_TOLERANCE) {
      throw new Error(`mob sync drift unbounded: ${worstDelta}px > ${MOB_WORST_TOLERANCE}px`);
    }
    if (primaryDriver.errors.length || secondaryDriver.errors.length) {
      throw new Error(`browser exceptions: ${primaryDriver.errors.length + secondaryDriver.errors.length}`);
    }
    console.log('PASS  dual-account mob position/action sync');
  } finally {
    for (const driver of [primaryDriver, secondaryDriver]) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
      fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
      console.log(`artifacts: ${driver.artifactDir}`);
      await driver.stop();
    }
  }
}

const ATTACK_STANCES = new Set([
  10, 11, 12, 13,             // shot
  15, 16, 17, 18, 19, 20,     // stab
  23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 // swing
]);

async function verifyRemoteActions(primaryDriver, secondaryDriver, secondaryId) {
  const localPoint = {
    x: await probe(secondaryDriver, 'msplayer_screen_x'),
    y: await probe(secondaryDriver, 'msplayer_screen_y')
  };
  const remotePoint = {
    x: await probe(primaryDriver, 'mschar_screen_x', [secondaryId], ['number']),
    y: await probe(primaryDriver, 'mschar_screen_y', [secondaryId], ['number'])
  };
  console.log(`[ui] local face at ${localPoint.x},${localPoint.y}; remote face at ${remotePoint.x},${remotePoint.y}`);

  await secondaryDriver.screenshot('social-ui-expression-baseline');
  await secondaryDriver.key('F1', 'F1', 112);
  await waitFor('local face expression applied via F1', () =>
    probe(secondaryDriver, 'msplayer_expression').then(value => value === 2), 5);
  await secondaryDriver.screenshot('social-ui-expression-local');
  await waitFor('remote face expression synchronized', () =>
    probe(primaryDriver, 'mschar_expression', [secondaryId], ['number'])
      .then(value => value === 2));
  await primaryDriver.screenshot('social-remote-expression');
  await waitFor('remote face expression reset after timeout', () =>
    probe(primaryDriver, 'mschar_expression', [secondaryId], ['number'])
      .then(value => value === 0), 10);

  // A preceding chat command can leave the IME field focused; then Ctrl
  // never reaches the game keymap. Submit once more to blur it, then retry
  // the attack because CDP key delivery is also lossy under asset load.
  for (let attempt = 0; attempt < 6
    && await probe(secondaryDriver, 'msime_field_focused') === 1; attempt += 1) {
    await secondaryDriver.key('Enter', 'Enter', 13);
    await sleep(250);
  }
  await waitFor('secondary input ready for attack', () =>
    probe(secondaryDriver, 'msime_field_focused').then(value => value === 0), 5);

  await primaryDriver.screenshot('social-remote-attack-baseline');
  let stance = -1;
  for (let attempt = 0; attempt < 5 && !ATTACK_STANCES.has(stance); attempt += 1) {
    await secondaryDriver.keyHold('Control', 'ControlLeft', 17, 120);
    for (let poll = 0; poll < 12 && !ATTACK_STANCES.has(stance); poll += 1) {
      stance = await probe(primaryDriver, 'mschar_stance', [secondaryId], ['number']);
      if (!ATTACK_STANCES.has(stance)) await sleep(100);
    }
  }
  if (!ATTACK_STANCES.has(stance)) {
    throw new Error(`Timed out: remote attack stance not rendered (stance=${stance})`);
  }
  const frame = await probe(primaryDriver, 'mschar_frame', [secondaryId], ['number']);
  if (frame < 0) throw new Error(`remote attack frame probe failed: ${frame}`);
  await primaryDriver.screenshot('social-remote-attack');
  console.log(`PASS  remote attack stance rendered (stance=${stance}, frame=${frame})`);
}

async function fullSocialFlow() {
  requireCredentials(primary, 'primary');
  requireCredentials(secondary, 'secondary');
  const basePort = Number(process.env.E2E_DEBUG_PORT || 9261);
  const primaryDriver = await boot(primary, basePort, primaryCard);
  let secondaryDriver = await boot(secondary, basePort + 1, secondaryCard);
  try {
    const primaryId = await probe(primaryDriver, 'msplayer_id');
    const secondaryId = await probe(secondaryDriver, 'msplayer_id');
    const primaryLevel = await probe(primaryDriver, 'msplayer_level');
    const secondaryLevel = await probe(secondaryDriver, 'msplayer_level');
    console.log(`social character ids primary=${primaryId} secondary=${secondaryId}`);
    if ((primaryLevel < 10) !== (secondaryLevel < 10)) {
      throw new Error('party participants must both be starters or both be level 10 or higher');
    }
    for (const driver of [primaryDriver, secondaryDriver]) {
      await chat(driver, '/party leave');
    }
    await waitFor('party invite state cleaned', async () =>
      await probe(primaryDriver, 'msparty_id') === -1 &&
      await probe(secondaryDriver, 'msparty_id') === -1);

    await waitFor('remote character visible for profile card', () =>
      probe(primaryDriver, 'mschar_visible', [secondaryId], ['number']).then(value => value === 1));

    await verifyRemoteActions(primaryDriver, secondaryDriver, secondaryId);

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
    const userInfoOrigin = {
      x: await probe(primaryDriver, 'msuserinfo_x'),
      y: await probe(primaryDriver, 'msuserinfo_y')
    };
    await primaryDriver.click(userInfoOrigin.x + 177, userInfoOrigin.y + 35);
    await waitFor('party invite sent from character card', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value > 0));
    await primaryDriver.screenshot('social-card-party-invite');
    await chat(secondaryDriver, '/party deny');
    await waitFor('card party invite denied', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value === -1));
    await primaryDriver.key('Escape', 'Escape', 27);
    await waitFor('remote character info card closed', () =>
      probe(primaryDriver, 'msuserinfo_active').then(value => value === 0));
    await chat(primaryDriver, '/party leave');
    await waitFor('card invite party cleaned up', () =>
      probe(primaryDriver, 'msparty_id').then(value => value === -1));

    const contextPoint = {
      x: await probe(primaryDriver, 'mschar_screen_x', [secondaryId], ['number']),
      y: await probe(primaryDriver, 'mschar_screen_y', [secondaryId], ['number']) - 32
    };
    await primaryDriver.rightClick(contextPoint.x, contextPoint.y);
    await waitFor('remote right-click context retained', () =>
      probe(primaryDriver, 'msplayer_context_active').then(value => value === 1));
    await primaryDriver.click(contextPoint.x + 20, contextPoint.y + 60);
    await waitFor('party invite sent from right-click menu', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value > 0));
    await primaryDriver.screenshot('social-context-party-invite');
    await chat(secondaryDriver, '/party deny');
    await waitFor('context party invite denied', () =>
      probe(secondaryDriver, 'msparty_pending_id').then(value => value === -1));
    await chat(primaryDriver, '/party leave');
    await waitFor('context invite party cleaned up', () =>
      probe(primaryDriver, 'msparty_id').then(value => value === -1));

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
    secondaryDriver = await boot(secondary, basePort + 2, secondaryCard);
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

if (mode === 'actions') await actionSyncFlow();
else if (mode === 'grind') await grindToTen();
else if (mode === 'cleanup') await cleanupBuddyState(primary, 'primary');
else if (mode === 'cleanup-secondary') await cleanupBuddyState(secondary, 'secondary');
else if (mode === 'mobs') await mobSyncFlow();
else if (mode === 'coop') await coopFlow();
else await fullSocialFlow();
