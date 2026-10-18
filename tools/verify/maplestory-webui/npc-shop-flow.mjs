// Ariant NPC 商店端到端（v2）。
//
// v1 假设训练营地附近有商店 NPC，实况核查推翻：10000 出口 NPC 2101 只做
// 传送问答，20000 的 NPC 2000 不开店；全服仅 5 个开店脚本。v2 改用 Ariant
// (260000000) 的武器店老板 NPC 2100002（x≈1052，2100002.js 直接
// openShopNPC→sendShop），并把 e2e 角色迁到该地图走过去交易。
//
// 前置（操作者执行，凭据不入代码仓库）：
//   mysql ... maplestory <<'SQL'
//     UPDATE characters SET map=260000000 WHERE id=5;  -- e2eaccept1(card0)
//     DELETE FROM inventoryitems WHERE characterid=5 AND inventorytype=2
//       AND itemid=2000000;
//     INSERT INTO inventoryitems (type, characterid, accountid, itemid,
//         inventorytype, position, quantity, owner, petid, flag, expiration,
//         giftFrom)
//       VALUES (1, 5, 25, 2000000, 2, 1, 37, '', -1, 0, -1, '');
//   SQL
// 运行：E2E_NPC_SHOP=1 CHROME_BIN=/usr/bin/chromium-browser node run.mjs
//   （run.mjs 以 card0=e2eaccept1 进图后转入本流程）
//
// 断言：入图 260000000 → 右行至 NPC → 双击 → msshop_active → USE 页签
// （截图留档：SellState 图标列/文本列错位的上游行为证据）→ row0 选中 →
// SELL_ITEM 弹数量框（默认数量 1 回车确认，卖出 1 件 37→36）→ SELL_ALL
// 弹确认框回车确认（36→0，行清空）→ Escape 关店。
// 坐标全部来自 msshop_* 探针直读窗口几何；点击一律 retryClick 带状态谓词
// （资产流高负载下 CDP 点击会整击丢失）。

import { sleep } from './cdp.mjs';

const ARIANT_MAP = 260000000;
const NPC_X = 1052;

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

async function probe(driver, name) {
  return await driver.evaluate(`Module.ccall('${name}', 'number', [], [])`);
}

async function playerAt(driver) {
  const x = await probe(driver, 'msplayer_x');
  const sx = await probe(driver, 'msplayer_screen_x');
  const sy = await probe(driver, 'msplayer_screen_y');
  return { x, sx, sy };
}

// 按距离缩放的短冲刺：远距离 3s 长走，近距离缩到 0.2s 微调，避免惯性
// 冲过目标（v83 移动每步带滑行）。
async function walkTowards(driver, targetX, maxSeconds = 180) {
  for (let elapsed = 0; elapsed < maxSeconds;) {
    const { x } = await playerAt(driver);
    const dx = targetX - x;
    if (Math.abs(dx) <= 50) return x;
    const key = dx > 0 ? 'ArrowRight' : 'ArrowLeft';
    const holdMs = Math.max(200, Math.min(3000, Math.abs(dx)));
    await driver.keyHold(key, key, key === 'ArrowRight' ? 39 : 37, holdMs);
    elapsed += holdMs / 1000 + 0.4;
    await sleep(400);
  }
  return (await playerAt(driver)).x;
}

export async function verifyNpcShopFlow(driver) {
  await requireState('Ariant town map loaded (run the prep SQL first)', async () =>
    await probe(driver, 'msmap_id') === ARIANT_MAP, 60);
  await requireState('map asset safety gate released', async () =>
    await probe(driver, 'msstage_loading') === 0, 60);
  await sleep(3000);

  const finalX = await walkTowards(driver, NPC_X);
  if (Math.abs(finalX - NPC_X) > 90) {
    throw new Error(`could not approach NPC ${NPC_X}, stopped at x=${finalX}`);
  }
  await sleep(500);

  const { x, sx, sy } = await playerAt(driver);
  const npcScreenX = sx + (NPC_X - x);
  await driver.doubleClick(npcScreenX, sy - 60);
  await requireState('NPC shop window opened', async () =>
    await probe(driver, 'msshop_active') === 1, 10);
  await requireState('prepared USE stack visible to the client', async () =>
    await driver.evaluate(
      "Module.ccall('msinventory_quantity','number',['number'],[2000000])") === 37, 10);
  await sleep(1000);
  await driver.screenshot('20-npc-shop-open');

  // 以 buy row0 探针反推窗口原点（AreaButton 左上角锚定）：
  // row0 中心 = position + (6+199/2, 124+35/2)。
  // 探针直读窗口几何（点击在资产流高负载下会整击丢失，一律带谓词重试）。
  const ccall1 = async (name, arg) => await driver.evaluate(
    `Module.ccall('${name}','number',['number'],[${arg}])`);
  const buttonCenter = async (id) => ({
    x: await ccall1('msshop_button_x', id),
    y: await ccall1('msshop_button_y', id),
  });
  const sellRow0 = {
    x: await ccall1('msshop_sell_x', 0),
    y: await ccall1('msshop_sell_y', 0),
  };
  const sellAllButton = await buttonCenter(23);
  const useTab = await buttonCenter(4);
  console.log(`INFO  shop geometry row0=(${sellRow0.x},${sellRow0.y}) useTab=(${useTab.x},${useTab.y}) sellAll=(${sellAllButton.x},${sellAllButton.y})`);

  // 切到 USE 页签：卖侧行0 变为预备的 2000000 药水。
  await driver.retryClick(useTab.x, useTab.y, async () =>
    await ccall1('msshop_sell_item', 0) === 2000000);
  console.log('PASS  sell tab switched to USE (row0 is item 2000000)');
  await sleep(800);
  // SellState 证据：图标列与右侧文本列错开的上游布局。
  await driver.screenshot('21-npc-shop-sell-tab');

  // 行点击仅选中（AreaButton）；谓词 = 选中槽位探针翻转为 >= 0。
  await driver.retryClick(sellRow0.x, sellRow0.y, async () =>
    await probe(driver, 'msshop_selected') >= 0);
  console.log('PASS  sell row0 selected');
  await sleep(300);
  await driver.screenshot('21b-row-selected');

  // 行双击卖出（原 SELL_ITEM 按钮已移除；同槽二连击 = sell()）→
  // UIEnterNumber（NOTICE + 数字框；键盘路径只能用默认数量 1
  // 回车确认——数字未注册为 UI 焦点文本框，属上游移植限制）。
  await driver.retryClick(sellRow0.x, sellRow0.y, async () =>
    await probe(driver, 'msui_notice_active') === 1 &&
    await driver.evaluate('MapleWasmIME.active === true'));
  console.log('PASS  sell quantity notice opened (row double-click)');
  await sleep(400);
  await driver.screenshot('21c-sell-notice');
  await driver.key('Enter', 'Enter', 13);
  await requireState('sold 1 potion (default quantity, notice closed)', async () =>
    await probe(driver, 'msui_notice_active') === 0 &&
    await ccall1('msinventory_quantity', 2000000) === 36);
  await driver.screenshot('22-sold-one');

  // 售卖后库存变更回调会重建卖侧列表（selection 复位），重新选中再走
  // SELL_ALL：UIYesNo（非数字框的 NOTICE），Enter=确认全部售出。
  await driver.retryClick(sellRow0.x, sellRow0.y, async () =>
    await probe(driver, 'msshop_selected') >= 0);
  await driver.retryClick(sellAllButton.x, sellAllButton.y, async () =>
    await probe(driver, 'msui_notice_active') === 1 &&
    await driver.evaluate('MapleWasmIME.active === false'));
  console.log('PASS  sell-all confirmation opened');
  await sleep(400);
  await driver.screenshot('22b-sell-all-notice');
  await driver.key('Enter', 'Enter', 13);
  await requireState('sell-all confirmed (stack emptied)', async () =>
    await probe(driver, 'msui_notice_active') === 0 &&
    await ccall1('msinventory_quantity', 2000000) === 0 &&
    await ccall1('msshop_sell_item', 0) === 0);
  await driver.screenshot('22c-sell-all-done');

  await driver.key('Escape', 'Escape', 27);
  await requireState('NPC shop window closed', async () =>
    await probe(driver, 'msshop_active') === 0, 10);
  await driver.screenshot('23-npc-shop-closed');
}
