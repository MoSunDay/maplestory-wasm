import { sleep } from './cdp.mjs';

async function isReachable(url) {
  try {
    const response = await fetch(url, { signal: AbortSignal.timeout(3000) });
    return response.ok;
  } catch (error) {
    return false;
  }
}

export async function resolvePageUrl() {
  if (process.env.E2E_URL) {
    if (!await isReachable(process.env.E2E_URL))
      throw new Error(`E2E_URL is not reachable: ${process.env.E2E_URL}`);
    return process.env.E2E_URL;
  }

  const candidates = [
    'http://127.0.0.1:8000/web/index.html',
    'http://127.0.0.1:8001/web/index.html'
  ];
  for (const candidate of candidates)
    if (await isReachable(candidate)) return candidate;
  throw new Error(`No loopback WebUI endpoint; checked: ${candidates.join(', ')}. Set E2E_URL for a custom bind.`);
}

export function createRuntimeProbes(driver) {
  const number = name =>
    driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
  const numberArgument = (name, value) =>
    driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', ['number'], [${Number(value)}])`);
  return {
    currentUiState: () => number('msui_state'),
    loginNoticeActive: () => number('msui_login_notice_active'),
    loginNoticeMessage: () => number('msui_login_notice_message'),
    characterCreationCustomizing: () => number('msui_character_creation_customizing'),
    noticeActive: () => number('msui_notice_active'),
    mapAssetLoading: () => number('msstage_loading'),
    assetBlocking: () => number('msasset_blocking'),
    mapId: () => number('msmap_id'),
    mobCount: () => number('msmob_count'),
    nearestMobX: () => number('msnearest_mob_x'),
    nearestMobY: () => number('msnearest_mob_y'),
    dropCount: () => number('msdrop_count'),
    playerX: () => number('msplayer_x'),
    playerY: () => number('msplayer_y'),
    playerHp: () => number('msplayer_hp'),
    playerMaxHp: () => number('msplayer_max_hp'),
    playerMp: () => number('msplayer_mp'),
    playerMaxMp: () => number('msplayer_max_mp'),
    playerMeso: () => number('msplayer_meso'),
    petActive: () => number('mspet_active'),
    petX: () => number('mspet_x'),
    petY: () => number('mspet_y'),
    petStance: () => number('mspet_stance'),
    petHasLootTarget: () => number('mspet_has_loot_target'),
    petConfirmedPickups: () => number('mspet_confirmed_pickups'),
    petAutoHpItem: () => number('mspet_auto_hp_item'),
    petAutoMpItem: () => number('mspet_auto_mp_item'),
    petAutoHpThreshold: () => number('mspet_auto_hp_threshold'),
    petAutoMpThreshold: () => number('mspet_auto_mp_threshold'),
    petOptionsActive: () => number('mspet_options_active'),
    petOptionsHpItem: () => number('mspet_options_hp_item'),
    petOptionsMpItem: () => number('mspet_options_mp_item'),
    petOptionsMenuX: () => number('mspet_options_menu_x'),
    petOptionsMenuY: () => number('mspet_options_menu_y'),
    itemsMenuX: () => number('msitems_menu_x'),
    itemsMenuY: () => number('msitems_menu_y'),
    shopActive: () => number('msshop_active'),
    shopBuyItem: row => numberArgument('msshop_buy_item', row),
    shopBuyX: row => numberArgument('msshop_buy_x', row),
    shopBuyY: row => numberArgument('msshop_buy_y', row),
    inventoryQuantity: itemId => numberArgument('msinventory_quantity', itemId),
    inventorySlot: itemId => numberArgument('msinventory_slot', itemId),
    cashShopProbe: name => number(name)
  };
}

export function createAssetIdleWaiter(driver, requireState) {
  return async function requireAssetIdle(name = 'foreground assets ready') {
    let hiddenSince = null;
    await sleep(250);
    return requireState(name, async () => {
      const hidden = await driver.evaluate(
        "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
      if (!hidden) {
        hiddenSince = null;
        return false;
      }
      if (hiddenSince === null) hiddenSince = Date.now();
      return Date.now() - hiddenSince >= 1000;
    }, 45);
  };
}

async function walkRightThroughPortal(driver, probes, requireState,
    fromMap, toMap, portalX, portalY) {
  if (await probes.mapId() !== fromMap)
    throw new Error(`Tutorial mob route expected map ${fromMap}, got ${await probes.mapId()}`);

  await driver.click(400, 300);
  console.log(`INFO  tutorial route map=${fromMap} startX=${await probes.playerX()}`);
  await driver.send('Input.dispatchKeyEvent', {
    type: 'keyDown', key: 'ArrowRight', code: 'ArrowRight',
    windowsVirtualKeyCode: 39, nativeVirtualKeyCode: 39
  });
  try {
    for (let elapsed = 0;
      elapsed < 60000 && await probes.playerX() < portalX - 60;
      elapsed += 1000) {
      await driver.keyHold('Alt', 'AltLeft', 18, 120);
      await sleep(880);
    }
  } finally {
    await driver.send('Input.dispatchKeyEvent', {
      type: 'keyUp', key: 'ArrowRight', code: 'ArrowRight',
      windowsVirtualKeyCode: 39, nativeVirtualKeyCode: 39
    });
  }
  console.log(`INFO  tutorial route map=${fromMap} portalX=${await probes.playerX()}`);

  if (await probes.playerY() < portalY - 100) {
    await driver.send('Input.dispatchKeyEvent', {
      type: 'keyDown', key: 'ArrowDown', code: 'ArrowDown',
      windowsVirtualKeyCode: 40, nativeVirtualKeyCode: 40
    });
    await driver.keyHold('Alt', 'AltLeft', 18, 150);
    await driver.send('Input.dispatchKeyEvent', {
      type: 'keyUp', key: 'ArrowDown', code: 'ArrowDown',
      windowsVirtualKeyCode: 40, nativeVirtualKeyCode: 40
    });
    await sleep(1500);
  }

  const offsets = [0, -20, 20, -40, 40];
  for (let attempt = 0;
    attempt < offsets.length && await probes.mapId() === fromMap;
    attempt += 1) {
    const targetX = portalX + offsets[attempt];
    for (let adjustment = 0; adjustment < 24; adjustment += 1) {
      const delta = targetX - await probes.playerX();
      if (Math.abs(delta) <= 8) break;
      const right = delta > 0;
      await driver.keyHold(
        right ? 'ArrowRight' : 'ArrowLeft',
        right ? 'ArrowRight' : 'ArrowLeft',
        right ? 39 : 37,
        250
      );
    }
    await sleep(500);
    console.log(`INFO  tutorial portal attempt map=${fromMap} x=${await probes.playerX()} y=${await probes.playerY()}`);
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 500);
    await sleep(1000);
  }
  await requireState(`tutorial route entered map ${toMap}`, async () =>
    await probes.mapId() === toMap, 20);
  await requireState(`map ${toMap} asset safety gate released`, async () =>
    await probes.mapAssetLoading() === 0, 60);
}

export async function enterTutorialMobMap(driver, probes, requireState) {
  const route = [
    [10000, 20000, 1077, 480],
    [20000, 30000, 630, 210],
    [30000, 40000, 1046, 94]
  ];
  for (const step of route)
    await walkRightThroughPortal(driver, probes, requireState, ...step);
}
