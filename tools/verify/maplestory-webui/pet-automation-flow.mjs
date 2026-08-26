import fs from 'fs';
import { sleep } from './cdp.mjs';

const DEFAULT_HP_ITEM = 2000000;
const DEFAULT_MP_ITEM = 2000003;
const CONFIGURED_THRESHOLD = 98;
const MINIMUM_POTION_QUANTITY = 20;

async function moveToX(driver, probes, targetX, tolerance = 70) {
  for (let attempt = 0; attempt < 40; attempt += 1) {
    const delta = targetX - await probes.playerX();
    if (Math.abs(delta) <= tolerance) return;
    const right = delta > 0;
    await driver.keyHold(
      right ? 'ArrowRight' : 'ArrowLeft',
      right ? 'ArrowRight' : 'ArrowLeft',
      right ? 39 : 37,
      Math.min(400, Math.max(100, Math.abs(delta) * 2))
    );
    await sleep(100);
  }
  throw new Error(`Could not move near x=${targetX}; current=${await probes.playerX()}`);
}

async function ensurePetSummoned(driver, probes, requireState) {
  if (await probes.petActive() === 1) return;
  const puppySlot = await probes.inventorySlot(5000001);
  if (puppySlot < 1 || puppySlot > 24)
    throw new Error(`Starter puppy is unavailable in visible cash inventory: slot=${puppySlot}`);

  await driver.click(711, 574);
  await sleep(400);
  const menu = { x: await probes.itemsMenuX(), y: await probes.itemsMenuY() };
  if (!Number.isFinite(menu.x) || !Number.isFinite(menu.y) || menu.x < 0 || menu.y < 0)
    throw new Error(`Items menu point unavailable: ${JSON.stringify(menu)}`);
  await driver.click(menu.x, menu.y);
  await sleep(500);
  await requireState('item inventory assets ready', async () =>
    await probes.assetBlocking() === 0 &&
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')"),
    45);
  await sleep(500);
  await driver.click(438, 178);
  await sleep(500);
  await requireState('cash inventory assets ready', async () =>
    await probes.assetBlocking() === 0 &&
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')"),
    45);
  await sleep(500);
  await driver.screenshot('08f-pet-cash-inventory');
  const column = (puppySlot - 1) % 4;
  const row = Math.floor((puppySlot - 1) / 4);
  await driver.doubleClick(329 + column * 36, 218 + row * 35);
  await requireState('starter puppy summoned', async () => await probes.petActive() === 1);
  await driver.key('i', 'KeyI', 73, 'i');
}

async function buyPotion(driver, probes, requireState, row, itemId, minimumQuantity) {
  const before = await probes.inventoryQuantity(itemId);
  if (before >= minimumQuantity) return before;
  if (await probes.shopBuyItem(row) !== itemId) {
    throw new Error(`Potion shop row ${row} expected ${itemId}, got ${await probes.shopBuyItem(row)}`);
  }
  const point = { x: await probes.shopBuyX(row), y: await probes.shopBuyY(row) };
  await driver.doubleClick(point.x, point.y);
  await requireState(`buy quantity input for ${itemId}`, async () =>
    await driver.evaluate('MapleWasmIME.active === true'));
  await driver.compose(String(minimumQuantity - before));
  await sleep(150);
  await driver.key('Enter', 'Enter', 13);
  await requireState(`potion ${itemId} purchased`, async () =>
    await probes.inventoryQuantity(itemId) >= minimumQuantity);
  return await probes.inventoryQuantity(itemId);
}

async function findShopRow(probes, itemId) {
  for (let row = 0; row < 5; row += 1) {
    if (await probes.shopBuyItem(row) === itemId) return row;
  }
  throw new Error(`Potion ${itemId} is unavailable in the visible shop rows`);
}

async function ensurePotions(driver, probes, requireState, hpItem, mpItem) {
  if (await probes.mapId() !== 102000002)
    throw new Error(`Potion setup requires map 102000002, got ${await probes.mapId()}`);
  if (await probes.inventoryQuantity(hpItem) >= MINIMUM_POTION_QUANTITY &&
      await probes.inventoryQuantity(mpItem) >= MINIMUM_POTION_QUANTITY) return;

  await moveToX(driver, probes, -16);
  await driver.key(' ', 'Space', 32, ' ');
  await requireState('Perion potion shop active', async () => await probes.shopActive() === 1);
  await requireState('Perion potion shop assets ready', async () =>
    await probes.assetBlocking() === 0 &&
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')"),
    45);
  await sleep(1000);
  await driver.screenshot('08e-pet-potion-shop');
  await buyPotion(driver, probes, requireState,
    await findShopRow(probes, hpItem), hpItem, MINIMUM_POTION_QUANTITY);
  await buyPotion(driver, probes, requireState,
    await findShopRow(probes, mpItem), mpItem, MINIMUM_POTION_QUANTITY);
  await driver.screenshot('08f-pet-potions-purchased');
  await driver.key('Escape', 'Escape', 27);
  await requireState('potion shop closed', async () => await probes.shopActive() === 0);
}

async function openOptions(driver, probes, requireState) {
  await driver.click(400, 300);
  let point = { x: -32768, y: -32768 };
  for (let attempt = 0; attempt < 3 && point.x < 0; attempt += 1) {
    await driver.key('Escape', 'Escape', 27);
    await sleep(500);
    point = { x: await probes.petOptionsMenuX(), y: await probes.petOptionsMenuY() };
  }
  if (!Number.isFinite(point.x) || !Number.isFinite(point.y) || point.x < 0 || point.y < 0)
    throw new Error(`Pet options menu point unavailable: ${JSON.stringify(point)}`);
  await driver.click(point.x, point.y);
  await requireState('pet automation options active', async () =>
    await probes.petOptionsActive() === 1);
  await sleep(500);
  await requireState('pet automation option assets ready', async () =>
    await probes.assetBlocking() === 0 &&
    await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')"),
    45);
  await sleep(500);
}

async function selectPotion(driver, readSelection, target, y) {
  for (let attempt = 0; attempt < 8 && await readSelection() !== target; attempt += 1) {
    await driver.click(450, y);
    await sleep(250);
  }
  const selected = await readSelection();
  if (selected !== target)
    throw new Error(`Could not select potion ${target}; selected=${selected}`);
}

async function configureOptions(driver, probes, requireState, hpItem, mpItem, configure) {
  await openOptions(driver, probes, requireState);
  if (configure) {
    await driver.click(489, 204);
    await driver.click(489, 348);
    await selectPotion(driver, probes.petOptionsHpItem, hpItem, 274);
    await selectPotion(driver, probes.petOptionsMpItem, mpItem, 418);
  }
  const selected = {
    hp: await probes.petOptionsHpItem(),
    mp: await probes.petOptionsMpItem()
  };
  await driver.screenshot('08g-pet-automation-options');
  await driver.click(345, 516);
  await requireState('pet automation options saved and closed', async () =>
    await probes.petOptionsActive() === 0);
  return selected;
}

async function captureFollowAttempt(driver, probes, right) {
  const before = { playerX: await probes.playerX(), petX: await probes.petX() };
  await driver.evaluate(`(() => {
    const sample = { active: true, points: [] };
    const tick = () => {
      if (!sample.active) return;
      sample.points.push({
        playerX: Module.ccall('msplayer_x', 'number', [], []),
        petX: Module.ccall('mspet_x', 'number', [], []),
        petY: Module.ccall('mspet_y', 'number', [], []),
        stance: Module.ccall('mspet_stance', 'number', [], [])
      });
      requestAnimationFrame(tick);
    };
    window.petFollowEvidence = sample;
    requestAnimationFrame(tick);
  })()`);
  await driver.keyHold(
    right ? 'ArrowRight' : 'ArrowLeft',
    right ? 'ArrowRight' : 'ArrowLeft',
    right ? 39 : 37, 2200
  );
  await sleep(1500);
  const points = await driver.evaluate(`(() => {
    window.petFollowEvidence.active = false;
    return window.petFollowEvidence.points;
  })()`);
  const after = {
    playerX: await probes.playerX(), playerY: await probes.playerY(),
    petX: await probes.petX(), petY: await probes.petY(), stance: await probes.petStance()
  };
  return { before, after, points };
}

async function verifyFollow(driver, probes) {
  let attempt = await captureFollowAttempt(driver, probes, true);
  if (Math.abs(attempt.after.playerX - attempt.before.playerX) <= 50)
    attempt = await captureFollowAttempt(driver, probes, false);
  const { before, after, points } = attempt;
  const distance = Math.hypot(after.playerX - after.petX, after.playerY - after.petY);
  let reversals = 0;
  let direction = 0;
  for (let index = 6; index < points.length; index += 6) {
    if (points[index].stance !== 0 || points[index - 6].stance !== 0) continue;
    const delta = points[index].petX - points[index - 6].petX;
    if (Math.abs(delta) < 3) continue;
    const next = Math.sign(delta);
    if (next && direction && next !== direction) reversals += 1;
    if (next) direction = next;
  }
  if (Math.abs(after.playerX - before.playerX) <= 50 || distance > 260 || reversals > 2)
    throw new Error(`Pet follow stability failed: ${JSON.stringify({ before, after, distance, reversals })}`);
  return { before, after, distance, reversals, sampleCount: points.length };
}

async function verifyRuntimePickup(driver, probes) {
  if (await probes.mobCount() < 1)
    throw new Error(`Pet runtime pickup requires live mobs; map=${await probes.mapId()}`);
  const before = {
    pickups: await probes.petConfirmedPickups(), drops: await probes.dropCount(),
    meso: await probes.playerMeso()
  };
  await driver.evaluate(`(() => {
    const evidence = { active: true, targetSeen: false, maxDrops: 0 };
    const tick = () => {
      if (!evidence.active) return;
      evidence.targetSeen ||= Module.ccall('mspet_has_loot_target', 'number', [], []) === 1;
      evidence.maxDrops = Math.max(evidence.maxDrops,
        Module.ccall('msdrop_count', 'number', [], []));
      requestAnimationFrame(tick);
    };
    window.petLootEvidence = evidence;
    requestAnimationFrame(tick);
  })()`);
  const trace = [];
  for (let attempt = 0; attempt < 100 &&
      await probes.petConfirmedPickups() === before.pickups; attempt += 1) {
    if (await probes.dropCount() > 0) {
      await sleep(400);
      continue;
    }
    const mobX = await probes.nearestMobX();
    const playerX = await probes.playerX();
    if (attempt % 5 === 0) {
      trace.push({
        attempt, playerX, playerY: await probes.playerY(),
        mobX, mobY: await probes.nearestMobY(), mobs: await probes.mobCount(),
        drops: await probes.dropCount(), hp: await probes.playerHp()
      });
    }
    if (mobX <= -32768) {
      await driver.keyHold('ArrowRight', 'ArrowRight', 39, 180);
      await sleep(220);
      continue;
    }
    const delta = mobX - playerX;
    const right = delta >= 0;
    if (Math.abs(delta) > 65) {
      await driver.keyHold(
        right ? 'ArrowRight' : 'ArrowLeft',
        right ? 'ArrowRight' : 'ArrowLeft',
        right ? 39 : 37,
        Math.min(350, Math.max(80, Math.abs(delta) * 2))
      );
    } else {
      await driver.keyHold(
        right ? 'ArrowRight' : 'ArrowLeft',
        right ? 'ArrowRight' : 'ArrowLeft',
        right ? 39 : 37, 35
      );
    }
    await driver.keyHold('Control', 'ControlLeft', 17, 60);
    await sleep(400);
  }
  for (let second = 0; second < 15 &&
      await probes.petConfirmedPickups() === before.pickups; second += 1)
    await sleep(1000);
  const sampled = await driver.evaluate(`(() => {
    window.petLootEvidence.active = false;
    return window.petLootEvidence;
  })()`);
  const after = {
    pickups: await probes.petConfirmedPickups(), drops: await probes.dropCount(),
    meso: await probes.playerMeso()
  };
  fs.writeFileSync(`${driver.artifactDir}/pet-pickup-trace.json`,
    `${JSON.stringify({ before, after, sampled, trace }, null, 2)}\n`);
  if (after.pickups <= before.pickups)
    throw new Error(`Server-confirmed pet pickup was not observed: ${JSON.stringify({ after, sampled, trace })}`);
  console.log('PASS  server-confirmed pet pickup');
  if (!sampled.targetSeen || sampled.maxDrops < 1)
    throw new Error(`Pet loot target transition was not observed: ${JSON.stringify(sampled)}`);
  return { before, after, ...sampled };
}

export async function verifyPetAutomationFlow(driver, probes, requireState, mode) {
  const hpItem = Number(process.env.E2E_PET_HP_ITEM || DEFAULT_HP_ITEM);
  const mpItem = Number(process.env.E2E_PET_MP_ITEM || DEFAULT_MP_ITEM);
  if (mode === 'configure') await ensurePotions(driver, probes, requireState, hpItem, mpItem);
  await ensurePetSummoned(driver, probes, requireState);

  let selected = null;
  if (mode !== 'runtime')
    selected = await configureOptions(driver, probes, requireState, hpItem, mpItem, mode === 'configure');

  const configured = {
    hpItem: await probes.petAutoHpItem(), mpItem: await probes.petAutoMpItem(),
    hpThreshold: await probes.petAutoHpThreshold(), mpThreshold: await probes.petAutoMpThreshold(),
    hpQuantity: await probes.inventoryQuantity(hpItem), mpQuantity: await probes.inventoryQuantity(mpItem),
    hp: await probes.playerHp(), maxHp: await probes.playerMaxHp(),
    mp: await probes.playerMp(), maxMp: await probes.playerMaxMp()
  };
  if ((mode === 'configure' || mode === 'runtime') &&
      (configured.hpItem !== hpItem || configured.mpItem !== mpItem))
    throw new Error(`Persisted pet potion settings mismatch: ${JSON.stringify(configured)}`);
  if (mode === 'runtime' &&
      (configured.hpThreshold !== CONFIGURED_THRESHOLD ||
       configured.mpThreshold !== CONFIGURED_THRESHOLD))
    throw new Error(`Persisted pet thresholds mismatch: ${JSON.stringify(configured)}`);
  if (mode === 'configure' &&
      (configured.hpThreshold !== CONFIGURED_THRESHOLD ||
       configured.mpThreshold !== CONFIGURED_THRESHOLD))
    throw new Error(`Pet thresholds were not saved: ${JSON.stringify(configured)}`);
  if (mode === 'runtime' && (configured.hp <= 1 || configured.mp <= 1))
    throw new Error(`Pet auto recovery did not restore resources: ${JSON.stringify(configured)}`);

  const loot = mode === 'runtime'
    ? await verifyRuntimePickup(driver, probes)
    : null;
  const follow = await verifyFollow(driver, probes);
  const evidence = { mode, selected, configured, loot, follow };
  fs.writeFileSync(`${driver.artifactDir}/pet-automation-evidence.json`,
    `${JSON.stringify(evidence, null, 2)}\n`);
  await driver.screenshot('08h-pet-follow-recovered');
  console.log(`PASS  pet automation ${JSON.stringify(evidence)}`);
}
