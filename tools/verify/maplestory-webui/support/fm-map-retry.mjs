// FM（910000000）资源量大，进场后地图加载偶发失败/挂起：
// 轮询 msmap_id，加载失败（重试按钮可见）或长时间未就绪时主动触发
// 客户端内置 msmap_retry（Stage::retry_loading），直到地图就绪或超时。
const sleep = (ms) => new Promise((resolve) => { setTimeout(resolve, ms); });

const mapId = (driver) =>
  driver.evaluate("Module.ccall('msmap_id', 'number', [], [])");

const retryShown = (driver) => driver.evaluate(
  "!document.getElementById('asset-loading-retry').classList.contains('is-hidden')");

export async function waitForMapLoad(driver, expectedMapId, timeoutSeconds = 200) {
  // 定时强制重试会在加载进行中打断 FM 的慢加载（20+ chunk 持久化需 >16s），
  // 只在重试按钮出现（加载确定性失败）时才触发 msmap_retry，且重试后
  // 拉长等待让资源重新装载。
  const deadline = Date.now() + timeoutSeconds * 1000;
  for (let t = 0; ; t += 1) {
    const current = await mapId(driver);
    if (current === expectedMapId) return current;
    if (Date.now() > deadline) throw new Error(`地图加载超时：期望 ${expectedMapId} 实际 ${current}`);
    if (await retryShown(driver)) {
      console.log(`INFO  重试按钮可见(${current})，触发 msmap_retry (t=${t})`);
      await driver.evaluate("Module.ccall('msmap_retry', null, [], [])");
      await sleep(8000);
      continue;
    }
    await sleep(4000);
  }
}
