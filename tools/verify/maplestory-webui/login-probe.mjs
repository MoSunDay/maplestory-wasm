// 登录凭据实证探针：typeAscii('test1')x2 后用户名字段的真实内容。
//
// 背景：run.mjs / gm-shop-flow.mjs 以 test1/test1 登录，服务端命令日志却证明
// 落在账号 test1test1(id 26)。服务端 MapleClient.login 按 name=? 精确匹配，
// UILogin 发包干净，SaveLogin 默认 false 且 Settings 落在临时 FS（无预填），
// 静态排查无法定位 → 本探针在登录现场截图（用户名明文、密码星号计数）并
// dump IME 桥状态，配合 `SELECT id,name FROM accounts WHERE loggedin=1` 一锤
// 定音。
//
// 用法：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/login-probe.mjs
//   # 探针停在角色选择界面（保持会话存活）期间：
//   mysql ... -e "select id,name from accounts where loggedin=1;"
//
// 停止：Ctrl-C（或 LOGIN_PROBE_SUBMIT=0 只打字不回车，等 20s 自动退出）。

import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.LOGIN_PROBE_DEBUG_PORT || 9249);
const pageUrl = process.env.LOGIN_PROBE_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.LOGIN_PROBE_ACCOUNT || 'test1';
const password = process.env.LOGIN_PROBE_PASSWORD || 'test1';
const submit = process.env.LOGIN_PROBE_SUBMIT !== '0';

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });

async function waitFor(driver, predicate, timeoutSeconds, label) {
  for (let i = 0; i < timeoutSeconds; i += 1) {
    if (await predicate()) return true;
    await sleep(1000);
  }
  throw new Error(`Timed out: ${label}`);
}

async function imeSnapshot(driver) {
  return await driver.evaluate(`(() => {
    const input = document.getElementById('ime-input');
    return JSON.stringify({
      imeActive: !!(window.MapleWasmIME && window.MapleWasmIME.active),
      imeComposing: !!(window.MapleWasmIME && window.MapleWasmIME.composing),
      activeElement: document.activeElement ? (document.activeElement.id || document.activeElement.tagName) : null,
      imeValue: input ? input.value : null,
      imeCaret: input ? input.selectionStart : null
    });
  })()`);
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
try {
  await driver.start();
  await driver.navigate(pageUrl);
  await waitFor(driver, async () =>
    await driver.evaluate("typeof Module !== 'undefined' && Module.calledRun === true") &&
    await driver.evaluate("Module.ccall('msui_state','number',[],[])") === uiState.login,
    90, 'login UI active');

  await driver.screenshot('lp0-login-empty');
  console.log('[before]', await imeSnapshot(driver));

  // 与 run.mjs 完全一致的输入序列。
  await driver.click(340, 260);
  await sleep(300);
  console.log('[clicked username]', await imeSnapshot(driver));
  await driver.typeAscii(account);
  await sleep(300);
  console.log('[typed username]', await imeSnapshot(driver));
  await driver.screenshot('lp1-username-typed');

  console.log('[textarea rect]', await driver.evaluate(`(() => {
    const r = document.getElementById('ime-input').getBoundingClientRect();
    const canvas = document.getElementById('canvas').getBoundingClientRect();
    const p = { x: canvas.left + 340 * canvas.width / 800, y: canvas.top + 285 * canvas.height / 600 };
    const hit = document.elementFromPoint(p.x, p.y);
    return JSON.stringify({ rect: [r.x, r.y, r.width, r.height],
      clickPoint: [p.x, p.y],
      hitAtPassword: hit ? (hit.id || hit.tagName) : null });
  })()`));
  await driver.click(340, 285);
  await sleep(300);
  console.log('[clicked password]', await imeSnapshot(driver));
  await driver.typeAscii(password);
  await sleep(300);
  console.log('[typed password]', await imeSnapshot(driver));
  await driver.screenshot('lp2-both-typed');

  if (!submit) {
    console.log('LOGIN_PROBE_SUBMIT=0，保持页面 20s 供检查后退出');
    await sleep(20000);
  } else {
    await driver.key('Enter', 'Enter', 13);
    await waitFor(driver, async () =>
      await driver.evaluate("Module.ccall('msui_state','number',[],[])") >= uiState.world,
      45, 'world select after login');
    await driver.screenshot('lp3-world-select');
    console.log('[after login]', await imeSnapshot(driver));
    console.log('会话存活中——现在执行: ' +
      'mysql -h127.0.0.1 -uroot -p... maplestory -e "select id,name from accounts where loggedin=1;"');
    await sleep(15000);
    await driver.screenshot('lp4-final');
  }
  console.log('PASS  login probe finished');
} finally {
  await driver.stop();
}
