// 干净登出 helper —— e2e 收尾用"游戏内退出"替代杀浏览器，消除 ws-proxy 半开 TCP
// 让服务端 sessionClosed 拖 2-3 分钟才清 accounts.loggedin 的 anti-multiclient 残留。
//
// 客户端退出路径（代码依据，src/client/）：
//   1. IO/UI.cpp:303-333      ESC 不属于任何 escape_types 窗口时以 escape=true 转发；
//   2. IO/UIStateGame.cpp:242-247  pressed&&escape → statusbar->open_system_popup()；
//   3. IO/UITypes/UIStatusBar.cpp:670-673  open_system_popup 预选 LOGOUT
//      （popup.open(Panel::SYSTEM, Action::LOGOUT)，StatusBarPopup.cpp:249-267）；
//   4. IO/UIStateGame.cpp:249-258  Enter（KeyAction::RETURN，Keyboard.cpp:59 默认绑定）
//      → activate_popup_selection() → dispatch_popup_action(LOGOUT)；
//   5. IO/UITypes/StatusBar/StatusBarActions.cpp:72-85  PLAYER_DC(0x0C) →
//      Session::disconnect()（真实 WS Close，ws-proxy 随之关上游 TCP → 服务端
//      sessionClosed → disconnect 清 loggedin）→ 重连登录服 → UI::LOGIN。
// 因此主路径是 ESC → Enter，无需按钮坐标；点击路径仅作兜底：
//   - 行距/行号：StatusBarPopup.cpp:19-20 BUTTON_TOP=9/BUTTON_HEIGHT=25（post-Chaos）
//     与 StatusBarPreChaos.h popup_row_height()=15（v83 文本行）；
//   - v83 system 弹窗四行 Key Config/Pet Options/Game Options/Quit Game，
//     LOGOUT 是第 3 行（StatusBarPopup.cpp:27-35），PET_OPTIONS 第 1 行，
//     故 Quit 行中心 = mspet_options_menu 探针 + (0, +30)（2 行 x 15px）；
//   - post-Chaos（StatusBar2.img/System，StatusBarPopup.cpp:51-64）PET_OPTIONS
//     是 BtGameOption(index1)、LOGOUT 是 BtGameQuit(index9)，偏移 +8*25=200；
//   - 最终兜底用 WebGL 采样高亮行（open 时 LOGOUT 行恒处 MOUSEOVER/hover 高亮）。
//
// 纯函数风格：模块级只有常量与函数，无共享可变状态。

import { sleep } from './cdp.mjs';
import { readDbConfig, sqlQuery } from './pq-harness-db.mjs';

export const UI_STATE = Object.freeze({
  unknown: 0, login: 1, world: 2, characters: 3, creation: 4, game: 5, cashShop: 6,
});

// GameplayProbes.cpp MISSING_COORDINATE：探针无值时的哨兵。
const MISSING_COORDINATE = -32768;
// UIStatusBar.h Buttons 枚举：BT_MENU=4、BT_OPTIONS=5（0 起：WHISPER,CALLGM,
// CASHSHOP,TRADE,MENU,OPTIONS,...）。
const BT_MENU = 4;
const BT_OPTIONS = 5;
const ESC = { key: 'Escape', code: 'Escape', vk: 27 };
const ENTER = { key: 'Enter', code: 'Enter', vk: 13 };

const noLog = () => {};
const probeNumber = (driver, name, args = []) =>
  driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number',`
    + ` [${args.map(() => "'number'").join(',')}], [${args.join(',')}])`);
const uiStateOf = driver => probeNumber(driver, 'msui_state');
const escapeSql = value => String(value).replace(/'/g, "''");

const stateName = state => Object.entries(UI_STATE)
  .find(([, value]) => value === state)?.[0] || `unknown(${state})`;

async function pressKey(driver, k) {
  await driver.key(k.key, k.code, k.vk);
}

// system 弹窗打开的判定：PET_OPTIONS 行中心探针有值（两代弹窗都含该项）。
async function systemPopupOpen(driver) {
  const y = await probeNumber(driver, 'mspet_options_menu_y');
  return y !== MISSING_COORDINATE ? y : null;
}

// 按 ESC 直到 system 弹窗打开。首个 ESC 可能先关掉别的窗口/聊天焦点，重试即可。
async function openSystemPopup(driver, { tries = 3, log = noLog } = {}) {
  for (let i = 0; i < tries; i += 1) {
    await pressKey(driver, ESC);
    await sleep(350);
    const petY = await systemPopupOpen(driver);
    if (petY !== null) return petY;
    log(`ESC 后 system 弹窗未打开（第 ${i + 1}/${tries} 次），重试`);
  }
  throw new Error('ESC 未能打开系统菜单弹窗（mspet_options_menu_y 恒为 MISSING）');
}

// 轮询 msui_state 直到离开游戏态；返回最终状态。
async function pollLeftGame(driver, timeoutMs, pollMs = 250) {
  const deadline = Date.now() + timeoutMs;
  let state = await uiStateOf(driver);
  while (state === UI_STATE.game && Date.now() < deadline) {
    await sleep(pollMs);
    state = await uiStateOf(driver);
  }
  return state;
}

// 点击兜底：用 PET_OPTIONS 行中心推 LOGOUT 行中心（两代弹窗各一套行距）。
async function clickLogoutRowViaPetAnchor(driver, { log = noLog } = {}) {
  const petY = await openSystemPopup(driver, { log });
  const petX = await probeNumber(driver, 'mspet_options_menu_x');
  const candidates = [
    { dy: 30, label: 'v83 文本行(15px)x2' },
    { dy: 200, label: 'post-Chaos 按钮(25px)x8' },
  ];
  for (const candidate of candidates) {
    await driver.click(petX, petY + candidate.dy);
    const state = await pollLeftGame(driver, 4000);
    if (state !== UI_STATE.game) {
      log(`点击 Quit 行成功（${candidate.label} @${petX},${petY + candidate.dy}）`);
      return state;
    }
    log(`点击 (${petX},${petY + candidate.dy}) 未触发登出（${candidate.label}），换下一候选`);
    await openSystemPopup(driver, { log });
  }
  return UI_STATE.game;
}

// 最终兜底：WebGL 采样弹窗区域，点最亮（MOUSEOVER/hover 高亮）的一行。
async function clickBrightestRow(driver, { log = noLog } = {}) {
  const menuLeft = await probeNumber(driver, 'msstatusbar_control_bounds', [BT_MENU, 0]);
  const menuTop = await probeNumber(driver, 'msstatusbar_control_bounds', [BT_MENU, 1]);
  const optionsLeft = await probeNumber(driver, 'msstatusbar_control_bounds', [BT_OPTIONS, 0]);
  // v83 弹窗锚在菜单键左侧（UIStatusBar.cpp:351-358 system_anchor.l = menu.l-108），
  // post-Chaos 居中于 BtSystem；窗口取两者并集再外扩。
  const anchorLeft = Math.min(menuLeft, optionsLeft > MISSING_COORDINATE ? optionsLeft : menuLeft);
  if (menuLeft <= MISSING_COORDINATE || menuTop <= MISSING_COORDINATE) {
    log('状态栏按钮边界探针无值，跳过像素兜底');
    return UI_STATE.game;
  }
  const region = { x: Math.max(0, anchorLeft - 140), w: 160, y: Math.max(0, menuTop - 330), h: 322 };
  const expression = `(() => new Promise(resolve => { requestAnimationFrame(() => {
    const canvas = document.getElementById('canvas') || document.querySelector('canvas');
    const gl = canvas && (canvas.getContext('webgl2') || canvas.getContext('experimental-webgl'));
    if (!gl) return resolve(null);
    const region = ${JSON.stringify(region)};
    const scaleX = canvas.width / 800, scaleY = canvas.height / 600;
    const bound = gl.getParameter(gl.FRAMEBUFFER_BINDING);
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    const x0 = Math.round(region.x * scaleX), y0 = Math.round(region.y * scaleY);
    const w = Math.max(1, Math.round(region.w * scaleX)), h = Math.max(1, Math.round(region.h * scaleY));
    const buf = new Uint8Array(w * h * 4);
    gl.readPixels(x0, canvas.height - y0 - h, w, h, gl.RGBA, gl.UNSIGNED_BYTE, buf);
    gl.bindFramebuffer(gl.FRAMEBUFFER, bound);
    let best = null;
    for (let py = 0; py < h; py += 2) {
      let total = 0;
      for (let px = 0; px < w; px += 2) {
        const at = (py * w + px) * 4;
        total += (buf[at] + buf[at + 1] + buf[at + 2]) / 3;
      }
      const mean = total / Math.ceil(w / 2);
      if (!best || mean > best.mean) best = { mean, y: region.y + py / scaleY };
    }
    resolve(best);
  }); }))()`;
  await openSystemPopup(driver, { log });
  const envelope = await driver.send('Runtime.evaluate',
    { returnByValue: true, awaitPromise: true, expression });
  const brightest = envelope.result.value;
  if (!brightest || brightest.mean < 40) {
    log(`像素兜底未找到高亮行（best=${JSON.stringify(brightest)}）`);
    return UI_STATE.game;
  }
  const centerX = region.x + region.w / 2;
  await driver.click(centerX, Math.round(brightest.y));
  log(`像素兜底点击 (${centerX},${Math.round(brightest.y)}) 亮度 ${brightest.mean.toFixed(0)}`);
  return pollLeftGame(driver, 4000);
}

// 游戏内干净登出：ESC 预选 Quit Game → Enter 确认，轮询 msui_state 离开 5。
// 返回最终 msui_state（期望 UI_STATE.login=1；只要离开 5 即视为 WS 已关）。
// 不在游戏态时：已登录页返回 1，其余抛出带状态的清晰错误。
export async function logoutToLogin(driver, {
  timeoutMs = 30000, attempts = 3, pollMs = 250, log = noLog,
} = {}) {
  const startedAt = Date.now();
  const initial = await uiStateOf(driver);
  if (initial === UI_STATE.login) {
    log('已在登录页（msui_state=1），无需登出');
    return UI_STATE.login;
  }
  if (initial !== UI_STATE.game) {
    throw new Error(`登出前置失败：当前 msui_state=${stateName(initial)}，`
      + '需要 game(5)（现金商城内请先退出）');
  }
  const remaining = () => timeoutMs - (Date.now() - startedAt);
  for (let attempt = 1; attempt <= attempts && remaining() > 0; attempt += 1) {
    const budget = Math.max(4000, remaining());
    await openSystemPopup(driver, { log });
    await pressKey(driver, ENTER);
    const state = await pollLeftGame(driver, budget, pollMs);
    if (state !== UI_STATE.game) {
      log(`第 ${attempt} 轮 ESC+Enter 登出成功（${((Date.now() - startedAt) / 1000).toFixed(1)}s，`
        + `msui_state=${stateName(state)}）`);
      return state;
    }
    log(`第 ${attempt}/${attempts} 轮 ESC+Enter 后仍在游戏态，重试`);
  }
  for (const fallback of [clickLogoutRowViaPetAnchor, clickBrightestRow]) {
    if (remaining() <= 0) break;
    const state = await fallback(driver, { log });
    if (state !== UI_STATE.game) return state;
  }
  throw new Error(`干净登出失败：ESC+Enter x${attempts} 与点击兜底后 msui_state 仍为 `
    + `${stateName(await uiStateOf(driver))}（耗时 ${((Date.now() - startedAt) / 1000).toFixed(1)}s）`);
}

// 登出后轮询 DB 直到 accounts.loggedin=0（服务端 sessionClosed 异步清账）。
// 只观察不写库、不抛错：超时返回 ok:false 由调用方告警。
export async function waitForAccountLoggedOut(accountName, {
  timeoutMs = 15000, pollMs = 500, db = null,
} = {}) {
  const startedAt = Date.now();
  const config = db || readDbConfig();
  const sql = `SELECT loggedin FROM accounts WHERE name='${escapeSql(accountName)}';`;
  let last = NaN;
  while (Date.now() - startedAt <= timeoutMs) {
    try {
      const out = sqlQuery(sql, config);
      const rows = String(out).split(/\r?\n/).map(line => line.trim()).filter(Boolean);
      last = Number(rows[rows.length - 1]);
      if (last === 0) {
        return { ok: true, waitedMs: Date.now() - startedAt, lastValue: 0 };
      }
    } catch (error) {
      last = NaN;
    }
    await sleep(pollMs);
  }
  return { ok: false, waitedMs: Date.now() - startedAt, lastValue: last };
}
