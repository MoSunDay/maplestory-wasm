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
//   5. 入场图 settle 截图 + 解析服务端 [SoloPQ] 日志（spawned/removed 数）作为
//      补位队友的运行时证据（CWKPQ 惰性补挂标记 lazy 不判失败）；失败则
//      截图/存 prompt/浏览器 console/docker 日志后继续。
//   已知服务端问题（不修改服务端，仅取证报告）：12/13 招募 NPC 脚本在拼装
//   首个 sendSimple 菜单时调用 cm.getPlayer().isRecvPartySearchInviteEnabled()，
//   而 MapleCharacter 未提供该方法（本服无组队搜索功能），Nashorn 抛
//   TypeError → 对话无法弹出。CWKPQ(9201113) 的远征队菜单不经过该调用，
//   是唯一预期可完成的 PQ。
// 产物目录 /tmp/fm-pq-solo-flow/<序号>-<PQ名>/（每次运行重建）。运行：
//   CHROME_BIN=/usr/bin/chromium-browser \
//   node tools/verify/maplestory-webui/fm-pq-solo-flow.mjs
// 调试时可只跑部分 PQ：FM_PQ_SOLO_ONLY=CWKPQ,OrbisPQ node ...
// 高负载主机调试开关（不影响默认断言强度）：FM_PQ_SOLO_DIRECT=1 直刷
// 招募图跳过向导段；FM_PQ_SOLO_KEEP_PROFILE=1 复用 chrome-profile 热缓存。
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
  joinExpeditionPq, climbOrbisRope, leapMagatiaShelf, rideLudiMazePortal,
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
  // spawn：FM_PQ_SOLO_DIRECT 直刷时的出生门索引（选招募 NPC 同层最近的门，
// 免去 approachNpcLevel 跨层盲走——KPQ 出生点 0 在 (-1888,-60) 高台，
// NPC 9020000 在 (-540,421) 地面，门 32=(-849,373) 同层相距 309px）。
  { name: 'KerningPQ', option: 0, level: 25, recruitMap: 103000000, npcX: -540, npcY: 421, npcId: 9020000, entry: '103000800', spawn: 32 },
  { name: 'LudiPQ', option: 1, level: 42, recruitMap: 221024500, npcX: 189, npcY: 1664, npcId: 2040034, entry: '922010100' },
  // soloOptional：天然可单刷的副本（LudiMaze 迷宫无人数机关、BossRush
  // NPC 文案明示「独自一人挑战」），minPlayers=1 → fill=0 无补位属预期。
  { name: 'LudiMazePQ', option: 2, level: 60, recruitMap: 220000000, npcX: 76, npcY: -712, npcId: 9103001, entryPrefix: '8090500', soloOptional: true },
  { name: 'OrbisPQ', option: 3, level: 60, recruitMap: 200080101, npcX: 198, npcY: -47, npcId: 2013000, entry: '920010000' },
  // spawn=2（next00 门 1490,80）：与 NPC 同层，2767px 纯平走 ~25s；
  // 默认 sp(-1717,151) 距 NPC 440px 且低 123px，棘轮爬层在负载下 >100s。
  { name: 'PiratePQ', option: 4, level: 75, recruitMap: 251010404, npcX: -1277, npcY: 242, npcId: 2094000, entry: '925100000', spawn: 0 },
  { name: 'HenesysPQ', option: 5, level: 200, recruitMap: 100000200, npcX: 1434, npcY: 352, npcId: 1012112, entry: '910010000' },
  { name: 'EllinPQ', option: 6, level: 200, recruitMap: 300030100, npcX: 314, npcY: 150, npcId: 2133000, entry: '930000000' },
  { name: 'MagatiaA', option: 7, level: 78, recruitMap: 261000021, npcX: 10, npcY: 28, npcId: 2112003, entry: '926110000' },
  { name: 'MagatiaZ', option: 8, level: 78, recruitMap: 261000011, npcX: 158, npcY: 132, npcId: 2112004, entry: '926100000' },
  { name: 'AmoriaPQ', option: 9, level: 200, recruitMap: 670010100, npcX: 926, npcY: 2095, npcId: 9201048, entry: '670010200' },
  { name: 'CWKPQ', option: 10, level: 200, recruitMap: 610030020, npcX: 976, npcY: 94, npcId: 9201113, entryPrefix: '6100301', exped: true, climb: true, lazyTeammates: true, menuScan: { fromY: 200, toY: 340, step: 5 } },
  // spawn=0（sp 门 1,-66）：距 NPC(30,-38) 仅 29px，落地即达。
  { name: 'TreasurePQ', option: 11, level: 200, recruitMap: 674030100, npcX: 30, npcY: -38, npcId: 9220018, entry: '674030000', spawn: 0 },
  { name: 'BossRushPQ', option: 12, level: 200, recruitMap: 970030000, npcX: 64, npcY: 2137, npcId: 9000037, entryPrefix: '9700301', soloOptional: true },
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

// NPC 队友在场的运行时证据：服务端 [SoloPQ] 日志（EventInstanceManager
// setupSoloTeammates/removeSoloTeammates 打点）。注意 msother_character_count
// 只统计远程玩家字符表（GameplayProbes.cpp Stage::get_chars()），spawnNPC 的
// 队友不入该表、恒为 0，不可作为补位证据。
function soloSpawnEvidence(sinceIso) {
  try {
    const text = execSync(`docker logs ms-game-server --since ${sinceIso} 2>&1`,
      { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
    const spawned = [...text.matchAll(/\[SoloPQ\] instance [^ ]+ spawned (\d+) NPC teammates/g)]
      .reduce((sum, m) => sum + Number(m[1]), 0);
    const removed = (text.match(/\[SoloPQ\] instance [^ ]+ removed NPC teammates/g) || []).length;
    return { spawned, removed };
  } catch (error) {
    return { spawned: -1, removed: -1, error: error.message };
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
  // FM_PQ_SOLO_KEEP_PROFILE=1 时保留 chrome-profile/（HTTP + IndexedDB 的
  // WASM/NX 缓存），高负载主机上冷启动登录可能吃掉整个 120s bash 预算。
  if (process.env.FM_PQ_SOLO_KEEP_PROFILE && fs.existsSync(pqDir)) {
    for (const entry of fs.readdirSync(pqDir))
      if (entry !== 'chrome-profile')
        fs.rmSync(path.join(pqDir, entry), { recursive: true, force: true });
  } else {
    fs.rmSync(pqDir, { recursive: true, force: true });
  }
  fs.mkdirSync(pqDir, { recursive: true });
  const pqStartedAtIso = new Date().toISOString();
  console.log(`\n=== [${index + 1}/${PQS.length}] ${pq.name}（等级 ${pq.level}，选项 ${pq.option}）===`);
  for (let attempt = 0; ; attempt += 1) {
    let driver = null;
    let outcome = null;
    try {
      if (!await waitAccountFree(account, 40))
        console.log('WARN  账号登录态未及时清零，继续尝试');
      prepCharacter({
        account, charId, level: pq.level,
        map: process.env.FM_PQ_SOLO_DIRECT === '1' ? pq.recruitMap : 910000000,
        spawnpoint: process.env.FM_PQ_SOLO_DIRECT === '1' ? (pq.spawn ?? 0) : 0,
      });
      await startOnFreshProxy();
      driver = await startSession({
        binary: chromeBinary, debugPort: debugPortBase + index, artifactDir: pqDir,
        pageUrl, account, password, cardIndex,
        onRetry: error => console.log(`INFO  登录重试：${error.message}`),
      });
      // FM_PQ_SOLO_DIRECT=1：SQL 直刷到招募图，跳过向导传送段（向导→招募图
      // 链路已由 fm-pq-teleports.mjs 的 13/13 覆盖；本流程专注招募→入场证据）。
      if (process.env.FM_PQ_SOLO_DIRECT === '1') {
        // 直刷后客户端进入地图需数秒；立即读 mapId 常得 0（UI 未就绪），
        // 短等待内自愈，避免烧掉整轮会话。
        if (!await waitFor(async () => await mapId(driver) === pq.recruitMap, 25))
          throw new Error(`直刷出生未落在招募图 ${pq.recruitMap}（实测 ${await mapId(driver)}）`);
        await settleShot(driver, `${pq.name}-recruitmap`);
      } else {
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
      }
      if (pq.name === 'LudiPQ') {
        await climbLudiStairs(driver, pq);
        await settleShot(driver, `${pq.name}-climbed`);
      }
      if (pq.name === 'LudiMazePQ') await rideLudiMazePortal(driver, pq);
      if (pq.name === 'MagatiaA') await leapMagatiaShelf(driver, pq);
      if (pq.name === 'OrbisPQ') await climbOrbisRope(driver, pq);
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
      const menuRows = await mapOptionRows(driver, pq.menuScan);
      await settleShot(driver, `${pq.name}-menu`);
      if (pq.exped) await joinExpeditionPq(driver, pq, menuRows);
      else await joinRegularPq(driver, pq, menuRows);
      const entered = await waitFor(async () => entryReached(pq, await mapId(driver)), 90);
      const finalMap = await mapId(driver);
      const expected = pq.entryPrefix ? `${pq.entryPrefix}*` : pq.entry;
      if (!entered) throw new Error(`入场图未达到：期望 ${expected}，实测 ${finalMap}`);
      await settleShot(driver, `${pq.name}-entry`);
      // 补位队友运行时证据：服务端 [SoloPQ] 日志。CWKPQ 在 changedMap 惰性
      // 补挂，入场图阶段合法为 0（lazy 标记），其余 PQ 无日志即判 FAIL。
      const solo = soloSpawnEvidence(pqStartedAtIso);
      if (!pq.lazyTeammates && !pq.soloOptional && solo.spawned < 1)
        throw new Error(`服务端无 [SoloPQ] 补位日志（spawned=${solo.spawned}${solo.error ? '，' + solo.error : ''}）`);
      outcome = {
        ok: true,
        line: `${pq.name}: PASS(entry=${finalMap}, soloNpc=${solo.spawned}${pq.lazyTeammates ? ',lazy' : ''}${pq.soloOptional && solo.spawned < 1 ? ',soloOptional' : ''}${solo.removed ? `,cleaned=${solo.removed}` : ''})`,
        message: `${pq.name}: 单人模式进入入场图 ${expected}（实测 ${finalMap}，服务端补位队友 ${solo.spawned} 名${pq.lazyTeammates ? '，CWKPQ 惰性补挂于 changedMap' : ''}${pq.soloOptional && solo.spawned < 1 ? '（该副本可原生单刷，无补位为预期）' : ''}）`,
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

// 初始清场：KEEP_PROFILE 时先把各 PQ 目录里的 chrome-profile 暂存到
// artifactRoot 之外再整体删除（否则删 summary.txt 触发的全量清场会把
// 热缓存一并抹掉，重跑又回到冷启动 120s 超时）。清完再搬回原位。
const keepProfileOn = () => ['1', 'true'].includes(process.env.FM_PQ_SOLO_KEEP_PROFILE);
function wipeArtifactRoot() {
  const stashRoot = `${artifactRoot}.profile-stash`;
  const stashed = [];
  if (keepProfileOn() && fs.existsSync(artifactRoot)) {
    fs.rmSync(stashRoot, { recursive: true, force: true });
    fs.mkdirSync(stashRoot, { recursive: true });
    for (const name of fs.readdirSync(artifactRoot)) {
      const src = path.join(artifactRoot, name, 'chrome-profile');
      if (fs.existsSync(src)) {
        const dst = path.join(stashRoot, name);
        fs.renameSync(src, dst);
        stashed.push([dst, path.join(artifactRoot, name)]);
      }
    }
  }
  fs.rmSync(artifactRoot, { recursive: true, force: true });
  for (const [src, parent] of stashed) {
    fs.mkdirSync(parent, { recursive: true });
    fs.renameSync(src, path.join(parent, 'chrome-profile'));
  }
}

async function main() {
  installStallWatchdog(240, 'fm-pq-solo-flow');
  const fresh = ['1', 'true'].includes(process.env.FM_PQ_SOLO_FRESH);
  const resume = !fresh &&
    fs.existsSync(path.join(artifactRoot, '00-KerningPQ', 'summary.txt'));
  if (resume) console.log('INFO  检测到已有产物，按 summary.txt 续跑（FM_PQ_SOLO_FRESH=1 可强制全量）');
  else wipeArtifactRoot();
  fs.mkdirSync(artifactRoot, { recursive: true });
  const only = (process.env.FM_PQ_SOLO_ONLY || '').split(',').map(s => s.trim()).filter(Boolean);
  const list = PQS.filter(pq => only.length === 0 || only.includes(pq.name));
  // 按 ONLY 给定顺序迭代（未指定时保持 PQS 序）：否则头部未终态的 PQ
  // 会逐轮吃光 115s 预算，把列表后面的 PQ 完全堵死（Pirate/Orbis 实证）。
  const order = only.length
    ? only.map(name => PQS.findIndex(pq => pq.name === name)).filter(idx => idx >= 0)
    : PQS.map((_, idx) => idx);
  for (const i of order) {
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
