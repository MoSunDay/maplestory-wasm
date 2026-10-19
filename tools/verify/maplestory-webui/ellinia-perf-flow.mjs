// 魔法密林(101000000)渲染性能验证流程。
//
// 角色 SQL 预置 map=101000000(见 features/changelog/2026-10-19/
// ellinia-map-render-cpu-hotspots.md),真实交互登录进图后做两项采样:
//   1) LazyFS 空闲窗口网络请求增量 —— 验证 queuebitmap 去重前置后,
//      满载地图静止时不再产生持续的新 fetch;
//   2) rAF 帧时间 p95/max + longtask —— 验证平铺背景剔除后帧预算达标
//      (与 run.mjs 的 E2E_MAX_P95_FRAME_MS 同一口径,默认 34ms)。
//
// 用法:
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/ellinia-perf-flow.mjs
// 环境变量:ELLINIA_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT(默认 test1/
// test1/card0/http://127.0.0.1:8000/web/index.html/9266)、ELLINIA_MAP、
// ELLINIA_SETTLE_SECONDS、ELLINIA_SAMPLE_SECONDS、ELLINIA_MAX_P95_FRAME_MS、
// ELLINIA_MAX_LONG_TASK_MS、ELLINIA_MAX_IDLE_NET_REQUESTS。

import fs from 'fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.ELLINIA_DEBUG_PORT || 9266);
const pageUrl = process.env.ELLINIA_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.ELLINIA_ACCOUNT || 'test1';
const password = process.env.ELLINIA_PASSWORD || 'test1';
const cardIndex = Number(process.env.ELLINIA_CARD ?? 0);
const targetMap = Number(process.env.ELLINIA_MAP || 101000000);
const settleSeconds = Number(process.env.ELLINIA_SETTLE_SECONDS || 8);
const sampleSeconds = Number(process.env.ELLINIA_SAMPLE_SECONDS || 12);
const maxP95FrameMs = Number(process.env.ELLINIA_MAX_P95_FRAME_MS || 34);
const maxLongTaskMs = Number(process.env.ELLINIA_MAX_LONG_TASK_MS || 100);
const maxIdleNetRequests = Number(process.env.ELLINIA_MAX_IDLE_NET_REQUESTS || 0);

const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;

const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };

async function waitFor(predicate, seconds = 45) {
  for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); }
  return false;
}
const probe = (driver, name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const uiStateIs = (driver, expected) => () => probe(driver, 'msui_state').then(v => v === expected);
function assetsIdle(driver) {
  return async () => await probe(driver, 'msasset_blocking') === 0 &&
    await driver.evaluate(
      "document.getElementById('asset-loading-screen').classList.contains('is-hidden')");
}
async function evaluateAwait(driver, expression) {
  const result = await driver.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (result.exceptionDetails) throw new Error(`page evaluation failed: ${result.exceptionDetails.text}`);
  return result.result.value;
}

async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(uiStateIs(driver, uiState.login), 30)) throw new Error('login UI did not activate');
  await driver.clickField(340, 260); await driver.typeAscii(account);
  await driver.clickField(340, 285); await driver.typeAscii(password);
  await driver.key('Enter', 'Enter', 13);
  if (!await waitFor(uiStateIs(driver, uiState.world), 45)) throw new Error('world-select UI did not activate');
  await driver.retryClick(510, 216, uiStateIs(driver, uiState.characters));
  if (!await waitFor(uiStateIs(driver, uiState.characters), 45)) throw new Error('character-select UI did not activate');
  if (!await waitFor(assetsIdle(driver), 60)) throw new Error('char-select assets never idle');
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    for (let i = 0; i < 5 && !entered; i += 1) {
      entered = await uiStateIs(driver, uiState.game)(); if (!entered) await sleep(1000);
    }
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(assetsIdle(driver), 120)) throw new Error('in-game assets never idle');
}

const lazyStats = driver => evaluateAwait(driver,
  '(() => Module.LazyFS ? JSON.parse(JSON.stringify(Module.LazyFS.stats)) : null)()');

function startFrameSampler(driver) {
  return driver.evaluate(`(() => {
    const startedAt = performance.now();
    const sample = { frames: [], longTasks: [], previous: startedAt, startedAt, active: true };
    const tick = now => {
      if (!sample.active) return;
      sample.frames.push(now - sample.previous);
      sample.previous = now;
      requestAnimationFrame(tick);
    };
    if (typeof PerformanceObserver === 'function') {
      try {
        sample.observer = new PerformanceObserver(list => {
          for (const entry of list.getEntries()) {
            if (entry.startTime >= sample.startedAt) sample.longTasks.push(entry.duration);
          }
        });
        sample.observer.observe({ type: 'longtask' });
      } catch (_) {}
    }
    window.elliniaPerfSample = sample;
    requestAnimationFrame(tick);
  })()`);
}

async function collectFrameSample(driver) {
  const raw = await driver.evaluate(`(() => {
    const sample = window.elliniaPerfSample;
    sample.active = false;
    sample.observer?.disconnect();
    return JSON.stringify({ frames: sample.frames, longTasks: sample.longTasks });
  })()`);
  const { frames, longTasks } = JSON.parse(raw);
  const sorted = frames.slice(2).sort((a, b) => a - b);
  const at = p => sorted.length ? sorted[Math.min(sorted.length - 1, Math.ceil(sorted.length * p) - 1)] : Infinity;
  return {
    frameCount: sorted.length,
    medianFrameMs: Math.round(at(0.5) * 10) / 10,
    p95FrameMs: Math.round(at(0.95) * 10) / 10,
    maxFrameMs: Math.round((sorted[sorted.length - 1] || Infinity) * 10) / 10,
    longTaskCount: longTasks.length,
    maxLongTaskMs: longTasks.length ? Math.round(Math.max(...longTasks) * 10) / 10 : 0,
  };
}

async function main() {
  const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);

    const mapId = await probe(driver, 'msmap_id');
    report(mapId === targetMap, `地图就位: msmap_id=${mapId} (期望 ${targetMap})`);
    if (mapId !== targetMap) throw new Error(`角色不在目标地图(先 UPDATE characters SET map=${targetMap})`);
    const loading = await probe(driver, 'msstage_loading');
    report(loading === 0, `加载完成: msstage_loading=${loading}`);

    await sleep(settleSeconds * 1000);
    await driver.screenshot('ellinia-01-in-map');

    const statsBefore = await lazyStats(driver);
    await startFrameSampler(driver);
    await sleep(sampleSeconds * 1000);
    const statsAfter = await lazyStats(driver);
    const frames = await collectFrameSample(driver);
    await driver.screenshot('ellinia-02-sampled');

    const netDelta = (statsAfter?.totalRequests ?? 0) - (statsBefore?.totalRequests ?? 0);
    const cacheDelta = (statsAfter?.cacheMisses ?? 0) - (statsBefore?.cacheMisses ?? 0);
    const artifact = {
      mapId, sampleSeconds, statsBefore, statsAfter,
      netRequestDelta: netDelta, cacheMissDelta: cacheDelta, frames,
      thresholds: { maxP95FrameMs, maxLongTaskMs, maxIdleNetRequests },
    };
    fs.writeFileSync(`${driver.artifactDir}/ellinia-perf.json`, `${JSON.stringify(artifact, null, 2)}\n`);
    console.log(`      ${JSON.stringify({ frames, netDelta, cacheDelta })}`);

    report(netDelta <= maxIdleNetRequests,
      `空闲 ${sampleSeconds}s 网络请求增量 ${netDelta} (阈值 ≤ ${maxIdleNetRequests})`);
    report(frames.frameCount >= 60, `帧样本充足: ${frames.frameCount} 帧`);
    report(frames.p95FrameMs <= maxP95FrameMs,
      `帧时间 p95=${frames.p95FrameMs}ms (阈值 ≤ ${maxP95FrameMs})`);
    report(frames.maxLongTaskMs <= maxLongTaskMs,
      `最大 longtask=${frames.maxLongTaskMs}ms (阈值 ≤ ${maxLongTaskMs})`);
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
  } finally {
    if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  ELLINIA_CARD must be 0..3, got ${process.env.ELLINIA_CARD}`); process.exitCode = 1;
} else await main();
