// /雪地物理 e2e：行走不加速、松键后按官方 fs 摩擦乘数惯性滑行（El Nath
// 211000000, fs=0.2）。官方逻辑：fs 缩放整个地面摩擦（滑行≈旧版 1.5 倍
// 距离）、斜坡恒定下滑力（平地走廊无坡度，不影响本用例）。
//
// 场景与断言（对照 features/changelog/2026-11-01/snow-map-slide.md）：
//   1. 落图 211000000（El Nath 镇）后 physicstype=1(ICE)、站地 stance 21/22；
//   2. 按住 ArrowRight 1.6s：水平速度收敛到平台（≈1.2~1.5 px/帧，速度上限的
//      walkforce 决定），断言平台中位数 0.7~2.4 px/帧 —— 旧 bug 为 ≥3.5；
//   3. 松键：以 fs×全摩擦 滑行（数值模拟 ≈42px/760ms，约旧版 1.5 倍）停止，
//      断言滑行距离 8~90px、期间 stance 保持 34/35(WALK) 再回 21/22(STAND)；
//   4. 观察项（问五第 5 项）：滑行尾速趋零后按住 Down 可进 prone(7)，
//      且 prone 态 |hspeed|=0 不再滑 —— 只记录不断言。
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/snow-slide-flow.mjs
// 可用 SNOW_ACCOUNT/PASSWORD/CHARID/CARD/MAP/URL/DEBUG_PORT 覆盖默认值。
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { prepCharacter, waitAccountFree } from './pq-harness-db.mjs';

const account = process.env.SNOW_ACCOUNT || 'fmflow1';
const password = process.env.SNOW_PASSWORD || 'fmflow1';
const charId = Number(process.env.SNOW_CHARID ?? 129);
const cardIndex = Number(process.env.SNOW_CARD ?? 0);
const mapId = Number(process.env.SNOW_MAP ?? 211000000);
const debugPort = Number(process.env.SNOW_DEBUG_PORT || 9273);
const pageUrl = process.env.SNOW_URL || 'http://127.0.0.1:8000/web/index.html';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const outDir = '/tmp/snow-slide-flow';

const uiState = { login: 1, world: 2, characters: 3, game: 5 };
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
const stance = { stand: [21, 22], walk: [34, 35], jump: 5, fly: 3, prone: 7 };

const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}

function assetsIdle(driver) {
  return driver.evaluate("document.getElementById('ft-loading-screen') === null"
    + " || document.getElementById('ft-loading-screen').classList.contains('is-hidden')");
}

// 单次 evaluate 原子读取全部物理探针（×100 定点）。
const PHYS_EXPR = `(function () {
  const g = n => Module.ccall(n, 'number', [], []);
  return [g('msplayer_hspeed'), g('msplayer_vspeed'), g('msplayer_onground'),
          g('msplayer_physicstype'), g('msplayer_x'), g('msplayer_y'), g('msattack_stance')];
})()`;

async function snapshot(driver) {
  const [hs, vs, og, pt, x, y, st] = await driver.evaluate(PHYS_EXPR);
  return { hs: hs / 100, vs: vs / 100, onground: og, type: pt, x, y, stance: st };
}

async function sample(driver, milliseconds, intervalMs = 40) {
  const t0 = Date.now();
  const rows = [];
  while (Date.now() - t0 < milliseconds) {
    const s = await snapshot(driver);
    rows.push(Object.assign({ t: Date.now() - t0 }, s));
    await sleep(intervalMs);
  }
  return rows;
}

// 按住方向键的同时采样：keyHold 不 await，让采样循环与之交错。
async function holdAndSample(driver, key, code, vk, holdMs, sampleMs) {
  const holding = driver.keyHold(key, code, vk, holdMs);
  const rows = await sample(driver, sampleMs);
  await holding;
  return rows;
}

async function settleToFloor(driver, seconds = 15) {
  return waitFor(async () => (await snapshot(driver)).onground === 1, seconds);
}

function median(values) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.floor(sorted.length / 2)];
}

async function enterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90)) {
    throw new Error('WASM module did not initialize');
  }
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30)) {
    throw new Error('login UI did not activate');
  }
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 45)) {
    throw new Error('world-select UI did not activate');
  }
  await driver.retryClick(510, 216, async () => await probe(driver, 'msui_state') === uiState.characters);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 45)) {
    throw new Error('character-select UI did not activate');
  }
  if (!await waitFor(() => assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 5 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await probe(driver, 'msui_state') === uiState.game;
      if (!entered) await sleep(1000);
    }
    // 双击偶发被吞：选中态下 Enter 也能进游戏，作为回退。
    if (!entered) await driver.key('Enter', 'Enter', 13);
    for (let i = 0; i < 3 && !entered; i += 1) {
      entered = await probe(driver, 'msui_state') === uiState.game;
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) {
    await driver.screenshot('99-entergame-fail');
    console.log('ui_state=', await probe(driver, 'msui_state'),
      'notice=', await probe(driver, 'msui_login_notice_active'),
      'noticeMsgPtr=', await probe(driver, 'msui_login_notice_message'),
      'retry=', await probe(driver, 'msmap_retry'),
      'loading=', await probe(driver, 'msstage_loading'),
      'assetBlocking=', await probe(driver, 'msasset_blocking'));
    console.log('console-tail:', JSON.stringify(driver.logTail ? driver.logTail(20) : (driver.logs || []).slice(-20)));
    throw new Error(`game UI did not activate (card ${cardIndex})`);
  }
  if (!await waitFor(() => assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msmap_id') === mapId, 60)) {
    throw new Error(`expected map ${mapId}, got ${await probe(driver, 'msmap_id')}`);
  }
  await sleep(1500);
}

const failures = [];
function check(name, pass, detail) {
  console.log(`  ${pass ? 'PASS' : 'FAIL'}  ${name}  ${detail}`);
  if (!pass) failures.push(`${name}: ${detail}`);
}

async function main() {
  fs.mkdirSync(outDir, { recursive: true });
  prepCharacter({ account, charId, map: mapId, spawnpoint: 0 });
  if (!await waitAccountFree(account)) throw new Error(`account ${account} still logged in`);

  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  await driver.start();
  await driver.navigate(pageUrl);
  const report = { mapId, samples: {}, observations: {} };
  try {
    await enterGame(driver);
    await settleToFloor(driver);
    const base = await snapshot(driver);
    await driver.screenshot('01-baseline');
    console.log('baseline:', JSON.stringify(base));
    check('ice physics active', base.type === 1, `physicstype=${base.type} (expect 1=ICE; 0=NORMAL 或穿了雪鞋)`);
    check('grounded & standing', base.onground === 1 && stance.stand.includes(base.stance),
      `onground=${base.onground} stance=${base.stance}`);

    // 贴左墙，给后续向右的行走/滑行留出全图跑道。
    await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, 3500);
    await sleep(400);

    // —— 行走平台：按住右 1.6s，末段 40% 采样的 |hspeed| 中位数 ——
    const walkRows = await holdAndSample(driver, 'ArrowRight', 'ArrowRight', 39, 1600, 1700);
    const walkWindow = walkRows.filter(r => r.t >= 1600 * 0.6 && r.t <= 1600);
    const plateau = median(walkWindow.map(r => Math.abs(r.hs)));
    report.samples.walk = walkRows;
    report.plateauPxPerFrame = plateau;
    check('walk speed plateau 0.7~2.4 px/frame', plateau >= 0.7 && plateau <= 2.4,
      `median|hs|=${plateau.toFixed(2)} (旧 bug ≈4.0)`);
    const walkingStanceOk = walkWindow.every(r => stance.walk.includes(r.stance));
    check('walk stance during hold', walkingStanceOk,
      `stance set ${[...new Set(walkWindow.map(r => r.stance))].join(',')}`);

    // —— 松键滑行：releaseX 起算，直到 |hspeed| 归零 ——
    const slideRows = await sample(driver, 1400);
    const releaseX = walkRows.length ? walkRows[walkRows.length - 1].x : slideRows[0].x;
    let slidePx = 0; let walkAfterReleaseMs = 0; let stoppedAt = null;
    for (const r of slideRows) {
      slidePx = Math.max(slidePx, Math.abs(r.x - releaseX));
      if (Math.abs(r.hs) > 0.2) {
        if (stance.walk.includes(r.stance)) walkAfterReleaseMs = r.t;
      }
      if (stoppedAt === null && r.t > 300 && Math.abs(r.hs) === 0) stoppedAt = r.t;
    }
    const finalRow = slideRows[slideRows.length - 1];
    report.samples.slide = slideRows;
    report.slidePx = slidePx; report.walkAfterReleaseMs = walkAfterReleaseMs;
    await driver.screenshot('02-after-slide');
    check('slide distance 8~90px', slidePx >= 8 && slidePx <= 90, `slide=${slidePx.toFixed(1)}px (模拟 ≈42px, 旧版实测 22px)`);
    check('keeps walk stance while sliding', walkAfterReleaseMs >= 150, `walk stance until t=${walkAfterReleaseMs}ms`);
    check('comes to full stop', Math.abs(finalRow.hs) === 0 && stance.stand.includes(finalRow.stance),
      `final hs=${finalRow.hs} stance=${finalRow.stance}`);

    // —— 观察项：滑行尾速趋零后按 Down 进 prone，确认不再滑行 ——
    await driver.keyHold('ArrowRight', 'ArrowRight', 39, 900);
    const proneRows = await holdAndSample(driver, 'ArrowDown', 'ArrowDown', 40, 1200, 1300);
    const sawProne = proneRows.some(r => r.stance === stance.prone);
    const proneStill = proneRows.filter(r => r.stance === stance.prone)
      .every(r => Math.abs(r.hs) < 0.05);
    report.samples.prone = proneRows;
    report.observations.prone = { sawProne, proneStill,
      stances: [...new Set(proneRows.map(r => r.stance))] };
    console.log(`  NOTE  prone-on-snow: sawProne=${sawProne} staysStill=${proneStill}`
      + ` (prone 从静止进入；斜坡上会被下滑力持续拖行，本走廊为平地 —— 观察项不作断言)`);
    await driver.screenshot('03-prone');
  } finally {
    fs.writeFileSync(`${outDir}/report.json`, JSON.stringify(report, null, 2));
    for (const f of fs.readdirSync(driver.artifactDir)) {
      if (f.endsWith('.png')) fs.copyFileSync(`${driver.artifactDir}/${f}`, `${outDir}/${f}`);
    }
    console.log('artifacts:', driver.artifactDir);
    await driver.stop();
  }

  console.log(failures.length
    ? `FAILURES (${failures.length}):\n- ${failures.join('\n- ')}`
    : 'ALL CHECKS PASSED');
  process.exit(failures.length ? 1 : 0);
}

main().catch(error => { console.error(error); process.exit(1); });
