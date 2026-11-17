// 诊断冒烟：射手村左上传送门(in03)通往的 Hidden Street - An Empty House
// (100000002)。SQL 把 test1Hero 预置到该图后登录，观察：
//   1) msmap_id 是否进入 100000002，进图资产是否收敛；
//   2) 玩家 y 是否稳定落地（foothold 正常）还是持续下坠（物理/数据损坏）；
//   3) 左右行走的位移是否正常，截图取证渲染是否正常；
//   4) 运行期控制台无致命错误。
// spawnpoint 语义 = 目标图 portal 索引：0=sp 出生点(66,-266 落 FH26 平台)，
// 6=out00(-16,-341) —— 从射手村 in03 穿门进图的真实落点（L1 屋顶坡道）。
// HOUSE_SPAWN=6 可覆盖以验证传送门入口路径。
// 2026-11-15 排障结论：数据/物理/渲染/服务端全链路验证正常（6 轮回归 5 绿，
// 1 轮仅宽松日志正则误报）。此前"冻结/掉落"症状源于诊断脚本的 SQL 竞态：
// 旧会话在线时改 characters.map 会被断线存档回写覆盖，实际登录进了
// 100000000（射手村街道 y=274 被误读为"掉穿地图"）。
// 用法：node henesys-empty-house-flow.mjs  （MAP/ACCOUNT/PASSWORD/CARD/SPAWN/URL/DEBUG_PORT 可用环境变量覆盖）
import { BrowserDriver, sleep } from './cdp.mjs';
import { sqlExec, sqlQuery, waitAccountFree } from './pq-harness-db.mjs';
import { clickCharSelectStart } from './support/char-select-entry.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.HOUSE_DEBUG_PORT || 9261);
const pageUrl = process.env.HOUSE_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.HOUSE_ACCOUNT || 'test1';
const password = process.env.HOUSE_PASSWORD || 'test1';
const cardIndex = Number(process.env.HOUSE_CARD ?? 0);
const MAP = Number(process.env.HOUSE_MAP ?? 100000002);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
let failed = false;
const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };
const probe = async (driver, name) => {
  try { return await driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`); }
  catch { return -1; }
};
const waitFor = async (predicate, seconds = 45) => {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
};
const assetsIdle = driver => async () => {
  if (await probe(driver, 'msasset_blocking') !== 0) return false;
  try {
    return await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')") === true;
  } catch { return false; }
};

async function samplePos(driver, seconds) {
  const samples = [];
  for (let i = 0; i < seconds * 4; i += 1) {
    samples.push([await probe(driver, 'msplayer_x'), await probe(driver, 'msplayer_y')]);
    await sleep(250);
  }
  return samples;
}

const walk = async (driver, dir, ms) => {
  const key = dir > 0 ? 'ArrowRight' : 'ArrowLeft';
  const code = dir > 0 ? 39 : 37;
  await driver.keyHold(key, key, code, ms);
};

async function main() {
  // 顺序至关重要：必须先等服务端收尾旧会话再改 characters.map——服务端在旧
  // 会话断线时会用旧位置回写存档，后写的 map 会被覆盖，导致登录进错图、
  // 把城镇当地图误诊成"进图掉落/冻结"（2026-11-15 排障实证）。
  if (!await waitAccountFree(account, 40))
    console.log('WARN: account still loggedin, login may bounce');
  const spawn = Number(process.env.HOUSE_SPAWN ?? 0);
  // 写后回读：服务端"loggedin 归零"与"断线存档落库"的先后无契约保证，
  // 静置后回读一旦被旧存档覆盖即重写，彻底关死登录进错图的竞态窗口。
  for (let attempt = 1; attempt <= 3; attempt += 1) {
    sqlExec(
      `update accounts set loggedin=0 where name='${account}';` +
      `update characters set map=${MAP}, spawnpoint=${spawn} where name='${account}Hero';`);
    await sleep(2500);
    const row = sqlQuery(
      `select map, spawnpoint from characters where id=(select id from characters where name='${account}Hero' order by id desc limit 1);`);
    // mysql CLI 表格输出：表头行后每行一记录，末行应精确等于 "map<TAB>spawn"
    const lastRow = row.trim().split('\n').pop();
    const settled = lastRow === `${MAP}\t${spawn}`;
    console.log(`INFO  map 预置第 ${attempt} 次回读: ${lastRow}`);
    if (settled) break;
    if (attempt === 3) throw new Error('characters.map 预置 3 次仍被回写覆盖，放弃登录');
  }

  const driver = new BrowserDriver({ binary: chromeBinary, debugPort,
    artifactDir: process.env.HOUSE_ARTIFACT_DIR || undefined });
  try {
    console.log(`WebUI endpoint: ${pageUrl}`);
    await driver.start();
    await driver.navigate(pageUrl);
    if (!await waitFor(() => driver.evaluate(
      "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
      throw new Error('WASM module did not initialize');
    if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 120))
      throw new Error('login UI did not activate');
    await driver.clickField(340, 260); await driver.typeAscii(account);
    await driver.clickField(340, 285); await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 90))
      throw new Error('world-select UI did not activate');
    await driver.retryClick(510, 216, async () => await probe(driver, 'msui_state') === uiState.characters);
    if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 90))
      throw new Error('character-select UI did not activate');
    if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
    if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 90))
      throw new Error('char-select UI never enabled');

    // 选角：单击卡片选中，再单击 BtSelect。锚点随选角屏代际变化
    // （support/char-select-entry.mjs）：扁平屏热区中心 (656,453)，079 位图屏
    // 按钮 94x43 于 (440,495) → 中心 (487,516)。
    await driver.click(105 + 120 * cardIndex + 25, 210);
    await sleep(400);
    const entered = await clickCharSelectStart(driver,
      async () => await probe(driver, 'msui_state') === uiState.game, 8, 4000);
    if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
    if (!await waitFor(assetsIdle(driver), 150)) throw new Error('in-game assets never idle');

    const mapId = await probe(driver, 'msmap_id');
    report(mapId === MAP, `进入地图 ${MAP}（实测 msmap_id=${mapId}）`);
    if (mapId !== MAP) throw new Error('wrong map, aborting');
    await sleep(2500);
    await driver.screenshot('00-empty-house-spawn');

    // 落地稳定性：8 秒采样，y 应收敛（最大最小差 < 8px）。
    const settled = await samplePos(driver, 8);
    const ys = settled.map(([, y]) => y).filter(y => y > -30000);
    const ySpread = Math.max(...ys) - Math.min(...ys);
    report(ySpread < 8, `落地 y 稳定（spread=${ySpread}px，首样 y=${ys[0]}，末样 y=${ys[ys.length - 1]}）`);

    // 状态快照 + 长按右键密集采样
    const hp0 = await probe(driver, 'msplayer_hp');
    console.log(`INFO  spawn hp=${hp0}/${await probe(driver, 'msplayer_max_hp')} stance=${await probe(driver, 'mschar_stance')}` +
      ` others=${await probe(driver, 'msother_character_count')} loading=${await probe(driver, 'msstage_loading')}`);
    await driver.keyHold('ArrowRight', 'ArrowRight', 39, 3000);
    for (let i = 0; i < 8; i += 1) {
      console.log(`INFO  hold+${i} x=${await probe(driver, 'msplayer_x')} y=${await probe(driver, 'msplayer_y')}` +
        ` hp=${await probe(driver, 'msplayer_hp')} stance=${await probe(driver, 'mschar_stance')}` +
        ` frame=${await probe(driver, 'mschar_frame')}`);
      await sleep(250);
    }
    const yB = await probe(driver, 'msplayer_y');
    await driver.key('Alt', 'AltLeft', 18);
    await sleep(150);
    const yM = await probe(driver, 'msplayer_y');
    await sleep(900);
    const yA = await probe(driver, 'msplayer_y');
    report(yM < Math.min(yB, yA) - 5, `跳跃有起跳（${yB} -> ${yM} -> ${yA}）`);

    // 左右行走位移
    const x0 = await probe(driver, 'msplayer_x');
    await walk(driver, -1, 900); await sleep(700);
    const xl = await probe(driver, 'msplayer_x');
    await walk(driver, 1, 1400); await sleep(700);
    const xr = await probe(driver, 'msplayer_x');
    report(Math.abs(x0 - xl) > 40, `左移正常（${x0} -> ${xl}）`);
    report(Math.abs(xr - xl) > 80, `右移正常（${xl} -> ${xr}）`);
    await driver.screenshot('01-empty-house-walked');

    const mobCount = await probe(driver, 'msmob_count');
    console.log(`INFO  mobCount=${mobCount} pos=(${xr},${await probe(driver, 'msplayer_y')})`);

    const fatalLogs = driver.logs.filter(line =>
      /abort|runtimeerror|exception|\berror\b|failed to|offline/i.test(line));
    report(driver.errors.length === 0 && fatalLogs.length === 0,
      `运行时无错误（errors=${driver.errors.length}, fatalLogs=${fatalLogs.length}）`);
    for (const line of fatalLogs.slice(0, 8)) console.log('LOG', line.slice(0, 200));
  } catch (error) {
    failed = true;
    console.error('ERROR', error);
    try { await driver.screenshot('99-error'); } catch { /* best effort */ }
  } finally {
    await driver.stop();
  }
  console.log(failed ? 'EMPTY-HOUSE FLOW: FAILED' : 'EMPTY-HOUSE FLOW: ALL PASS');
  process.exit(failed ? 1 : 0);
}

await main();
