// 任务日志窗口（Q 键 = KeyAction::QUESTLOG，默认绑定）的端到端验证。
//
//   * 按 Q 打开任务日志（UIElement::QUESTLOG，经 UIStateGame 的
//     KeyAction::QUESTLOG 分支 emplace<UIQuestLog>）→ 断言
//     msui_questlog_active === 1，并截图。
//   * msui_questlog_rows 返回可见任务行数（窗口未创建时为 -1），断言
//     打开后为非负整数。
//   * UIQuestLog 是 TOGGLED 元素：再按一次 Q 经 pre_add 翻转关闭 →
//     active === 0；再按 Q 重开，按 Escape 走 send_key(escape) 分支
//     deactivate → active === 0。
//   * 点击列表行 → msui_questlog_selected 反映选中任务 id（打开时
//     rebuild_rows 会自动选中首行，因此 rows>1 时改点第二行断言切换）。
//   * rows > 9 时对列表中部发 CDP mouseWheel 滚轮事件（Slider 无 offset
//     探针，仅弱断言窗口保持 active 并截图）。
//
// 探针 _msui_questlog_active / _msui_questlog_rows /
// _msui_questlog_selected 由
// src/client/CMakeLists.txt 的 EXPORTED_FUNCTIONS 导出；窗口实现位于
// IO/UITypes/Quest/UIQuestLog.{h,cpp}，数据源 Character/QuestLog +
// Quest.nx QuestInfo.img。
//
// 前置（操作者执行，凭据不入代码仓库）：
//   update accounts set loggedin=0 where name='test2';
//   update characters set map=910000000, spawnpoint=0 where id=219;
//   test2 的 card0 是 test2ILA（character id=219, accountid=64）。
//   keymap 需有 Q=16→4/8（KeyType::MENU=4 + KeyAction::QUESTLOG=8）。
//   两个易错点：① keymap 的 `key` 列是客户端 Keytable 的 MS 扫描码下标，
//   Q 是 16——GLFW 键码 81 对应的槽位其实是 PAGE_DOWN，写 81 不会生效；
//   ② KeyType 枚举 MENU=4/ACTION=5，误写成 5/8 时 ACTION 分支不认
//   action 8，Q 不会开窗。另需走位/交互用 Space/Alt/Ctrl
//   （57→5/54、56→5/53、29→5/52）。
// 运行：
//   QUESTLOG_ACCOUNT=test2 QUESTLOG_PASSWORD=test2 \
//   QUESTLOG_CARD=0 CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/questlog-flow.mjs

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.QUESTLOG_DEBUG_PORT || 9257);
const pageUrl = process.env.QUESTLOG_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.QUESTLOG_ACCOUNT || 'test2';
const password = process.env.QUESTLOG_PASSWORD || 'test2';
const cardIndex = Number(process.env.QUESTLOG_CARD ?? 0);

const HUB = 910000000;

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
};
const probe = (driver, name) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const questlogActive = driver => probe(driver, 'msui_questlog_active');
const questlogRows = driver => probe(driver, 'msui_questlog_rows');
const questlogSelected = driver => probe(driver, 'msui_questlog_selected');
const questlogForfeitArmed = driver => probe(driver, 'msui_questlog_forfeit_armed');

// 列表行命中区：PosQUEST 默认 (150,120)（Configuration.h）+
// LIST_OFFSET{14,70}（UIQuestLog.h）+ 行高 24（QuestLogModel.h）。
// 第一行中心 = (150+14+80, 120+70+12) = (244,202)，第二行 y+24。
const LIST_FIRST_ROW = { x: 244, y: 202 };
const LIST_ROW_STRIDE = 24;

async function wheelAt(driver, gameX, gameY, deltaY) {
  const point = await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const bounds = canvas.getBoundingClientRect();
    return {
      x: bounds.left + ${gameX} * bounds.width / canvas.width,
      // scene renders VIEWYOFFSET(10) lower than design space (see cdp.mjs)
      y: bounds.top + (${gameY} + 10) * bounds.height / canvas.height
    };
  })()`);
  await driver.send('Input.dispatchMouseEvent', {
    type: 'mouseMoved', x: point.x, y: point.y, button: 'none'
  });
  await driver.send('Input.dispatchMouseEvent', {
    type: 'mouseWheel', x: point.x, y: point.y, button: 'none',
    deltaX: 0, deltaY
  });
}

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

async function pressQuestLogKey(driver) {
  // 发送的是 GLFW keycode Q=81；DB keymap 的 `key` 列是客户端 Keytable
  // 的 MS 扫描码下标（Q=16），绑定须写成 16→4/8（MENU/QUESTLOG）——
  // 误写 81 实际落到 PAGE_DOWN 槽位，Q 不会开窗（见文件头注释）。
  await driver.key('q', 'KeyQ', 81);
}

async function testQuestLog(driver) {
  await pressQuestLogKey(driver);
  const opened = await waitFor(async () => await questlogActive(driver) === 1, 8);
  report(opened, 'Q 打开任务日志（msui_questlog_active=1）');
  if (!opened) throw new Error(`任务日志未打开：active=${await questlogActive(driver)}`);
  await settleShot(driver, 'questlog-open');

  const rows = await questlogRows(driver);
  report(Number.isInteger(rows) && rows >= 0,
    `任务日志行数为非负整数（rows=${rows}）`);

  if (rows > 0) {
    const initial = await questlogSelected(driver);
    if (initial === -1) {
      await driver.click(LIST_FIRST_ROW.x, LIST_FIRST_ROW.y);
      const picked = await waitFor(
        async () => await questlogSelected(driver) !== -1, 8);
      report(picked,
        `点击第一行选中任务（selected=${await questlogSelected(driver)}）`);
    } else if (rows > 1) {
      // 打开时已自动选中首行：点第二行断言选中切换。
      await driver.click(LIST_FIRST_ROW.x, LIST_FIRST_ROW.y + LIST_ROW_STRIDE);
      const second = await questlogSelected(driver);
      report(second !== initial && second >= 0,
        `点击第二行切换选中（${initial} → ${second}）`);
    } else {
      report(true, `单行时首行已选中（selected=${initial}）`);
    }
    await settleShot(driver, 'questlog-selected');

    // 放弃按钮探针：进行中 Tab 选中行后应可用（0=禁用/1=可用）。
    const armed = await questlogForfeitArmed(driver);
    report(armed === 1 || armed === 0,
      `放弃按钮探针可达（armed=${armed}）`);
    report(armed === 1, '选中进行中任务后放弃按钮可用');

    if (rows > 9) {
      // 滚轮用例：向列表中部发 wheel（deltaY<0 → offset+1）；无 offset
      // 探针，仅断言窗口仍 active 并留截图。
      await wheelAt(driver, LIST_FIRST_ROW.x, LIST_FIRST_ROW.y + 4 * LIST_ROW_STRIDE, -120);
      await wheelAt(driver, LIST_FIRST_ROW.x, LIST_FIRST_ROW.y + 4 * LIST_ROW_STRIDE, -120);
      report(await questlogActive(driver) === 1, '滚轮滚动后窗口保持打开');
      await settleShot(driver, 'questlog-scrolled');
    } else {
      console.log(`INFO  rows=${rows} ≤ 9，跳过滚轮用例`);
    }
  }

  await pressQuestLogKey(driver);
  const toggledOff = await waitFor(async () => await questlogActive(driver) === 0, 8);
  report(toggledOff,
    `再次按 Q 翻转关闭（TOGGLED，实测 active=${await questlogActive(driver)}）`);

  await pressQuestLogKey(driver);
  const reopened = await waitFor(async () => await questlogActive(driver) === 1, 8);
  report(reopened, '第三次按 Q 重新打开任务日志');
  await driver.key('Escape', 'Escape', 27);
  const escaped = await waitFor(async () => await questlogActive(driver) === 0, 8);
  report(escaped,
    `Escape 走 escape 分支关闭（实测 active=${await questlogActive(driver)}）`);
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    const map = await probe(driver, 'msmap_id');
    report(map === HUB, `登录地图 = 910000000（实测 ${map}）`);
    if (map !== HUB) throw new Error(`登录地图不是自由市场：${map}`);

    await testQuestLog(driver);
    await settleShot(driver, 'questlog-closed');
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
  console.log(`FAIL  QUESTLOG_CARD must be 0..3, got ${process.env.QUESTLOG_CARD}`);
  process.exitCode = 1;
} else await main();
