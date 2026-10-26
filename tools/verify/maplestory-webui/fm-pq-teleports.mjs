// 交付物 1：自由市场「玩法向导」9010000 的 13 条 PQ 传送全部验证。
//
// 单浏览器会话：登录 test2/card0 → 自由市场(910000000) → 走到向导 x=850 →
// Space 打开菜单（13 个 PQ 选项 + #L99# 我要离开）。对每个选项 0..12：
//   1. 悬停扫描（hover 行重绘为橙色 + 页面内 WebGL 采样）得到选项行带；
//   2. 点击第 i 行 → 等待 msmap_id == 目的地（60s，资源按需流式加载）；
//   3. 截图 fm-pq-teleport-<i>-<mapid>；
//   4. /gohome（gohome-flow 的聊天原语）回到 910000000，走回 850 重新开单。
// 产物目录 /tmp/fm-pq-teleports（每次运行重建，全新 IndexedDB 避免陈旧
// NX 缓存）。运行：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/fm-pq-teleports.mjs
import fs from 'fs';
import path from 'path';
import { sleep } from './cdp.mjs';
import {
  closeDialogue, installStallWatchdog, mapId, mapOptionRows, noteProgress, openNpc,
  prepCharacter, probe, promptText, clickOptionRow, sendChat, settleShot, startSession,
  waitAccountFree, waitFor, writeArtifacts,
} from './pq-harness.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.FM_PQ_TP_DEBUG_PORT || 9332);
const pageUrl = process.env.FM_PQ_TP_URL || 'http://127.0.0.1:8000/web/index.html';
// 共享服务端上另有一条自动化流程在用 test2/test2ILA（同角色抢操作会把本
// 流程的点击/传送打散），因此默认改用同批次的空闲账号 test3 / test3NL。
const account = process.env.FM_PQ_TP_ACCOUNT || 'test3';
const password = process.env.FM_PQ_TP_PASSWORD || 'test3';
const cardIndex = Number(process.env.FM_PQ_TP_CARD ?? 0);
const charId = Number(process.env.FM_PQ_TP_CHAR_ID ?? 223);
const artifactDir = process.env.FM_PQ_TP_ARTIFACTS || '/tmp/fm-pq-teleports';

const HUB = 910000000;
const GUIDE_X = 850;
// 与 scripts/npc/9010000.js 的 PQ_MAPS 一一对应（选项 i 的目的地）。
const DESTINATIONS = [
  103000000, 221024500, 220000000, 200080101, 251010404, 100000200,
  300030100, 261000021, 261000011, 670010100, 610030020, 674030100, 970030000,
];
const NAMES = [
  'KerningPQ', 'LudiPQ', 'LudiMazePQ', 'OrbisPQ', 'PiratePQ', 'HenesysPQ',
  'EllinPQ', 'MagatiaA', 'MagatiaZ', 'AmoriaPQ', 'CWKPQ', 'TreasurePQ', 'BossRushPQ',
];

const results = [];
let failed = false;
const report = (ok, message) => {
  noteProgress();
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`);
  if (!ok) failed = true;
};

async function reopenGuide(driver) {
  await openNpc(driver, GUIDE_X, '玩法向导 9010000');
  const prompt = await promptText(driver);
  if (!prompt.includes('玩法向导') || !prompt.includes('组队任务'))
    throw new Error(`玩法向导菜单未出现（prompt=${JSON.stringify(prompt.slice(0, 80))}）`);
  const rows = await mapOptionRows(driver, { stopAfterRows: DESTINATIONS.length + 1 });
  if (rows.length < DESTINATIONS.length + 1)
    throw new Error(`选项行扫描不全：${rows.length} 行（期望 ≥14）`);
  return rows;
}

async function goHomeAndReturn(driver) {
  await sendChat(driver, '/gohome');
  const home = await waitFor(async () => await mapId(driver) === HUB, 30);
  const after = await mapId(driver);
  report(home, `/gohome 回到 910000000（实测 ${after}）`);
  if (!home) throw new Error(`/gohome 未回到自由市场：${after}`);
  await sleep(1200);
}

// 共享环境：ms-ws-proxy 会被其它流程周期性重建（实测 8 分钟一次），客户端
// 断连后 CDP 命令超时/套接字关闭。此类「会话死亡」不算传送验证失败，
// 应当重启浏览器会话并重试当前选项。
// 「assets not idle」同样按可重试处理：CWKPQ(610030020) 等大图的资源
// 流依赖 ws-proxy，重建窗口内会停摆导致 90s 内不空闲（实测 12:30 的
// teleports 运行在选项 10 处遇此情形），换新会话（资源已在服务端缓存）
// 重试即可恢复。
const SESSION_DEAD = /\[cdp\]|login never reached|did not activate|never idle|assets not idle/;

async function main() {
  installStallWatchdog(240, 'fm-pq-teleports');
  fs.rmSync(artifactDir, { recursive: true, force: true });
  fs.mkdirSync(artifactDir, { recursive: true });
  if (!await waitAccountFree(account))
    console.log('WARN  账号登录态未及时清零，继续尝试');
  prepCharacter({ account, charId });

  const login = () => startSession({
    binary: chromeBinary, debugPort, artifactDir, pageUrl,
    account, password, cardIndex,
    onRetry: error => console.log(`INFO  登录重试：${error.message}`),
  });

  let driver;
  try {
    driver = await login();
    let rows = null;
    let restarts = 0;
    for (let i = 0; i < DESTINATIONS.length;) {
      const expected = DESTINATIONS[i];
      if (rows === null) {
        // 会话（重）启动后的公共前置：等地图真正加载（刚进游戏 msmap_id
        // 可能短暂为 0，此时聊天命令会被吞）、归位到自由市场并打开向导菜单。
        if (!await waitFor(async () => (await mapId(driver)) !== 0, 45))
          throw new Error(`地图长时间未加载（mapId=0）`);
        if (await mapId(driver) !== HUB) {
          console.log(`INFO  出生地图为 ${await mapId(driver)}，/gohome 归位`);
          await sendChat(driver, '/gohome');
          if (!await waitFor(async () => await mapId(driver) === HUB, 30))
            throw new Error(`/gohome 归位失败：${await mapId(driver)}`);
        }
        rows = await reopenGuide(driver);
        if (i === 0) report(true, `玩法向导菜单打开，悬停扫描到 ${rows.length} 个选项行`);
      }
      try {
        await clickOptionRow(driver, rows, i, `${NAMES[i]} 选项 ${i}`);
        const arrived = await waitFor(async () => await mapId(driver) === expected, 60);
        const reached = await mapId(driver);
        report(arrived && reached === expected,
          `选项 ${i} ${NAMES[i]}: 传送到 ${expected}（实测 ${reached}）`);
        if (!arrived) throw new Error(`未到达 ${expected}（实测 ${reached}）`);
        await settleShot(driver, `fm-pq-teleport-${i}-${expected}`);
        await closeDialogue(driver, `${NAMES[i]} 传送后`);
        await goHomeAndReturn(driver);
        rows = await reopenGuide(driver);
        // 放在整个选项周期（传送→截图→归位→重开菜单）成功之后再落账，
        // 避免会话中断重试导致同一选项重复计数。
        results.push(`${NAMES[i]}(选项${i}) -> ${expected}: PASS`);
        i += 1;
      } catch (error) {
        if (SESSION_DEAD.test(error.message) && restarts < 6) {
          restarts += 1;
          console.log(`INFO  选项 ${i} ${NAMES[i]} 会话中断（${error.message}），重启会话重试`);
          await driver.stop();
          rows = null;
          let relaunched = false;
          for (let round = 0; round < 3 && !relaunched; round += 1) {
            try {
              driver = await login();
              relaunched = true;
            } catch (loginError) {
              console.log(`INFO  重启会话失败（${loginError.message}），再试`);
              await sleep(3000);
            }
          }
          if (relaunched) continue;
        }
        report(false, `选项 ${i} ${NAMES[i]}: ${error.message}`);
        results.push(`${NAMES[i]}(选项${i}) -> ${expected}: FAIL(${error.message})`);
        try {
          await driver.screenshot(`fm-pq-teleport-${i}-FAIL`);
          await closeDialogue(driver, `${NAMES[i]} 失败恢复`);
          await goHomeAndReturn(driver);
          rows = await reopenGuide(driver);
        } catch (recoverError) {
          throw new Error(`选项 ${i} 之后无法恢复流程：${recoverError.message}`);
        }
        i += 1;
      }
    }
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
  } finally {
    if (driver) {
      writeArtifacts(driver, driver.artifactDir, {
        'summary.txt': `${results.join('\n')}\n`,
      });
      console.log(`artifacts: ${driver.artifactDir}`);
      await driver.stop();
    }
  }

  console.log('—— fm-pq-teleports 汇总 ——');
  for (const line of results) console.log(`  ${line}`);
  const passed = results.filter(line => line.endsWith(': PASS')).length;
  console.log(`PASS ${passed}/${DESTINATIONS.length}`);
  if (failed) process.exitCode = 1;
}

await main();
