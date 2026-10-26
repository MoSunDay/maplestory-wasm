// 商城「确认弹窗输入隔离」验收：A 弹窗开启时双击弹窗外储物格不转移；B 弹窗外滚轮不翻页；
// C 选中项翻页离开后 Enter 不弹确认，可见选中 Enter 仍弹确认、Escape 取消不落账；
// D Maple Points 余额 0 时确认购买弹客户端余额不足提示，pending 保持 0、不落账。
// 前置：WASM 含 UIStateCashShop 模态守卫，web-server/ws-proxy/assets-server 在跑；截图 01-06 存
//   MODAL_GUARDS_ARTIFACT_DIR（默认 /tmp/cash-shop-modal-guards）。
// Run: CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-modal-guards.mjs
// Run(快跑 D): MODAL_GUARDS_BALANCE_ONLY=1 node tools/verify/maplestory-webui/cash-shop-modal-guards.mjs
//   —— 只跑 D 余额不足分支，跳过储物格初始化 / 滚轮校准 / A、B、C 守卫，不改动余额数据。
// Env: MODAL_GUARDS_BALANCE_ONLY(=1 仅跑 D) / MODAL_GUARDS_ACCOUNT / MODAL_GUARDS_PASSWORD / MODAL_GUARDS_CARD / MODAL_GUARDS_ARTIFACT_DIR / CHROME_BIN / MODAL_GUARDS_DEBUG_PORT / MODAL_GUARDS_URL / MS_SERVER_CONFIG / MS_UI_ELEMENT_HEADER

import fs from 'fs';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';
import { createRuntimeProbes } from './runtime-flow.mjs';

const artifactDir = process.env.MODAL_GUARDS_ARTIFACT_DIR || '/tmp/cash-shop-modal-guards';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.MODAL_GUARDS_DEBUG_PORT || 9269);
const pageUrl = process.env.MODAL_GUARDS_URL || 'http://127.0.0.1:8000/web/index.html';
const configPath = process.env.MS_SERVER_CONFIG || '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const uiElementHeader = process.env.MS_UI_ELEMENT_HEADER || '/data/maplestory-wasm/src/client/IO/UIElement.h';
const account = process.env.MODAL_GUARDS_ACCOUNT || 'test1', password = process.env.MODAL_GUARDS_PASSWORD || 'test1', cardIndex = Number(process.env.MODAL_GUARDS_CARD ?? 0), CHEAP_SN = '80000002';
const balanceOnly = process.env.MODAL_GUARDS_BALANCE_ONLY === '1';
const CATALOG_PAGE = 12, PAGE1_BASE = 0, PAGE2_BASE = CATALOG_PAGE;
const SEARCH_BOX = [300, 50], SEARCH_CLEAR = [480, 46];
const LOCKER_TAB = [579, 43], SLOT0_POINT = [550, 106], NOTICE_POINT = [400, 240];
// D 用：左栏「全部」分类行 (CATEGORY_POS{4,58} + CATEGORY_SIZE{90,22})、
// 币种第 1 格 Maple Points (CURRENCY_POS{100,64} + 1 * 52, CURRENCY_SIZE{48,16})、
// 装备子菜单关闭行 (EQSUB_CLOSE_POS{4,36} + 90x22)，避免子菜单盖住分类栏。
const ALL_CATEGORY = [49, 69], MAPLE_POINT_CHIP = [176, 72], EQSUB_CLOSE = [49, 47];
const pager = Object.freeze({ next: [488, 557], prev: [115, 557] });
const cardCenter = v => ({ x: 100 + (v % 4) * 101 + 42, y: 88 + Math.floor(v / 4) * 148 + 40 });
const WHEEL_POINT = Object.freeze(cardCenter(0));
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });

// msui_front_at 返回 UIElement::Type 序号（67/81 实为 UIElement.h 行号）；按头文件解析真实序号，兼容显式 = N。
function frontTypeOrdinals(file) {
  const body = /enum Type\s*\{([\s\S]*?)\}/.exec(fs.readFileSync(file, 'utf8'));
  if (!body) throw new Error(`UIElement::Type enum not found in ${file}`);
  const ord = {}; let next = 0;
  for (const entry of body[1].replace(/\/\/[^\n]*/g, '').split(',')) {
    const [name, value] = entry.split('=').map(part => part.trim());
    if (!name) continue;
    if (value !== undefined) next = Number(value);
    ord[name] = next; next += 1;
  }
  if (ord.NOTICE === undefined || ord.CASHSHOP === undefined) throw new Error(`NOTICE/CASHSHOP missing in ${file}`);
  return { notice: ord.NOTICE, cashShop: ord.CASHSHOP };
}
const FRONT = frontTypeOrdinals(uiElementHeader);

let failed = false, passed = 0;
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (ok) passed += 1; else failed = true;
}
async function waitFor(predicate, seconds = 30) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return await predicate();
}
async function requireState(name, predicate, seconds = 30) {
  if (!await waitFor(predicate, seconds)) throw new Error(`Timed out: ${name}`);
  report(true, name);
}
const stateIs = (read, expected) => async () => await read() === expected;
const uiStateIs = (driver, probes, expected) => () => probes.currentUiState().then(value => value === expected);
function assetsIdle(driver, probes) {
  let hiddenSince = 0;
  return async () => {
    const hidden = await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
    if (!hidden || await probes.assetBlocking() !== 0) { hiddenSince = 0; return false; }
    if (!hiddenSince) hiddenSince = Date.now();
    return Date.now() - hiddenSince >= 1000;
  };
}

// DB 预备（pet-ui-interactions 同款访问方式，仅重置登录态，不硬编码凭据）
function readDbConfig(file) {
  const values = Object.fromEntries(fs.readFileSync(file, 'utf8').split(/\r?\n/)
    .filter(line => line.indexOf('=') > 0)
    .map(line => [line.slice(0, line.indexOf('=')).trim(), line.slice(line.indexOf('=') + 1).trim()]));
  const match = /\/\/([^/:]+)(?::(\d+))?\/([^?]+)/.exec(values.URL || '');
  return { host: match?.[1] || '127.0.0.1', port: match?.[2] || '3306',
    database: match?.[3] || 'maplestory', user: values.DB_USER || 'root', password: values.DB_PASS || '' };
}
function mysqlRun(db, sql) {
  return execFileSync('mysql', ['-h', db.host, '-P', db.port, '-u', db.user, db.database,
    '--batch', '--skip-column-names', '-e', sql],
  { encoding: 'utf8', env: { ...process.env, MYSQL_PWD: db.password }, stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}
function prepareDatabase() {
  mysqlRun(readDbConfig(configPath), `UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  report(true, `DB: account ${account} loggedin reset`);
}

// 登录链（pet-ui-interactions 同款，含登录公告 Enter 重试）
async function loginAndEnterGame(driver, probes) {
  await requireState('WASM module initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await requireState('login UI active', uiStateIs(driver, probes, uiState.login), 30);
  let advanced = false;
  for (let attempt = 0; attempt < 4 && !advanced; attempt += 1) {
    await driver.clickField(340, 260); await driver.typeAscii(account);
    await driver.clickField(340, 285); await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () =>
      [uiState.world, uiState.characters].includes(await probes.currentUiState()), 20);
    if (!advanced && await probes.loginNoticeActive() === 1) { await driver.key('Enter', 'Enter', 13); await sleep(2000); }
  }
  if (!advanced) throw new Error('login never advanced past the login UI');
  await driver.retryClick(510, 216, uiStateIs(driver, probes, uiState.characters));
  console.log('DIAG char-select', await probes.currentUiState(), await driver.evaluate("['mscharlist_phase','mscharlist_character_index','mscharlist_remaining_bytes'].map(n => Module.ccall(n,'number',[],[]))").catch(() => 'n/a'));
  await requireState('character-select UI active', uiStateIs(driver, probes, uiState.characters), 45);
  await requireState('character-select assets ready', assetsIdle(driver, probes), 60);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(105 + 120 * cardIndex + 25, 210);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await probes.currentUiState() === uiState.game;
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error('game field did not activate');
  await requireState('game assets ready', assetsIdle(driver, probes), 90);
}
async function enterCashShop(driver, probes) {
  // 商城入口随 HUD 世代移动：新版 (597,571)，传统 v83 (562,583)。
  let active = false;
  for (const [x, y] of [[597, 571], [562, 583]]) {
    await driver.click(x, y);
    active = await waitFor(uiStateIs(driver, probes, uiState.cashShop), 15);
    if (active) break;
  }
  if (!active) throw new Error('cash-shop UI did not activate');
  await requireState('cash-shop assets ready', assetsIdle(driver, probes), 60);
}

// cdp.mjs 没有滚轮助手：先 mouseMoved 更新游戏内光标，再 mouseWheel（CSS 像素换算同 click）。
async function mouseWheel(driver, gameX, gameY, deltaY) {
  const point = await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const bounds = canvas.getBoundingClientRect();
    return { x: bounds.left + ${gameX} * bounds.width / canvas.width,
      y: bounds.top + ${gameY} * bounds.height / canvas.height };
  })()`);
  await driver.send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: point.x, y: point.y, button: 'none' });
  await driver.send('Input.dispatchMouseEvent', {
    type: 'mouseWheel', x: point.x, y: point.y, button: 'none', deltaX: 0, deltaY
  });
}
async function expectFront(driver, label, x, y, name, expected) {
  const actual = await driver.evaluate(`Module.ccall('msui_front_at','number',['number','number'],[${x},${y}])`);
  if (actual !== expected)
    throw new Error(`${label}: (${x},${y}) front=${actual}, expected ${name}(${expected}); covered by a modal`);
}
async function snapshot(cash) {
  return { pending: await cash('mscashshop_pending'), locker: await cash('mscashshop_locker_count'),
    inventory: await cash('mscashshop_inventory_count'), nx: await cash('mscashshop_nx_credit') };
}
const stateText = s => `pending=${s.pending} locker=${s.locker} inv=${s.inventory} nx=${s.nx}`;
const sameState = (a, b) => stateText(a) === stateText(b);
const requireNoPending = (state, label) => { if (state.pending !== 0) throw new Error(`${label}: expected pending 0, got ${state.pending}`); };
async function dismissNotice(driver, cash, label) {
  if (await cash('msui_notice_active') !== 1) return;
  await driver.key('Escape', 'Escape', 27);
  await requireState(`${label}: Escape dismisses the confirmation`, stateIs(() => cash('msui_notice_active'), 0), 10);
  if (!await waitFor(stateIs(() => cash('mscashshop_pending'), 0), 15))
    throw new Error(`${label}: pending did not return to 0 after dismissing the confirmation`);
  await sleep(250);
}
// 无页基探针时读左栏页基：点第 0 张卡；弹出确认说明点击前已选中，点击前 selected_left 即页基。
async function leftPageBase(driver, cash, label) {
  await requireState(`${label}: no modal before page probe`, stateIs(() => cash('msui_notice_active'), 0), 10);
  await requireState(`${label}: no pending transfer`, stateIs(() => cash('mscashshop_pending'), 0), 10);
  const before = await cash('mscashshop_selected_left');
  const card = cardCenter(0);
  await driver.click(card.x, card.y);
  await sleep(350);
  let base = await cash('mscashshop_selected_left');
  if (await cash('msui_notice_active') === 1) {
    await driver.key('Escape', 'Escape', 27);
    await requireState(`${label}: page probe confirmation dismissed`,
      stateIs(() => cash('msui_notice_active'), 0), 10);
    base = before;
  }
  if (base < 0 || base % CATALOG_PAGE !== 0)
    throw new Error(`${label}: page-base probe=${base}, expected >=0 and %${CATALOG_PAGE}===0`);
  return base;
}
async function ensurePageBase(driver, probes, cash, targetBase, label) {
  for (let attempt = 0; attempt < 6; attempt += 1) {
    const base = await leftPageBase(driver, cash, `${label} step ${attempt}`);
    if (base === targetBase) return base;
    const step = base < targetBase ? pager.next : pager.prev;
    await driver.click(step[0], step[1]);
    await sleep(500);
  }
  throw new Error(`${label}: left catalog never reached page base ${targetBase}`);
}

// 滚轮符号标定：无弹窗时在左栏滚动，页基前进即证明原语可用；两种符号都不前进则失败。
async function calibrateWheel(driver, probes, cash) {
  await ensurePageBase(driver, probes, cash, PAGE1_BASE, 'wheel calibration start');
  for (const delta of [100, -100]) {
    for (let attempt = 0; attempt < 2; attempt += 1) {
      await ensurePageBase(driver, probes, cash, PAGE1_BASE, `wheel calibration ${delta}`);
      await mouseWheel(driver, WHEEL_POINT.x, WHEEL_POINT.y, delta);
      await sleep(900);
      const base = await leftPageBase(driver, cash, `wheel calibration ${delta}`);
      if (base > PAGE1_BASE && base % CATALOG_PAGE === 0) {
        await ensurePageBase(driver, probes, cash, PAGE1_BASE, 'wheel calibration reset');
        report(true, `wheel calibration: deltaY=${delta} advances left catalog to base ${base}`);
        return delta;
      }
      console.log(`INFO  wheel calibration deltaY=${delta} left base ${base} (no advance)`);
    }
  }
  throw new Error('wheel calibration failed: neither deltaY sign advanced the left catalog');
}
// 右栏锁定 LOCKER；储物格为空时用 1 NX 的便宜商品实测购买一件。
async function provisionLockerItem(driver, probes, cash) {
  const nx = await cash('mscashshop_nx_credit');
  if (nx < 1) throw new Error(`cannot provision a locker item: NX credit is ${nx}`);
  const fullCount = await cash('mscashshop_filtered_count'), lockerBefore = await cash('mscashshop_locker_count');
  await driver.clickField(...SEARCH_BOX);
  await requireState('provision: search focused', () => driver.evaluate('MapleWasmIME.active === true'), 10);
  await driver.compose(CHEAP_SN);
  await sleep(250);
  await driver.key('Enter', 'Enter', 13);
  await requireState('provision: cheap SN search narrows to one card', async () => await cash('mscashshop_filtered_count') === 1, 10);
  const card = cardCenter(0);
  await requireState('provision: search result assets ready', assetsIdle(driver, probes), 45);
  await driver.click(card.x, card.y);
  await requireState('provision: cheap card selected', async () => await cash('mscashshop_selected_left') === 0, 10);
  await sleep(450);
  await driver.click(card.x, card.y);
  await requireState('provision: purchase confirmation opens', stateIs(() => cash('msui_notice_active'), 1), 10);
  await driver.key('Enter', 'Enter', 13);
  await requireState('provision: purchase lands in the locker', async () =>
    await cash('mscashshop_pending') === 0 && await cash('msui_notice_active') === 0 &&
    await cash('mscashshop_locker_count') === lockerBefore + 1, 20);
  // 清空搜索：聚焦输入框、逐次 Backspace 清文本，再点右侧「×」恢复全量目录。
  await driver.clickField(...SEARCH_BOX);
  await requireState('provision: search refocused', () => driver.evaluate('MapleWasmIME.active === true'), 10);
  await driver.compose('9');
  await sleep(200);
  for (let i = 0; i < 10 &&
    await driver.evaluate("document.getElementById('ime-input').value") !== ''; i += 1) {
    await driver.key('Backspace', 'Backspace', 8); await sleep(150);
  }
  await requireState('provision: search text cleared',
    () => driver.evaluate("document.getElementById('ime-input').value === ''"), 10);
  await sleep(200);
  await driver.click(...SEARCH_CLEAR);
  await requireState('provision: full catalog restored', stateIs(() => cash('mscashshop_filtered_count'), fullCount), 10);
}
async function setupLockerPane(driver, probes, cash) {
  await driver.click(...LOCKER_TAB);
  await sleep(400);
  if (await cash('mscashshop_locker_count') < 1) await provisionLockerItem(driver, probes, cash);
  await driver.click(...LOCKER_TAB);
  await sleep(400);
  const count = await cash('mscashshop_locker_count');
  if (count < 1) throw new Error(`locker pane still empty after setup (locker_count=${count})`);
  const ready = await driver.retryClick(SLOT0_POINT[0], SLOT0_POINT[1],
    stateIs(() => cash('mscashshop_selected_right_row'), 0), 3, 1500);
  if (!ready) throw new Error('locker slot 0 never became selectable (LOCKER tab not active?)');
  report(true, `locker pane ready: ${count} item(s), slot 0 selectable`);
}

// A：确认弹窗打开时，双击框外的右栏储物格不得发转移。
async function verifySlotDoubleClickGuard(driver, probes, cash) {
  await ensurePageBase(driver, probes, cash, PAGE1_BASE, 'A page 1');
  await requireState('A catalog assets ready', assetsIdle(driver, probes), 45);
  await sleep(450);  // 与页基探针点击拉开距离，避免被判为双击
  const card = cardCenter(0);
  await driver.click(card.x, card.y);
  await requireState('A: purchase confirmation opens', stateIs(() => cash('msui_notice_active'), 1), 10);
  await sleep(300);
  await expectFront(driver, 'A slot', SLOT0_POINT[0], SLOT0_POINT[1], 'CASHSHOP', FRONT.cashShop);
  await expectFront(driver, 'A notice', NOTICE_POINT[0], NOTICE_POINT[1], 'NOTICE', FRONT.notice);
  const before = await snapshot(cash);
  requireNoPending(before, 'A');
  await driver.doubleClick(...SLOT0_POINT);
  await sleep(1000);
  const after = await snapshot(cash);
  report(await cash('msui_notice_active') === 1 && sameState(before, after),
    `A: slot double-click outside the notice is ignored (${stateText(before)} -> ${stateText(after)})`);
  await driver.screenshot('01-slot-doubleclick-guard');
  await dismissNotice(driver, cash, 'A');
}

// B：确认弹窗打开时，左栏滚轮不得翻动底层目录。
async function verifyWheelPageGuard(driver, probes, cash, wheelDelta) {
  const baseBefore = await ensurePageBase(driver, probes, cash, PAGE1_BASE, 'B page 1');
  await requireState('B catalog assets ready', assetsIdle(driver, probes), 45);
  await sleep(450);
  const card = cardCenter(0);
  await driver.click(card.x, card.y);
  await requireState('B: purchase confirmation opens', stateIs(() => cash('msui_notice_active'), 1), 10);
  await sleep(300);
  await expectFront(driver, 'B wheel', WHEEL_POINT.x, WHEEL_POINT.y, 'CASHSHOP', FRONT.cashShop);
  await expectFront(driver, 'B notice', NOTICE_POINT[0], NOTICE_POINT[1], 'NOTICE', FRONT.notice);
  const before = await snapshot(cash);
  requireNoPending(before, 'B');
  await mouseWheel(driver, WHEEL_POINT.x, WHEEL_POINT.y, wheelDelta);
  await sleep(1000);
  const after = await snapshot(cash);
  report(await cash('msui_notice_active') === 1 && sameState(before, after),
    `B: catalog wheel outside the notice is ignored (${stateText(before)} -> ${stateText(after)})`);
  await driver.screenshot('02-wheel-page-guard');
  await dismissNotice(driver, cash, 'B');
  const baseAfter = await leftPageBase(driver, cash, 'B page base after wheel');
  report(baseAfter === baseBefore, `B: wheel left the underlying page base at ${baseBefore} (got ${baseAfter})`);
}

// C：页面切走后 Enter 不得弹确认；可见选中 Enter 仍弹确认，Escape 取消不落账。
async function verifyStaleSelectionEnterGuard(driver, probes, cash) {
  await ensurePageBase(driver, probes, cash, PAGE2_BASE, 'C page 2');
  await requireState('C catalog assets ready', assetsIdle(driver, probes), 45);
  await sleep(450);
  const card = cardCenter(1);
  await driver.click(card.x, card.y);
  await requireState('C: page card 1 selected', stateIs(() => cash('mscashshop_selected_left'), PAGE2_BASE + 1), 10);
  const staleItem = await cash('mscashshop_highlighted_item');
  let advanced = false;
  for (let step = 0; step < 6 && !advanced; step += 1) {
    await driver.click(...pager.next);
    await sleep(500);
    advanced = await cash('mscashshop_highlighted_item') !== staleItem;
  }
  if (!advanced) throw new Error(`C: left catalog never paged away from highlighted item ${staleItem}`);
  report(true, `C: paged off the stale selection (highlight ${staleItem} -> ${await cash('mscashshop_highlighted_item')})`);
  await requireState('C paged catalog assets ready', assetsIdle(driver, probes), 45);
  const before = await snapshot(cash);
  requireNoPending(before, 'C');
  await driver.key('Enter', 'Enter', 13);
  await sleep(800);
  const after = await snapshot(cash);
  report(await cash('msui_notice_active') === 0 && sameState(before, after),
    `C: Enter on an off-page selection opens no confirmation (${stateText(before)} -> ${stateText(after)})`);
  await driver.screenshot('03-stale-selection-enter');

  const base = await leftPageBase(driver, cash, 'C positive control');
  await requireState('C positive-control assets ready', assetsIdle(driver, probes), 45);
  await sleep(450);
  await driver.click(card.x, card.y);
  await requireState('C: visible card 1 selected', stateIs(() => cash('mscashshop_selected_left'), base + 1), 10);
  const controlBefore = await snapshot(cash);
  requireNoPending(controlBefore, 'C positive control');
  await driver.key('Enter', 'Enter', 13);
  await requireState('C: visible selection Enter opens the confirmation', stateIs(() => cash('msui_notice_active'), 1), 10);
  await driver.screenshot('04-visible-enter-confirmation');
  await driver.key('Escape', 'Escape', 27);
  await requireState('C: Escape cancels the visible-selection confirmation',
    stateIs(() => cash('msui_notice_active'), 0), 10);
  const controlAfter = await snapshot(cash);
  report(sameState(controlBefore, controlAfter),
    `C: cancelled visible confirmation keeps locker/inventory/NX (${stateText(controlBefore)} -> ${stateText(controlAfter)})`);
  await driver.screenshot('05-visible-enter-cancelled');
}

// 搜索 1 NX 商品：先按一次空查询复位，再敲 SN，保证 applied_query 一定变化
// （残留查询与输入相同时会吞掉 Enter），同时让左栏重建回第 0 页、清掉旧选中。
async function searchCheapCommodity(driver, probes, cash) {
  await driver.clickField(...SEARCH_BOX);
  await requireState('D: search focused', () => driver.evaluate('MapleWasmIME.active === true'), 10);
  for (const query of ['', CHEAP_SN]) {
    await driver.clickField(...SEARCH_BOX);
    await driver.compose(query);
    await sleep(200);
    await driver.key('Enter', 'Enter', 13);
    await sleep(300);
  }
  await requireState('D: cheap SN search narrows to one card',
    stateIs(() => cash('mscashshop_filtered_count'), 1), 10);
  await requireState('D: search result assets ready', assetsIdle(driver, probes), 45);
}

// D：Maple Points 余额 0 时确认购买，走 request_purchase 的客户端余额不足分支：
// 延迟 UIOk 提示、pending 保持 0、储物格与 NX 均不落账，Escape 关闭提示。
async function verifyInsufficientBalanceNotice(driver, probes, cash) {
  await requireState('D: no modal before the balance guard', stateIs(() => cash('msui_notice_active'), 0), 10);
  await requireState('D: no pending transfer before the balance guard', stateIs(() => cash('mscashshop_pending'), 0), 10);
  if (await cash('mscashshop_equip_menu_open') === 1) {
    await driver.click(...EQSUB_CLOSE);  // 装备子菜单会盖住分类栏，先关掉
    await sleep(300);
  }
  await expectFront(driver, 'D currency chip', MAPLE_POINT_CHIP[0], MAPLE_POINT_CHIP[1], 'CASHSHOP', FRONT.cashShop);
  await driver.click(...MAPLE_POINT_CHIP);  // 支付币种切到 Maple Points（余额 0）
  await sleep(400);
  await driver.click(...ALL_CATEGORY);  // 「全部」分类，保证搜索能命中 1 NX 商品
  await sleep(400);
  await searchCheapCommodity(driver, probes, cash);
  const card = cardCenter(0);
  await driver.click(card.x, card.y);
  await sleep(450);
  if (await cash('msui_notice_active') !== 1) {  // 首击只选中时，再点一次才弹确认
    await requireState('D: cheap card selected', stateIs(() => cash('mscashshop_selected_left'), 0), 10);
    await sleep(450);
    await driver.click(card.x, card.y);
  }
  await requireState('D: purchase confirmation opens', stateIs(() => cash('msui_notice_active'), 1), 10);
  const before = await snapshot(cash);
  requireNoPending(before, 'D');
  await driver.key('Enter', 'Enter', 13);  // 确认购买：余额不足走客户端分支并延迟弹 UIOk
  await requireState('D: insufficient balance keeps the modal and pending 0', async () =>
    await cash('msui_notice_active') === 1 && await cash('mscashshop_pending') === 0, 15);
  await sleep(600);  // 延迟 UIOk 在下一次 update 才 emplace，等提示落定
  await requireState('D: deferred error notice stays up', async () =>
    await cash('msui_notice_active') === 1 && await cash('mscashshop_pending') === 0, 10);
  const after = await snapshot(cash);
  report(after.pending === 0 && after.locker === before.locker && after.nx === before.nx,
    `D: Maple Points 0 blocks the purchase without landing anything (${stateText(before)} -> ${stateText(after)})`);
  await driver.screenshot('06-insufficient-balance');
  await driver.key('Escape', 'Escape', 27);
  await requireState('D: Escape dismisses the insufficient-balance notice',
    stateIs(() => cash('msui_notice_active'), 0), 10);
  await requireState('D: no pending transfer after the balance guard', stateIs(() => cash('mscashshop_pending'), 0), 10);
}

async function cleanupCashShop(driver, probes, cash) {
  if (await probes.currentUiState() !== uiState.cashShop) return;
  for (let attempt = 0; attempt < 3 && await cash('msui_notice_active') === 1; attempt += 1) {
    await driver.key('Escape', 'Escape', 27); await sleep(300);
  }
  await waitFor(stateIs(() => cash('mscashshop_pending'), 0), 20);
  await driver.key('Escape', 'Escape', 27);
  if (await waitFor(uiStateIs(driver, probes, uiState.game), 20)) console.log('PASS  cleanup: cash shop exited');
}

async function main() {
  prepareDatabase();
  console.log(`front types from ${uiElementHeader}: NOTICE=${FRONT.notice} CASHSHOP=${FRONT.cashShop}`);
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir });
  const probes = createRuntimeProbes(driver);
  const cash = name => probes.cashShopProbe(name);
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver, probes);
    await enterCashShop(driver, probes);
    if (balanceOnly) {
      await verifyInsufficientBalanceNotice(driver, probes, cash);
    } else {
      await setupLockerPane(driver, probes, cash);
      const wheelDelta = await calibrateWheel(driver, probes, cash);
      await verifySlotDoubleClickGuard(driver, probes, cash);
      await verifyWheelPageGuard(driver, probes, cash, wheelDelta);
      await verifyStaleSelectionEnterGuard(driver, probes, cash);
      await verifyInsufficientBalanceNotice(driver, probes, cash);
    }
    console.log(`${failed ? 'FAIL' : 'PASS'} cash shop modal guards: ${passed} checks`);
  } catch (error) {
    report(false, `cash shop modal guards aborted: ${error.message}`);
    console.error(error);
    try { await driver.screenshot('zz-failure'); } catch (nested) {}
  } finally {
    try { await cleanupCashShop(driver, probes, cash); } catch (error) {
      console.error(`cleanup skipped: ${error.message}`);
    }
    if (driver.errors.length) fs.writeFileSync(`${artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}
await main();
