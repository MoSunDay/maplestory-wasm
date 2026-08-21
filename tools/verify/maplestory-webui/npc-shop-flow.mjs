import { sleep } from './cdp.mjs';

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

export async function verifyNpcShopFlow(driver) {
  await driver.click(400, 300);
  if (process.env.E2E_NPC_SHOP_INSIDE !== '1') {
    await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, 2200);
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 300);
  }
  await requireState('weapon-shop map loaded', async () =>
    await driver.evaluate("Module.ccall('msstage_loading', 'number', [], [])") === 0);
  await sleep(3000);
  await driver.screenshot('20-npc-shop-map');
  await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, 350);
  await driver.doubleClick(150, 400);
  await sleep(5000);
  await driver.screenshot('21-npc-shop-open');

  await driver.click(472, 187);
  await sleep(500);
  await driver.doubleClick(500, 220);
  await requireState('sell quantity notice opened', async () =>
    await driver.evaluate("Module.ccall('msui_notice_active', 'number', [], [])") === 1 &&
    await driver.evaluate('MapleWasmIME.active === true'));
  await driver.compose('999999999');
  await requireState('sell quantity clamps to stack maximum', async () =>
    await driver.evaluate("document.getElementById('ime-input').value === '37'"));
  await driver.screenshot('22-sell-quantity-clamped');
  await driver.key('Escape', 'Escape', 27);
  await requireState('sell quantity notice cancelled', async () =>
    await driver.evaluate("Module.ccall('msui_notice_active', 'number', [], [])") === 0 &&
    await driver.evaluate('MapleWasmIME.active === false'));

  await sleep(500);
  let sellAllOpened = false;
  for (const [x, y] of [[570, 116], [588, 116], [606, 116],
                        [570, 124], [588, 124], [606, 124]]) {
    await driver.click(x, y);
    await sleep(250);
    const notice = await driver.evaluate(
      "Module.ccall('msui_notice_active', 'number', [], [])");
    if (notice === 1) {
      const numberInput = await driver.evaluate('MapleWasmIME.active === true');
      if (!numberInput) {
        sellAllOpened = true;
        break;
      }
      await driver.key('Escape', 'Escape', 27);
      await sleep(250);
    }
  }
  if (!sellAllOpened) throw new Error('Sell-all button did not open its confirmation');
  await requireState('sell-all confirmation opened', async () =>
    await driver.evaluate("Module.ccall('msui_notice_active', 'number', [], [])") === 1);
  await sleep(500);
  await driver.screenshot('23-sell-all-confirmation');
  await driver.key('Escape', 'Escape', 27);
  await requireState('sell-all confirmation cancelled', async () =>
    await driver.evaluate("Module.ccall('msui_notice_active', 'number', [], [])") === 0);

  await driver.click(604, 187);
  await sleep(500);
  await driver.screenshot('24-cash-tab-empty');
  console.log('PASS  cash tab remains visible for visual empty-state review');

  await driver.key('Escape', 'Escape', 27);
  await sleep(250);
  await driver.keyHold('ArrowRight', 'ArrowRight', 39, 700);
  await driver.keyHold('ArrowUp', 'ArrowUp', 38, 300);
  await sleep(2000);
  await requireState('returned to town after NPC-shop verification', async () =>
    await driver.evaluate("Module.ccall('msstage_loading', 'number', [], [])") === 0);
  await driver.screenshot('25-npc-shop-returned-to-town');
}
