// FM 组队任务(PQ) e2e 流程的共享 harness（纯函数，无 class）。
//
// 汇集 fm-gameplay-guides / fm-service-npcs / gohome-flow / npc-embed-icons
// 中已经验证过的驱动原语，供 fm-pq-teleports.mjs 与 fm-pq-solo-flow.mjs
// 复用：
//   * 登录进游戏（含 reason=7“已登录”一次性重试，凭据不入库）；
//   * 走位 / Space 开 NPC / Escape 关闭（gohome-flow 的 /聊天发送）；
//   * 悬停扫描：cdp.mjs 的 move() 触发 UINpcTalk 选项行 hover 重绘为
//     ORANGE(255,128,0)，页面内 WebGL readPixels 采样条带（模式来自
//     npc-embed-icons.mjs）按行统计橙色像素，聚成 ≥8px 的行带即选项行，
//     只有“行带覆盖当前光标 y”才算命中（排除对话框外的地图底色）；
//   * YES/OK 按钮：由 UINpcTalk.cpp 常量推导中心点（YES_NO 先放 NO 再放
//     YES；UtilDlgEx top 宽 529、BUTTON_MARGIN 20、GAP 6、按钮宽 46 →
//     YES 中心 x=570；type4(sendOk) 走 SELECTION 按钮组 → OK 中心 x=518；
//     vtile=8 时按钮中心 y≈394），点击后以“对话关闭/地图变化/提示变化”
//     为效果断言，未命中则沿 footer 网格扫描，误点只会落在空白处。
import { execFileSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';
import { decodePng, findDialogButtons } from './png-probe.mjs';

export const CONFIG_INI =
  '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';

// 与 scripts/test_accounts/create_accounts.py 的 read_db_config 相同：凭据
// 运行时从服务端 configuration.ini 解析，绝不硬编码。
export function readDbConfig(iniPath = CONFIG_INI) {
  const values = {};
  for (const raw of fs.readFileSync(iniPath, 'utf8').split(/\r?\n/)) {
    const at = raw.indexOf('=');
    if (at <= 0) continue;
    const key = raw.slice(0, at).trim();
    if (!(key in values)) values[key] = raw.slice(at + 1).trim();
  }
  let host = '127.0.0.1';
  let database = 'maplestory';
  const url = values.URL || '';
  if (url) {
    const rest = url.split('//')[1] || '';
    const [hostPart, tail = ''] = rest.split('/');
    if (hostPart.includes(':')) host = hostPart.split(':')[0];
    if (tail.split('?')[0]) database = tail.split('?')[0];
  }
  return {
    host, database,
    user: values.DB_USER || 'root',
    password: values.DB_PASS || '',
  };
}

export function sqlExec(sql, config = readDbConfig()) {
  return execFileSync(
    'mysql',
    ['-h', config.host, '-u', config.user, config.database],
    { input: sql, encoding: 'utf8', env: { ...process.env, MYSQL_PWD: config.password } },
  );
}

export function sqlQuery(sql, config = readDbConfig()) {
  return sqlExec(sql, config);
}

// 上一会话浏览器被杀后，服务端需要几秒处理断线并把账号 loggedin 归零；
// 连续两次观测到 0 才继续，避免下一轮登录撞上“角色仍在游戏中”。
export async function waitAccountFree(account = 'test2', seconds = 40) {
  const loggedin = () => {
    try {
      const out = sqlQuery(
        `select loggedin from accounts where name='${account}';`);
      return Number((out.split('\n')[1] || '').trim());
    } catch (error) { return -1; }
  };
  let zeroes = 0;
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (loggedin() === 0) zeroes += 1; else zeroes = 0;
    if (zeroes >= 2) return true;
    await sleep(1200);
  }
  return false;
}

// 每轮浏览器会话之间的标准 SQL 预处理（角色必须离线）：清登录态、重置到
// 自由市场、按 PQ 需要调整等级。keymap 需包含 Space(57→ACTION/NPCCHAT)、
// Alt(56→JUMP)、Ctrl(29→ATTACK)（见 fm-gameplay-guides.mjs 头注；曾发生过
// 被整表重建抹掉的情况，因此每次预写回）。
export function prepCharacter({ level = null, account = 'test2', charId = 219 } = {}) {
  const levelSql = Number.isInteger(level)
    ? `update characters set level=${level} where id=${charId};\n` : '';
  sqlExec(
    `update accounts set loggedin=0 where name='${account}';\n` +
    `update characters set map=910000000, spawnpoint=0 where id=${charId};\n` +
    `delete from keymap where characterid=${charId} and \`key\` in (29, 56, 57);\n` +
    `insert into keymap (characterid, \`key\`, type, action) values ` +
    `(${charId}, 57, 5, 54), (${charId}, 56, 5, 53), (${charId}, 29, 5, 52);\n` +
    levelSql,
  );
}

let lastProgressAt = Date.now();
export const noteProgress = () => { lastProgressAt = Date.now(); };
export const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
export const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });

export const probe = async (driver, name) => {
  const value = await driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
  noteProgress();
  return value;
};
export const promptText = async driver => {
  const value = await driver.evaluate("Module.ccall('msnpc_prompt', 'string', [], [])");
  noteProgress();
  return value;
};
export const playerX = driver => probe(driver, 'msplayer_x');
export const mapId = driver => probe(driver, 'msmap_id');

export async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) {
    if (await predicate()) return true;
    await sleep(250);
  }
  return false;
}

// —— 停滞看门狗 ——————————————————————————————————————————————
// 共享环境风险：ms-ws-proxy 可能被外部重建，客户端连接中断后页面主线程
// 可能忙转，CDP Runtime.evaluate 永远 pending，流程挂死且无任何输出。
// 流程在关键里程碑调用 noteProgress()；超过阈值无进展则强制退出（非零码），
// 由外层包装命令清理残留 chromium（pkill -f '<artifactDir>/chrome-profile'）。
export function installStallWatchdog(seconds = 240, label = 'flow') {
  const timer = setInterval(() => {
    if (Date.now() - lastProgressAt > seconds * 1000) {
      console.log(`FAIL  ${label} 停滞超过 ${seconds}s（疑似 ws-proxy 被外部重建/页面无响应），强制中止`);
      process.exit(1);
    }
  }, 15000);
  timer.unref();
  return timer;
}

export function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}

export async function settleShot(driver, name, settleMs = 1500) {
  if (!await waitFor(assetsIdle(driver), 90))
    throw new Error(`assets not idle before ${name}`);
  await sleep(settleMs);
  await driver.screenshot(name);
}

// —— 登录输入（确定性 + 游戏态验证）—————————————————————————
// UILogin 的账号框是普通文本框（IME 镜像通道，ImeBridge::focus_field 会把
// 字段文本同步进 DOM textarea），密码框是 crypted 字段（GLFW 键盘通道，
// 无法从页面读取）。共享环境下 CDP 键盘事件偶发被丢弃（实测密码少字符
// → 服务端回 WRONG_PASSWORD=3），因此：
//   * 账号：clickField 后直接 ccall msime_input 写入（与 JS 桥 syncToGame
//     同路径），再点击账号框用 textarea 镜像读回游戏侧文本做断言；
//   * 密码：只能键盘输入（逐字符加间隔降低丢失率），落位与否交给服务端
//     登录结果判定：WRONG_PASSWORD(3) 就地清空重输，不做整页重载。
const ACCOUNT_AT = [340, 260];
const PASSWORD_AT = [340, 285];

// 游戏侧账号框当前文本（点击触发 focus_field 后 textarea 镜像的就是它）。
export async function loginAccountText(driver) {
  await driver.click(ACCOUNT_AT[0], ACCOUNT_AT[1]);
  await sleep(180);
  return driver.evaluate(
    "((document.getElementById('ime-input') || { value: null }).value)");
}

async function clearFieldKeys(driver) {
  for (let i = 0; i < 24; i += 1) await driver.key('Backspace', 'Backspace', 8);
}

// 写账号（IME 直写，键盘兜底）并断言游戏侧文本一致。
async function typeAccount(driver, account) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.clickField(ACCOUNT_AT[0], ACCOUNT_AT[1]);
    if (attempt === 0) {
      await driver.evaluate(
        `Module.ccall('msime_input', null, ['string', 'number'], [${JSON.stringify(account)}, ${account.length}])`);
    } else {
      await clearFieldKeys(driver);
      await driver.typeAscii(account);
    }
    if (await loginAccountText(driver) === account) return;
    console.log('INFO  账号框写入未生效，重试');
    await driver.screenshot(`login-account-${attempt}`);
  }
  throw new Error('账号框写入失败');
}

async function typePassword(driver, password, clearFirst) {
  await driver.clickField(PASSWORD_AT[0], PASSWORD_AT[1]);
  if (clearFirst) await clearFieldKeys(driver);
  await driver.typeAscii(password);
}

// 返回 'ok' | 'retry'（可就地重试）| 'reload'（需要整页重载）。
export async function attemptLogin(driver, account, password, hadNotice = null) {
  await typeAccount(driver, account);
  await typePassword(driver, password, hadNotice !== null);
  for (let enterTry = 0; enterTry < 3; enterTry += 1) {
    await driver.key('Enter', 'Enter', 13);
    // busy 标签页可能吞掉第一次 Enter；既没离开登录界面、也没弹公告就再按。
    if (await waitFor(async () =>
      await probe(driver, 'msui_state') !== uiState.login ||
      await probe(driver, 'msui_login_notice_active') === 1, 8)) break;
  }
  const state = await probe(driver, 'msui_state');
  if (state !== uiState.login) return 'ok';
  const notice = await probe(driver, 'msui_login_notice_message');
  if (notice === 3 || notice === 20 || notice === 7) {
    // 3=密码错（键盘丢字符）20=账号错 7=已登录：都可以在本地处理后重试。
    console.log(`INFO  登录被拒（notice id=${notice}），关闭公告后重试`);
    await driver.key('Enter', 'Enter', 13);
    await sleep(800);
    return 'retry';
  }
  if (notice >= 0) return 'reload';
  return 'reload';
}

export async function loginAndEnterGame(driver, { account, password, cardIndex, pageUrl }) {
  const wasmReady = () => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'");
  const stateIs = state => async () => await probe(driver, 'msui_state') === state;
  if (!await waitFor(wasmReady, 90)) throw new Error('WASM module did not initialize');
  let hadNotice = null;
  for (let attempt = 0; ; attempt += 1) {
    if (!await waitFor(stateIs(uiState.login), 30))
      throw new Error('login UI did not activate');
    const outcome = await attemptLogin(driver, account, password, hadNotice);
    hadNotice = outcome === 'retry' ? true : null;
    if (outcome === 'ok') break;
    if (outcome === 'retry' && attempt < 5) continue;
    if (attempt >= 2) throw new Error('login never reached world-select');
    const notice = await probe(driver, 'msui_login_notice_message');
    console.log(`INFO  登录未进入世界选择（state=${await probe(driver, 'msui_state')} notice id=${notice}），重载页面重试`);
    await driver.screenshot(`login-retry-${attempt}`);
    await driver.key('Enter', 'Enter', 13);
    await sleep(3000);
    // 先回 about:blank 再进目标页，避免同文档导航下旧页面的 Module/焦点
    // 立刻通过等待检查，导致重试输入落在被换掉的文档里。
    await driver.send('Page.navigate', { url: 'about:blank' });
    await sleep(1200);
    await driver.navigate(pageUrl);
    if (!await waitFor(wasmReady, 90)) throw new Error('WASM module did not re-initialize');
  }
  await driver.retryClick(510, 216, stateIs(uiState.characters));
  if (!await waitFor(stateIs(uiState.characters), 45))
    throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60))
    throw new Error('char-select assets never idle');
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    entered = await waitFor(stateIs(uiState.game), 5);
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(assetsIdle(driver), 120))
    throw new Error('in-game assets never idle');
}

// 启动浏览器并登录进游戏。登录阶段受共享服务端负载影响偶发卡死，
// 用“全新浏览器档案重启一次”兜底（新 profile 也规避任何残留标签页状态）。
export async function startSession({
  binary, debugPort, artifactDir, pageUrl, account, password, cardIndex,
  onRetry = () => {},
}) {
  for (let round = 0; ; round += 1) {
    const profileDir = path.join(artifactDir, `chrome-profile${round ? `-${round}` : ''}`);
    const driver = new BrowserDriver({ binary, debugPort, artifactDir, profileDir });
    try {
      await driver.start();
      await driver.navigate(pageUrl);
      await loginAndEnterGame(driver, { account, password, cardIndex, pageUrl });
      return driver;
    } catch (error) {
      await driver.stop();
      const transient = /login never reached|did not activate|never idle|did not initialize/
        .test(error.message);
      if (round >= 1 || !transient) throw error;
      onRetry(error);
      await sleep(2000);
    }
  }
}

export async function walkTowards(driver, targetX, maxSeconds = 120) {
  const start = Date.now();
  let lastX = null;
  let stuck = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await playerX(driver);
    const dx = targetX - x;
    if (Math.abs(dx) <= 10) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuck += 1;
      if (stuck >= 3) {
        // 「按住方向 + 起跳」跨越台阶/矮墙（原地跳无法产生水平位移）。
        const right = targetX > x;
        const hold = driver.keyHold(
          right ? 'ArrowRight' : 'ArrowLeft',
          right ? 'ArrowRight' : 'ArrowLeft',
          right ? 39 : 37, 900);
        await sleep(250);
        // 按住跳跃键一段时间（瞬时 keyDown/keyUp 在负载下会被客户端丢帧）
        await driver.keyHold('Alt', 'AltLeft', 18, 350);
        await hold;
        await sleep(500);
        const after = await playerX(driver);
        if (Math.abs(after - x) < 2 && stuck >= 8)
          throw new Error(`walk stuck at x=${x} target=${targetX}`);
      }
    } else stuck = 0;
    lastX = x;
    const ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(100);
  }
  const finalX = await playerX(driver);
  if (Math.abs(finalX - targetX) > 10)
    throw new Error(`walk timeout: x=${finalX} target=${targetX}`);
  return finalX;
}

// 走到与 NPC 同一垂直层（对话要求 |dy|<=80）。多层的招募地图上，出生点常
// 在顶层而 NPC 在地面：先正常走到 NPC 的 x，若层不对则向两侧「越过去」扫
// 探（借坡道/台阶/掉落换层），找到同层位置后再折返到 NPC 的 x。
export async function approachNpcLevel(driver, npcX, npcY, label) {
  const playerY = () => probe(driver, 'msplayer_y');
  await walkTowards(driver, npcX, 150).catch(error => {
    console.log(`INFO  ${label}: ${error.message}（继续找同层路径）`);
  });
  if (Math.abs((await playerY()) - npcY) <= 60) return true;
  const firstDir = Math.sign(npcX - (await playerX(driver))) || 1;
  for (const dir of [firstDir, -firstDir]) {
    const right = dir > 0;
    let lastX = await playerX(driver);
    for (let step = 0; step < 70; step += 1) {
      await driver.keyHold(right ? 'ArrowRight' : 'ArrowLeft',
        right ? 'ArrowRight' : 'ArrowLeft', right ? 39 : 37, 300);
      await sleep(120);
      const x = await playerX(driver);
      const y = await playerY();
      if (Math.abs(y - npcY) <= 60) {
        await walkTowards(driver, npcX, 90);
        if (Math.abs((await playerY()) - npcY) <= 60) return true;
        break;
      }
      if (Math.abs(x - lastX) < 2 && step > 3) break;
      lastX = x;
    }
    await walkTowards(driver, npcX, 90).catch(() => {});
  }
  // NPC 在玩家上方且平走换层失败：招募图多为「台阶跳跃图」，用「按住
  // 朝向 + 起跳」的棘轮攀爬（CWKPQ 山谷已验证的原语）。台阶常左右交错
  // （如 Ludi 221024500 左侧楼梯），固定方向会卡在台阶边缘，改为每拍
  // 左右交替、短按（窄步不越界）；每拍检查 |dy|≤60 即停，成功后走回 NPC x。
  if ((await playerY()) - npcY > 60) {
    for (let roundDir of [firstDir, -firstDir]) {
      let lastY = await playerY();
      let flat = 0;
      for (let step = 0; step < 60; step += 1) {
        const right = (step % 2 === 0 ? roundDir : -roundDir) > 0;
        const hold = driver.keyHold(right ? 'ArrowRight' : 'ArrowLeft',
          right ? 'ArrowRight' : 'ArrowLeft', right ? 39 : 37, 260);
        await sleep(120);
        await driver.keyHold('Alt', 'AltLeft', 18, 60);
        await hold;
        await sleep(380);
        const y = await playerY();
        if (Math.abs(y - npcY) <= 60) {
          await walkTowards(driver, npcX, 90).catch(() => {});
          if (Math.abs((await playerY()) - npcY) <= 60) return true;
          break;
        }
        if (Math.abs(y - lastY) < 2) flat += 1; else flat = 0;
        lastY = y;
        if (flat >= 10) break;
      }
      await walkTowards(driver, npcX, 90).catch(() => {});
      if (Math.abs((await playerY()) - npcY) <= 60) return true;
    }
  }
  console.log(`INFO  ${label}: 未能对齐 NPC 层（玩家 y=${await playerY()}，NPC y=${npcY}）`);
  return false;
}

export async function openNpcAt(driver, npcX, npcY, label) {
  const playerY = () => probe(driver, 'msplayer_y');
  let levelOk = await approachNpcLevel(driver, npcX, npcY, label);
  const finalX = await playerX(driver);
  if (Math.abs(finalX - npcX) > 120) throw new Error(`${label}: 走位失败（x=${finalX}）`);
  await sleep(400);
  for (let attempt = 0; attempt < 4; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 1 ||
      await probe(driver, 'msshop_active') === 1, 8)) return;
    // NPC 常站在矮台上：小高度差时原地跳一下再试 Space。
    if (!levelOk && attempt === 1) {
      await driver.keyHold('Alt', 'AltLeft', 18, 300);
      await sleep(600);
    }
    await sleep(500);
  }
  const y = await playerY();
  throw new Error(`${label}: Space 未打开对话` +
    (Math.abs(y - npcY) <= 80 ? '' : `（玩家 y=${y} 与 NPC y=${npcY} 不同层）`));
}

export async function openNpc(driver, x, label) {
  const finalX = await walkTowards(driver, x, 150);
  if (Math.abs(finalX - x) > 100) throw new Error(`${label}: 走位失败（x=${finalX}）`);
  await sleep(400);
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 1 ||
      await probe(driver, 'msshop_active') === 1, 8)) return;
    await sleep(500);
  }
  throw new Error(`${label}: Space 未打开对话/商店`);
}

// 只在对话仍打开时按 Escape（无对话时 Escape 会打开系统菜单，吞掉后续按键）。
export async function closeDialogue(driver, label) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (await probe(driver, 'msnpc_active') === 0 &&
      await probe(driver, 'msshop_active') === 0) {
      await sleep(300);
      return true;
    }
    await driver.key('Escape', 'Escape', 27);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 0 &&
      await probe(driver, 'msshop_active') === 0, 5)) {
      await sleep(400);
      return true;
    }
  }
  throw new Error(`${label}: 窗口未关闭`);
}

// gohome-flow.mjs 的聊天发送原语：Enter 开聊天框 -> 输入 -> Enter 发送。
export async function sendChat(driver, message) {  await driver.key('Enter', 'Enter', 13);
  await sleep(150);
  await driver.typeAscii(message);
  await sleep(200);
  await driver.key('Enter', 'Enter', 13);
}

// —— 悬停扫描 ————————————————————————————————————————————————
// 页面内按行统计橙色像素（ORANGE = 255,128,0；抗锯齿边缘会向米色对话底
// (≈240,220,180) 混合，阈值 r>180、g∈[90,200]、b<90 只收 alpha>0.5 的字芯）。
// 返回 [[gameY, orangePixelCount], ...]（仅非零行）。
export async function sampleOrangeRows(driver, region) {
  const result = await driver.send('Runtime.evaluate', {
    expression: `(() => new Promise(resolve => {
      requestAnimationFrame(() => {
        const region = ${JSON.stringify(region)};
        const canvas = document.getElementById('canvas');
        const gl = canvas.getContext('webgl2') || canvas.getContext('webgl')
          || canvas.getContext('experimental-webgl');
        if (!gl) { resolve({ error: 'game canvas exposes no WebGL context' }); return; }
        const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
        const x0 = Math.round(region.x0 * scaleX), x1 = Math.round(region.x1 * scaleX);
        const y0 = Math.round(region.y0 * scaleY), y1 = Math.round(region.y1 * scaleY);
        const width = x1 - x0 + 1, height = y1 - y0 + 1;
        const pixels = new Uint8Array(width * height * 4);
        const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.readPixels(x0, canvas.height - 1 - y1, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
        const rows = [];
        for (let ry = 0; ry < height; ry += 1) {
          let orange = 0;
          for (let x = 0; x < width; x += 1) {
            const o = (ry * width + x) * 4;
            if (pixels[o] > 180 && pixels[o + 1] >= 90 && pixels[o + 1] <= 200 && pixels[o + 2] < 90)
              orange += 1;
          }
          if (orange > 0) rows.push([region.y1 - ry, orange]);
        }
        resolve(rows);
      });
    }))()`,
    awaitPromise: true,
    returnByValue: true,
  });
  const value = result.result.value;
  if (!value || value.error) throw new Error(`orange sampling failed: ${JSON.stringify(value)}`);
  return value;
}

export function orangeBands(rows, minHeight = 8) {
  const bands = [];
  let run = null;
  for (const [y] of [...rows].sort((a, b) => a[0] - b[0])) {
    if (run && y - run.bottom <= 1) run.bottom = y;
    else {
      if (run && run.bottom - run.top + 1 >= minHeight) bands.push(run);
      run = { top: y, bottom: y };
    }
  }
  if (run && run.bottom - run.top + 1 >= minHeight) bands.push(run);
  return bands.map(b => ({
    top: b.top, bottom: b.bottom, center: Math.round((b.top + b.bottom) / 2),
  }));
}

// 沿对话列移动光标（x=340 在选项矩形 x∈[292,612] 内），每次移动后采样，
// 只接受“覆盖当前光标 y”的橙色行带（因果过滤掉对话框外的地图底色）。
// 单次采样可能读到悬停重绘之前的帧，因此无命中时补采一次；第一相位没
// 扫够 stopAfterRows 行时再补一个错位相位取并集。返回自上而下排序的
// 选项行带 = 选项 0..N。
export async function mapOptionRows(driver, {
  x = 340, fromY = 115, toY = 500, step = 5, stopAfterRows = Infinity,
  region = { x0: 300, x1: 480, y0: 100, y1: 510 },
} = {}) {
  if (await probe(driver, 'msnpc_active') !== 1)
    throw new Error('选项行扫描要求 NPC 对话处于打开状态');
  const found = new Map();
  const hoverAndSample = async y => {
    await driver.move(x, y);
    await sleep(80);
    let bands = orangeBands(await sampleOrangeRows(driver, region))
      .filter(band => band.top - 2 <= y && y <= band.bottom + 2);
    if (bands.length === 0) {
      await sleep(150);
      bands = orangeBands(await sampleOrangeRows(driver, region))
        .filter(band => band.top - 2 <= y && y <= band.bottom + 2);
    }
    for (const band of bands) found.set(band.top, band);
  };
  const scanPass = async offset => {
    for (let y = fromY + offset; y <= toY; y += step) {
      await hoverAndSample(y);
      if (found.size >= stopAfterRows) return true;
    }
    return false;
  };
  if (!await scanPass(0)) await scanPass(Math.max(2, step - 2));
  await driver.move(700, 60); // 光标移出对话，避免悬停态残留
  return [...found.values()].sort((a, b) => a.top - b.top);
}

// 悬停扫描得到的行带是“画布坐标”（UI 场景整体下移 VIEWYOFFSET=10px 绘
// 制），而点击命中矩形在未偏移的 UI 坐标系里——实测点击 band.top-10 恰
// 好命中所选行（191→选项0），band 中心反而会点到下一行。取 top-6 落在
// 命中矩形中部（矩形约 [top-10, top+5]）。
// 另：招募菜单的正文行也会被悬停扫描成行带（KerningPQ 实测：需求说明
// 3 行 + 真选项 3 行 + 底部区域 3 行＝9 带，rows[0] 落在正文上，点击无效
// 且无任何封包）。因此点击后校验「对话推进」（换提示/关框/换图），无效
// 则从该带起继续向下尝试最多 6 个候选带。
export async function clickOptionRow(driver, rows, index, label = '') {
  const row = rows[index];
  if (!row) throw new Error(`选项行 ${index} 未找到（${label}；共 ${rows.length} 行）`);
  const promptBefore = await promptText(driver).catch(() => '');
  const mapBefore = await mapId(driver).catch(() => 0);
  const advanced = async () => {
    if ((await probe(driver, 'msnpc_active')) === 0) return true;
    if ((await mapId(driver)) !== mapBefore) return true;
    return (await promptText(driver)) !== promptBefore;
  };
  for (let i = index; i < rows.length && i < index + 12; i += 1) {
    await driver.click(340, rows[i].top - 6);
    if (await waitFor(advanced, 2)) return rows[i];
  }
  return row;
}

// —— YES / OK 按钮 ————————————————————————————————————————————
// 点击对话底部按钮并等待效果（对话关闭 / 地图变化 / 提示变化）。x 取自
// UINpcTalk/WZ 常量推导的列，y 未命中时沿 footer 网格扫描；该列的误点只会
// 落在对话空白处，不会触发其它按钮。
// 点击对话底部按钮并等待效果（对话关闭 / 地图变化 / 提示变化）。
// 截图像素实证（CWKPQ-created.png vs fm-pq-teleport-0）：对话按钮槽位于
// x≈545..590（Yes 在 570 命中）；sendOk 的 OK 同槽，旧 x=518 偏左落空，
// 且按钮精灵实际绘制在 y≈477..494（旧扫描上界 454 也够不到）。因此
// 每种按钮给多个 x 候选 ×（浅+深）y 网格全扫。该列的误点只会落在对
// 话空白处，不会触发其它按钮。
export const dialogButtonXs = Object.freeze({
  yes: [570],
  no: [622, 610],
  // 620：CWKPQ 远征系列对话框的黄底 OK（实测按钮块 x598~641、cy≈481，
  // 位于白盒下方，视觉定位的蓝底/白底判据都覆盖不到）。放在首位：固定
  // 网格每个 x 要扫 ~30 个 y（每个 5s 生效等待），旧顺序会先空扫 10 分钟。
  ok: [620, 566, 572, 598, 518, 575],
});

function footerScanYs(center = 394) {
  const ys = [];
  for (let delta = 0; delta <= 60; delta += 4) ys.push(center + delta, center - delta);
  for (let y = 462; y <= 496; y += 4) ys.push(y);   // 按钮精灵实际行（截图实证）
  return [...new Set(ys)].filter(y => y > 160 && y < 520);
}

// 按钮实测为「蓝色底带 + 绿色字素」：结束(左) 确认/否(右) 是(中右)。
// 字素 y 随文本高度浮动（同一 OK 出现过 y311 与 y425+），故先视觉定位，
// 旧的固定网格只作兜底。kind → 候选选择：ok/no 取最右，yes 取右二。
function pickVisionButtons(buttons, kind) {
  const sorted = [...buttons].sort((a, b) => a.x - b.x);
  if (!sorted.length) return [];
  const rightmost = sorted[sorted.length - 1];
  if (kind === 'yes' && sorted.length >= 2) return [sorted[sorted.length - 2], rightmost];
  if (kind === 'yes') return [rightmost];
  return [rightmost, ...sorted.slice(0, -1).reverse()];
}

// 截图坐标 → 游戏内部坐标：canvas 实际显示尺寸≠800×600（如 662×497、
// 左缘 79），driver.click 收的是内部坐标并自行做正向映射，故这里必须
// 先做逆向换算，否则视觉定位的按钮会被点偏约 5%。
export async function visionDialogButtons(driver) {
  try {
    const scale = await driver.evaluate(`(() => {
      const c = document.getElementById('canvas');
      const b = c.getBoundingClientRect();
      return { left: b.left, top: b.top, sw: b.width / c.width, sh: b.height / c.height };
    })()`);
    return findDialogButtons(decodePng(await driver.capture()))
      .map(b => ({
        ...b,
        x: Math.round((b.x - scale.left) / scale.sw),
        y: Math.round((b.y - scale.top) / scale.sh),
      }));
  } catch {
    return [];
  }
}

export async function clickDialogButton(driver, kind, effect, effectSeconds = 3) {
  const seen = await visionDialogButtons(driver);
  if (seen.length)
    console.log(`INFO  对话按钮视觉定位 ${kind}: ${seen.map(b => `(${b.x},${b.y})`).join(' ')}`);
  else
    console.log(`INFO  对话按钮视觉定位 ${kind}: 无命中，退回固定网格`);
  for (const b of pickVisionButtons(seen, kind)) {
    await driver.click(b.x, b.y);
    if (await waitFor(effect, effectSeconds)) return b.y;
  }
  const xs = dialogButtonXs[kind];
  for (const x of xs) {
    for (const y of footerScanYs()) {
      await driver.click(x, y);
      if (await waitFor(effect, effectSeconds)) return y;
    }
  }
  throw new Error(`${kind} button click had no effect`);
}

export function dialogAdvanced(driver, { promptBefore, mapBefore }) {
  return async () => {
    if (await probe(driver, 'msnpc_active') === 0) return true;
    const mapNow = await mapId(driver);
    if (mapBefore !== undefined && mapNow !== mapBefore) return true;
    const promptNow = await promptText(driver);
    return promptBefore !== undefined && promptNow !== promptBefore;
  };
}

export function writeArtifacts(driver, dir, extra = {}) {
  fs.mkdirSync(dir, { recursive: true });
  if (driver.errors.length)
    fs.writeFileSync(path.join(dir, 'browser-errors.log'), `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(path.join(dir, 'browser-console.log'), `${driver.logs.join('\n')}\n`);
  for (const [name, text] of Object.entries(extra))
    fs.writeFileSync(path.join(dir, name), text);
}
