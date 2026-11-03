// /水下游泳 e2e：官方 1:1 行为实测（Aqua Road 230000000）。
//
// 场景与断言（对照 features/changelog/2026-11-01/aqua-road-swim.md）：
//   1. 落图 230000000 水底站立（onground=1, stance 21/22）；
//   2. 水底小跳（Alt）：起跳 ~25px 后回落着地，期间出现 stance 5(JUMP)；
//   3. 按住 ArrowUp（水底直接起游，无需先跳）：离地、stance 3(FLY)、
//      physicstype=2(SWIMMING)、900ms 上升 Δy ≤ -100px、|vspeed| 平台 1.5~4.0；
//   4. 按住 Alt 上升与 ArrowUp 等效：两者 Δy 差 ≤ 60px（FlyState:476 统一处理）；
//   5. 松键缓沉：终速 0.05~0.8 px/帧（SWIMGRAVFORCE 0.03/SWIMFRICTION 0.08 ⇒ 0.375），
//      下沉期间 stance 保持 3(FLY)；
//   6. 水平游：按住 ArrowRight Δx ≥ 90px、|hspeed| 平台 1.5~4.5（≈3.1）；
//   7. 水底行走：沉底后按住 ArrowRight，stance 34/35(WALK)、onground=1。
// 全程 mapId 守卫不变（防止 UP 触碰传送门）。
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/aqua-swim-flow.mjs
// 可用 AQUA_ACCOUNT/PASSWORD/CHARID/CARD/MAP/URL/DEBUG_PORT 覆盖默认值。
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { prepCharacter, waitAccountFree } from './pq-harness-db.mjs';

const account = process.env.AQUA_ACCOUNT || 'fmflow1';
const password = process.env.AQUA_PASSWORD || 'fmflow1';
const charId = Number(process.env.AQUA_CHARID ?? 129);
const cardIndex = Number(process.env.AQUA_CARD ?? 0);
const mapId = Number(process.env.AQUA_MAP ?? 230000000);
const debugPort = Number(process.env.AQUA_DEBUG_PORT || 9274);
const pageUrl = process.env.AQUA_URL || 'http://127.0.0.1:8000/web/index.html';
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const outDir = '/tmp/aqua-swim-flow';

const uiState = { login: 1, world: 2, characters: 3, game: 5 };
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
const stance = { stand: [21, 22], walk: [34, 35], jump: 5, fly: 3 };

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

const PHYS_EXPR = `(function () {
  const g = n => Module.ccall(n, 'number', [], []);
  return [g('msplayer_hspeed'), g('msplayer_vspeed'), g('msplayer_onground'),
          g('msplayer_physicstype'), g('msplayer_x'), g('msplayer_y'), g('msattack_stance'),
          g('msmap_id')];
})()`;

async function snapshot(driver) {
  const [hs, vs, og, pt, x, y, st, mid] = await driver.evaluate(PHYS_EXPR);
  return { hs: hs / 100, vs: vs / 100, onground: og, type: pt, x, y, stance: st, mapId: mid };
}

async function sample(driver, milliseconds, intervalMs = 40) {
  const t0 = Date.now();
  const rows = [];
  while (Date.now() - t0 < milliseconds) {
    const s = await snapshot(driver);
    if (s.mapId !== mapId) throw new Error(`map changed to ${s.mapId} (portal hit?)`);
    rows.push(Object.assign({ t: Date.now() - t0 }, s));
    await sleep(intervalMs);
  }
  return rows;
}

async function holdAndSample(driver, key, code, vk, holdMs, sampleMs) {
  const holding = driver.keyHold(key, code, vk, holdMs);
  const rows = await sample(driver, sampleMs);
  await holding;
  return rows;
}

function median(values) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.floor(sorted.length / 2)];
}

async function settleToFloor(driver, seconds = 10) {
  return waitFor(async () => (await snapshot(driver)).onground === 1, seconds);
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
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
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
  const report = { mapId, samples: {} };
  try {
    await enterGame(driver);
    await settleToFloor(driver, 15);
    const base = await snapshot(driver);
    await driver.screenshot('01-baseline');
    console.log('baseline:', JSON.stringify(base));
    check('standing on water floor', base.onground === 1 && stance.stand.includes(base.stance),
      `onground=${base.onground} stance=${base.stance} type=${base.type}`);

    // —— 水底小跳：~25px 起跳并回落 ——
    const y0 = base.y;
    const hopRows = await holdAndSample(driver, 'Alt', 'AltLeft', 18, 350, 2800);
    const minY = Math.min(...hopRows.map(r => r.y));
    const backOnFloor = hopRows[hopRows.length - 1].onground === 1
      || await settleToFloor(driver, 6);
    const sawJump = hopRows.some(r => r.stance === stance.jump);
    report.samples.hop = hopRows;
    await driver.screenshot('02-after-hop');
    check('bottom hop rises >=15px', minY <= y0 - 15, `rise=${(y0 - minY).toFixed(1)}px (期望 ≈25px)`);
    check('hop returns to floor', backOnFloor, 'onground restored after hop');
    check('jump stance observed', sawJump, `stances ${[...new Set(hopRows.map(r => r.stance))].join(',')}`);

    // 远离出生传送门再开始 UP 测试（防 UP 触发传送）。
    await driver.keyHold('ArrowRight', 'ArrowRight', 39, 1500);
    await settleToFloor(driver, 8);

    // —— ArrowUp 直接从水底起游 ——
    const startY = (await snapshot(driver)).y;
    const upRows = await holdAndSample(driver, 'ArrowUp', 'ArrowUp', 38, 900, 1000);
    const dyUp = upRows[upRows.length - 1].y - startY;
    const upWindow = upRows.filter(r => r.t >= 900 * 0.6 && r.t <= 900);
    const vsPlateauUp = median(upWindow.map(r => Math.abs(r.vs)));
    const leftFloor = upRows.some(r => r.onground === 0);
    const sawFlyUp = upRows.some(r => r.stance === stance.fly);
    const sawSwimType = upRows.some(r => r.type === 2);
    report.samples.arrowUp = upRows; report.dyUp = dyUp; report.vsPlateauUp = vsPlateauUp;
    check('ArrowUp swims up from floor', dyUp <= -100 && leftFloor,
      `Δy=${dyUp.toFixed(1)}px leftFloor=${leftFloor}`);
    check('fly stance while swimming', sawFlyUp, `stances ${[...new Set(upRows.map(r => r.stance))].join(',')}`);
    check('swimming physics type', sawSwimType, `types ${[...new Set(upRows.map(r => r.type))].join(',')}`);
    check('vspeed plateau 1.5~4.0', vsPlateauUp >= 1.5 && vsPlateauUp <= 4.0,
      `median|vs|=${vsPlateauUp.toFixed(2)} (期望 ≈2.75)`);
    await driver.screenshot('03-swim-up');

    // —— 松键缓沉 ——
    const sinkRows = await sample(driver, 1600);
    const sinkWindow = sinkRows.filter(r => r.t >= 1200);
    const sinkVs = median(sinkWindow.map(r => r.vs));
    report.samples.sink = sinkRows; report.sinkVs = sinkVs;
    check('sink terminal speed 0.05~0.8', sinkVs >= 0.05 && sinkVs <= 0.8,
      `median vs=${sinkVs.toFixed(3)} px/frame (期望 ≈0.375)`);
    check('stays fly stance while sinking',
      sinkWindow.every(r => r.stance === stance.fly),
      `stances ${[...new Set(sinkWindow.map(r => r.stance))].join(',')}`);

    // —— Alt 与 ArrowUp 等效 ——
    await settleToFloor(driver, 12);
    const startYAlt = (await snapshot(driver)).y;
    const altRows = await holdAndSample(driver, 'Alt', 'AltLeft', 18, 900, 1000);
    const dyAlt = altRows[altRows.length - 1].y - startYAlt;
    report.samples.altUp = altRows; report.dyAlt = dyAlt;
    check('Alt swims up equivalently', dyAlt <= -100 && Math.abs(dyAlt - dyUp) <= 60,
      `Δy(Alt)=${dyAlt.toFixed(1)}px vs Δy(↑)=${dyUp.toFixed(1)}px`);
    await driver.screenshot('04-swim-alt');

    // —— 水平游 ——
    const startX = (await snapshot(driver)).x;
    const swimRows = await holdAndSample(driver, 'ArrowRight', 'ArrowRight', 39, 900, 1000);
    const dx = swimRows[swimRows.length - 1].x - startX;
    const hWindow = swimRows.filter(r => r.t >= 900 * 0.6 && r.t <= 900);
    const hsPlateau = median(hWindow.map(r => Math.abs(r.hs)));
    report.samples.swimRight = swimRows; report.dxSwim = dx; report.hsPlateau = hsPlateau;
    check('swim right dx >= 90px', dx >= 90, `Δx=${dx.toFixed(1)}px`);
    check('hspeed plateau 1.5~4.5', hsPlateau >= 1.5 && hsPlateau <= 4.5,
      `median|hs|=${hsPlateau.toFixed(2)} (期望 ≈3.1)`);

    // —— 水底行走 ——
    await settleToFloor(driver, 12);
    const walkRows = await holdAndSample(driver, 'ArrowRight', 'ArrowRight', 39, 900, 1000);
    const sawWalk = walkRows.some(r => stance.walk.includes(r.stance) && r.onground === 1);
    report.samples.bottomWalk = walkRows;
    check('walk stance on water floor', sawWalk,
      `stances ${[...new Set(walkRows.map(r => r.stance))].join(',')} onground=${[...new Set(walkRows.map(r => r.onground))].join(',')}`);
    await driver.screenshot('05-bottom-walk');
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
