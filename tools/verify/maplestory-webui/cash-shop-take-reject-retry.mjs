// 现金商城「取出被拒 → 超时解锁 → 清背包重试」+ 传统状态栏聊天栏默认收起
// 的浏览器专项验收。
//
// 修复的两个回归点：
//   1) 取出（TAKE）被服务端拒绝后客户端不得把商城锁死。server patch 0005
//      后装备类取出落 CASH 栏，满栏时回 0x144（enableCSUse）→ 修复1b 即时
//      弹错解锁；旧服静默不回包 → 修复1a 超时解锁兜底。两路都要求
//      require_reenter=0、can_mutate=1、弹窗可关、仓库物品原位保留；清空
//      CASH 栏重进商城后重试可成功取出（用户场景闭环）。
//      满栏构造：ItemFactory 以 (byte)position 读档，128 号位加载即溢出，
//      DB 只能预填 CASH 1..127（5210000 券）；仓库第 1 顶帽子取出落 128 号
//      位补满，第 2 顶取出验证拒绝。帽子由 DB 预插账号级仓库（type=3 行），
//      不走 SN 购买（复购结算在实验服上偶发 >90s 延迟，见 known-issues）。
//   2) 状态栏聊天栏默认收起：进图后无聚焦输入框、不再有黑色日志盒穿透
//      仓库/任务窗口底部；点开/Enter 打开、发送后自动收起（Enter 打开路径
//      由 run.mjs 的 E2E_IME_CHAT 覆盖，此处用打开按钮绕开 CDP 丢键）。
//
// 断言使用客户端只读探针：
//   msime_field_focused / msui_state / msui_enabled / msui_notice_active /
//   mscashshop_locker_count / mscashshop_inventory_count / mscashshop_nx_credit /
//   mscashshop_selected_right_row / mscashshop_pending / mscashshop_pending_op /
//   mscashshop_require_reenter / mscashshop_can_mutate
//
// 阶段 A（聊天）先行，B-D（商城）任一阶段失败仍继续汇报（REJECT_FAIL_FAST=1
// 时立即中止）。B 需先跑 DB 预填（1..127 券，见文件头）并重启游戏服。
// Run: CHROME_BIN=/usr/bin/chromium-browser node \
//   tools/verify/maplestory-webui/cash-shop-take-reject-retry.mjs

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.REJECT_DEBUG_PORT || 9259);
const pageUrl = process.env.REJECT_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.REJECT_ACCOUNT || 'test1';
const password = process.env.REJECT_PASSWORD || 'test1';
const cardIndex = Number(process.env.REJECT_CARD || 1);
const itemId = Number(process.env.REJECT_ITEM_ID || 1002000);
const failFast = process.env.REJECT_FAIL_FAST === '1';

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });
const rightGrid = Object.freeze({ x: 517, y: 88, pitch: 39, hit: 18 });
const pagerNext = Object.freeze({ x: 488, y: 557 });
const chatfieldCenter = Object.freeze({ x: 283, y: 581 });
const rightPageSize = 21;
const charCard = index => ({ x: 105 + 120 * index + 25, y: 210 });

let failed = false;
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
// A busy renderer can stall a Runtime.evaluate past the 45s CDP timeout
// (e.g. mid asset-stream); retry once before giving up so one hiccup does
// not abort the phase.
async function runPhase(label, body) {
  console.log(`--- phase ${label} ---`);
  try {
    await body();
  } catch (error) {
    report(false, `phase ${label} aborted: ${error.message}`);
    throw error;
  }
}
async function probe(driver, name, args = []) {
  const call = () => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', ${JSON.stringify(args.map(() => 'number'))},${JSON.stringify(args)})`);
  try { return await call(); } catch { await sleep(2000); }
  try { return await call(); } catch { return -1; }
}
function uiStateIs(driver, expected) {
  return () => probe(driver, 'msui_state').then(value => value === expected);
}
function assetsIdle(driver) {
  let hiddenSince = 0;
  return async () => {
    const hidden = await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
    if (!hidden || await probe(driver, 'msasset_blocking') !== 0) {
      hiddenSince = 0;
      return false;
    }
    if (!hiddenSince) hiddenSince = Date.now();
    return Date.now() - hiddenSince >= 1000;
  };
}
async function waitFor(predicate, seconds = 30) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    try { if (await predicate()) return true; } catch { /* transient stall */ }
    await sleep(250);
  }
  return false;
}
async function requireState(name, predicate, seconds = 30) {
  if (!await waitFor(predicate, seconds)) throw new Error(`Timed out: ${name}`);
  console.log(`PASS  ${name}`);
}

async function loginAndEnterGame(driver) {
  await requireState('WASM module initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await requireState('login UI active', uiStateIs(driver, uiState.login), 30);
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  await driver.clickField(340, 285);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  await requireState('world-select UI active', uiStateIs(driver, uiState.world), 45);
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  await requireState('character-select UI active', uiStateIs(driver, uiState.characters), 45);
  await requireState('character-select assets ready', assetsIdle(driver), 60);
  const center = charCard(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error('game field did not activate');
  await requireState('game assets ready', assetsIdle(driver), 90);
}

async function enterCashShop(driver) {
  // 商城入口按钮随 HUD 世代移动（新版 (597,571)，传统 v83 (562,583)）。
  let active = false;
  for (const [x, y] of [[597, 571], [562, 583]]) {
    await driver.click(x, y);
    active = await waitFor(uiStateIs(driver, uiState.cashShop), 15);
    if (active) break;
  }
  if (!active) throw new Error('cash-shop UI did not activate');
  console.log('PASS  cash-shop UI active');
  await requireState('cash-shop assets ready', assetsIdle(driver), 60);
}

async function openChat(driver) {
  if (await probe(driver, 'mschatbar_open') === 1
    && await probe(driver, 'msime_field_focused') === 1) return true;
  const bx = await probe(driver, 'mschatbar_openbutton_x');
  const by = await probe(driver, 'mschatbar_openbutton_y');
  for (let i = 0; i < 6; i += 1) {
    if (await probe(driver, 'mschatbar_open') !== 1) {
      if (bx !== -32768) {
        // Post-chaos bar: the button toggles, click it while collapsed.
        await driver.click(bx, by);
      } else {
        // Pre-chaos (v83) bar has no open button; Enter focuses the field.
        await driver.key('Enter', 'Enter', 13);
      }
      await sleep(400);
    }
    if (await probe(driver, 'mschatbar_open') === 1
      && await probe(driver, 'msime_field_focused') !== 1) {
      // The log may be up with the field NORMAL; a second Enter hands the
      // input row the page IME (fallback: click the pre-chaos input row).
      await driver.key('Enter', 'Enter', 13);
      await sleep(300);
      if (await probe(driver, 'msime_field_focused') !== 1
        && bx === -32768) {
        await driver.click(chatfieldCenter.x, chatfieldCenter.y);
      }
    }
    for (let j = 0; j < 4; j += 1) {
      if (await probe(driver, 'mschatbar_open') === 1
        && await probe(driver, 'msime_field_focused') === 1) return true;
      await sleep(250);
    }
  }
  return await probe(driver, 'mschatbar_open') === 1
    && await probe(driver, 'msime_field_focused') === 1;
}

async function sendChat(driver, text) {
  if (!await openChat(driver)) {
    throw new Error('chat field never opened (open-button clicks kept missing)');
  }
  await driver.compose(text);
  await requireState('chat text entered', () => driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(text)}`), 5);
  // Send: Enter goes through the DOM textarea here, but stay drop-tolerant.
  for (let i = 0; i < 6; i += 1) {
    await driver.key('Enter', 'Enter', 13);
    let done = false;
    for (let j = 0; j < 4 && !done; j += 1) {
      done = await probe(driver, 'msime_field_focused') === 0;
      if (!done) await sleep(250);
    }
    if (done) break;
  }
  await sleep(200);
}

// v83 take-out: double-click the locker slot; older builds only wire the
// explicit transfer button, so fall back to clicking it (680,239 64x16).
// The client settles a take in well under a second, so sample fast:
// pending==1 OR can_mutate==0 (pending locks mutation) proves dispatch,
// and a locker count drop proves the take also settled server-side.
async function sampleDispatch(driver, ms) {
  let sawPending = false, sawTakeOp = false, pend = -1;
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    pend = await probe(driver, 'mscashshop_pending');
    if (pend === 1) {
      sawPending = true;
      if (await probe(driver, 'mscashshop_pending_op') === 2) sawTakeOp = true;
    }
    if (sawPending && pend === 0) break;
    await sleep(120);
  }
  return { sawPending, sawTakeOp };
}

async function takeFromLocker(driver, index) {
  console.log(`[take] can_mutate=${await probe(driver, 'mscashshop_can_mutate')} require_reenter=${await probe(driver, 'mscashshop_require_reenter')} balances_ready=${await probe(driver, 'mscashshop_balances_ready')} nx=${await probe(driver, 'mscashshop_nx_credit')} cashid=${await probe(driver, 'mscashshop_selected_right_cash_id')}`);
  const lockerBefore = await probe(driver, 'mscashshop_locker_count');
  const point = lockerSlotPoint(index % rightPageSize);
  // Window.cpp synthesizes an in-game doubleclick from any two presses
  // <=0.35s apart at the same spot. The selection press must therefore be
  // separated from the doubleclick pair, and the pair itself must be tight
  // (~80ms): sleep first, then use the driver's fast doubleClick. Otherwise
  // the pair's first press joins the SELECTION press into a stray doubleclick
  // that fires the take early, and the pair's second press fires a SECOND
  // doubleclick after the take reply switched the pane — putting the item
  // straight back into the locker.
  await sleep(500);
  await driver.doubleClick(point.x, point.y);
  const fast = await sampleDispatch(driver, 4000);
  if (fast.sawPending)
    return { via: 'dblclick', ...fast };
  const dropped = await probe(driver, 'mscashshop_locker_count') === lockerBefore - 1;
  if (dropped)
    return { via: 'settled', sawTakeOp: fast.sawTakeOp, sawPending: false };
  // TRANSFER button fallback: selection from the first press persists.
  await driver.click(712, 247);
  const viaButton = await sampleDispatch(driver, 4000);
  if (viaButton.sawPending)
    return { via: 'button', ...viaButton };
  return { via: 'none', sawTakeOp: viaButton.sawTakeOp, sawPending: false };
}

const lockerSlotPoint = index => ({
  x: rightGrid.x + (index % 7) * rightGrid.pitch + rightGrid.hit,
  y: rightGrid.y + Math.floor(index / 7) * rightGrid.pitch + rightGrid.hit
});

// 重进商城后仓库列表整包重载、选中被复位；新购物品按服务端 append 顺序
// 落在传入 index（页码超出时先翻页），点击该格恢复选中。
async function selectLockerSlot(driver, index) {
  const page = Math.floor(index / rightPageSize);
  for (let i = 0; i < page; i += 1) await driver.click(pagerNext.x, pagerNext.y);
  const point = lockerSlotPoint(index % rightPageSize);
  await driver.click(point.x, point.y);
  await requireState(`locker slot ${index} selected`, async () =>
    await probe(driver, 'mscashshop_selected_right_row') === index % rightPageSize, 10);
}

// A：状态栏聊天栏默认收起（修复 2）。亮度采样在暗色地图上没有区分度
// （test1 出生图 40001 本身近黑），所以用 mschatbar_open / IME 探针做
// 判定，截图留档供人工核对。
async function phaseA(driver) {
  report(await probe(driver, 'msime_field_focused') === 0
    && await probe(driver, 'mschatbar_open') === 0,
    'chat bar starts collapsed (closed, no focused field)');
  await driver.screenshot('a0-chat-collapsed');
  report(await openChat(driver), 'chat open button opens the chat input');
  await driver.screenshot('a1-chat-open');
  await sendChat(driver, 'hello');
  await driver.screenshot('a2-chat-after-send');
  report(await probe(driver, 'mschatbar_open') === 0
    && await probe(driver, 'msime_field_focused') === 0,
    'chat bar folds and releases the IME after sending');
}

// B：装满 CASH 栏（server patch 0005 后装备类取出目标栏是 CASH）。
// ItemFactory 以 (byte)position 读档，128 号位加载即溢出丢弃 → DB 最多
// 预填 1..127；128 号位由 phase C 的第一件取出补上。
async function phaseB(driver) {
  let final = 0;
  if (process.env.REJECT_FILL_DB !== '0') {
    final = await waitFor(async () =>
      await probe(driver, 'msinv_count', [5]) >= 100, 20) ? 100 : 0;
  }
  if (final >= 100) console.log('PASS  cash inventory pre-filled from DB (1..127)');
  report(final >= 100, `cash inventory pre-filled (${final}/100+ visible slots)`);
  if (final < 100) throw new Error('cash inventory not pre-filled; reject scenario impossible');
}

let shopBefore = null;

// C：CASH 栏满后取出被拒（修复 1 上半）。两顶帽子由 DB 预插到账号级
// 仓库（type=3 行，插入前仓库存量 3 件 → 帽子落在第 1、3 行）。
async function phaseC(driver) {
  await enterCashShop(driver);
  const before = {
    locker: await probe(driver, 'mscashshop_locker_count'),
    inventory: await probe(driver, 'mscashshop_inventory_count'),
    nx: await probe(driver, 'mscashshop_nx_credit')
  };
  shopBefore = before;
  const capRowFirst = Number(process.env.REJECT_CAP_ROW_FIRST || 1);
  const capRowSecond = Number(process.env.REJECT_CAP_ROW_SECOND || 3);
  report(before.locker >= capRowSecond + 1,
    `locker holds both preset caps (count=${before.locker}, rows ${capRowFirst}/${capRowSecond})`);
  if (before.locker < capRowSecond + 1) {
    throw new Error('preset caps missing from locker; DB setup skipped?');
  }
  // 第 1 件：DB 预填只能占 1..127，这件取出落 128 号位把 CASH 栏真正填满。
  await selectLockerSlot(driver, capRowFirst);
  const take1 = await takeFromLocker(driver, capRowFirst);
  report(take1.via !== 'none', `first take dispatched (via ${take1.via})`);
  report(!take1.sawPending || take1.sawTakeOp, 'take pending op was TAKE (when sampled)');
  // Trace the whole resolution: one slow server round-trip should clear
  // pending, drop the locker by one and raise the CASH count by one.
  const trace = [];
  let filled = false;
  for (let i = 0; i < 40 && !filled; i += 1) {
    const t = {
      pend: await probe(driver, 'mscashshop_pending'),
      op: await probe(driver, 'mscashshop_pending_op'),
      inv: await probe(driver, 'mscashshop_inventory_count'),
      locker: await probe(driver, 'mscashshop_locker_count')
    };
    trace.push(`${(i * 0.5).toFixed(1)}s pend=${t.pend} op=${t.op} inv=${t.inv} locker=${t.locker}`);
    filled = t.pend === 0 && t.locker === before.locker - 1 && t.inv === before.inventory + 1;
    if (!filled) await sleep(500);
  }
  console.log(trace.join('\n'));
  if (!filled) throw new Error('first take did not fill the last cash slot (see trace)');
  console.log('PASS  cash inventory now full (slot 128 taken)');
  // 第 2 件：满栏取出被拒绝。取出成功会切到 INVENTORY 页并把仓库行序
  // 前移一格：先回 LOCKER 页签，再选第二顶帽子（行号 -1）。
  await driver.click(579, 43);
  await requireState('locker tab active again', async () =>
    await probe(driver, 'mscashshop_locker_count') === 3, 10);
  await selectLockerSlot(driver, capRowSecond - 1);
  const take2 = await takeFromLocker(driver, capRowSecond - 1);
  report(take2.via !== 'none', `second take dispatched (via ${take2.via})`);
  report(!take2.sawPending || take2.sawTakeOp, 'take pending op was TAKE (when sampled)');
  // 拒绝双路径：服务器满栏时回 0x144（enableCSUse → 修复1b 即时报错解锁），
  // 或旧服静默不回包（修复1a 超时解锁）。两者 pending 都必须清零。
  await requireState('reject resolves pending (server reply or timeout)', async () =>
    await probe(driver, 'mscashshop_pending') === 0, 20);
  const inventoryAfter = await probe(driver, 'mscashshop_inventory_count');
  if (inventoryAfter !== before.inventory + 1) {
    throw new Error(`unexpected inventory count ${inventoryAfter} (want ${before.inventory + 1})`);
  }
  report(await probe(driver, 'mscashshop_require_reenter') === 0,
    'reject does not lock the shop (require_reenter=0)');
  report(await probe(driver, 'mscashshop_can_mutate') === 1,
    'buttons stay live after reject (can_mutate=1)');
  await sleep(600);
  report(await probe(driver, 'msui_notice_active') === 1,
    'reject error dialog is visible');
  await driver.key('Enter', 'Enter', 13);
  await requireState('reject error dialog dismissed', async () =>
    await probe(driver, 'msui_notice_active') === 0, 5);
  report(await probe(driver, 'mscashshop_locker_count') === before.locker - 1,
    'rejected take kept the item in the locker');
  await driver.screenshot('c1-after-reject');
}

// D：清 CASH 栏后重试成功（修复 1 下半，用户场景闭环）。!clearslot cash
// 清 1..100（穿戴覆盖位 101+ 不动），重试取出应成功。
async function phaseD(driver) {
  if (!shopBefore) throw new Error('phase C did not capture the shop snapshot; retry unverifiable');
  const before = shopBefore;
  await driver.key('Escape', 'Escape', 27);
  await requireState('cash shop exits back to game', uiStateIs(driver, uiState.game), 30);
  await requireState('returned map assets ready', assetsIdle(driver), 60);
  await requireState('game UI enabled', async () =>
    await probe(driver, 'msui_enabled') === 1, 60);
  await sendChat(driver, '!clearslot cash');
  await sleep(1200);
  // !clearslot is a GM command: with gm=0 it prints as plain chat and the
  // CASH tab stays full, so the retry below is rejected for the wrong reason
  // and the success poll can never converge. Fail fast instead.
  const cashLeft = await probe(driver, 'msinv_count', [5]);
  report(cashLeft < 100,
    `clearslot freed the CASH tab (${cashLeft} slots left)`);
  if (cashLeft >= 100)
    throw new Error('!clearslot cash had no effect — is the character GM?');
  await enterCashShop(driver);
  report(await probe(driver, 'mscashshop_locker_count') === before.locker - 1,
    'item still in locker after re-enter');
  // take#1 移除了 row1 的旧帽子 → 第二顶帽子前移到 row 2。
  let row2 = await probe(driver, 'mscashshop_selected_right_row');
  if (row2 < 0) await selectLockerSlot(driver, 2);
  row2 = await probe(driver, 'mscashshop_selected_right_row');
  report(row2 >= 0, `locker row selected for retry (row=${row2})`);
  if (row2 < 0) throw new Error('locker row not selected after re-enter');
  const preRetry = {
    inventory: await probe(driver, 'mscashshop_inventory_count')
  };
  const point = lockerSlotPoint(row2);
  // Same press-isolation as takeFromLocker: the row-selection press must not
  // synthesize a stray doubleclick with the retry pair's first press.
  await sleep(500);
  await driver.doubleClick(point.x, point.y);
  await requireState('retry after recovery takes the item', async () => {
    // A synthesized second take after the CASH tab refills gets rejected and
    // raises the error dialog; dismiss it and re-sample — the retry itself
    // already succeeded at that point.
    if (await probe(driver, 'msui_notice_active') === 1) {
      await driver.key('Enter', 'Enter', 13);
      await sleep(300);
    }
    return await probe(driver, 'mscashshop_pending') === 0 &&
      await probe(driver, 'mscashshop_locker_count') === before.locker - 2 &&
      await probe(driver, 'mscashshop_inventory_count') === preRetry.inventory + 1;
  }, 60);
  await driver.screenshot('d1-retry-taken');
  await driver.key('Escape', 'Escape', 27);
  await requireState('cash shop exits back to game (final)', uiStateIs(driver, uiState.game), 30);
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    // Frame capture from document start: wrap the WebSocket constructor so
    // both directions of the game socket (:8090) are hex-logged. LazyFS
    // traffic (:8765) is skipped.
    await driver.send('Page.addScriptToEvaluateOnNewDocument', { source: `
      (function () {
        const hex = u8 => Array.from(u8).map(b => b.toString(16).padStart(2, '0')).join(' ');
        const OrigWS = WebSocket;
        window.WebSocket = function (url, protocols) {
          const ws = protocols === undefined ? new OrigWS(url) : new OrigWS(url, protocols);
          if (String(url).includes(':8090')) {
            const orig = ws.send.bind(ws);
            ws.send = d => { try { const v = d instanceof Blob ? null : new Uint8Array(d.buffer || d, d.byteOffset || 0, d.byteLength); if (v) console.log('[G-SEND] ' + hex(v)); } catch (e) {} return orig(d); };
            ws.addEventListener('message', ev => { try {
              const raw = ev.data;
              const dump = b => console.log('[G-RECV] ' + hex(new Uint8Array(b)));
              if (raw instanceof Blob) raw.arrayBuffer().then(dump); else dump(raw.buffer || raw);
            } catch (e) {} });
          }
          return ws;
        };
        window.WebSocket.prototype = OrigWS.prototype;
      })();` });
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await requireState('game UI enabled', async () =>
      await probe(driver, 'msui_enabled') === 1, 60);
    // Debug aid: log outgoing game-protocol frames so artifact console
    // dumps show exactly which cash-shop ops the client sent.
    await driver.evaluate(`(function(){ const o = WebSocket.prototype.send;
      const hex = u8 => Array.from(u8).map(b=>b.toString(16).padStart(2,'0')).join(' ');
      WebSocket.prototype.send = function(d){ try {
        if (d instanceof Blob) {
          d.arrayBuffer().then(b => console.log('[WS-SEND blob] ' + hex(new Uint8Array(b))));
        } else {
          console.log('[WS-SEND] ' + hex(new Uint8Array(d.buffer || d, d.byteOffset || 0, d.byteLength)));
        }
      } catch(e){ console.log('[WS-SEND err] ' + e.message); } return o.call(this, d); }; })()`);
    await runPhase('A', () => phaseA(driver));
    await runPhase('B', () => phaseB(driver));
    await runPhase('C', () => phaseC(driver));
    await runPhase('D', () => phaseD(driver));
  } catch (error) {
    report(false, `cash shop take-reject-retry flow aborted: ${error.message}`);
    console.error(error);
  } finally {
    if (driver.errors.length) {
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    }
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  REJECT_CARD must be a card index 0..3, got ${process.env.REJECT_CARD}`);
  process.exitCode = 1;
} else {
  await main();
}
