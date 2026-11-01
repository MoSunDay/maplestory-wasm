// PQ e2e harness —— 登录进游戏与会话管理层（拆自 pq-harness.mjs）。
// 登录输入（确定性 + 游戏态验证）：UILogin 的账号框是普通文本框（IME 镜像
// 通道），密码框是 crypted 字段（GLFW 键盘通道，无法从页面读取）。共享环境
// 下 CDP 键盘事件偶发被丢弃（实测密码少字符 → 服务端回 WRONG_PASSWORD=3）。
import path from 'path';
import { BrowserDriver, sleep } from './cdp.mjs';
import { assetsIdle, cardCenter, probe, promptText, settleShot, uiState, waitFor } from './pq-harness-game.mjs';

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
  // 高负载下 clickField 的焦点常晚数百毫秒才落到游戏侧，首写打空后又
  // 只能整浏览器重启（40-60s）。这里先做最多 4 轮「点击→等待→直写→
  // 校验」的廉价重试（每轮 ~1s），键盘路径作为最终兜底。
  for (let warmup = 0; warmup < 4; warmup += 1) {
    await driver.clickField(ACCOUNT_AT[0], ACCOUNT_AT[1]);
    await sleep(300 + warmup * 200);
    await driver.evaluate(
      `Module.ccall('msime_input', null, ['string', 'number'], [${JSON.stringify(account)}, ${account.length}])`);
    if (await loginAccountText(driver) === account) return;
    if (warmup === 3) console.log('INFO  账号框直写未生效，改键盘路径');
  }
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.clickField(ACCOUNT_AT[0], ACCOUNT_AT[1]);
    await clearFieldKeys(driver);
    await driver.typeAscii(account);
    if (await loginAccountText(driver) === account) return;
    console.log('INFO  账号框写入未生效，重试');
    await driver.screenshot(`login-account-${attempt}`);
  }
  throw new Error('账号框写入失败');
}

async function typePassword(driver, password, clearFirst) {
  await driver.clickField(PASSWORD_AT[0], PASSWORD_AT[1]);
  if (clearFirst) {
    // 无间隔的连发退格在繁忙标签页会被合并丢弃，反而放大输入噪声。
    for (let i = 0; i < 24; i += 1) {
      await driver.key('Backspace', 'Backspace', 8);
      await sleep(15);
    }
  }
  await driver.typeAscii(password, 80);
  await sleep(400);   // 末尾键位落定后再交给调用方按 Enter，降低丢尾字符概率
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
    // 关公告的 Enter 同样可能被丢：不验证就返回会让 notice 残留盖住登录框，
    // 后续所有点击（含 clickField）全部落在 notice 层上。
    console.log(`INFO  登录被拒（notice id=${notice}），关闭公告后重试`);
    for (let closeTry = 0; closeTry < 3; closeTry += 1) {
      await driver.key('Enter', 'Enter', 13);
      await sleep(600);
      if (await probe(driver, 'msui_login_notice_active') !== 1) break;
    }
    if (await probe(driver, 'msui_login_notice_active') === 1) {
      console.log('INFO  公告未能关闭（Enter 被丢弃），转整页重载');
      return 'reload';
    }
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
    let outcome;
    try {
      outcome = await attemptLogin(driver, account, password, hadNotice);
    } catch (error) {
      // 焦点竞争/CDP 求值超时是瞬时错误：按 reload 路径整页重来，而不是炸穿外层。
      if (!/never gained focus|command timed out/.test(error.message) || attempt >= 2) throw error;
      console.log(`INFO  登录输入中断（${error.message.split('\n')[0]}），重载页面重试`);
      outcome = 'reload';
    }
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

