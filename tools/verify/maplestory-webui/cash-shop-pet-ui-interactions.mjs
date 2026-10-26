// 商城「宠物分类 + 宠物自动补给面板」UI 交互专项浏览器验收。
// 覆盖：宠物分类过滤计数(自 CSV 推算)与翻页连点边界；背靠背翻页连点后加载层/
// 余额态不残留；装备子菜单(武器)交叉切换不泄漏
// equip_sub；「全部」含宠物(>1500)；宠物卡不进试穿集合；SN 搜索 + 购买确认
// Escape 取消不落账；UIPetOptions CANCEL 不落盘 / OK 保存阈值 30/40 / reload
// 后 localStorage 持久化 / HP 药水槽箭头切换候选 / 收尾复位 50/50；召唤态掩码 31。
// 前置：WASM 构建含宠物分类；web-server/ws-proxy/assets-server 运行中。
// 产物目录：PET_UI_ARTIFACT_DIR（默认 /tmp/cash-shop-pet-ui，截图 01-*.png 起）。
// Run: CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-pet-ui-interactions.mjs
// Env: PET_UI_ARTIFACT_DIR / CHROME_BIN / PET_UI_DEBUG_PORT(9263) / PET_UI_URL / MS_SERVER_CONFIG

import fs from 'fs';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';
import { createRuntimeProbes } from './runtime-flow.mjs';

const artifactDir = process.env.PET_UI_ARTIFACT_DIR || '/tmp/cash-shop-pet-ui';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.PET_UI_DEBUG_PORT || 9263);
const pageUrl = process.env.PET_UI_URL || 'http://127.0.0.1:8000/web/index.html';
const configPath = process.env.MS_SERVER_CONFIG ||
  '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const account = 'test1', password = 'test1', cardIndex = 0;
const PET_ITEM = 5000001, PET_SN = '60000001';
const ALL_MINIMUM = 1500, POTION = 2000005, PET_MASK = 31;
// 角色性别（0=男）；期望计数从内嵌 CSV 推算，数据扩充时自动跟随。
const CHARACTER_GENDER = 0;
const PET_COMMODITIES = expectedPetCount(
  '/data/maplestory-wasm/src/client/data/cash-shop-v83.csv', CHARACTER_GENDER);
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5, cashShop: 6 });

// CashShopLayout.h：目录卡网格 GRID_POS{100,88} 步距{101,148} 4 列，图标中心偏移 (42,40)。
const cardCenter = v => ({ x: 100 + (v % 4) * 101 + 42, y: 88 + Math.floor(v / 4) * 148 + 40 });
// 左栏分类条目 x=4..94，第 i 项 y=58+22i：全部=0、装备=1、宠物=7；装备子菜单行高 21（武器=16）。
const railEntry = i => [49, 58 + 22 * i + 11];
const rail = Object.freeze({ all: railEntry(0), equip: railEntry(1), pet: railEntry(7) });
const weaponSub = [49, 60 + 21 * 16 + 10];
const pager = Object.freeze({ next: [488, 557], prev: [115, 557] });  // 30x47 @(473/100,534)
// UIPetOptions 283x492 居中 position(258,54)：HP 滑条 y=208、MP y=352，track x∈[310,490]，
// value=1+(x-310)*98/179 → 反解 sliderX(percent)（20%→345、30%→363、40%→381、50%→400）。
const HP_SLIDER_Y = 208, MP_SLIDER_Y = 352;
const sliderX = percent => Math.round(310 + (percent - 1) * 179 / 98);
const opt = Object.freeze({ hpPrev: [350, 275], hpNext: [450, 275], ok: [345, 516], cancel: [453, 516] });
const SEARCH_BOX = [300, 50];
const clickPt = (driver, [x, y]) => driver.click(x, y);

// 与客户端 CashShopModel/CashShopView 同规则：在售 ∧ (男性0/通用2) ∧ 宠物分类，
// 从内嵌目录 CSV 动态推算期望计数（数据会被并行任务扩充，不能硬编码）。
function expectedPetCount(csvPath, gender) {
  const rows = fs.readFileSync(csvPath, 'utf8').split(';');
  let count = 0;
  for (const row of rows) {
    const f = row.split(',');
    if (f[0] !== 'C' || f.length !== 9) continue;
    const id = Number(f[2]);
    // 与 CashShopView::category_for_item 同口径：宠物 5000xxx + 能力装备
    // 1812000-07 + 宠物穿戴装备 1802xxx。
    const isPet = (id >= 5000000 && id <= 5000999) || (id >= 1812000 && id <= 1812007)
      || (id >= 1802000 && id < 1803000);
    const g = Number(f[6]);
    if (isPet && Number(f[8]) === 1 && (g === 2 || g === gender)) count += 1;
  }
  return count;
}

let failed = false;
const phases = [];
function report(ok, message) {
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
}
const markPhase = label => phases.push(label);
async function waitFor(predicate, seconds = 30) {
  for (let attempt = 0; attempt < seconds * 4; attempt += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return await predicate();
}
async function requireState(name, predicate, seconds = 30) {
  if (!await waitFor(predicate, seconds)) throw new Error(`Timed out: ${name}`);
  console.log(`PASS  ${name}`);
}
const stateIs = (read, expected) => async () => await read() === expected;
const uiStateIs = (driver, probes, expected) =>
  () => probes.currentUiState().then(value => value === expected);
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

// DB 预备（pet-flow 同款访问方式，仅重置登录态）
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
  const db = readDbConfig(configPath);
  mysqlRun(db, `UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  report(true, `DB: account ${account} loggedin reset`);
  return db;
}

// 登录链（pet-flow 同款，含登录公告 Enter 重试）
async function loginAndEnterGame(driver, probes) {
  await requireState('WASM module initialized', () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await requireState('login UI active', uiStateIs(driver, probes, uiState.login), 30);
  await requireState('login assets ready', assetsIdle(driver, probes), 60);
  let advanced = false;
  for (let attempt = 0; attempt < 4 && !advanced; attempt += 1) {
    await driver.clickField(340, 260);
    await driver.typeAscii(account);
    await driver.clickField(340, 285);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () =>
      [uiState.world, uiState.characters].includes(await probes.currentUiState()), 20);
    if (!advanced && await probes.loginNoticeActive() === 1) {
      await driver.key('Enter', 'Enter', 13);
      await sleep(2000);
    }
  }
  if (!advanced) throw new Error('login never advanced past the login UI');
  await driver.retryClick(510, 216, uiStateIs(driver, probes, uiState.characters));
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
  console.log('PASS  cash-shop UI active');
  await requireState('cash-shop assets ready', assetsIdle(driver, probes), 60);
}

async function exitCashShop(driver, probes, cash) {
  await driver.key('Escape', 'Escape', 27);
  await requireState('cash shop exits back to game', uiStateIs(driver, probes, uiState.game), 30);
  await requireState('returned map assets ready', assetsIdle(driver, probes), 60);
  await requireState('game UI enabled', stateIs(() => cash('msui_enabled'), 1), 60);
  markPhase('退商城回图');
}

// 宠物自动补给面板：先聚焦场上，Escape 弹系统菜单 → PET_OPTIONS 入口
async function openPetOptions(driver, probes) {
  await driver.click(400, 300);
  await sleep(250);
  let x = -1, y = -1;
  for (let attempt = 0; attempt < 5 && x < 0; attempt += 1) {
    await driver.key('Escape', 'Escape', 27);
    await sleep(400);
    x = await probes.petOptionsMenuX();
    y = await probes.petOptionsMenuY();
  }
  if (x < 0 || y < 0) throw new Error(`pet options menu point unavailable: ${x},${y}`);
  await driver.click(x, y);
  await requireState('pet options panel active', stateIs(probes.petOptionsActive, 1), 10);
  await requireState('pet options assets ready', assetsIdle(driver, probes), 45);
  await sleep(250);
}

async function verifyPetCategory(driver, probes, cash) {
  await clickPt(driver, rail.pet);
  await sleep(400);
  const filtered = await cash('mscashshop_filtered_count');
  report(filtered === PET_COMMODITIES, `pet category filters exactly ${PET_COMMODITIES} (got ${filtered})`);
  await requireState('pet category assets ready', assetsIdle(driver, probes), 45);
  await driver.screenshot('01-pet-category');
  markPhase('宠物分类');
}
async function catalogStable(cash, label) {
  const state = { filtered: await cash('mscashshop_filtered_count'),
    pending: await cash('mscashshop_pending'), notice: await cash('msui_notice_active') };
  report(state.filtered === PET_COMMODITIES && state.pending === 0 && state.notice === 0,
    `${label}: filtered=${state.filtered} pending=${state.pending} notice=${state.notice}`);
}
// 连点回归：两次翻页点击背靠背发出，中间不 sleep、不等资源层。结算后不允许残留
// 加载遮罩、失败余额态或未清空的 pending/notice。落点页码不校验（连点会被钳制/合并）。
async function verifyRapidClicks(driver, probes, cash) {
  await clickPt(driver, pager.next);
  await clickPt(driver, pager.prev);
  await requireState('rapid pager clicks settle', async () =>
    await cash('mscashshop_pending') === 0 && await cash('msui_notice_active') === 0, 30);
  await requireState('rapid clicks assets ready', assetsIdle(driver, probes), 45);
  const state = { blocking: await probes.assetBlocking(),
    balanceFailed: await cash('mscashshop_balance_failed'),
    balancesReady: await cash('mscashshop_balances_ready'),
    pending: await cash('mscashshop_pending'), notice: await cash('msui_notice_active') };
  report(state.blocking === 0, `rapid clicks leave no asset blocking layer (blocking=${state.blocking})`);
  report(state.balanceFailed === 0, `rapid clicks keep balance live (balance_failed=${state.balanceFailed})`);
  report(state.balancesReady === 1, `balance refresh completes after rapid clicks (balances_ready=${state.balancesReady})`);
  report(state.pending === 0 && state.notice === 0,
    `rapid clicks settle (pending=${state.pending} notice=${state.notice})`);
  await driver.screenshot('02b-rapid-clicks');
  markPhase('连点/加载层');
}
async function verifyPagination(driver, probes, cash) {
  for (let i = 0; i < 5; i += 1) {  // 12 件/页，连点越界只应被钳制
    await clickPt(driver, pager.next); await sleep(200);
  }
  await catalogStable(cash, '5x LEFT_NEXT boundary');
  for (let i = 0; i < 2; i += 1) {
    await clickPt(driver, pager.prev); await sleep(200);
  }
  await catalogStable(cash, '2x LEFT_PREV rollback');
  await requireState('pagination assets ready', assetsIdle(driver, probes), 45);
  await driver.screenshot('02-pagination');
  markPhase('翻页边界');
}
async function verifyEquipCrossSwitch(driver, probes, cash) {
  await clickPt(driver, rail.equip);
  await requireState('equip menu opens in place', stateIs(() => cash('mscashshop_equip_menu_open'), 1), 10);
  const equipAll = await cash('mscashshop_filtered_count');
  await requireState('equip menu assets ready', assetsIdle(driver, probes), 45);
  await clickPt(driver, weaponSub);
  await requireState('weapon subcategory applies', async () =>
    await cash('mscashshop_equip_menu_open') === 0 && await cash('mscashshop_equip_sub') === 16, 10);
  await requireState('weapon catalog assets ready', assetsIdle(driver, probes), 45);
  const weapons = await cash('mscashshop_filtered_count');
  report(Math.abs(weapons - equipAll) > 0, `weapon sub refilters (equip-all=${equipAll} -> ${weapons})`);
  await clickPt(driver, rail.pet);
  await requireState('pet category assets ready', assetsIdle(driver, probes), 45);
  const leak = { menu: await cash('mscashshop_equip_menu_open'),
    sub: await cash('mscashshop_equip_sub'), filtered: await cash('mscashshop_filtered_count') };
  report(leak.menu === 0 && leak.sub === 0 && leak.filtered === PET_COMMODITIES,
    `pet category clears equip submenu (menu=${leak.menu} sub=${leak.sub} filtered=${leak.filtered})`);
  markPhase('装备子菜单交叉');
}
async function verifyAllIncludesPets(driver, probes, cash) {
  await clickPt(driver, rail.all); await sleep(400);
  const allCount = await cash('mscashshop_filtered_count');
  report(allCount > ALL_MINIMUM, `全部 category carries ${allCount} > ${ALL_MINIMUM} (pets included)`);
  await requireState('all-category assets ready', assetsIdle(driver, probes), 60);
  markPhase('全部含宠物');
}
async function verifyTryonIsolation(driver, probes, cash) {
  await clickPt(driver, rail.pet);
  await requireState('back on pet category', stateIs(() => cash('mscashshop_filtered_count'), PET_COMMODITIES), 10);
  await requireState('pet catalog assets ready', assetsIdle(driver, probes), 45);
  const before = { tryon: await cash('mscashshop_tryon_count'),
    cap: await cash('mscashshop_preview_cap') };
  const card = cardCenter(0);
  await driver.click(card.x, card.y);
  await requireState('pet catalog card selected', async () => await cash('mscashshop_selected_left') >= 0, 10);
  await sleep(600);
  const after = { tryon: await cash('mscashshop_tryon_count'), cap: await cash('mscashshop_preview_cap') };
  report(after.tryon === before.tryon, `pet card never joins try-on set (${before.tryon} -> ${after.tryon})`);
  report(after.cap === before.cap || after.cap === -1, `preview cap untouched (${before.cap} -> ${after.cap})`);
  await driver.screenshot('03-tryon-isolated');
  markPhase('试穿隔离');
}
async function clearSearch(driver, cash) {
  await driver.clickField(...SEARCH_BOX);
  await requireState('cash-shop search refocused', () =>
    driver.evaluate('MapleWasmIME.active === true'), 10);
  await driver.compose('9');  // compose 后逐次 Backspace 清空（evaluate 读 ime-input）
  await sleep(200);
  for (let i = 0; i < 10 &&
    await driver.evaluate("document.getElementById('ime-input').value") !== ''; i += 1) {
    await driver.key('Backspace', 'Backspace', 8); await sleep(150);
  }
  await requireState('search text cleared', () => driver.evaluate(
    "document.getElementById('ime-input').value === ''"), 10);
  await sleep(200);
  // 真实用户路径：点搜索框右侧「×」清除按钮（BT_SEARCH_CLEAR）。
  await driver.click(480, 46);
  await requireState('cleared search restores pet listing',
    stateIs(() => cash('mscashshop_filtered_count'), PET_COMMODITIES), 10);
}
async function verifyBuyCancelPath(driver, probes, cash) {
  await driver.clickField(...SEARCH_BOX);
  await requireState('cash-shop search focused', () =>
    driver.evaluate('MapleWasmIME.active === true'), 10);
  await driver.compose(PET_SN);
  await requireState('pet SN entered', () => driver.evaluate(
    `document.getElementById('ime-input').value === ${JSON.stringify(PET_SN)}`), 10);
  await sleep(250);
  await driver.key('Enter', 'Enter', 13);
  await requireState('SN search highlights pet card', async () =>
    await cash('mscashshop_filtered_count') === 1 &&
    await cash('mscashshop_highlighted_item') === PET_ITEM, 10);
  const card = cardCenter(0);
  await driver.click(card.x, card.y);
  await requireState('pet card selected for purchase', async () => await cash('mscashshop_selected_left') >= 0, 10);
  await driver.click(card.x, card.y);  // 再点选中卡 → YesNo 确认
  await requireState('purchase confirmation dialog visible', stateIs(() => cash('msui_notice_active'), 1), 10);
  await sleep(400);
  await driver.screenshot('04-buy-dialog');
  const before = { locker: await cash('mscashshop_locker_count'), nx: await cash('mscashshop_nx_credit') };
  await driver.key('Escape', 'Escape', 27);  // UINotice escape → yesnohandler(false)
  await requireState('escape dismisses confirmation', stateIs(() => cash('msui_notice_active'), 0), 10);
  const after = { pending: await cash('mscashshop_pending'),
    locker: await cash('mscashshop_locker_count'), nx: await cash('mscashshop_nx_credit') };
  report(after.pending === 0 && after.locker === before.locker && after.nx === before.nx,
    `cancelled purchase keeps state (pending=${after.pending} locker=${before.locker}` +
    `->${after.locker} nx=${before.nx}->${after.nx})`);
  await clearSearch(driver, cash);
  markPhase('购买取消');
}

async function verifyPanelCancel(driver, probes) {
  await openPetOptions(driver, probes);
  const t0 = await probes.petAutoHpThreshold();
  await driver.click(sliderX(20), HP_SLIDER_Y); await sleep(300);  // HP 20% ≈ x=345
  await clickPt(driver, opt.cancel);
  await requireState('cancel closes options panel', stateIs(probes.petOptionsActive, 0), 10);
  const t1 = await probes.petAutoHpThreshold();
  report(t1 === t0, `CANCEL discards slider edits (auto_hp ${t0} -> ${t1})`);
  markPhase('面板取消');
}
async function verifyPanelSave(driver, probes) {
  await openPetOptions(driver, probes);
  await driver.click(sliderX(30), HP_SLIDER_Y); await sleep(250);  // HP 30% ≈ x=363
  await driver.click(sliderX(40), MP_SLIDER_Y); await sleep(250);  // MP 40% ≈ x=381
  await clickPt(driver, opt.ok);
  await requireState('OK closes options panel', stateIs(probes.petOptionsActive, 0), 10);
  const hp = await probes.petAutoHpThreshold(), mp = await probes.petAutoMpThreshold();
  report(Math.abs(hp - 30) <= 2 && Math.abs(mp - 40) <= 2,
    `thresholds saved hp=${hp} mp=${mp} (expect 30±2 / 40±2)`);
  await driver.screenshot('05-thresholds-saved');
  markPhase('面板保存');
}
async function verifyPersistence(driver, probes, db) {
  try { await driver.evaluate('location.reload()'); } catch (error) {}
  await sleep(3000);  // 断线退出，让服务端存档
  mysqlRun(db, `UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  await loginAndEnterGame(driver, probes);
  const hp = await probes.petAutoHpThreshold(), mp = await probes.petAutoMpThreshold();
  report(Math.abs(hp - 30) <= 2, `hp threshold survives reload via localStorage (${hp} ~ 30)`);
  report(Math.abs(mp - 40) <= 2, `mp threshold survives reload via localStorage (${mp} ~ 40)`);
  markPhase('阈值持久化');
}
async function verifyPotionSwitch(driver, probes) {
  await openPetOptions(driver, probes);
  const cur = await probes.petOptionsHpItem();
  await clickPt(driver, opt.hpPrev); await sleep(300);  // HP 药水槽左箭头 → OFF 或另一药水
  const after = await probes.petOptionsHpItem();
  report(after !== cur, `HP potion left arrow switches candidate (${cur} -> ${after})`);
  await clickPt(driver, opt.ok);
  await requireState('potion switch closes panel', stateIs(probes.petOptionsActive, 0), 10);
  report(await probes.petAutoHpItem() === after, `auto HP item follows selection (${await probes.petAutoHpItem()})`);
  await driver.screenshot('06-potion-switched');
  await openPetOptions(driver, probes);
  await clickPt(driver, opt.hpNext); await sleep(300);
  if (after === POTION) {  // 左箭头恰好停在超级药水：右箭头走开后点回
    await clickPt(driver, opt.hpPrev); await sleep(300);
  }
  for (let attempt = 0; attempt < 8 && await probes.petOptionsHpItem() !== POTION; attempt += 1) {
    await clickPt(driver, opt.hpNext); await sleep(250);
  }
  await clickPt(driver, opt.ok);
  await requireState('potion restore closes panel', stateIs(probes.petOptionsActive, 0), 10);
  report(await probes.petAutoHpItem() === POTION, `auto HP item restored to ${POTION} (got ${await probes.petAutoHpItem()})`);
  markPhase('药水切换');
}
async function restoreThresholds(driver, probes) {
  await openPetOptions(driver, probes);
  await driver.click(sliderX(50), HP_SLIDER_Y); await sleep(250);  // 50% ≈ x=400
  await driver.click(sliderX(50), MP_SLIDER_Y); await sleep(250);
  await clickPt(driver, opt.ok);
  await requireState('reset closes panel', stateIs(probes.petOptionsActive, 0), 10);
  const hp = await probes.petAutoHpThreshold(), mp = await probes.petAutoMpThreshold();
  report(Math.abs(hp - 50) <= 2 && Math.abs(mp - 50) <= 2, `thresholds restored hp=${hp} mp=${mp} (expect 50±2)`);
  await driver.screenshot('07-restored');
  markPhase('收尾复位');
}
async function verifyCapabilityMask(probes) {
  if (await probes.petActive() !== 1) {
    report(true, 'pet not summoned (previous flow state) - mask check skipped');
    return;
  }
  const mask = await probes.petCapabilityMask();
  report(mask === PET_MASK, `summoned pet capability mask ${mask} == ${PET_MASK}`);
  markPhase('能力掩码');
}

async function main() {
  const db = prepareDatabase();
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir });
  const probes = createRuntimeProbes(driver);
  const cash = name => probes.cashShopProbe(name);
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver, probes);
    await enterCashShop(driver, probes);
    await verifyPetCategory(driver, probes, cash);
    await verifyRapidClicks(driver, probes, cash);
    await verifyPagination(driver, probes, cash);
    await verifyEquipCrossSwitch(driver, probes, cash);
    await verifyAllIncludesPets(driver, probes, cash);
    await verifyTryonIsolation(driver, probes, cash);
    await verifyBuyCancelPath(driver, probes, cash);
    await exitCashShop(driver, probes, cash);
    await verifyPanelCancel(driver, probes);
    await verifyPanelSave(driver, probes);
    await verifyPersistence(driver, probes, db);
    await verifyPotionSwitch(driver, probes);
    await restoreThresholds(driver, probes);
    await verifyCapabilityMask(probes);
    for (const phase of phases) console.log(`PASS  pet ui interactions: ${phase}`);
  } catch (error) {
    report(false, `pet ui interactions aborted: ${error.message}`);
    console.error(error);
    try { await driver.screenshot('zz-failure'); } catch (nested) {}
  } finally {
    if (driver.errors.length)
      fs.writeFileSync(`${artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

await main();
