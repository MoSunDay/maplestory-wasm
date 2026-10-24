// 浏览器 E2E：状态栏快捷栏（v83 quickSlot）技能绑定全流程。
//
// 覆盖链路：登录 → 进入角色 → 打开技能书 → 选中任意已学习非被动技能 →
// slot0 绑定/悬停/点击使用/右键清空 → slot7 拖拽绑定/右键清空 →
// Hero's Will 施法后的冷却倒计时显示。
// 两档分辨率均需通过：QS_PRESET=0（800x600，默认）与 QS_PRESET=1（1280x720）。
//
// 关键语义：每次 use/assign 之后、rightclick 之前必须等待
// probes.assetBlocking() === 0。施法（Combat::use_move）会触发 LazyFS
// 按需流式加载并阻塞输入，blocking 期间派发的右键会被整击吞掉。
//
// 用法（依赖本地 assets/ws/server 服务，页面默认探测 loopback 8000/8001，
// 可用 E2E_URL 覆盖）：
//   QS_PRESET=0 node tools/verify/maplestory-webui/quickslot-flow.mjs
//   QS_PRESET=1 node tools/verify/maplestory-webui/quickslot-flow.mjs
//
// 环境变量：
//   QS_PRESET            0=800x600（默认），1=1280x720
//   QS_ACCOUNT           登录账号（默认 test1，沿用仓库 E2E 惯例）
//   QS_PASSWORD          登录密码（必填，避免在代码中写入凭据）
//   QS_DEBUG_PORT        CDP 调试端口（默认 9335）
//   QS_ARTIFACT_DIR      截图/日志目录（默认 /tmp/qs-flow-<preset>）
//   QS_PLAYER_ID         登录角色 ID（默认 215，即 test1Hero）
//   QS_PLAYER_LEVEL      登录角色等级（默认 200）
//   CHROME_BIN           Chromium 路径（默认 /usr/bin/chromium-browser）

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';
import { resolvePageUrl, createRuntimeProbes } from './runtime-flow.mjs';

const preset = Number(process.env.QS_PRESET || 0);
if (preset !== 0 && preset !== 1) {
  throw new Error(`QS_PRESET must be 0 (800x600) or 1 (1280x720), got ${process.env.QS_PRESET}`);
}
const [viewW, viewH] = preset === 1 ? [1280, 720] : [800, 600];
const account = process.env.QS_ACCOUNT || 'test1';
const password = process.env.QS_PASSWORD;
if (!password) {
  throw new Error('QS_PASSWORD is required');
}
const expectedPlayerId = Number(process.env.QS_PLAYER_ID || 215);
const expectedPlayerLevel = Number(process.env.QS_PLAYER_LEVEL || 200);
const artifactDir = process.env.QS_ARTIFACT_DIR || `/tmp/qs-flow-${preset}`;

const driver = new BrowserDriver({
  binary: process.env.CHROME_BIN || '/usr/bin/chromium-browser',
  debugPort: Number(process.env.QS_DEBUG_PORT || 9335),
  artifactDir,
  profileDir: `${artifactDir}/profile`
});
const probes = createRuntimeProbes(driver);
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const EMPTY_SLOTS = [0, 0, 0, 0, 0, 0, 0, 0];
const state = () => probes.currentUiState();

function equal(name, actual, expected) {
  if (JSON.stringify(actual) !== JSON.stringify(expected)) {
    throw new Error(`${name}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
  }
  console.log(`PASS  ${name} = ${JSON.stringify(actual)}`);
}

async function waitFor(name, predicate, seconds = 90) {
  for (let attempt = 0; attempt < seconds * 5; attempt += 1) {
    if (await predicate()) {
      console.log(`PASS  ${name}`);
      return;
    }
    await sleep(200);
  }
  throw new Error(`Timed out: ${name}`);
}

// LazyFS blocking 输入锁：流式加载期间鼠标/按键会被整击丢弃，
// 任何交互（打开技能书、右键清空）之前都必须等到 0。
const waitAssetsIdle = label =>
  waitFor(label, async () => await probes.assetBlocking() === 0, 60);

async function quickslots() {
  const values = [];
  for (let index = 0; index < 8; index += 1) {
    values.push(await probes.quickslotSkill(index));
  }
  return values;
}

// 只移动鼠标（悬停验证），把游戏坐标换算到 canvas 的 CSS 盒。
async function hover(x, y) {
  const point = await driver.evaluate(`(() => {
    const canvas = document.getElementById('canvas');
    const bounds = canvas.getBoundingClientRect();
    return {
      x: bounds.left + ${x} * bounds.width / canvas.width,
      y: bounds.top + ${y} * bounds.height / canvas.height
    };
  })()`);
  await driver.send('Input.dispatchMouseEvent', {
    type: 'mouseMoved', x: point.x, y: point.y, button: 'none'
  });
}

// v83 底部 HUD 布局（StatusBarPreChaos.h）：151x80 面板右锚在视口右下，
// 顶边高出状态栏 9px。技能书窗口是视口内固定坐标，与分辨率无关；
// 状态栏技能按钮随 HUD 右锚。
const panelX = viewW - 151;
const panelY = viewH - 90;
const slotCenter = index => ({
  x: panelX + [20, 54, 87, 124][index % 4],
  y: panelY + (index < 4 ? 24 : 59)
});
const skillbookOrigin = { x: 50, y: 150 };
const tabCenters = [13, 23, 34, 49, 66];
const skillIconCenter = row => ({
  x: skillbookOrigin.x + 29,
  y: skillbookOrigin.y + 110 + row * 40
});
// 状态栏技能按钮：取按钮中心偏左的稳定点（viewW - 217 + 14），
// 避免 canvas 缩放把截断的点击点算到相邻按钮上。
const skillbookButton = { x: viewW - 217 + 14, y: viewH - 66 };

async function login() {
  await driver.clickField(340, 260);
  await driver.typeAscii(account);
  // Tab 切到密码框，避免点击焦点丢失时把密码敲进账号框。
  await driver.key('Tab', 'Tab', 9);
  await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
}

async function enterGame() {
  await waitFor('post-login UI', async () =>
    [uiState.world, uiState.characters, uiState.game].includes(await state()), 90);
  if (await state() === uiState.world) {
    await waitAssetsIdle('world select assets idle');
    // v83 世界选择：先选频道，确认按钮才可用。
    await driver.click(120, 172);
    await sleep(500);
    await driver.click(510, 216);
    await waitFor('character select', async () =>
      [uiState.characters, uiState.game].includes(await state()), 90);
  }
  if (await state() !== uiState.game) {
    await waitAssetsIdle('character select assets idle');
    await sleep(1000);
    // 双击第一张角色卡（中心 130,210）进入游戏，预览流未挂载时点击会被丢弃。
    for (let attempt = 0; attempt < 6 && await state() !== uiState.game; attempt += 1) {
      console.log(`INFO  character card attempt=${attempt} state=${await state()}`);
      await driver.doubleClick(130, 210);
      await sleep(2000);
    }
    if (await state() !== uiState.game) await driver.screenshot('charselect-stuck');
  }
  await waitFor('in game', async () => await state() === uiState.game, 90);
  await waitFor('map loaded', async () => await probes.mapId() > 0, 90);
  await waitFor('map assets idle', async () =>
    await probes.mapAssetLoading() === 0 && await probes.assetBlocking() === 0, 120);
}

// 重试打开技能书：每次尝试前等 blocking 清零，失败时输出命中诊断
// （点击是否落在状态栏命中区与技能按钮上）。
async function openSkillbook(label) {
  for (let attempt = 0; attempt < 6; attempt += 1) {
    await waitAssetsIdle(`${label} input ready (attempt ${attempt})`);
    await driver.click(skillbookButton.x, skillbookButton.y);
    await sleep(1000);
    if (await probes.skillbookActive() === 1) return;
    const hit = await driver.evaluate(
      `Module.ccall('msstatusbar_hit','number',['number','number'],[${skillbookButton.x},${skillbookButton.y}])`);
    const button = await driver.evaluate(
      `Module.ccall('msstatusbar_button_at','number',['number','number'],[${skillbookButton.x},${skillbookButton.y}])`);
    console.log(`INFO  ${label} skillbook attempt=${attempt} active=${await probes.skillbookActive()} hit=${hit} button=${button}`);
  }
  throw new Error(`skillbook did not open (${label})`);
}

// 遍历页签与四行图标，选中任意已学习非被动技能。
// 不断言具体技能 ID（不同存档/职业的可学技能不同），返回真实选中值。
async function selectAnySkill() {
  for (let tab = 0; tab < 5; tab += 1) {
    await driver.click(skillbookOrigin.x + tabCenters[tab], skillbookOrigin.y + 33);
    await sleep(350);
    for (let row = 0; row < 4; row += 1) {
      const center = skillIconCenter(row);
      await driver.click(center.x, center.y);
      await sleep(300);
      const selected = await probes.skillbookSelected();
      if (selected > 0) {
        console.log(`PASS  skill selected tab=${tab} row=${row} id=${selected}`);
        return { id: selected, tab, row };
      }
    }
  }
  throw new Error('no learned non-passive skill could be selected');
}

// The fourth-job book has ten rows; scroll its 4-row window and click each
// visible icon until the requested skill (Hero's Will) is selected.
async function selectFourthJobSkill(target) {
  await driver.click(skillbookOrigin.x + tabCenters[4], skillbookOrigin.y + 33);
  await sleep(350);
  for (let scroll = 0; scroll <= 6; scroll += 1) {
    if (scroll > 0) {
      // Scrollbar down arrow: window-relative (154, 236), 12x12 texture.
      await driver.click(skillbookOrigin.x + 160, skillbookOrigin.y + 242);
      await sleep(250);
    }
    for (let row = 0; row < 4; row += 1) {
      const center = skillIconCenter(row);
      await driver.click(center.x, center.y);
      await sleep(250);
      if (await probes.skillbookSelected() === target) {
        console.log(`PASS  fourth-job skill selected scroll=${scroll} row=${row} id=${target}`);
        return;
      }
    }
  }
  throw new Error(`skill ${target} not found in fourth-job book`);
}

try {
  const url = await resolvePageUrl();
  await driver.start();
  await driver.setNetworkLatency(0);
  await driver.navigate(url);
  await waitFor('module ready', async () => await driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true"
    + " && Module.ccall('msui_state','number',[],[]) > 0"), 90);
  // 分辨率档位在切入 GAME 状态时从 localStorage 读取，须在进图前写入。
  await driver.evaluate(`localStorage.setItem('maplestory.display.resolution', '${preset}')`);
  await waitFor('login', async () => await state() === uiState.login, 60);
  await waitFor('loading screen hidden', async () => await driver.evaluate(
    "document.getElementById('loading-screen').classList.contains('is-hidden')"), 90);
  await waitFor('input enabled', async () => await driver.evaluate(
    "Module.ccall('msui_enabled','number',[],[])") === 1, 60);
  await waitAssetsIdle('login assets idle');

  await login();
  await enterGame();

  equal('player id', await probes.playerId(), expectedPlayerId);
  equal('player level', await driver.evaluate(
    "Module.ccall('msplayer_level','number',[],[])"), expectedPlayerLevel);
  equal(`runtime resolution ${viewW}x${viewH}`, await driver.evaluate(`(() => {
    const n = name => Module.ccall(name, 'number', [], []);
    return [n('msui_resolution_width'), n('msui_resolution_height')];
  })()`), [viewW, viewH]);
  console.log(`INFO  hp ${await probes.playerHp()}/${await probes.playerMaxHp()}`
    + ` mp ${await probes.playerMp()}/${await probes.playerMaxMp()}`);
  equal('initial quickslots', await quickslots(), EMPTY_SLOTS);
  await hover(400, 300);
  await sleep(300);
  equal('initial hovered', await probes.quickslotHovered(), -1);

  await driver.screenshot('initial-hud');
  await openSkillbook('initial');
  await driver.screenshot('skillbook');

  const firstSkill = await selectAnySkill();
  equal('selected before assign', await probes.skillbookSelected(), firstSkill.id);

  const first = slotCenter(0);
  await hover(first.x, first.y);
  await sleep(300);
  equal('hover slot 0', await probes.quickslotHovered(), 0);
  await driver.click(first.x, first.y);
  await sleep(600);
  equal('slot 0 assigned', (await quickslots())[0], firstSkill.id);
  equal('other slots empty', (await quickslots()).slice(1), EMPTY_SLOTS.slice(1));
  equal('selection cleared after assign', await probes.skillbookSelected(), 0);
  equal('hover owns assigned slot', await probes.quickslotHovered(), 0);
  await waitAssetsIdle('slot 0 assign assets idle');
  await driver.screenshot('slot0-assigned');

  await hover(400, 300);
  await sleep(300);
  equal('hover leaves quickslot', await probes.quickslotHovered(), -1);
  await hover(first.x, first.y);
  await sleep(300);
  equal('hover returns to assigned slot', await probes.quickslotHovered(), 0);
  await driver.screenshot('slot0-tooltip');

  // 点击已绑定槽位经 Combat::use_move 施法；施法后的 LazyFS 加载期间
  // 不能派发右键，故先等 blocking 清零。
  await driver.click(first.x, first.y);
  await sleep(500);
  equal('use keeps slot 0', (await quickslots())[0], firstSkill.id);
  equal('use keeps selection cleared', await probes.skillbookSelected(), 0);
  equal('state healthy after use', await state(), uiState.game);
  await waitAssetsIdle('slot 0 use assets idle');
  await driver.rightClick(first.x, first.y);
  await sleep(500);
  equal('right click clears slot 0', await quickslots(), EMPTY_SLOTS);
  await driver.screenshot('slot0-cleared');

  await openSkillbook('second');
  const secondSkill = await selectAnySkill();
  const secondIcon = skillIconCenter(secondSkill.row);
  const last = slotCenter(7);
  await driver.drag(secondIcon.x, secondIcon.y, last.x, last.y);
  await sleep(600);
  equal('slot 7 drag assigned', (await quickslots())[7], secondSkill.id);
  equal('slot 7 selection cleared', await probes.skillbookSelected(), 0);
  await waitAssetsIdle('slot 7 assign assets idle');
  await driver.screenshot('slot7-assigned');
  await driver.rightClick(last.x, last.y);
  await sleep(500);
  equal('right click clears slot 7', await quickslots(), EMPTY_SLOTS);
  await driver.screenshot('slot7-cleared');

  // Hero's Will (1121011) starts a server-driven cooldown. The quickslot
  // panel must render the remaining seconds over the skill icon.
  await openSkillbook('cooldown');
  await selectFourthJobSkill(1121011);

  const cooldownSlot = slotCenter(0);
  await driver.click(cooldownSlot.x, cooldownSlot.y);
  await sleep(600);
  equal('cooldown skill assigned', (await quickslots())[0], 1121011);
  await driver.screenshot('slot0-cooldown-assigned');
  await driver.click(cooldownSlot.x, cooldownSlot.y);
  await sleep(500);
  await waitAssetsIdle('cooldown use assets idle');
  await waitFor('quickslot cooldown active',
    async () => await probes.quickslotCooldown(0) > 0, 20);
  const cooldownSeconds = await probes.quickslotCooldown(0);
  const cooldownLabelLength = await probes.quickslotCooldownLabel(0);
  console.log(`INFO  quickslot cooldown ${cooldownSeconds}s label=${cooldownLabelLength}`);
  if (cooldownLabelLength <= 0) {
    throw new Error('quickslot cooldown label was not populated');
  }
  // Move off the slot so the skill tooltip does not overlap the count.
  await hover(400, 300);
  await sleep(350);
  await driver.screenshot('slot0-cooldown');
  await driver.rightClick(cooldownSlot.x, cooldownSlot.y);
  await sleep(500);
  equal('right click clears cooldown slot', await quickslots(), EMPTY_SLOTS);

  equal('state final', await state(), uiState.game);
  if (driver.errors.length) throw new Error(`browser errors: ${driver.errors.join('\n')}`);
  await driver.screenshot('final');
  console.log(`DONE  preset=${preset} ${viewW}x${viewH}`);
} finally {
  fs.writeFileSync(`${artifactDir}/console.log`, driver.logs.join('\n'));
  fs.writeFileSync(`${artifactDir}/errors.log`, driver.errors.join('\n'));
  await driver.stop();
}
