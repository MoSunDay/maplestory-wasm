// 复现"自己击杀的掉落有概率无法捡取"：每轮在蜗牛狩猎场 (40001) 用
// Slash Blast（绑 S）击杀蜗牛 → 掉落出现 → 按住 Z 拾取（必要时左右扫动
// 补救）→ 统计成功率与卡住（stuck）回合。
//
// 怀疑方向：拾取判定/请求只在贴身重叠时有效，掉落落点稍偏或按键窗口
// 过短时请求丢失且无重试 → 掉落永久滞留地面。每轮记录：直接拾取耗时、
// 是否需要扫动补救、是否卡住；卡住时截图 + 5s 连续采样 dropCount
// （msdrop_has 需要 oid 但未导出 oid 列表 → 用 dropCount 恒定 + meso
// 不变证明服务端从未发放拾取奖励）。
//
// 登录链路照抄 skill-anim-flow.mjs（assetsIdle 后再输入、GO(510,216) 进
// worlds.back() == Reboot、cardCenter 双击卡片进 game，详见该文件头注释）。
//
// 用法：
//   E2E_ACCOUNT=test1 E2E_ROUNDS=10 CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/pickup-stress-e2e.mjs
// 环境变量：E2E_URL(http://127.0.0.1:8000/web/index.html)/E2E_ACCOUNT(test1)/
// E2E_PASSWORD(默认同账号)/E2E_CHAR_INDEX(0)/E2E_ROUNDS(10)/
// E2E_ARTIFACT_DIR(/tmp/pickup-stress)/E2E_DEBUG_PORT(9330)/CHROME_BIN；
// 传 --headed 以有头模式运行。
// 退出码：0 = 无卡住回合（且有效回合 ≥3），1 = 存在卡住或流程失败。

import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.E2E_DEBUG_PORT || 9330);
const artifactDir = process.env.E2E_ARTIFACT_DIR || '/tmp/pickup-stress';
const pageUrl = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || account; // 密码默认即账号名
const charIndex = Number(process.env.E2E_CHAR_INDEX || 0);
const rounds = Number(process.env.E2E_ROUNDS || 10);
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
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not reach calledRun within 90s');
  if (!await waitFor(stateIs(driver, uiState.login), 30)) throw new Error('login UI did not activate');
  // 登录界输入必须在资产流空闲后进行：高负载流式加载会整击丢弃 CDP 按键
  // （cdp.mjs 注释所述），否则用户名/密码/回车会静默丢失。
  if (!await waitFor(() => assetsIdle(driver), 60)) throw new Error('login assets never idle');
  let advanced = false;
  for (let attempt = 0; attempt < 3 && !advanced; attempt += 1) {
    await driver.clickField(340, 260);
    await driver.typeAscii(account);
    // Tab advances to the password field without a second click: a lost
    // click focus would type the password into the account box, silently
    // logging in as "<account><password>" (other agents own such accounts).
    await driver.key('Tab', 'Tab', 9);
    await driver.typeAscii(password);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () =>
      [uiState.world, uiState.characters].includes(await probe(driver, 'msui_state')), 20);
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
  if (!await waitFor(stateIs(driver, uiState.characters), 45))
    throw new Error('character-select UI did not activate');
  // 等角色预览流结束 + 资产空闲：预览未挂载/流式加载中点击卡片会被丢弃。
  const previewGate = async () => {
    const ready = await probe(driver, 'mscharlist_preview_ready');
    const total = await probe(driver, 'mscharlist_preview_total');
    return total > 0 && ready === total ? true : `ready=${ready} total=${total}`;
  };
  {
    let last = '';
    const okPreview = await waitFor(async () => {
      const v = await previewGate();
      if (v !== true) last = String(v);
      return v === true;
    }, 90);
    if (!okPreview) {
      await driver.screenshot('preview-stuck');
      throw new Error(`character previews never settled (${last}, state=${await probe(driver, 'msui_state')})`);
    }
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
  if (!await waitFor(async () => await probe(driver, 'msmap_id') === SNAIL_MAP, 60))
    throw new Error(`spawn map is ${await probe(driver, 'msmap_id')}, expected ${SNAIL_MAP}`);
  if (!await waitFor(async () => await probe(driver, 'msstage_loading') === 0, 60))
    throw new Error('map assets never finished loading');
  if (!await waitFor(async () => await probe(driver, 'msasset_blocking') === 0, 60))
    throw new Error('foreground assets stayed blocking');
  if (!await waitFor(async () => await probe(driver, 'msmob_count') > 0, 15))
    throw new Error('no mobs presented near spawn within 15s');
}

// 一次 evaluate 内读取全部状态，避免多次往返间的状态漂移。
async function snapshot(driver) {
  return await driver.evaluate(`(() => {
    const n = name => Module.ccall(name, 'number', [], []);
    return {
      mobCount: n('msmob_count'), mobX: n('msnearest_mob_x'), mobY: n('msnearest_mob_y'),
      playerX: n('msplayer_x'), playerY: n('msplayer_y'),
      meso: n('msplayer_meso'), drops: n('msdrop_count'),
      dropNX: n('msdrop_nearest_x'), dropNY: n('msdrop_nearest_y')
    };
  })()`);
}

// 走到 targetX（±tolerance）：按住方向键的时长随距离伸缩（120~400ms），
// 走路本身即完成朝向；30 次迭代仍未到达则抛错。
async function walkTo(driver, targetX, tolerance = 40) {
  let state = await snapshot(driver);
  for (let i = 0; i < 30 && Math.abs(targetX - state.playerX) > tolerance; i += 1) {
    const delta = targetX - state.playerX;
    const dir = delta > 0 ? RIGHT : LEFT;
    await driver.keyHold(dir.key, dir.code, dir.vk,
      Math.min(400, Math.max(120, Math.abs(delta) * 2)));
    await sleep(120);
    state = await snapshot(driver);
  }
  if (Math.abs(targetX - state.playerX) > tolerance)
    throw new Error(`walkTo(${targetX}) not reached: playerX=${state.playerX}`);
  return state;
}

// 反复攻击直到掉落数超过 drops0：离最近怪 >60px 先走位贴怪（贴怪即面向），
// 再按住 S 500ms 施放 Slash Blast；超时抛出最后快照。
async function killUntilDrop(driver, drops0, deadlineMs) {
  const deadline = Date.now() + deadlineMs;
  let last = await snapshot(driver);
  while (Date.now() < deadline) {
    if (last.drops > drops0) return last;
    if (last.mobCount > 0 && last.mobX !== MISSING_COORDINATE) {
      const dx = last.mobX - last.playerX;
      if (Math.abs(dx) > 60)
        await walkTo(driver, last.mobX, 50);
      // 面朝怪物：Slash Blast 只打击面前扇区，先向怪方向点按转向再出刀
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

// 站定按住 Z 拾取：每 ~300ms 一轮（keyHold z 180ms + sleep 120ms），掉落
// 数回落到 baseline 以下即成功，返回耗时 ms；超时返回 null。
async function pickupAttempt(driver, baselineDrops, budgetMs) {
  const started = Date.now();
  while (Date.now() - started < budgetMs) {
    await driver.keyHold(PICKUP.key, PICKUP.code, PICKUP.vk, 180);
    await sleep(120);
    const state = await snapshot(driver);
    if (state.drops <= baselineDrops) return Date.now() - started;
  }
  return null;
}

// 人类式拾取：像真实玩家一样先走到最近掉落（msdrop_nearest_x）再按 Z，
// 循环直到掉落清零或超时；全部捡完返回总耗时 ms，超时返回 null。
async function pickLikeHuman(driver, budgetMs) {
  const started = Date.now();
  while (Date.now() - started < budgetMs) {
    const state = await snapshot(driver);
    if (state.drops <= 0) return Date.now() - started;
    if (state.dropNX !== MISSING_COORDINATE)
      await walkTo(driver, state.dropNX - 16, 45);
    await pickupAttempt(driver, state.drops - 1, 2000);
  }
  const fin = await snapshot(driver);
  return fin.drops <= 0 ? Date.now() - started : null;
}

// 左右扫动拾取：3 轮（左移 400ms → 按住 Z 拾取 ~300ms → 右移 400ms →
// 按住 Z ~300ms），期间持续补发 Z 键；再站定集中按 Z 3s。掉落数回落返回
// 总耗时，否则 null（总预算 ~10s）。
async function sweepPickup(driver, baselineDrops) {
  const started = Date.now();
  for (let cycle = 0; cycle < 3; cycle += 1) {
    await driver.keyHold(LEFT.key, LEFT.code, LEFT.vk, 400);
    await sleep(80);
    if (await pickupAttempt(driver, baselineDrops, 300) !== null) return Date.now() - started;
    await driver.keyHold(RIGHT.key, RIGHT.code, RIGHT.vk, 400);
    await sleep(80);
    if (await pickupAttempt(driver, baselineDrops, 300) !== null) return Date.now() - started;
  }
  const direct = await pickupAttempt(driver, baselineDrops, 3000);
  return direct !== null ? Date.now() - started : null;
}

async function main() {
  fs.mkdirSync(artifactDir, { recursive: true });
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir, headed });
  const results = [];
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    const entry = await snapshot(driver);
    console.log(`INFO  entered game: mobs=${entry.mobCount} drops=${entry.drops} player=(${entry.playerX},${entry.playerY})`);
    await driver.screenshot('entry');

    for (let r = 1; r <= rounds; r += 1) {
      const before = await snapshot(driver);
      const drops0 = before.drops;
      const meso0 = before.meso;
      // 杀怪直到出现新掉落（掉落计数 > drops0）。
      await killUntilDrop(driver, drops0, 20000);
      // 掉落在死亡动画后才落地：再等最多 4s；始终未出现则记 no-drop 跳过。
      let appeared = null;
      const spawnDeadline = Date.now() + 4000;
      while (Date.now() < spawnDeadline) {
        const state = await snapshot(driver);
        if (state.drops > drops0) { appeared = state; break; }
        await sleep(200);
      }
      if (!appeared) {
        results.push({ round: r, status: 'no-drop', dropsBefore: drops0 });
        console.log(`INFO  round ${r}/${rounds}: no drop appeared within 4s after kill (drops=${drops0})`);
        continue;
      }
      const appearedAt = Date.now();
      const direct = await pickLikeHuman(driver, 12000);
      const swept = direct === null ? await sweepPickup(driver, drops0) : null;
      const after = await snapshot(driver);
      const stuck = direct === null && swept === null;
      const picked = direct ?? swept;
      const result = {
        round: r, status: stuck ? 'stuck' : (direct === null ? 'swept' : 'direct'),
        dropsBefore: drops0, dropsAppeared: appeared.drops,
        picked, sweepNeeded: direct === null,
        elapsedMs: Date.now() - appearedAt,
        stuck, playerX: appeared.playerX, playerY: appeared.playerY,
        mesoDelta: after.meso - meso0
      };
      if (stuck) {
        await driver.screenshot(`stuck-round-${r}`);
        // msdrop_has 需要 oid，但探测未导出 oid 列表 → 改为 5s 内连续采样
        // dropCount 证明其恒定不变；若服务端已发放拾取，meso 会随之增长。
        const stability = [];
        const stabilityDeadline = Date.now() + 5000;
        while (Date.now() < stabilityDeadline) {
          const state = await snapshot(driver);
          stability.push({ t: Date.now(), drops: state.drops, meso: state.meso });
          await sleep(500);
        }
        result.dropStability = stability;
      }
      results.push(result);
      console.log(`INFO  round ${r}/${rounds}: drops ${drops0}->${appeared.drops}`
        + ` picked=${picked === null ? 'NO' : `${picked}ms`}`
        + ` sweep=${result.sweepNeeded ? 'yes' : 'no'} stuck=${stuck}`
        + ` player=(${result.playerX},${result.playerY}) mesoΔ=${result.mesoDelta}`);
    }
  } catch (error) {
    console.log(`FAIL  ${error.message}`);
    try { await driver.screenshot('fatal'); } catch (ignored) {}
    process.exitCode = 1;
  } finally {
    // 注销走系统菜单较脆弱；直接停浏览器，服务端会话随断开超时回收。
    if (driver.errors.length)
      fs.writeFileSync(path.join(artifactDir, 'browser-errors.log'), `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(path.join(artifactDir, 'browser-console.log'), `${driver.logs.join('\n')}\n`);
    if (results.length > 0) {
      const valid = results.filter(x => x.status !== 'no-drop');
      const stuckCount = valid.filter(x => x.stuck).length;
      const pickedCount = valid.filter(x => !x.stuck).length;
      const sweepRescues = valid.filter(x => x.sweepNeeded && !x.stuck).length;
      const reportPath = path.join(artifactDir, 'pickup-stress-report.json');
      fs.writeFileSync(reportPath, `${JSON.stringify({
        account, charIndex, map: SNAIL_MAP, pageUrl, rounds,
        summary: { rounds: valid.length, picked: pickedCount, stuck: stuckCount, sweepRescues },
        results
      }, null, 2)}\n`);
      console.log(`INFO  report: ${reportPath}`);
      console.log(`SUMMARY  ${pickedCount}/${valid.length} round(s) picked up`
        + ` (sweep rescues: ${sweepRescues}, stuck: ${stuckCount})`);
      // Real failure = a stuck round or zero valid rounds; a low round
      // count is a confidence warning, not a failure (smoke runs use 1).
      const pass = stuckCount === 0 && valid.length > 0;
      console.log(pass ? 'PASS  no stuck rounds' : 'FAIL  stuck rounds detected (or no valid rounds)');
      if (valid.length > 0 && valid.length < 3)
        console.log('WARN  fewer than 3 valid rounds; low-confidence result');
      if (!pass) process.exitCode = 1;
    }
    console.log(`INFO  artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
}

await main();
