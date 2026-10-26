// PQ e2e harness —— 游戏态探针 / 通用等待 / 停滞看门狗层（拆自 pq-harness.mjs）。
import { sleep } from './cdp.mjs';

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
