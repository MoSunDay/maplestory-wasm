// 诊断"自己击杀的掉落无法捡取"：采样玩家坐标 vs 最近可拾取掉落中心
// （msdrop_nearest_x/y）的几何关系，判断 30px 拾取窗口是否可达——若掉落
// 长期滞留而采样距离普遍 >30px，说明落点超出窗口、按 Z 的请求不生效。
// 运行期按 ~500ms 节奏补发 Z，预算 40%/70% 处各扫动一次，逐帧记录玩家/
// 掉落/宠物坐标与 meso；结束后输出最小 |dx|/|dy|/距离、30/60/100px 内的样本数与最近 10 帧样本。
// 只测量不判定成败：正常结束恒退出 0，致命错误退出 1（附 fatal 截图）。
// 用法：
//   E2E_ACCOUNT=test1 E2E_DIAG_SECONDS=30 CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/drop-reach-diagnose.mjs
// 环境变量：E2E_URL(http://127.0.0.1:8000/web/index.html)/E2E_ACCOUNT(test1)/
// E2E_PASSWORD(默认同账号)/E2E_CHAR_INDEX(0)/E2E_DIAG_SECONDS(30)/
// E2E_ARTIFACT_DIR(/tmp/drop-reach)/E2E_DEBUG_PORT(9330)/CHROME_BIN；传 --headed 以有头模式运行。

import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.E2E_DEBUG_PORT || 9330);
const artifactDir = process.env.E2E_ARTIFACT_DIR || '/tmp/drop-reach';
const pageUrl = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || account; // 密码默认即账号名
const charIndex = Number(process.env.E2E_CHAR_INDEX || 0);
const diagSeconds = Number(process.env.E2E_DIAG_SECONDS || 30);
const headed = process.argv.includes('--headed');

const SNAIL_MAP = 40001;
const MISSING_COORDINATE = -32768; // GameplayProbes 缺省坐标哨兵
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
// 拾取键 Z、攻击技能 Slash Blast 绑 S、方向键（与 keymap 准备脚本一致）。
const PICKUP = { key: 'z', code: 'KeyZ', vk: 90 };
const ATTACK = { key: 's', code: 'KeyS', vk: 83 };
const LEFT = { key: 'ArrowLeft', code: 'ArrowLeft', vk: 37 };
const RIGHT = { key: 'ArrowRight', code: 'ArrowRight', vk: 39 };

async function probe(driver, name) {
  return await driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
}
const stateIs = (driver, expected) => async () => await probe(driver, 'msui_state') === expected;

async function waitFor(predicate, seconds = 45, stepMs = 250) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await predicate()) return true;
    await sleep(stepMs);
  }
  return await predicate();
}

async function assetsIdle(driver) {
  return await probe(driver, 'msasset_blocking') === 0 && await driver.evaluate(
    "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

// 与 skill-anim-flow.mjs 相同的登录链：登录 → GO(510,216) → 预览流结束 →
// 双击 cardIndex 卡片 → 校验地图 40001 / 舞台加载完成 / 资产空闲 / 有怪。
async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate("typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not reach calledRun within 90s');
  if (!await waitFor(stateIs(driver, uiState.login), 30)) throw new Error('login UI did not activate');
  // 登录界输入必须在资产流空闲后进行：流式加载中 CDP 按键会被整击丢弃。
  if (!await waitFor(() => assetsIdle(driver), 60)) throw new Error('login assets never idle');
  let advanced = false;
  for (let attempt = 0; attempt < 3 && !advanced; attempt += 1) {
    await driver.clickField(340, 260);
    await driver.typeAscii(account);
    // Tab 进密码框：焦点丢失会把密码打进账号框，误登录成 "<account><password>"。
    await driver.key('Tab', 'Tab', 9);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () => [uiState.world, uiState.characters].includes(await probe(driver, 'msui_state')), 20);
    if (!advanced) {
      // 弹出登录公告（如 reason 7 已登录）时按回车关掉再重试。
      if (await probe(driver, 'msui_login_notice_active') === 1) {
        await driver.key('Enter', 'Enter', 13);
        await sleep(3000);
      }
    }
  }
  if (!advanced) throw new Error('login never advanced past the login UI (3 attempts)');
  // GO 即进入 worlds.back() == world 4 Reboot（见 skill-anim-flow 头注释）。
  await driver.retryClick(510, 216, stateIs(driver, uiState.characters));
  if (!await waitFor(stateIs(driver, uiState.characters), 45)) throw new Error('character-select UI did not activate');
  // 等角色预览流结束 + 资产空闲：预览未挂载/流式加载中点击卡片会被丢弃。
  const previewGate = async () => {
    const ready = await probe(driver, 'mscharlist_preview_ready');
    const total = await probe(driver, 'mscharlist_preview_total');
    return total > 0 && ready === total ? true : `ready=${ready} total=${total}`;
  };
  let previewLast = '';
  const okPreview = await waitFor(async () => {
    const v = await previewGate();
    if (v !== true) previewLast = String(v);
    return v === true;
  }, 90);
  if (!okPreview) {
    await driver.screenshot('preview-stuck');
    throw new Error(`character previews never settled (${previewLast}, state=${await probe(driver, 'msui_state')})`);
  }
  if (!await waitFor(() => assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
  const center = cardCenter(charIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await stateIs(driver, uiState.game)();
      if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error(`game UI did not activate (card ${charIndex})`);
  if (!await waitFor(async () => await probe(driver, 'msmap_id') === SNAIL_MAP, 60)) throw new Error(`spawn map is ${await probe(driver, 'msmap_id')}, expected ${SNAIL_MAP}`);
  if (!await waitFor(async () => await probe(driver, 'msstage_loading') === 0, 60)) throw new Error('map assets never finished loading');
  if (!await waitFor(async () => await probe(driver, 'msasset_blocking') === 0, 60)) throw new Error('foreground assets stayed blocking');
  if (!await waitFor(async () => await probe(driver, 'msmob_count') > 0, 15)) throw new Error('no mobs presented near spawn within 15s');
}

// 一次 evaluate 内读取全部状态，避免多次往返间的状态漂移。
async function snapshot(driver) {
  return await driver.evaluate(`(() => {
    const n = name => Module.ccall(name, 'number', [], []);
    return {
      mobCount: n('msmob_count'), mobX: n('msnearest_mob_x'), mobY: n('msnearest_mob_y'),
      playerX: n('msplayer_x'), playerY: n('msplayer_y'),
      meso: n('msplayer_meso'), drops: n('msdrop_count'),
      dropNX: n('msdrop_nearest_x'), dropNY: n('msdrop_nearest_y'),
      petX: n('mspet_x'), petY: n('mspet_y'), hp: n('msplayer_hp')
    };
  })()`);
}

// 走到 targetX（±tolerance）：按住方向键时长随距离伸缩（120~400ms），走路即朝向；30 次未到则抛错。
async function walkTo(driver, targetX, tolerance = 40) {
  let state = await snapshot(driver);
  for (let i = 0; i < 30 && Math.abs(targetX - state.playerX) > tolerance; i += 1) {
    const delta = targetX - state.playerX;
    const dir = delta > 0 ? RIGHT : LEFT;
    await driver.keyHold(dir.key, dir.code, dir.vk, Math.min(400, Math.max(120, Math.abs(delta) * 2)));
    await sleep(120);
    state = await snapshot(driver);
  }
  if (Math.abs(targetX - state.playerX) > tolerance) throw new Error(`walkTo(${targetX}) not reached: playerX=${state.playerX}`);
  return state;
}

// 反复攻击直到掉落数超过 drops0：离最近怪 >60px 先走位贴怪并转向，再按住 S 500ms；超时抛最后快照。
async function killUntilDrop(driver, drops0, deadlineMs) {
  const deadline = Date.now() + deadlineMs;
  let last = await snapshot(driver);
  while (Date.now() < deadline) {
    if (last.drops > drops0) return last;
    if (last.mobCount > 0 && last.mobX !== MISSING_COORDINATE) {
      const dx = last.mobX - last.playerX;
      if (Math.abs(dx) > 60) await walkTo(driver, last.mobX, 50);
      // 面朝怪物：Slash Blast 只打击面前扇区，先点按转向再出刀
      const face = (last.mobX > (await snapshot(driver)).playerX) ? RIGHT : LEFT;
      await driver.keyHold(face.key, face.code, face.vk, 150);
    }
    await driver.keyHold(ATTACK.key, ATTACK.code, ATTACK.vk, 500);
    await sleep(500);
    last = await snapshot(driver);
  }
  if (last.drops > drops0) return last;
  throw new Error(`no new drop within ${deadlineMs}ms (last=${JSON.stringify(last)})`);
}

// 组装一行样本：dx/dy 为玩家-掉落位移，掉落坐标为哨兵值时记 null。
const makeRow = (t, s, sentZ, sweep) => ({
  t, drops: s.drops, playerX: s.playerX, playerY: s.playerY, dropNX: s.dropNX, dropNY: s.dropNY,
  dx: s.dropNX === MISSING_COORDINATE ? null : s.playerX - s.dropNX,
  dy: s.dropNY === MISSING_COORDINATE ? null : s.playerY - s.dropNY,
  meso: s.meso, petX: s.petX, petY: s.petY, sentZ, ...(sweep ? { sweep: true } : {})
});

// 扫动一轮：左 400ms → Z 200ms → 右 800ms → Z 200ms，每步后各记一行。
async function sweepOnce(driver, record) {
  await driver.keyHold(LEFT.key, LEFT.code, LEFT.vk, 400);
  record(await snapshot(driver), false, true);
  await driver.keyHold(PICKUP.key, PICKUP.code, PICKUP.vk, 200);
  record(await snapshot(driver), true, true);
  await driver.keyHold(RIGHT.key, RIGHT.code, RIGHT.vk, 800);
  record(await snapshot(driver), false, true);
  await driver.keyHold(PICKUP.key, PICKUP.code, PICKUP.vk, 200);
  record(await snapshot(driver), true, true);
}

async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir, headed });
  const rows = [];
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver); // 已断言地图 40001 与 mob 数 > 0
    const before = await snapshot(driver);
    const drops0 = before.drops;
    console.log(`INFO  entered game: mobs=${before.mobCount} drops=${drops0} player=(${before.playerX},${before.playerY}) hp=${before.hp}`);
    await driver.screenshot('entry');
    if (drops0 === 0) {
      // 先原地连杀 killSeconds 秒（AoE 会一次掉多堆），制造掉落堆叠再采样
      const killSeconds = Number(process.env.E2E_KILL_SECONDS || 0);
      if (killSeconds > 0) {
        const killDeadline = Date.now() + killSeconds * 1000;
        while (Date.now() < killDeadline) {
          const s = await snapshot(driver);
          if (s.mobCount > 0 && s.mobX !== MISSING_COORDINATE) {
            if (Math.abs(s.mobX - s.playerX) > 60) await walkTo(driver, s.mobX, 50);
            const face = (s.mobX > s.playerX) ? RIGHT : LEFT;
            await driver.keyHold(face.key, face.code, face.vk, 150);
          }
          await driver.keyHold(ATTACK.key, ATTACK.code, ATTACK.vk, 500);
          await sleep(400);
        }
      } else {
        await killUntilDrop(driver, drops0, 20000);
      }
      const spawnDeadline = Date.now() + 4000; // 掉落在死亡动画后才落地
      while (Date.now() < spawnDeadline && (await snapshot(driver)).drops <= drops0) await sleep(200);
    }
    const started = Date.now();
    const budget = diagSeconds * 1000;
    const marks = [{ at: 0.4, done: false }, { at: 0.7, done: false }];
    const record = (s, sentZ, sweep) => rows.push(makeRow(Date.now() - started, s, sentZ, sweep));
    for (let i = 0; Date.now() - started < budget; i += 1) {
      for (const m of marks) // 预算 40%/70% 处各扫动一次
        if (!m.done && Date.now() - started >= budget * m.at) { m.done = true; await sweepOnce(driver, record); }
      const sentZ = i % 2 === 0; // 隔次按 Z，即 ~500ms 节奏补发拾取键
      if (sentZ) await driver.keyHold(PICKUP.key, PICKUP.code, PICKUP.vk, 180);
      record(await snapshot(driver), sentZ, false);
      await sleep(250);
    }
    const after = await snapshot(driver);
    const dist = r => (r.dx === null || r.dy === null) ? null : Math.hypot(r.dx, r.dy);
    const valid = rows.map(r => ({ ...r, dist: dist(r) })).filter(r => r.dist !== null);
    console.log(`INFO  drops: initial=${drops0} final=${after.drops} (delta ${after.drops - drops0}) meso delta=${after.meso - before.meso}`);
    if (valid.length === 0) {
      console.log(`INFO  no row had a drop target (dropNX stayed ${MISSING_COORDINATE}) in ${rows.length} row(s)`);
    } else {
      const minOf = f => Math.min(...valid.map(f));
      console.log(`INFO  min |dx|=${minOf(r => Math.abs(r.dx)).toFixed(1)} min |dy|=${minOf(r => Math.abs(r.dy)).toFixed(1)} min dist=${minOf(r => r.dist).toFixed(1)} (${valid.length}/${rows.length} rows with drop target)`);
      const bucket = (lo, hi) => valid.filter(r => r.dist > lo && r.dist <= hi).length;
      const b30 = bucket(-1, 30), b60 = bucket(30, 60), b100 = bucket(60, 100);
      console.log(`INFO  dist buckets: <=30px:${b30} 30-60px:${b60} 60-100px:${b100} >100px:${valid.length - b30 - b60 - b100}`);
      for (const r of valid.slice().sort((a, b) => a.dist - b.dist).slice(0, 10)) console.log(`INFO  near ${JSON.stringify(r)}`);
    }
    fs.mkdirSync(artifactDir, { recursive: true });
    const samplesPath = path.join(artifactDir, 'drop-reach-samples.json');
    fs.writeFileSync(samplesPath, `${JSON.stringify({ account, charIndex, map: SNAIL_MAP, pageUrl, diagSeconds, rows }, null, 2)}\n`);
    console.log(`INFO  samples: ${samplesPath}`);
    await driver.screenshot('drop-reach-final');
  } catch (error) {
    console.log(`FAIL  ${error.message}`);
    try { await driver.screenshot('fatal'); } catch (ignored) {}
    process.exitCode = 1;
  } finally {
    // 注销走系统菜单较脆弱；直接停浏览器，服务端会话随断开超时回收。
    if (driver.errors.length) fs.writeFileSync(path.join(artifactDir, 'browser-errors.log'), `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(path.join(artifactDir, 'browser-console.log'), `${driver.logs.join('\n')}\n`);
    console.log(`INFO  artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
}

await main();
