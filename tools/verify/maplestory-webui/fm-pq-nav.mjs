// fm-pq-solo-flow 的地图导航/对话攻略配方（从主流程拆出，保持各自 <400 行）：
// climbLudiStairs＝Ludi 221024500 窄台阶楼梯逐级跳。
// climbCwkValley＝CWKPQ 山谷跳跃图棘轮攀爬；rideLudiMazePortal＝LudiMaze
// 高台隐藏传送门；joinExpeditionPq＝CWKPQ 远征两段式对话。
import { sleep } from './cdp.mjs';
import {
  clickDialogButton, clickOptionRow, mapId, mapOptionRows, noteProgress,
  openNpcAt, playerX, probe, promptText, settleShot, waitFor, walkTowards,
} from './pq-harness.mjs';


// CWKPQ 招募图 610030020 是跳跃图（WZ foothold 实测）：出生台 y=216
// （x -1080..-395）与地面 y=36 之间、以及地面右端 x=575 与 NPC 台 y=96
// 之间都是整高的 foothold 垂墙，平走会原地冻住；唯一路线是借 -784..-444
// 的台阶（y 209→72）「按住右 + 连跳」棘轮爬升，从顶层右缘跳过 -395 墙
// 落到地面，再在 575 墙前跳上 NPC 台（NPC 9201113 @ 976,94）。
export async function climbCwkValley(driver, pq) {
  const pos = () => Promise.all([probe(driver, 'msplayer_x'), probe(driver, 'msplayer_y')])
    .then(([x, y]) => ({ x, y }));
  let p = await pos();
  console.log(`INFO  CWKPQ 爬升起点 x=${p.x} y=${p.y}`);
  if (p.y <= 40) console.log('INFO  CWKPQ: 已在地面层，跳过爬升');
  // A 台阶棘轮爬升：每拍＝按住右 450ms（中途起跳），直到落到地面 y<=40
  for (let i = 0; i < 90 && p.y > 40; i += 1) {
    noteProgress();
    const hold = driver.keyHold('ArrowRight', 'ArrowRight', 39, 450);
    await sleep(150);
    await driver.keyHold('Alt', 'AltLeft', 18, 60);
    await hold;
    await sleep(350);
    const q = await pos();
    if (q.x !== p.x || q.y !== p.y) console.log(`INFO  CWKPQ 爬升 i=${i} x=${q.x} y=${q.y}`);
    p = q;
  }
  if (p.y > 40) throw new Error(`CWKPQ 山谷爬升失败（x=${p.x}, y=${p.y}）`);
  // B 地面右行：x=575 墙前连撞 2 拍即「按住右 + 跳」越墙落到 NPC 台
  let stuck = 0;
  let lastX = p.x;
  for (let i = 0; i < 40 && p.y <= 40; i += 1) {
    noteProgress();
    await driver.keyHold('ArrowRight', 'ArrowRight', 39, 350);
    await sleep(250);
    p = await pos();
    if (Math.abs(p.x - lastX) < 3) stuck += 1; else stuck = 0;
    lastX = p.x;
    if (stuck >= 2) {
      console.log(`INFO  CWKPQ 撞墙 x=${p.x} y=${p.y}，跳跃跨越`);
      const hold = driver.keyHold('ArrowRight', 'ArrowRight', 39, 700);
      await sleep(250);
      await driver.keyHold('Alt', 'AltLeft', 18, 60);
      await hold;
      await sleep(500);
      stuck = 0;
      p = await pos();
    }
  }
  if (p.y <= 40) throw new Error(`CWKPQ 未到达 NPC 台（x=${p.x}, y=${p.y}）`);
  console.log(`INFO  CWKPQ 到达 NPC 台 x=${p.x} y=${p.y}`);
  // C NPC 台上收尾走位（openNpcAt 会再校验层并对 x 精修）
  await walkTowards(driver, pq.npcX, 60);
}

// LudiMaze 招募图 220000000：NPC 9103001 在 y≈-705 的高台上，台阶链到
// -339 后与 -618 之间无落脚点，跳跃不可达；地图自带隐藏传送门对
// ph00(367,45↔平台 y47) ↔ ph00_1(242,-706=NPC 同层)（pt=10，客户端
// 在门框范围 x±25 内按 ↑ 触发，见 Stage::check_portals）。跳上 ph00
// 所在小平台按 ↑ 传送，再走回 NPC。
export async function rideLudiMazePortal(driver, pq) {
  // 门框平台链：主步道 y=102 → 86 → 66 → 47（ph00 平台 x[324,415]）。
  // 循环只保证「跳上台」，不保证 x 落进门框 x±25（[342,392]）——之前
  // 就因此按了 ↑ 却没触发 check_portals（y 66 -> 66）。
  await walkTowards(driver, 367, 30).catch(() => {});
  for (let i = 0; i < 8; i += 1) {
    if (await probe(driver, 'msplayer_y') <= 70) break;
    const right = (await playerX(driver)) < 367;
    const hold = driver.keyHold(right ? 'ArrowRight' : 'ArrowLeft',
      right ? 'ArrowRight' : 'ArrowLeft', right ? 39 : 37, 200);
    await sleep(60);
    await driver.keyHold('Alt', 'AltLeft', 18, 60);
    await hold;
    await sleep(450);
  }
  // 实证（round2 调试）：贴地短按 ↑ 一律不触发（12 连按全部落空），
  // 但「起跳后空中按 ↑」两次实测均成功传送（y=47 → y=-705，即 ph00_1
  // 出生点正上方）。机制：跳跃顶点 y≈-3 仍在门框 y[-55,70] 内，此时
  // 按下 ↑ 触发 Stage::check_portals → find_warp_at。按相位错开重试。
  for (let t = 0; t < 6; t += 1) {
    await walkTowards(driver, 367, 15).catch(() => {});
    const x = await playerX(driver);
    if (x < 346 || x > 388)
      throw new Error(`LudiMazePortal: 门框前走位失败（x=${x}，需 346..388）`);
    if (await probe(driver, 'msplayer_y') < -600) break;   // 已在 NPC 层
    const hold = driver.keyHold('Alt', 'AltLeft', 18, 70);
    await sleep(110 + t * 25);           // 起跳后顶点附近（错开相位）
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 50);
    await hold;
    await sleep(850);
    const y = await probe(driver, 'msplayer_y');
    console.log(`INFO  LudiMazePortal: 第 ${t + 1} 次空中 ↑ 后 y=${y}`);
    if (y < -600) break;
  }
  const yAfter = await probe(driver, 'msplayer_y');
  if (yAfter > -600)
    throw new Error(`LudiMazePortal: 空中 ↑ 未传送（y=${yAfter}，需 < -600）`);
  await walkTowards(driver, pq.npcX, 30).catch(() => {});
}

// Ludi PQ 招募图 221024500：左侧「钢琴键」窄台阶楼梯（WZ foothold 实测）
// 从地面 y=2012 到 NPC 平台 y=1670（x[82,257]，NPC 2040034 @189,1664）
// 共 8 级、级差 38~59px、台阶宽仅 16~62px 且左右交错。楼梯底第一级
// （y1953）在 x[-81,-65]/[-127,-106]；相邻台阶多有 x 重叠 → 路线＝走到
// 每级的「重叠 x」原地跳（垂直上升 ~80px 足够跨级），仅两处（1913→1871、
// 1714→1670）重叠为零/单点，需小幅右漂移。每跳校验 y 至少降 25px，
// 失败则左右小步挪 ±8px 重试（窄台上防冲过头）。
// WZ foothold 复核（stair 级段全量重列）后按「来源层」逐级排布，每跳
// ≤45px 垂直、落带 ≥40px 宽；交叉区 x[-82,129] 由 1871/1833/1787/1789
// 四层全覆盖，偏左/偏右落点都会落在某一层上并继续自愈：
// 1913(x[-82,-20]) → 1871(x[-20,23]) → 1833(x[19,81]) → 1787(x[-82,21])
// 或 1789(x[67,129]) → 1752(x[67,129]) → 1714(x[19,81]) → 1670 NPC 台。
const LUDI_STAIR_STOPS = [
  { x: -95, from: 2012 },               // 地面 → y1955（x[-104,-84]，实落 -83）
  { x: -90, from: 1955, drift: 50 },    // 右漂 ~+48 → y1913（实落 -41）
  { x: -30, from: 1913, aim: 8 },       // 自然漂 → y1871（走位 ±8 留在
                                        // x[-82,-20] 层内，默认 ±30 会右出沿）
  { x: 0, from: 1871, drift: 30, aim: 8 },   // 右漂 +28 → y1833 x[19,81]
                                        // （round7/8 实证 ✓；±8 防左出 -20 沿）
  { x: 23, from: 1833, drift: -15, aim: 3 }, // 左跳上翻 46px → y1787 x[-82,21]：
                                        // 1833 左沿 19，1787 右沿 21——发射窗
                                        // 只在重叠柱 x[19,23]；aim±3 全在台上，
                                        // 起跳后 −15 落 x~5-11 ∈ 1787（左向落
                                        // 带宽裕）。round8 实证 x=12 停位已出沿。
  { x: 15, from: 1787, drift: 80, aim: 4 },  // 右跳上翻 33px → 楼梯踏 y1751-54
                                        // x[67,129]（收敛 ±4 → 11..19 +70 →
                                        // 81..89，避开左缘 67 缺口与右沿 21）
  { x: 90, from: 1752, drift: -40, aim: 8 }, // 左跳上翻 ~38px → y1714 x[19,81]
  { x: 50, from: 1714, drift: 100, aim: 8 }, // 右跳上翻 ~44px → y1670 NPC 台
                                        // fh95/97 x[82,257]
];

// Henesys 100000200 层叠搁架（y 小＝高）：364 NPC 层(x[1357,1523]) 为顶，
// 424(x[637,1343])、484(x[1343,1433])、544(x[1433,1523]) 逐级向右下错位，
// 右端 x1433/1343 处各有一道 60px 竖墙（round5 实证：drop 左行被墙挡死
// 在 x1433 y544 原地）。正确路线是「左跳上翻」：jumpforce 4.5 / 重力
// 0.14 → 跳升极限 ~72px，60px 上升可行（round4 失败是落点 x 太窄，非高
// 度不足）；各级落带 90/706/166px 均宽。
const HENESYS_ROOF_STOPS = [
  { x: 1480, from: 574 },               // 跳上 544（30px，实证通过）
  { x: 1445, from: 544, drift: -50 },   // 左跳上翻 60px 墙 → 484（落 ~1400）
  { x: 1370, from: 484, drift: -50 },   // 左跳上翻 → 424（落 ~1325，右缘 1343 内）
  { x: 1340, from: 424, drift: 40 },    // 右跳跨 14px 缺口上翻 → 364 NPC 层
];

// Amoria 670010100：玩家落在 y2220 步道，NPC 台 y2100（NPC 9201048 @926，
// 台层 x[495,1244]）。中间 y2164 仅一条长台 x[955,1243]。实测（probe_amoria3/4）：
// 空中横移仅 ~0.2px/ms，从 x890 起跳顶点 x≈908 够不到台缘 955 —— 必须把起跳
// 点贴近台缘；且 y2100 层在 y2164 上方 64px（非下方），落台后是「原地直跳
// 80px 穿层回落」而不是走边下落。
const AMORIA_STAIR_STOPS = [
  { x: 945, from: 2220, drift: 100 },   // 贴缘起跳 + 轻右漂：顶点 x≈965 已入
                                        // 台面 [955,1243]，回落落上 y2164
  { x: 976, from: 2164 },               // 原地直跳：顶点 y2084 高过 2100 层，
                                        // 上升穿层（下方穿越合法）后回落其上
];

async function stairHop(driver, driftMs = 0) {
  if (!driftMs) {
    await driver.keyHold('Alt', 'AltLeft', 18, 70);
  } else {
    const right = driftMs > 0;
    const key = right ? 'ArrowRight' : 'ArrowLeft';
    const hold = driver.keyHold(key, key, right ? 39 : 37, Math.abs(driftMs));
    await sleep(50);
    await driver.keyHold('Alt', 'AltLeft', 18, 70);
    await hold;
  }
  // 跳跃全程 ~1.2s（上升 ~0.6s + 下落 ~0.6s）：过早采样会拿到空中途中
  // 坐标，导致「已落到更高级却判失败」或反向误判。
  await sleep(1100);
}

export async function climbStairs(driver, pq, label, stops) {
  const pos = () => Promise.all([playerX(driver), probe(driver, 'msplayer_y')])
    .then(([x, y]) => ({ x, y }));
  let p = await pos();
  console.log(`INFO  ${label} 起点 x=${p.x} y=${p.y}`);
  // 外层整体重试：任何一步跌落到「该级来源层以下 30px+」（多半是坠回
  // 地面）都无法原地恢复，从第一级重新爬（每级落带宽 ≥40px，多次尝试
  // 收敛概率高；单次重爬约 30-40s）。
  let done = false;
  for (let attempt = 0; attempt < 3 && !done; attempt += 1) {
    if (attempt > 0)
      console.log(`INFO  ${label} 整体重试 ${attempt + 1}（当前 x=${p.x} y=${p.y}）`);
    let restart = false;
    for (const stop of stops) {
      if (p.y <= stop.from - 25) continue;        // 已在更高台阶（跳级/残留）
      if (p.y > stop.from + 60) { restart = true; break; }  // 坠落过多级 → 重来
                                       // （+60 而非 +30：隔壁平行层如 1833
                                       //  vs 1787 仅差 46，应走位自愈而非重爬）
      await walkTowards(driver, stop.x, stop.aim ?? 30).catch(() => {});
      let ok = false;
      // 失败后：回到 stop.x 重新对准，并轮换横漂力度（盲推会把人从窄踏
      // 面推下去——walkTowards 的 ±10 收敛对窄踏面太松）。
      // 顺序＝[基准, 右加, 左减]：先右后左。Ludi y1871 级实证：左漂 −30
      // 会落在下方 y1913 级，随后的 walkTowards 回中会走出 1913 右缘
      // （x−20 下方无任何层）直接坠回地面；右漂 +30 则稳落 y1833。
      const base = stop.drift == null ? 0 : stop.drift;
      const variants = [base, base + 30, base - 30];
      for (let r = 0; r < 6 && !ok; r += 1) {
        if (r > 0) await walkTowards(driver, stop.x, stop.aim ?? 12).catch(() => {});
        if (stop.drop) {
          // 走下边沿：目标层就在脚下正下方（层叠搁架），向 drop 方向多走
          // 400ms 掉下去即可——比 60px 边缘跳（移动跳上升极限不稳）可靠。
          const key = stop.drop > 0 ? 'ArrowRight' : 'ArrowLeft';
          const code = stop.drop > 0 ? 39 : 37;
          await driver.keyHold(key, key, code, 400);
          await sleep(700);
        } else {
          await stairHop(driver, variants[r % 3]);
        }
        p = await pos();
        ok = p.y <= stop.from - 25;
        // 逐试留痕：跳全程 ~1.2s，700ms 采样可能仍在空中（y 途中值），
        // 落点诊断必须看最终静止位置。
        console.log(`INFO  ${label} from=y${stop.from} 第${r}试 drift=${variants[r % 3]} → x=${p.x} y=${p.y}${ok ? ' ✓' : ''}`);
      }
      if (!ok) { restart = true; break; }
      console.log(`INFO  ${label} 过 from=y${stop.from} 级 → x=${p.x} y=${p.y}`);
    }
    done = !restart;
  }
  if (!done)
    throw new Error(`${label} 多次整体重试后仍未登顶（x=${p.x} y=${p.y}）`);
  await walkTowards(driver, pq.npcX, 30).catch(() => {});
  p = await pos();
  console.log(`INFO  ${label} 登顶 x=${p.x} y=${p.y}`);
}

// Orbis 200080101：地面 x=30 有直通 NPC 层的长绳（y2=172→y1=-39，WZ
// ladderRope #2；绳底贴地面站立按住 ↑ 即挂绳）。分段爬到 y<-20 出绳，
// 平台沿 fh13/14 右行到 NPC 198。
export async function climbOrbisRope(driver, pq) {
  await walkTowards(driver, 30, 40).catch(() => {});
  let y = await probe(driver, 'msplayer_y');
  for (let r = 0; r < 8 && y > -20; r += 1) {
    await driver.keyHold('ArrowUp', 'ArrowUp', 38, 700);
    await sleep(350);
    y = await probe(driver, 'msplayer_y');
    console.log(`INFO  Orbis 绳第 ${r + 1} 段 y=${y}`);
  }
  if (y > -20) throw new Error(`OrbisPQ: 绳索攀爬未到 NPC 层（y=${y}）`);
  await walkTowards(driver, pq.npcX, 40).catch(() => {});
}

export const climbLudiStairs = (driver, pq) =>
  climbStairs(driver, pq, 'Ludi 楼梯', LUDI_STAIR_STOPS);
// Henesys 出生点可能在左端 454 长步道（x[-128,848]）：skip 逻辑会因
// 454 高于 574/544 级而跳过前几级，但 454 步道右行会跌落到 514→574
// 层，再无路回到屋顶梯区（574→424 差 175px 单跳不可达）。先统一把
// 人引到梯区 x1460（各层右行/左行都能汇集到 574/544），再按级爬。
export async function climbHenesysRoof(driver, pq) {
  await walkTowards(driver, 1460, 45).catch(() => {});
  await climbStairs(driver, pq, 'Henesys 屋顶', HENESYS_ROOF_STOPS);
}
export const climbAmoriaSteps = (driver, pq) =>
  climbStairs(driver, pq, 'Amoria 台阶', AMORIA_STAIR_STOPS);

// MagatiaA 261000021：NPC 台 y31（NPC 2112003 @10）距地板 y148 达 117px，
// 超过单跳 ~80px 且无台阶/绳索/传送门——但出生点 sp0(-246,-148) 落在
// y=1 高层（tread x[-131,-94] 为最近 NPC 台的一块）。向右走到 -100 附近
// 「按住右 + 起跳」跨过 54px 空隙即可落上 NPC 台；若已跌落地板（y148）
// 则本会话无法恢复，抛错换下一轮重试（重新传送又会回到高层）。
export async function leapMagatiaShelf(driver, pq) {
  const pos = () => Promise.all([playerX(driver), probe(driver, 'msplayer_y')])
    .then(([x, y]) => ({ x, y }));
  let p = await pos();
  console.log(`INFO  MagatiaA 跳台起点 x=${p.x} y=${p.y}`);
  if (p.y > 100)
    throw new Error(`MagatiaA: 已跌落地板（y=${p.y}），本会话无法上 NPC 台，需重传`);
  await walkTowards(driver, -100, 30).catch(() => {});
  for (let r = 0; r < 5; r += 1) {
    const hold = driver.keyHold('ArrowRight', 'ArrowRight', 39, 320);
    await sleep(80);
    await driver.keyHold('Alt', 'AltLeft', 18, 70);
    await hold;
    await sleep(800);
    p = await pos();
    console.log(`INFO  MagatiaA 跳台第 ${r + 1} 跳 x=${p.x} y=${p.y}`);
    if (p.y > 100)
      throw new Error(`MagatiaA: 跳台落空跌回地板（x=${p.x} y=${p.y}），需重传`);
    if (p.y <= 56) break;
    await walkTowards(driver, -100, 20).catch(() => {});
  }
  if (p.y > 56)
    throw new Error(`MagatiaA: 未跳上 NPC 台（x=${p.x} y=${p.y}）`);
  await walkTowards(driver, pq.npcX, 30).catch(() => {});
}

// CWKPQ 9201113：远征队（expedition）两段式——先「马上开始吧！」创建远征
// 并 OK 关闭，再重开对话点「开始战斗！」，sendOk 确认后由服务器直接传送。
// CWKPQ 远征两段式。注意本服 createExpedition 返回 void（脚本按 int 判断
// 0/正/负），Nashorn 拿到 undefined 三路全假 → 显示「启动远征队时发生了
// 意外错误」——但远征队作为副作用已经创建（队长＝本人）。因此第一段的
// 成功判据放宽为任意含「远征」的 sendOk；重开对话后即进入管理菜单。
// 菜单形态判据用「行数」而非 prompt（msnpc_prompt 可能返回上一帧的旧文
// 本）：创建菜单 2 行（马上开始/再等等），管理菜单 3 行（成员/开始战斗/
// 终止）。若上一轮残留远征（同角色已是队长），首开直接是管理菜单。
export async function joinExpeditionPq(driver, pq, rows) {
  const firstPrompt = await promptText(driver);
  const creationMenu = firstPrompt.includes('马上开始') || rows.length <= 2;
  if (creationMenu) {
    await clickOptionRow(driver, rows, 0, `${pq.name} 创建远征`);
    if (!await waitFor(async () => (await promptText(driver)).includes('远征'), 8))
      throw new Error(`创建远征确认未出现（prompt=${JSON.stringify((await promptText(driver)).slice(0, 80))}）`);
    const created = await promptText(driver);
    if (created.includes('用完了'))
      throw new Error(`远征挑战次数已用完（prompt=${JSON.stringify(created.slice(0, 80))}）`);
    await settleShot(driver, `${pq.name}-created`);
    await clickDialogButton(driver, 'ok', async () =>
      (await probe(driver, 'msnpc_active')) === 0, 5);
    await sleep(800);
    console.log(`INFO  ${pq.name} 远征注册侧效应已确认，重开 NPC 验证管理菜单`);
    await openNpcAt(driver, pq.npcX, pq.npcY, `${pq.name} 招募 NPC ${pq.npcId}`);
    // 全量扫描：管理菜单上方的说明文字会形成假行带，stopAfterRows=4
    // 会恰好停在假行带上（同 fm-pq-solo-flow 招募菜单的经验）。
    // 链接带实测在 y∈[230,290]，窄窗 step=5 扫 ~28 次即可，省出的时间
    // 是 CWKPQ 三段链（爬升+创建+重开）能否挤进 115s 的关键。
    rows = await mapOptionRows(driver, { fromY: 200, toY: 340, step: 5 });
    if (rows.length === 0) rows = await mapOptionRows(driver);
    const managePrompt = await promptText(driver);
    console.log(`INFO  ${pq.name} 重开后 rows=${rows.length} prompt=${JSON.stringify(managePrompt.slice(0, 60))}`);
    // msnpc_prompt 只返回首行，管理菜单首行「你想做什么？」即判据；
    // 创建菜单首行是「你想组建一支队伍……」，可区分。
    if (!managePrompt.startsWith('你想做什么') && !managePrompt.includes('开始战斗'))
      throw new Error(`创建远征后未见管理菜单（rows=${rows.length}，prompt=${JSON.stringify(managePrompt.slice(0, 80))}）`);
  }
  await clickOptionRow(driver, rows, 1, `${pq.name} 开始战斗`);
  if (!await waitFor(async () => (await promptText(driver)).includes('远征即将开始'), 8))
    throw new Error(`开始战斗确认未出现（prompt=${JSON.stringify((await promptText(driver)).slice(0, 80))}）`);
  await settleShot(driver, `${pq.name}-starting`);
  await clickDialogButton(driver, 'ok', async () =>
    (await probe(driver, 'msnpc_active')) === 0 ||
    !(await mapId(driver) === pq.recruitMap), 8);
}

// 各导航配方（const 箭头函数不提升，集中在文件末尾汇总导出，避免 TDZ）
export const navRecipes = {
  climbCwkValley, climbLudiStairs, climbHenesysRoof, climbAmoriaSteps,
  climbOrbisRope, leapMagatiaShelf, rideLudiMazePortal, joinExpeditionPq,
};
