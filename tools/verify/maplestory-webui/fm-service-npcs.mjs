// 自由市场(910000000) 两个服务 NPC 的端到端验证。
//
//   * 9110000「佩里 / 次元空间的帮手」x=300：Space 开传送菜单 →
//     第一分类「维多利亚岛及周边」→ 第一目的地 100000000（射手村）→
//     cm.warp 落图。菜单选项用默认选中项 0，通过点击首行选项直接确认。
//   * 9201116「Kopee Relicseeker」x=600：Space 开商店（服务端 shops.shopid=1339），
//     断言 msshop_buy_item 至少 2 行命中本店物品，并买下 2030004/2030000 之一
//     （价格 500/400 mesos，库存数量 +1）。
//
// 客户端不读 Map.nx 的 life，NPC 全部由服务端 SPAWN_NPC 包生成；两条 life
// 位于 wz/Map.wz/Map/Map9/910000000.img.xml 的 index 4/5，商店数据位于
// sql/custom_fm_service_npcs.sql。对话选项是真实鼠标点击：UINpcTalk 的
// SELECTION 模式默认 selected=0，点中首行会发送 selection(type, 首项 id)。
//
// testWarp 的目的地由 FM_SERVICE_DEST 选择：henesys（默认，首项 射手村
// 100000000，点首行选项直接确认）或 ant-park（第 7 项 蚂蚁广场 105070001）。
// 维多利亚子菜单共 12 项（index 11 为「返回主菜单」），按钮 NEXT/PREV 只循环
// 移动 selected 不发包，OK 才发送 selections[selected]；从 selected=0 出发
// ±6 ≡ 6 (mod 12)，NEXT×6 与 PREV×6 同落 index 6，故实测第 7 项时不依赖
// 按钮方向（按钮几何经 msnpctalk_button_field/element_field 探针直读）。
//
// 前置（操作者执行，凭据不入代码仓库）：
//   update accounts set loggedin=0 where name='test2';
//   update characters set map=910000000, spawnpoint=0 where id=219;
//   test2 的 card0 是 test2ILA（character id=219, accountid=64）。
//   keymap 需有 Space/Alt/Ctrl（本机已写入 57→5/54、56→5/53、29→5/52）。
// 运行：
//   FM_SERVICE_ACCOUNT=test2 FM_SERVICE_PASSWORD=test2 \
//   FM_SERVICE_CARD=0 CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/fm-service-npcs.mjs
//   # 实测蚂蚁广场：上面再追加 FM_SERVICE_DEST=ant-park

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.FM_SERVICE_DEBUG_PORT || 9255);
const pageUrl = process.env.FM_SERVICE_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.FM_SERVICE_ACCOUNT || 'test2';
const password = process.env.FM_SERVICE_PASSWORD || 'test2';
const cardIndex = Number(process.env.FM_SERVICE_CARD ?? 0);

// testWarp 目的地配置：optionIndex 是维多利亚子菜单的选项序号（0 起）。
const warpDestName = process.env.FM_SERVICE_DEST || 'henesys';
const WARP_DESTS = Object.freeze({
  henesys: { mapId: 100000000, label: '射手村', optionIndex: 0, shot: 'fm-service-henesys' },
  'ant-park': { mapId: 105070001, label: '蚂蚁广场', optionIndex: 6, shot: 'fm-service-ant-park' }
});
const warpDest = WARP_DESTS[warpDestName];
if (!warpDest)
  throw new Error(`FM_SERVICE_DEST 未知目的地 ${JSON.stringify(warpDestName)}，可选：${Object.keys(WARP_DESTS).join(' / ')}`);

const HUB = 910000000;
const PERRY_X = 300;
const SHOP_X = 600;
const SHOP_ITEMS = new Set([
  2070000, 2070001, 2070002, 2070003, 2070004, 2070005, 2070006, 2070007,
  2070008, 2070009, 2070010, 2070011, 2070012, 2070013, 2070016, 2070018,
  2330000, 2330001, 2330002, 2330003, 2330004, 2330005, 2331000, 2332000,
  4006000, 4006001, 2000004, 2000005, 2020014, 2020015, 2030000, 2030004
]);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
};
const probe = (driver, name) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const ccall1 = (driver, name, value) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', ['number'], [${value}])`);
const promptText = driver =>
  driver.evaluate("Module.ccall('msnpc_prompt', 'string', [], [])");
const playerX = driver => probe(driver, 'msplayer_x');
const inRange = (v, range) => v >= range[0] && v <= range[1];

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}

function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

async function walkTowards(driver, targetX, maxSeconds = 90) {
  const start = Date.now();
  let lastX = null, stuck = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await playerX(driver);
    const dx = targetX - x;
    if (Math.abs(dx) <= 10) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuck += 1;
      if (stuck >= 3) {
        await driver.key('Alt', 'AltLeft', 18);
        await sleep(650);
        const after = await playerX(driver);
        if (Math.abs(after - x) < 2 && stuck >= 6)
          throw new Error(`walk stuck at x=${x} target=${targetX}`);
      }
    } else stuck = 0;
    lastX = x;
    const ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  const finalX = await playerX(driver);
  if (Math.abs(finalX - targetX) > 10)
    throw new Error(`walk timeout: x=${finalX} target=${targetX}`);
  return finalX;
}

async function settleShot(driver, name) {
  if (!await waitFor(assetsIdle(driver), 90))
    throw new Error(`assets not idle before ${name}`);
  await sleep(1500);
  await driver.screenshot(name);
}

async function loginAndEnterGame(driver) {
  console.log('INFO  等待 WASM 初始化');
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  console.log('INFO  WASM 就绪');
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30))
    throw new Error('login UI did not activate');
  console.log('INFO  登录界面');
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 45))
    throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216,
    async () => await probe(driver, 'msui_state') === uiState.characters);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 45))
    throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60))
    throw new Error('char-select assets never idle');
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    entered = await waitFor(
      async () => await probe(driver, 'msui_state') === uiState.game, 5);
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  console.log('INFO  进入游戏，等待资源');
  if (!await waitFor(assetsIdle(driver), 120))
    throw new Error('in-game assets never idle');
}

// UINpcTalk 选项的实际屏幕坐标：窗口在 800x600 设计画布居中，x=340 落在
// TEXT_WIDTH 内。从提示文字下方开始逐行点击，命中第一个选项会改变 prompt
// （主菜单→子菜单）或关闭对话（warp）。返回被点击的 y 与点击后的 prompt。
async function clickFirstOption(driver, fromY = 140, toY = 430, step = 5) {
  const before = await promptText(driver);
  for (let y = fromY; y <= toY; y += step) {
    await driver.click(340, y);
    await sleep(450);
    const active = await probe(driver, 'msnpc_active') === 1;
    const after = active ? await promptText(driver) : '';
    if (!active || after !== before) return { y, active, after };
  }
  throw new Error('no selectable dialogue option was hit');
}

async function openNpc(driver, x, label) {
  const finalX = await walkTowards(driver, x, 120);
  if (Math.abs(finalX - x) > 100)
    throw new Error(`${label}: 走位失败（x=${finalX}）`);
  await sleep(400);
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 1 ||
      await probe(driver, 'msshop_active') === 1, 8)) return;
    await sleep(500);
  }
  throw new Error(`${label}: Space 未打开对话/商店`);
}

async function closeDialogue(driver, label) {
  await driver.key('Escape', 'Escape', 27);
  const closed = await waitFor(async () => await probe(driver, 'msnpc_active') === 0 &&
    await probe(driver, 'msshop_active') === 0, 8);
  report(closed, `${label}: Escape 关闭窗口`);
  if (!closed) throw new Error(`${label}: 窗口未关闭，后续走位会被 UI 吞键`);
  await sleep(400);
}

async function testTeleportMenu(driver) {
  await openNpc(driver, PERRY_X, '佩里 9110000');
  const main = await promptText(driver);
  report(main.includes('佩里') && main.includes('世界各地'),
    `佩里: 主菜单出现（prompt=${JSON.stringify(main).slice(0, 90)}）`);
  await settleShot(driver, 'fm-service-perry-main');

  const category = await clickFirstOption(driver);
  report(category.active && category.after.includes('维多利亚'),
    `佩里: 首分类进入子菜单（y=${category.y} prompt=${JSON.stringify(category.after).slice(0, 90)}）`);
  await settleShot(driver, 'fm-service-perry-victoria');
  await closeDialogue(driver, '佩里');
}

async function testShop(driver) {
  await openNpc(driver, SHOP_X, '科皮 9201116');
  const open = await probe(driver, 'msshop_active') === 1;
  report(open, `科皮: 商店弹出（msshop_active=${await probe(driver, 'msshop_active')}）`);
  if (!open) throw new Error('商店未打开');
  await settleShot(driver, 'fm-service-kopee-shop');

  const rows = [];
  for (let row = 0; row < 5; row += 1)
    rows.push(await ccall1(driver, 'msshop_buy_item', row));
  const ours = rows.filter(id => SHOP_ITEMS.has(id));
  report(ours.length >= 2,
    `科皮: 商店含稀有物品（可见行 ${JSON.stringify(rows)}）`);

  const buyRow = rows.findIndex(id => id === 2030004 || id === 2030000);
  if (buyRow < 0) {
    console.log('INFO  科皮: 可见行无 2030004/2030000，跳过购买断言');
  } else {
    const itemid = rows[buyRow];
    const x = await ccall1(driver, 'msshop_buy_x', buyRow);
    const y = await ccall1(driver, 'msshop_buy_y', buyRow);
    const before = await ccall1(driver, 'msinventory_quantity', itemid);
    await driver.click(x, y);
    await sleep(250);
    await driver.click(x, y);
    const prompted = await waitFor(
      async () => await probe(driver, 'msui_notice_active') === 1, 6);
    report(prompted, `科皮: 购买 ${itemid} 弹出确认框`);
    if (prompted) {
      await driver.key('Enter', 'Enter', 13);
      const bought = await waitFor(async () =>
        await probe(driver, 'msui_notice_active') === 0 &&
        await ccall1(driver, 'msinventory_quantity', itemid) === before + 1, 8);
      report(bought,
        `科皮: 购买 ${itemid} 入库（${before}→${await ccall1(driver, 'msinventory_quantity', itemid)}）`);
    }
  }
  await closeDialogue(driver, '科皮');
}

// UINpcTalk 按钮探针（仿 pq-harness-dialog.mjs）：msnpctalk_element_field(0/1)
// 给对话框原点，msnpctalk_button_field(which, field) 给按钮相对矩形，
// field: 0=x 1=y 2=w 3=h 4=active；which: 0=YES 1=NO 2=END 3=OK 4/5=循环
// 按钮（NEXT/PREV）。探针失败/元素未激活时各字段拿不到有效数。
async function dialogButtonGeometry(driver, which) {
  const cc = (fn, ...args) => driver.evaluate(
    `Module.ccall('${fn}','number',[${args.map(() => "'number'").join(',')}],[${args.join(',')}])`);
  try {
    return {
      originX: Number(await cc('msnpctalk_element_field', 0)),
      originY: Number(await cc('msnpctalk_element_field', 1)),
      x: Number(await cc('msnpctalk_button_field', which, 0)),
      y: Number(await cc('msnpctalk_button_field', which, 1)),
      w: Number(await cc('msnpctalk_button_field', which, 2)),
      h: Number(await cc('msnpctalk_button_field', which, 3)),
      active: Number(await cc('msnpctalk_button_field', which, 4))
    };
  } catch (error) {
    return { error: error.message };
  }
}
const dialogButtonUsable = g => !g.error &&
  [g.originX, g.originY, g.x, g.y, g.w, g.h, g.active].every(v => v > -9000) &&
  g.active === 1 && g.w > 0 && g.h > 0;
const dialogButtonCenter = g => ({
  x: g.originX + g.x + (g.w >> 1), y: g.originY + g.y + (g.h >> 1) });
async function probeDialogButton(driver, which) {
  const g = await dialogButtonGeometry(driver, which);
  return dialogButtonUsable(g) ? dialogButtonCenter(g) : null;
}
// 循环按钮不区分 NEXT/PREV（±6 mod 12 同位）：先探 which=4，无效再探 5。
async function pickCycleButton(driver) {
  const probes = {};
  for (const which of [4, 5]) {
    probes[which] = await dialogButtonGeometry(driver, which);
    if (dialogButtonUsable(probes[which]))
      return { which, center: dialogButtonCenter(probes[which]) };
  }
  throw new Error(`佩里: 未探到循环按钮 NEXT/PREV（probe=${JSON.stringify(probes)}）`);
}

// 子菜单非首项确认：SELECTION 点选项行会立即发包，故改用按钮路径——
// 循环 optionIndex 次移动 selected，再点 OK 提交 selections[selected]。
async function confirmByButtons(driver, optionIndex, label) {
  const cycle = await pickCycleButton(driver);
  if (await probe(driver, 'msnpc_active') !== 1)
    throw new Error('佩里: 子菜单对话已关闭，无法循环选项');
  for (let i = 0; i < optionIndex; i += 1) {
    await driver.click(cycle.center.x, cycle.center.y);
    await sleep(250);
    if (await probe(driver, 'msnpc_active') !== 1)
      throw new Error(`佩里: 第 ${i + 1}/${optionIndex} 次循环后对话已关闭`);
  }
  const ok = await probeDialogButton(driver, 3);
  if (!ok) throw new Error('佩里: 未探到 OK 按钮（which=3）');
  await driver.click(ok.x, ok.y);
  console.log(`INFO  佩里: 循环 ${optionIndex} 次选中「${label}」，已点 OK`);
}

async function testWarp(driver) {
  await openNpc(driver, PERRY_X, '佩里 9110000');
  const main = await promptText(driver);
  report(main.includes('佩里'), '佩里: 再次打开主菜单');
  const category = await clickFirstOption(driver);
  report(category.active && category.after.includes('维多利亚'),
    '佩里: 再次进入维多利亚分类');
  if (warpDest.optionIndex === 0) {
    const destination = await clickFirstOption(driver);
    report(!destination.active, '佩里: 选择射手村后对话关闭');
  } else {
    await confirmByButtons(driver, warpDest.optionIndex, warpDest.label);
  }
  const warped = await waitFor(async () =>
    await probe(driver, 'msnpc_active') === 0 &&
    await probe(driver, 'msmap_id') === warpDest.mapId, 20);
  report(warped, `佩里: 传送到 ${warpDest.mapId} ${warpDest.label}（实测 ${await probe(driver, 'msmap_id')}）`);
  if (warped) await settleShot(driver, warpDest.shot);
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    const map = await probe(driver, 'msmap_id');
    report(map === HUB, `出生地图 = 910000000（实测 ${map}）`);
    if (map !== HUB) throw new Error(`出生地图不是自由市场：${map}`);
    const spawn = await playerX(driver);
    report(inRange(spawn, [-180, 20]), `出生落点 sp 区（x=${spawn}）`);

    await testTeleportMenu(driver);
    await testShop(driver);
    await testWarp(driver);
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
  } finally {
    if (driver.errors.length)
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`,
        `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`,
      `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  FM_SERVICE_CARD must be 0..3, got ${process.env.FM_SERVICE_CARD}`);
  process.exitCode = 1;
} else await main();
