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
  if (!Array.isArray(data)) throw new Error(`E2E_KEYMAP_JSON is not a list: ${keymapPath}`);
  const mine = data.filter(entry => String(entry.char || '').startsWith(account));
  if (mine.length === 0) console.log(`WARN  no roster entries start with "${account}"; falling back to entry ${charIndex} of the whole file`);
  const pool = mine.length > 0 ? mine : data;
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
    await driver.clickField(340, 285);
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
  if (!await waitFor(async () => {
    const ready = await probe(driver, 'mscharlist_preview_ready');
    const total = await probe(driver, 'mscharlist_preview_total');
    return total > 0 && ready === total;
  }, 90)) throw new Error('character previews never settled');
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
  return { name: 'damage', x0: cx - 40, y0: cy - 15, x1: cx + 40, y1: cy + 15 };
}

function clampRect(rect) {
  const clamp = (value, low, high) => Math.min(Math.max(Math.round(value), low), high);
  const x0 = clamp(rect.x0, 0, 799), x1 = clamp(rect.x1, x0 + 1, 800);
  const y0 = clamp(rect.y0, 0, 599), y1 = clamp(rect.y1, y0 + 1, 600);
  return { name: rect.name, x0, y0, x1, y1 };
}

async function damageLuma(driver, state) {
  const means = await sampleLuminance(driver, [clampRect(mobDamageRegion(state))]);
  return means.damage;
}

async function verifySkill(driver, charName, skill) {
  const letter = String(skill.key || '').toUpperCase();
  if (!/^[A-Z]$/.test(letter)) throw new Error(`bad key binding: ${JSON.stringify(skill)}`);
  const press = keyPressEvent(letter);

  const baseline = await sampleState(driver);
  const baselineLuma = baseline.mobX !== MISSING_COORDINATE
    ? await damageLuma(driver, baseline) : null;

  const frames = new Set([baseline.frame]);
  const stances = new Set([baseline.stance]);
  let afterimage = false;
  let stanceMax = baseline.stance;
  let mobCountMax = baseline.mobCount;
  let lumaDeltaMax = 0;
  const lumaSeries = [baselineLuma];

  await driver.startScreencast(`${charName}-${skill.skill}`);
  let frameCount = 0;
  try {
    await driver.keyHold(press.key, press.code, press.vk, ATTACK_HOLD_MS);
    for (let i = 0; i < SAMPLE_COUNT; i += 1) {
      await sleep(SAMPLE_STEP_MS);
      const state = await sampleState(driver);
      frames.add(state.frame);
      stances.add(state.stance);
      stanceMax = Math.max(stanceMax, state.stance);
      if (state.afterimage === 1) afterimage = true;
      mobCountMax = Math.max(mobCountMax, state.mobCount);
      if (state.mobX !== MISSING_COORDINATE && baselineLuma !== null) {
        const luma = await damageLuma(driver, state);
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
  const damageVisual = baselineLuma !== null && lumaDeltaMax >= damageLumaDelta; // (d)
  return {
    skill: skill.skill, name: skill.name || '', key: letter,
    animation: animation ? 'ok' : 'fail', damageVisual: damageVisual ? 'ok' : 'fail',
    frames: frames.size, frameCount, stanceMax, afterimage,
    lumaBaseline: baselineLuma, lumaDeltaMax,
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
        const verdict = await verifySkill(driver, charName, skill);
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
    const ok = results.filter(r => r.animation === 'ok' && r.damageVisual === 'ok').length;
    console.log(`SUMMARY  ${charName}: ${ok}/${results.length} skill(s) passed animation+damage checks`);
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
