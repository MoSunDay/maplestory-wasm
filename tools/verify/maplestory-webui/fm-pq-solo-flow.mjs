// 交付物 2：solo 模式（ServerConstants.USE_PARTYQUEST_SOLO_MODE）逐个进入
// 玩法向导 9010000 列出的全部 13 个组队任务的浏览器 e2e。
//
// 每个 PQ 一个全新浏览器会话（独立 debug 端口 + profile，避免互相污染）：
//   1. waitAccountFree + prepCharacter（按 PQ 要求把 test3NL 调到对应等级，
//      出生点重置回自由市场 910000000，keymap 回填）；
//   2. 登录进游戏 → 走到向导 x=850 Space 开菜单 → 点击第 i 个 PQ 选项 →
//      等待传送到该 PQ 的招募地图（实测与 9010000.js PQ_MAPS 一致）；
//   3. walkTowards 到招募 NPC 的 x → Space 开对话 → 悬停扫描菜单行 →
//      点击「我想参加组队任务」行（CWKPQ 9201113 例外：两段式远征队流程，
//      先点行 0「马上开始吧！」创建远征 + OK，重开对话再点行 1「开始战斗！」+ OK）；
//   4. 无队伍时出现 sendYesNo「单人模式：NPC 队友将与你协同进行副本……」，
//      截图后点 YES → 等待进入入场图（LudiMaze/CWKPQ/BossRush 按前缀断言）；
//   5. 入场图 settle 截图 + 读取 msother_character_count（NPC 队友数）作为
//      补位队友的证据；失败则截图/存 prompt/浏览器 console/docker 日志后继续。
//   已知服务端问题（不修改服务端，仅取证报告）：12/13 招募 NPC 脚本在拼装
//   首个 sendSimple 菜单时调用 cm.getPlayer().isRecvPartySearchInviteEnabled()，
//   而 MapleCharacter 未提供该方法（本服无组队搜索功能），Nashorn 抛
//   TypeError → 对话无法弹出。CWKPQ(9201113) 的远征队菜单不经过该调用，
//   是唯一预期可完成的 PQ。
// 产物目录 /tmp/fm-pq-solo-flow/<序号>-<PQ名>/（每次运行重建）。运行：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/fm-pq-solo-flow.mjs
// 调试时可只跑部分 PQ：FM_PQ_SOLO_ONLY=CWKPQ,OrbisPQ node ...
import fs from 'fs';
import path from 'path';
import { execSync } from 'node:child_process';
import { sleep } from './cdp.mjs';
import {
  clickDialogButton, clickOptionRow, closeDialogue, installStallWatchdog,
  mapId, mapOptionRows, noteProgress, openNpc, openNpcAt, prepCharacter, probe,
  promptText, sendChat, settleShot, startSession, waitAccountFree, waitFor,
  walkTowards, writeArtifacts,
} from './pq-harness.mjs';
import {
  climbAmoriaSteps, climbCwkValley, climbHenesysRoof, climbLudiStairs,
  joinExpeditionPq, leapMagatiaShelf, rideLudiMazePortal,
} from './fm-pq-nav.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPortBase = Number(process.env.FM_PQ_SOLO_DEBUG_PORT || 9350);
const pageUrl = process.env.FM_PQ_SOLO_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.FM_PQ_SOLO_ACCOUNT || 'test3';
const password = process.env.FM_PQ_SOLO_PASSWORD || 'test3';
const cardIndex = Number(process.env.FM_PQ_SOLO_CARD ?? 0);
const charId = Number(process.env.FM_PQ_SOLO_CHAR_ID ?? 223);
const artifactRoot = process.env.FM_PQ_SOLO_ARTIFACTS || '/tmp/fm-pq-solo-flow';

const HUB = 910000000;
const GUIDE_X = 850;
const GUIDE_ROWS = 14;
// option = 玩法向导菜单里的选项序号；entryPrefix 用于实例化后地图带随机
// 房间号的 PQ（LudiMaze 809050000..14 / CWKPQ 610030100..610030800 /
// BossRush 970030100+）。等级与 scripts/npc/<id>.js 的门槛一致。
const PQS = [
  { name: 'KerningPQ', option: 0, level: 25, recruitMap: 103000000, npcX: -540, npcY: 421, npcId: 9020000, entry: '103000800' },
  { name: 'LudiPQ', option: 1, level: 42, recruitMap: 221024500, npcX: 189, npcY: 1664, npcId: 2040034, entry: '922010100' },
  { name: 'LudiMazePQ', option: 2, level: 60, recruitMap: 220000000, npcX: 76, npcY: -712, npcId: 9103001, entryPrefix: '8090500' },
  { name: 'OrbisPQ', option: 3, level: 60, recruitMap: 200080101, npcX: 198, npcY: -47, npcId: 2013000, entry: '920010000' },
  { name: 'PiratePQ', option: 4, level: 75, recruitMap: 251010404, npcX: -1277, npcY: 28, npcId: 2094000, entry: '925100000' },
  { name: 'HenesysPQ', option: 5, level: 200, recruitMap: 100000200, npcX: 1434, npcY: 352, npcId: 1012112, entry: '910010000' },
  { name: 'EllinPQ', option: 6, level: 200, recruitMap: 300030100, npcX: 314, npcY: -63, npcId: 2133000, entry: '930000000' },
  { name: 'MagatiaA', option: 7, level: 78, recruitMap: 261000021, npcX: 10, npcY: 28, npcId: 2112003, entry: '926110000' },
  { name: 'MagatiaZ', option: 8, level: 78, recruitMap: 261000011, npcX: 158, npcY: 132, npcId: 2112004, entry: '926100000' },
  { name: 'AmoriaPQ', option: 9, level: 200, recruitMap: 670010100, npcX: 926, npcY: 2095, npcId: 9201048, entry: '670010200' },
  { name: 'CWKPQ', option: 10, level: 200, recruitMap: 610030020, npcX: 976, npcY: 94, npcId: 9201113, entryPrefix: '6100301', exped: true, climb: true },
  { name: 'TreasurePQ', option: 11, level: 200, recruitMap: 674030100, npcX: 30, npcY: -38, npcId: 9220018, entry: '674030000' },
  { name: 'BossRushPQ', option: 12, level: 200, recruitMap: 970030000, npcX: 64, npcY: 2137, npcId: 9000037, entryPrefix: '9700301' },
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
  const rows = await mapOptionRows(driver, { stopAfterRows: GUIDE_ROWS });
  if (rows.length < GUIDE_ROWS) throw new Error(`选项行扫描不全：${rows.length} 行`);
  return rows;
}

const entryReached = (pq, map) =>
  pq.entryPrefix ? String(map).startsWith(pq.entryPrefix) : String(map) === pq.entry;

function captureServerLog(pqDir, sinceSeconds) {
  try {
    const text = execSync(`docker logs ms-game-server --since ${sinceSeconds}s 2>&1`,
      { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
    fs.writeFileSync(path.join(pqDir, 'docker-ms-game-server.log'), text);
  } catch (error) {
    fs.writeFileSync(path.join(pqDir, 'docker-ms-game-server.log'),
      `docker logs 采集失败：${error.message}\n`);
  }
}

// 招募 NPC 对话打不开时，从服务端日志提取「该 NPC」的脚本异常摘要
// （锚定 `Error thrown: game/npcs/<id>.txt`，避免误伤其它 NPC 的报错）。
function serverErrorExcerpt(sinceSeconds, npcId) {
  try {
    const text = execSync(`docker logs ms-game-server --since ${sinceSeconds}s 2>&1`,
      { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
    const lines = text.split('\n');
    const anchors = [];
    lines.forEach((line, i) => {
      if (line.includes(`npcs/${npcId}.txt`)) anchors.push(i);
    });
    // 兜底：CWKPQ 这类失败不在 npc 脚本文件处报错，而是挂在封包处理器
    // （Error thrown: ...NPCMoreTalkHandler.txt）——取最后一个 Error thrown。
    if (!anchors.length) {
      lines.forEach((line, i) => {
        if (line.includes('Error thrown:')) anchors.push(i);
      });
    }
    if (!anchors.length) return '';
    const lo = Math.max(0, anchors[0] - 2);
    const hi = Math.min(lines.length, anchors[anchors.length - 1] + 7);
    return lines.slice(lo, hi).join('\n');
  } catch (error) {
    return '';
  }
}

async function reconcileHome(driver) {
  // 刚进游戏时 msmap_id 可能短暂为 0（地图尚未加载完成），此时发聊天命令
  // 会被吞、后续方向键也可能落进残留的聊天框——先等真实地图号出现。
  if (!await waitFor(async () => (await mapId(driver)) !== 0, 45))
    throw new Error('地图长时间未加载（mapId=0）');
  if (await mapId(driver) === HUB) return;
  await sendChat(driver, '/gohome');
  if (!await waitFor(async () => await mapId(driver) === HUB, 30)) {
    await sendChat(driver, '/gohome');
    if (!await waitFor(async () => await mapId(driver) === HUB, 30))
      throw new Error(`/gohome 归位失败：${await mapId(driver)}`);
  }
  await sleep(1200);
}


// 常规 PQ：招募菜单行 0 → 服务器 sendYesNo 单人模式确认 → YES 传送。
async function joinRegularPq(driver, pq, rows) {
  await clickOptionRow(driver, rows, 0, `${pq.name} 参加组队任务`);
  if (!await waitFor(async () => (await promptText(driver)).includes('单人模式'), 10))
    throw new Error(`单人模式确认未出现（prompt=${JSON.stringify((await promptText(driver)).slice(0, 120))}）`);
  await settleShot(driver, `${pq.name}-yesno`);
  // 点 YES 的生效判据＝换图（传送）或出现新提示（如「已有其他队伍」sendOk）。
  // 单纯「对话框关闭」不算命中：YES 未打中时客户端常把落点按「否」处理，
  // 同样关框但停留在招募图——此判据让 clickDialogButton 继续尝试下一个
  // 候选按钮，而不是把误 decline 记成成功。
  const promptBefore = await promptText(driver);
  let closedSince = 0;
  await clickDialogButton(driver, 'yes', async () => {
    if ((await probe(driver, 'msnpc_active')) === 1)
      return (await promptText(driver)) !== promptBefore;
    if (!closedSince) closedSince = Date.now();
    await sleep(250);
    return Date.now() - closedSince >= 2000
      && (await mapId(driver)) !== pq.recruitMap;
  }, 8);
}

// 共享环境：ms-ws-proxy 会被其它流程周期性重建（实测 8 分钟一次），客户端
// 断连后 CDP 命令超时/套接字关闭。会话死亡不记为该 PQ 的失败证据，先重启
// 会话重试一次；重试仍失败或错误与连接无关才落盘证据并判 FAIL。
const SESSION_DEAD = /\[cdp\]|login never reached|did not activate|never idle/;

// 共享环境的 ws-proxy 约每 8-12 分钟被外部重建一次（docker inspect 的
// StartedAt 可观测）。在重建刚发生后的「新鲜窗口」启动会话，能给每个
// PQ 留出最长的干净跑道；会话死亡则等待下一个新鲜窗口再重试（至多 3 次）。
function proxyAgeSeconds() {
  try {
    const startedAt = execSync("docker inspect ms-ws-proxy --format '{{.State.StartedAt}}'",
      { encoding: 'utf8' }).trim();
    return Math.max(0, (Date.now() - Date.parse(startedAt)) / 1000);
  } catch (error) {
    return 0;
  }
}

async function startOnFreshProxy() {
  const age = proxyAgeSeconds();
  // 仅当「重启看起来临近」（观察到的重建周期约 8-12 分钟）才等待新窗口；
  // 若已超过 12 分钟仍未重建，说明外部重建已停，直接开跑，靠会话重试兜底。
  if (age < 420 || age > 720) return;
  console.log(`INFO  ws-proxy 已运行 ${Math.round(age)}s（重建临近），等待新鲜窗口…`);
  for (let i = 0; i < 60; i += 1) {
    noteProgress();
    if (proxyAgeSeconds() < 150) return;
    await sleep(10000);
  }
}

async function runSoloPq(pq, index) {
  const pqDir = path.join(artifactRoot, `${String(index).padStart(2, '0')}-${pq.name}`);
  fs.rmSync(pqDir, { recursive: true, force: true });
  fs.mkdirSync(pqDir, { recursive: true });
  console.log(`\n=== [${index + 1}/${PQS.length}] ${pq.name}（等级 ${pq.level}，选项 ${pq.option}）===`);
  for (let attempt = 0; ; attempt += 1) {
    let driver = null;
    let outcome = null;
    try {
      if (!await waitAccountFree(account, 40))
        console.log('WARN  账号登录态未及时清零，继续尝试');
      prepCharacter({ account, charId, level: pq.level });
      await startOnFreshProxy();
      driver = await startSession({
        binary: chromeBinary, debugPort: debugPortBase + index, artifactDir: pqDir,
        pageUrl, account, password, cardIndex,
        onRetry: error => console.log(`INFO  登录重试：${error.message}`),
      });
      if (await mapId(driver) !== HUB) {
        console.log(`INFO  出生地图 ${await mapId(driver)}，/gohome 归位`);
      }
      await reconcileHome(driver);
      const rows = await reopenGuide(driver);
      await clickOptionRow(driver, rows, pq.option, `${pq.name} 向导选项 ${pq.option}`);
      if (!await waitFor(async () => await mapId(driver) === pq.recruitMap, 60))
        throw new Error(`向导未传送到 ${pq.recruitMap}（实测 ${await mapId(driver)}）`);
      await settleShot(driver, `${pq.name}-recruitmap`);
      // 向导 NPC 在传送后不会自动关闭对话；不关掉的话 Space 会去翻旧对话
      // 而不是打开招募 NPC。
      await closeDialogue(driver, `${pq.name} 向导传送后`);
      if (pq.name === 'LudiPQ') {
        await climbLudiStairs(driver, pq);
        await settleShot(driver, `${pq.name}-climbed`);
      }
      if (pq.name === 'LudiMazePQ') await rideLudiMazePortal(driver, pq);
      if (pq.name === 'MagatiaA') await leapMagatiaShelf(driver, pq);
      if (pq.name === 'HenesysPQ') {
        await climbHenesysRoof(driver, pq);
        await settleShot(driver, `${pq.name}-climbed`);
      }
      if (pq.name === 'AmoriaPQ') {
        await climbAmoriaSteps(driver, pq);
        await settleShot(driver, `${pq.name}-climbed`);
      }
      if (pq.climb) {
        await climbCwkValley(driver, pq);
        await settleShot(driver, `${pq.name}-climbed`);
      }
      await openNpcAt(driver, pq.npcX, pq.npcY, `${pq.name} 招募 NPC ${pq.npcId}`);
      // 全量扫描：招募菜单正文（需求说明等）也会被悬停扫描识别成行带，
      // stopAfterRows=4 会停在开头的假行带上（KerningPQ 实测 9~12 带，真
      // 选项在中部 313/332/351），必须扫完整个对话再由 clickOptionRow 的
      // 候选推进逻辑逐带点选。
      const menuRows = await mapOptionRows(driver);
      await settleShot(driver, `${pq.name}-menu`);
      if (pq.exped) await joinExpeditionPq(driver, pq, menuRows);
      else await joinRegularPq(driver, pq, menuRows);
      const entered = await waitFor(async () => entryReached(pq, await mapId(driver)), 90);
      const finalMap = await mapId(driver);
      const npcs = await probe(driver, 'msother_character_count');
      const expected = pq.entryPrefix ? `${pq.entryPrefix}*` : pq.entry;
      if (!entered) throw new Error(`入场图未达到：期望 ${expected}，实测 ${finalMap}`);
      await settleShot(driver, `${pq.name}-entry`);
      outcome = {
        ok: true,
        line: `${pq.name}: PASS(entry=${finalMap}, npcTeammates=${npcs})`,
        message: `${pq.name}: 单人模式进入入场图 ${expected}（实测 ${finalMap}，NPC 队友 ${npcs}）`,
      };
    } catch (error) {
      if (SESSION_DEAD.test(error.message) && attempt < 2) {
        console.log(`INFO  ${pq.name} 会话中断（${error.message}），重启会话重试`);
        try { await driver?.stop(); } catch { /* 忽略 */ }
        driver = null;
        continue;
      }
      outcome = {
        ok: false,
        line: `${pq.name}: FAIL(${error.message})`,
        message: `${pq.name}: ${error.message}`,
      };
      // 招募 NPC 无响应 → 大概率服务端脚本异常；把根因摘要并进 FAIL 结论。
      const excerpt = serverErrorExcerpt(150, pq.npcId);
      if (excerpt) {
        const cause = excerpt.match(/(?:ClassCastException|TypeError|ScriptException|NoSuchMethodException|\w*Exception)[^\n]*/);
        fs.writeFileSync(path.join(pqDir, 'server-error-excerpt.txt'), `${excerpt}\n`);
        if (cause) {
          const suffix = `服务端脚本异常：${cause[0]}`;
          outcome.line = `${pq.name}: FAIL(${error.message}；${suffix})`;
          outcome.message = `${pq.name}: ${error.message}；${suffix}`;
        }
      }
      try { await driver?.screenshot(`${pq.name}-FAIL`); } catch { /* 已停止 */ }
    } finally {
      if (driver) {
        let prompt = '';
        try { prompt = await promptText(driver); } catch { /* 已断开 */ }
        writeArtifacts(driver, pqDir, {
          'prompt-final.txt': `${prompt}\n`,
          'summary.txt': `${outcome ? outcome.line : 'session interrupted'}\n`,
        });
        await driver.stop();
      }
    }
    report(outcome.ok, outcome.message);
    results.push(outcome.line);
    if (!outcome.ok) captureServerLog(pqDir, 300);
    break;
  }
}

// 断点续跑：外部环境会周期性 SIGKILL 整个 e2e 进程，一次跑不完 13 个 PQ。
// 每个 PQ 的产物目录里有 summary.txt（成功或「服务端脚本异常」的 FAIL 视
// 为终态）；未完成/环境性失败的 PQ 在下一轮重跑。FM_PQ_SOLO_FRESH=1 全量。
const pqDirOf = index => path.join(artifactRoot, `${String(index).padStart(2, '0')}-${PQS[index].name}`);
function terminalOutcome(index) {
  try {
    const summary = fs.readFileSync(path.join(pqDirOf(index), 'summary.txt'), 'utf8');
    return summary.includes(': PASS') || summary.includes('服务端脚本异常');
  } catch (error) {
    return false;
  }
}

async function main() {
  installStallWatchdog(240, 'fm-pq-solo-flow');
  const fresh = ['1', 'true'].includes(process.env.FM_PQ_SOLO_FRESH);
  const resume = !fresh &&
    fs.existsSync(path.join(artifactRoot, '00-KerningPQ', 'summary.txt'));
  if (resume) console.log('INFO  检测到已有产物，按 summary.txt 续跑（FM_PQ_SOLO_FRESH=1 可强制全量）');
  else fs.rmSync(artifactRoot, { recursive: true, force: true });
  fs.mkdirSync(artifactRoot, { recursive: true });
  const only = (process.env.FM_PQ_SOLO_ONLY || '').split(',').map(s => s.trim()).filter(Boolean);
  const list = PQS.filter(pq => only.length === 0 || only.includes(pq.name));
  for (let i = 0; i < PQS.length; i += 1) {
    if (!list.includes(PQS[i])) continue;
    if (resume && terminalOutcome(i)) {
      const prior = fs.readFileSync(path.join(pqDirOf(i), 'summary.txt'), 'utf8').trim();
      console.log(`INFO  [${i + 1}/${PQS.length}] ${PQS[i].name} 已有终态结论，跳过：${prior}`);
      results.push(prior);
      continue;
    }
    await runSoloPq(PQS[i], i);
  }
  console.log('\n—— fm-pq-solo-flow 汇总 ——');
  for (const line of results) console.log(`  ${line}`);
  const passed = results.filter(line => line.includes(': PASS')).length;
  console.log(`PASS ${passed}/${list.length}`);
  if (failed) process.exitCode = 1;
}

await main();
