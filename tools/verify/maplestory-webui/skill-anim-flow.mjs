// 逐角色客户端技能动画验证（keymap 按键 → 动画/伤害取证）。
//
// 前置：scripts/test_accounts/skill-keymap-prep.py 已把 12 个测试角色移到
// 蜗牛狩猎场 (40001) 并把攻击技能绑到字母键，同时写出 /tmp/skill_keymap.json
// （[{"char":"test1Hero","keys":[{"key":"A","skill":1221009,"name":"..."}]}]）。
//
// 世界选择：当前客户端世界选择屏只渲染一个世界按钮并绑定服务器公告的
// 最后一个世界（UIWorldSelect 用 worlds.back()，见
// src/client/IO/UITypes/UIWorldSelect.cpp）；world.ini 公告 5 个世界
// (Scania/Bera/Aurora/Elysium/Reboot)，最后即 world 4 "Reboot" —— 测试角色
// 所在世界。因此点击 GO (510,216) 即进入 Reboot，无需点击世界卡片；
// run.mjs 的 doubleClick(255,260) 是频道板变体，不是世界卡坐标。
//
// 每个技能：基线 stance/frame → startScreencast → keyHold 600ms → 每 100ms
// 采样 1.5s（stance/frame/afterimage/mob 数/最近怪坐标 + 怪头上方 80x30
// 伤害数字区亮度）→ stopScreencast。判据 (a) frame 变化 (b) stance!=0 或
// afterimage (c) screencast >= 3 帧 (d) 伤害区亮度相对基线变化。
//
// 用法：
//   E2E_ACCOUNT=test1 E2E_CHAR_INDEX=0 E2E_KEYMAP_JSON=/tmp/skill_keymap.json \
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/skill-anim-flow.mjs
// 环境变量：E2E_ACCOUNT/E2E_PASSWORD/E2E_CHAR_INDEX(0-3)/E2E_KEYMAP_JSON/
// E2E_ARTIFACT_DIR(/tmp/skill-anim)/E2E_DEBUG_PORT(9310)/E2E_URL/
// E2E_DAMAGE_LUMA_DELTA(4)/E2E_DRY_RUN=1(只验证 roster 加载)。

import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';
import { sampleLuminance } from './layout-pixels.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.E2E_DEBUG_PORT || 9310);
const artifactDir = process.env.E2E_ARTIFACT_DIR || '/tmp/skill-anim';
const pageUrl = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || account; // 密码默认即账号名
const charIndex = Number(process.env.E2E_CHAR_INDEX || 0);
const keymapPath = process.env.E2E_KEYMAP_JSON || '/tmp/skill_keymap.json';
const damageLumaDelta = Number(process.env.E2E_DAMAGE_LUMA_DELTA || 4);

const SNAIL_MAP = 40001;
const MISSING_COORDINATE = -32768; // GameplayProbes 缺省坐标哨兵
const ATTACK_HOLD_MS = 600;
const SAMPLE_STEP_MS = 100;
const SAMPLE_COUNT = 15; // 1.5s 采样窗口
const CANVAS_W = 800, CANVAS_H = 600; // 游戏逻辑画布尺寸（sampleLuminance 同款坐标）
const DAMAGE_REGION_W = 80, DAMAGE_REGION_H = 30; // 怪头上方伤害数字区原始尺寸
const PROBE_MIN_W = 24, PROBE_MIN_H = 12; // 探测区最低有效宽高
const PROBE_APPROACH_RETRIES = 3; // 探测区无效时的最大走位重试次数
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });

let failed = false;
const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };

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

// roster：本账号名下的第 charIndex 张卡对应的 keymap 条目。
function loadRoster() {
  const data = JSON.parse(fs.readFileSync(keymapPath, 'utf8'));
  const roster = Array.isArray(data) ? data : data.chars;
  const mine = roster.filter(entry => String(entry.char || '').startsWith(account));
  if (mine.length === 0) console.log(`WARN  no roster entries start with "${account}"; falling back to entry ${charIndex} of the whole file`);
  const pool = mine.length > 0 ? mine : roster;
  const entry = pool[charIndex];
  if (!entry || !Array.isArray(entry.keys)) {
    throw new Error(`no keymap entry for account ${account} index ${charIndex} in ${keymapPath}`);
  }
  return entry;
}

// 浏览器按键参数：key='a' / code='KeyA' / vk=65（typeAscii 同款）。
function keyPressEvent(letter) {
  return { key: letter.toLowerCase(), code: `Key${letter}`, vk: letter.charCodeAt(0) };
}

async function assetsIdle(driver) {
  return await probe(driver, 'msasset_blocking') === 0 && await driver.evaluate(
    "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

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
  // GO 即进入 worlds.back() == world 4 Reboot（见文件头说明）。
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
    throw new Error(`spawn map is ${await probe(driver, 'msmap_id')}, expected ${SNAIL_MAP} (run skill-keymap-prep.py first)`);
  if (!await waitFor(async () => await probe(driver, 'msstage_loading') === 0, 60))
    throw new Error('map assets never finished loading');
  if (!await waitFor(async () => await probe(driver, 'msasset_blocking') === 0, 60))
    throw new Error('foreground assets stayed blocking');
  if (!await waitFor(async () => await probe(driver, 'msmob_count') > 0, 15))
    throw new Error('no mobs presented near spawn within 15s');
}

async function sampleState(driver) {
  return await driver.evaluate(`(() => {
    const n = name => Module.ccall(name, 'number', [], []);
    return {
      stance: n('msattack_stance'), frame: n('msattack_frame'),
      afterimage: n('msattack_afterimage_presented'), mobCount: n('msmob_count'),
      mobX: n('msnearest_mob_x'), mobY: n('msnearest_mob_y'),
      playerX: n('msplayer_x'), playerY: n('msplayer_y'),
      playerScreenX: n('msplayer_screen_x'), playerScreenY: n('msplayer_screen_y')
    };
  })()`);
}

// msnearest_mob_* 是世界坐标；视口偏移 = 玩家屏幕坐标 - 玩家世界坐标。
// 伤害数字区：以 (怪屏幕x, 怪屏幕y-60) 为中心的 80x30。
function mobDamageRegion(state) {
  const viewX = state.playerScreenX - state.playerX;
  const viewY = state.playerScreenY - state.playerY;
  const cx = Math.round(state.mobX + viewX);
  const cy = Math.round(state.mobY + viewY - 60);
  return { name: 'damage', x0: cx - DAMAGE_REGION_W / 2, y0: cy - DAMAGE_REGION_H / 2,
    x1: cx + DAMAGE_REGION_W / 2, y1: cy + DAMAGE_REGION_H / 2 };
}

function clampRect(rect) {
  const clamp = (value, low, high) => Math.min(Math.max(Math.round(value), low), high);
  const x0 = clamp(rect.x0, 0, CANVAS_W - 1), x1 = clamp(rect.x1, x0 + 1, CANVAS_W);
  const y0 = clamp(rect.y0, 0, CANVAS_H - 1), y1 = clamp(rect.y1, y0 + 1, CANVAS_H);
  return { name: rect.name, x0, y0, x1, y1 };
}

// 探测区有效性：最近怪缺失，或夹取后任边贴画布边缘（目标多半在画布外，
// clampRect 把区域夹成了画布边缘的静止条），或宽高低于下限 / 较原始
// 80x30 缩水过半 → 无效返回 null；有效返回夹取后的 rect。
function validProbeRect(state) {
  if (state.mobX === MISSING_COORDINATE) return null;
  const rect = clampRect(mobDamageRegion(state));
  const width = rect.x1 - rect.x0;
  const height = rect.y1 - rect.y0;
  const offCanvas = rect.x0 <= 0 || rect.y0 <= 0 || rect.x1 >= CANVAS_W || rect.y1 >= CANVAS_H;
  const undersized = width < PROBE_MIN_W || height < PROBE_MIN_H
    || width * 2 < DAMAGE_REGION_W || height * 2 < DAMAGE_REGION_H;
  return offCanvas || undersized ? null : rect;
}

async function damageLuma(driver, rect) {
  const means = await sampleLuminance(driver, [rect]);
  return means.damage;
}

async function approachNearestMob(driver) {
  const state = await sampleState(driver);
  if (state.mobX === MISSING_COORDINATE) return;
  const viewX = state.playerScreenX - state.playerX;
  const viewY = state.playerScreenY - state.playerY;
  // Click the ground ~50px above the mob's feet, clamped inside the field
  // viewport (y <= 375 keeps the click off the chat bar at the bottom).
  const x = Math.min(Math.max(Math.round(state.mobX + viewX), 30), 770);
  const y = Math.min(Math.max(Math.round(state.mobY + viewY - 50), 40), 375);
  await driver.click(x, y);
  await sleep(900);
}

// 采样前建立有效探测：最近怪屏坐标缺失或探测区无效（见 validProbeRect）→
// 重走位贴怪后重建 region 再校验；最多 PROBE_APPROACH_RETRIES 次重试，
// 仍无效返回 null（调用方记 damageVisual=no-valid-probe）。
async function establishDamageProbe(driver) {
  for (let attempt = 0; ; attempt += 1) {
    const state = await sampleState(driver);
    const rect = validProbeRect(state);
    if (rect) return { state, rect };
    if (attempt >= PROBE_APPROACH_RETRIES) return null;
    await approachNearestMob(driver);
  }
}

async function verifySkill(driver, charName, skill) {
  const letter = String(skill.key || '').toUpperCase();
  if (!/^[A-Z]$/.test(letter)) throw new Error(`bad key binding: ${JSON.stringify(skill)}`);
  const press = keyPressEvent(letter);
  // summon: 6s window — damage numbers come from the summon's own attacks
  const sampleCount = skill.summon || skill.special === 'summon' ? 60 : SAMPLE_COUNT;

  // 采样前先确保有效探测区：缺失/无效则走位贴怪重建（最多 3 次重试），
  // 仍无效则本技能伤害判定记 no-valid-probe 并 console.warn（map 40001
  // ~27 只蜗牛，正常走位总能贴上一只，出现即说明有新问题）。
  const damageProbe = await establishDamageProbe(driver);
  if (!damageProbe) {
    console.warn(`WARN  no-valid-probe: ${charName} skill ${skill.skill} key ${letter}:`
      + ` no on-canvas damage region after ${PROBE_APPROACH_RETRIES} approach retries`);
  }
  const baseline = damageProbe ? damageProbe.state : await sampleState(driver);
  const baselineLuma = damageProbe ? await damageLuma(driver, damageProbe.rect) : null;
  const mobCountStart = baseline.mobCount;

  const frames = new Set([baseline.frame]);
  const stances = new Set([baseline.stance]);
  let afterimage = false;
  let stanceMax = baseline.stance;
  let mobCountMax = baseline.mobCount;
  let lumaDeltaMax = 0;
  let mobDepleted = false;
  const lumaSeries = [baselineLuma];

  await driver.startScreencast(`${charName}-${skill.skill}`);
  let frameCount = 0;
  try {
    await driver.keyHold(press.key, press.code, press.vk, ATTACK_HOLD_MS);
    for (let i = 0; i < sampleCount; i += 1) {
      await sleep(SAMPLE_STEP_MS);
      const state = await sampleState(driver);
      frames.add(state.frame);
      stances.add(state.stance);
      stanceMax = Math.max(stanceMax, state.stance);
      if (state.afterimage === 1) afterimage = true;
      mobCountMax = Math.max(mobCountMax, state.mobCount);
      // 窗口期怪枯竭守卫：mobCount 较窗口起点跌超 30%（清场技打空探测区）
      // 时提前收窗，调用方会重走位后重测该技能一次。
      if (mobCountStart > 0 && state.mobCount < mobCountStart * 0.7) { mobDepleted = true; break; }
      const rect = baselineLuma !== null ? validProbeRect(state) : null;
      if (rect) {
        const luma = await damageLuma(driver, rect);
        lumaSeries.push(luma);
        lumaDeltaMax = Math.max(lumaDeltaMax, Math.abs(luma - baselineLuma));
      }
    }
  } finally {
    ({ frameCount } = await driver.stopScreencast());
  }

  const animationAdvanced = frames.size >= 2;               // (a) 帧推进
  const stanceOrAfterimage = stanceMax !== 0 || afterimage; // (b) 非待机或有残影
  const animation = animationAdvanced && stanceOrAfterimage && frameCount >= 3; // (c)
  const damageVisual = baselineLuma === null ? 'no-valid-probe'
    : lumaDeltaMax >= damageLumaDelta ? 'ok' : 'fail';      // (d)
  return {
    skill: skill.skill, name: skill.name || '', key: letter,
    animation: animation ? 'ok' : 'fail', damageVisual,
    summon: skill.summon || skill.special === 'summon' ? true : false,
    frames: frames.size, frameCount, stanceMax, afterimage,
    lumaBaseline: baselineLuma, lumaDeltaMax, mobDepleted,
    stances: [...stances].sort((a, b) => a - b),
    lumaSeries: lumaSeries.map(value => Math.round(value * 100) / 100),
    mobCountMax
  };
}

async function main() {
  let roster;
  try {
    roster = loadRoster();
  } catch (error) {
    console.log(`FAIL  cannot load keymap roster: ${error.message}`);
    process.exitCode = 1;
    return;
  }
  const charName = roster.char;
  const skills = roster.keys;
  console.log(`INFO  ${account}/${charName} (card ${charIndex}): ${skills.length} bound skill(s) from ${keymapPath}`);
  if (process.env.E2E_DRY_RUN === '1') {
    for (const skill of skills) console.log(`INFO  key ${skill.key} -> skill ${skill.skill} ${skill.name || ''}`);
    console.log('INFO  dry-run complete (E2E_DRY_RUN=1)');
    return;
  }
  fs.mkdirSync(artifactDir, { recursive: true });
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort, artifactDir });
  const results = [];
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    await driver.screenshot(`${charName}-entry`);
    for (const skill of skills) {
      try {
        if (!skill.clientAttack && !skill.summon) {
          // This WASM client port plays no local attack for the skill
          // (SkillData::flags_of ATTACK table), so key-press produces no
          // animation/damage numbers. Record as skip, not failure.
          results.push({ skill: skill.skill, name: skill.name || '', key: skill.key,
            animation: 'skip', damageVisual: 'skip' });
          console.log(`SKIP   ${charName} skill ${skill.skill}${skill.name ? ` (${skill.name})` : ''} key ${skill.key}: not a client-side attack in this port`);
          continue;
        }
        await approachNearestMob(driver);
        let verdict = await verifySkill(driver, charName, skill);
        // 怪枯竭守卫触发且该窗口未取到伤害证据：重走位贴怪后重测一次（取
        // 重测结果；首轮已 ok 则保留，避免丢掉已捕获的波峰）。
        if (verdict.mobDepleted && verdict.damageVisual !== 'ok') {
          console.log(`INFO  ${charName} skill ${verdict.skill}: mob count dropped >30% mid-window, re-approach + retest once`);
          await approachNearestMob(driver);
          verdict = await verifySkill(driver, charName, skill);
        }
        results.push(verdict);
        report(verdict.animation === 'ok' && verdict.damageVisual === 'ok',
          `${charName} skill ${verdict.skill}${verdict.name ? ` (${verdict.name})` : ''} key ${verdict.key}:`
          + ` animation=${verdict.animation} frames=${verdict.frames}/${verdict.frameCount}`
          + ` stanceMax=${verdict.stanceMax} afterimage=${verdict.afterimage}`
          + ` damage=${verdict.damageVisual} (Δluma=${verdict.lumaDeltaMax})`);
      } catch (error) {
        results.push({ skill: skill.skill, name: skill.name || '', key: skill.key, error: error.message });
        report(false, `${charName} skill ${skill.skill} key ${skill.key}: ${error.message}`);
      }
    }
    const graded = results.filter(r => r.animation !== 'skip');
    const ok = graded.filter(r => r.animation === 'ok' && r.damageVisual === 'ok').length;
    console.log(`SUMMARY  ${charName}: ${ok}/${graded.length} skill(s) passed animation+damage checks (${results.length - graded.length} skipped)`);
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
  } finally {
    // 注销走系统菜单较脆弱；直接停浏览器，服务端会话随断开超时回收。
    if (driver.errors.length) {
      fs.writeFileSync(path.join(artifactDir, `browser-errors-${charName}.log`), `${driver.errors.join('\n')}\n`);
    }
    fs.writeFileSync(path.join(artifactDir, `browser-console-${charName}.log`), `${driver.logs.join('\n')}\n`);
    const resultsPath = path.join(artifactDir, `results-${charName}.json`);
    fs.writeFileSync(resultsPath, `${JSON.stringify({
      char: charName, account, charIndex, map: SNAIL_MAP, pageUrl, results
    }, null, 2)}\n`);
    console.log(`INFO  results: ${resultsPath}`);
    console.log(`INFO  artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(charIndex) || charIndex < 0 || charIndex > 3) {
  console.log(`FAIL  E2E_CHAR_INDEX must be 0..3, got ${process.env.E2E_CHAR_INDEX}`);
  process.exitCode = 1;
} else await main();
