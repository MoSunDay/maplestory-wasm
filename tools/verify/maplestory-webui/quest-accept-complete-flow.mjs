// 任务系统「接取 → 完成（含选一奖励）」与「服务端拒绝 0x0A」端到端验收。
//
// 覆盖：
//   1) 接取：NPC 1022002 前按 Space → 客户端 fallback 打开 UIQuestDialog
//      (2047 Hero's Gladius)，NEXT 翻到末页 → YES。断言对话框关闭且
//      **不弹 UIOk**：服务端 mode 8 STARTED 变体是 [short][int][int 0]
//      (10B)，若误按 FINISH 变体解析就会在接取时误弹完成提示。
//   2) 完成：再次 Space → completing 对话框。翻到选择页后断言两个奖励格
//      (64x40)，点第二个奖励格 → selected_index=1，YES 提交。服务端回
//      mode 8 FINISH 变体 [short][int][short nextquest] (8B) → UIOk
//      「任务【Hero's Gladius】完成！ 新任务【Rewakening the Gladius】可以接取了。」，
//      断言 msui_uiok_message_hash 与同串 FNV-1a 相等。
//   3) 选一奖励落库：2047 的 Act.img/1/item 有 prop=-1 的 1302015 /
//      1032012 两件奖励；选 index=1 → 服务端只发 1032012。断言
//      queststatus(2047).status=2、ETC 4031144 被扣、EQUIP 出现 1032012
//      且没有 1302015。
//   4) 0x0A 拒绝：客户端本地 acceptable 不校验 start 侧道具；4934
//      Manji's Approval 的 Check.img/0/item 需要 4031447。服务端
//      ItemRequirement 先回 0x0E，QuestActionHandler 随后回 0x0A，客户端
//      UIOk 最终显示「任务【Manji's Approval】暂时无法完成，请确认任务条件。」。
//      断言 UIOk 哈希、4934 仍未开始（queststatus 无行）。
// 探针（src/client/IO/ImeBridge.cpp，导出见 src/client/CMakeLists.txt）：
//   msui_uiok_active / msui_uiok_message_hash
//   msui_questdialog_active / msui_questdialog_button_field(which,field)
//   msui_questdialog_element_field(field) / msui_questdialog_reward_field(index,field)
//   button which: 0=NEXT 1=PREV 2=YES 3=NO；field: 0=x 1=y 2=w 3=h 4=active
//   element field: 4=on_last_page 5=selection_count 6=selected_index
//                  7=page_index 8=declining 9=quest_id 10=completing 11=scripted
//   reward field: 0=x 1=y 2=w 3=h（相对对话框）；越界 -9998，未激活 -9999
//
// 夹具（默认脚本自动准备；QUESTFLOW_SKIP_DB=1 跳过，由操作者按下方清单准备）：
//   * test2/test2，card0 = test2ILA（character id=281，world=4）。
//   * characters.map=102000000, spawnpoint=7：NPC 1022002 就在出生点右侧。
//   * queststatus: 2008=2（2047 接取前置）；删除 2047（NOT_STARTED）；
//     删除 4934；4928=1（4934 的接取前置要求 4928 STARTED）。
//   * inventoryitems: ETC 注入 4031144 x1；删除 EQUIP 1032012（落库断言基线）。
//   * 若服务端还缓存旧角色，SQL 改动后需 docker restart ms-game-server。
//   * keymap: Space=57→5/54（INTERACT）、Enter=13、Escape=27。
//     Space 的 `key` 列是客户端 Keytable 的 MS 扫描码（57），不是 GLFW 32。
//   * 4934/4928 两行在测试结束后按快照还原，避免破坏“全任务完成”夹具。
// 运行：
//   QUESTFLOW_ACCOUNT=test2 QUESTFLOW_PASSWORD=test2 QUESTFLOW_CARD=0 \
//   CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/
//   quest-accept-complete-flow.mjs
// Env: QUESTFLOW_ACCOUNT/PASSWORD/CARD/CHARACTER_ID/DEBUG_PORT/URL/ARTIFACT_DIR/
//      MS_SERVER_CONFIG/SKIP_DB

import fs from 'fs';
import { execFileSync } from 'child_process';
import { BrowserDriver, sleep } from './cdp.mjs';

const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const debugPort = Number(process.env.QUESTFLOW_DEBUG_PORT || 9281);
const pageUrl = process.env.QUESTFLOW_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.QUESTFLOW_ACCOUNT || 'test2';
const password = process.env.QUESTFLOW_PASSWORD || 'test2';
const cardIndex = Number(process.env.QUESTFLOW_CARD ?? 0);
const characterId = Number(process.env.QUESTFLOW_CHARACTER_ID || 281);
const artifactDir = process.env.QUESTFLOW_ARTIFACT_DIR || '/tmp/quest-accept-complete-flow';
const configPath = process.env.MS_SERVER_CONFIG
  || '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const skipDb = process.env.QUESTFLOW_SKIP_DB === '1';
const snapshotPath = `${artifactDir}/queststatus-snapshot.json`;
const MAP = 102000000, SPAWN = 7;
const QID = 2047, REJECT_QID = 4934, REJECT_PREQ = 4928;
const QUEST_ITEM = 4031144, REWARD_SELECTED = 1032012, REWARD_OTHER = 1302015;
const COMPLETE_TEXT = "任务【Hero's Gladius】完成！ 新任务【Rewakening the Gladius】可以接取了。";
const REJECT_TEXT = "任务【Manji's Approval】暂时无法完成，请确认任务条件。";
const uiState = Object.freeze({ login: 1, world: 2, characters: 3, game: 5 });
const cardCenter = i => ({ x: 105 + 120 * i + 25, y: 210 });
let failed = false;
let dbSnapshot = null;
const report = (ok, message) => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${message}`); if (!ok) failed = true; };

const probe = (driver, name, ...args) => driver.evaluate(
  `Module.ccall(${JSON.stringify(name)},'number',${JSON.stringify(args.map(() => 'number'))},${JSON.stringify(args)})`);
const uiokActive = d => probe(d, 'msui_uiok_active');
const uiokHash = d => probe(d, 'msui_uiok_message_hash');
const dialogActive = d => probe(d, 'msui_questdialog_active');
const dialogField = (d, f) => probe(d, 'msui_questdialog_element_field', f);
const dialogButton = (d, w, f) => probe(d, 'msui_questdialog_button_field', w, f);
const rewardField = (d, i, f) => probe(d, 'msui_questdialog_reward_field', i, f);
const hash = text => {
  let h = 2166136261 >>> 0;
  for (const byte of Buffer.from(text, 'utf8')) {
    h ^= byte;
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h | 0;
};

async function waitFor(fn, seconds = 20) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    try { if (await fn()) return true; } catch { /* page busy during asset load */ }
    await sleep(250);
  }
  return false;
}

function readDbConfig(file) {
  const values = Object.fromEntries(fs.readFileSync(file, 'utf-8').split(/\r?\n/)
    .filter(line => line.indexOf('=') > 0)
    .map(line => [line.slice(0, line.indexOf('=')).trim(), line.slice(line.indexOf('=') + 1).trim()]));
  const match = /\/\/([^/:]+)(?::(\d+))?\/([^?]+)/.exec(values.URL || '');
  return {
    host: match?.[1] || '127.0.0.1', port: match?.[2] || '3306',
    database: match?.[3] || 'maplestory',
    user: values.DB_USER || 'root', password: values.DB_PASS || ''
  };
}

const db = skipDb ? null : readDbConfig(configPath);
function mysqlRun(sql) {
  return execFileSync('mysql', ['-h', db.host, '-P', db.port, '-u', db.user,
    db.database, '--batch', '--skip-column-names', '-e', sql],
  { encoding: 'utf8', env: { ...process.env, MYSQL_PWD: db.password },
    stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}
const queryRows = sql => mysqlRun(sql).split('\n').filter(Boolean).map(r => r.split('\t'));

function snapshotQuestRows() {
  return queryRows(
    `SELECT quest,status,time,expires,forfeited,info FROM queststatus
     WHERE characterid=${characterId} AND quest IN (${REJECT_QID},${REJECT_PREQ})`);
}

function prepareDatabase() {
  // 4934/4928 的原始行在结束时还原；其余夹具行按“可重复重跑”归一化。
  // 快照先落盘：上一轮若被中途杀掉（未走 finally），下一轮沿用文件里的
  // 真正原始行，避免把已污染的 4928=STARTED 当成基线再还原回去。
  if (fs.existsSync(snapshotPath)) {
    dbSnapshot = JSON.parse(fs.readFileSync(snapshotPath, 'utf8'));
    console.log('INFO  复用上一轮未还原的 queststatus 快照');
  } else {
    dbSnapshot = snapshotQuestRows();
    fs.mkdirSync(artifactDir, { recursive: true });
    fs.writeFileSync(snapshotPath, JSON.stringify(dbSnapshot));
  }
  mysqlRun(`UPDATE accounts SET loggedin=0 WHERE name='${account}';`);
  mysqlRun(`UPDATE characters SET map=${MAP}, spawnpoint=${SPAWN}
            WHERE id=${characterId};`);
  mysqlRun(`DELETE FROM queststatus WHERE characterid=${characterId}
            AND quest IN (${QID},${REJECT_QID});`);
  mysqlRun(`DELETE FROM queststatus WHERE characterid=${characterId}
            AND quest=${REJECT_PREQ};
            INSERT INTO queststatus (characterid,quest,status,time,expires,forfeited,info)
            VALUES (${characterId},${REJECT_PREQ},1,UNIX_TIMESTAMP(),0,0,0);`);
  mysqlRun(`DELETE FROM inventoryitems WHERE characterid=${characterId}
            AND itemid IN (${QUEST_ITEM},${REWARD_SELECTED});
            INSERT INTO inventoryitems
              (type,characterid,accountid,itemid,inventorytype,position,
               quantity,owner,petid,flag,expiration,giftFrom)
            VALUES (1,${characterId},NULL,${QUEST_ITEM},4,1,1,'',-1,0,-1,'');`);
  const ready = queryRows(`SELECT status FROM queststatus
    WHERE characterid=${characterId} AND quest=2008`)[0]?.[0];
  if (ready !== '2') throw new Error(`queststatus 2008 status=${ready}, need 2 (2047 start prerequisite)`);
  console.log(`INFO  DB 夹具就绪：map=${MAP} spawn=${SPAWN} 4934=NOT_STARTED 4928=STARTED ${QUEST_ITEM}x1`);
}

function restoreDatabase() {
  if (!db || dbSnapshot === null) return;
  try {
    mysqlRun(`DELETE FROM queststatus WHERE characterid=${characterId}
              AND quest IN (${REJECT_QID},${REJECT_PREQ});`);
    if (dbSnapshot.length) {
      const values = dbSnapshot.map(([quest, status, time, expires, forfeited, info]) =>
        `(${characterId},${quest},${status},${time},${expires},${forfeited},${info})`);
      mysqlRun(`INSERT INTO queststatus
        (characterid,quest,status,time,expires,forfeited,info) VALUES
        ${values.join(',')};`);
    }
    fs.rmSync(snapshotPath, { force: true });
    console.log('INFO  已还原 4934/4928 queststatus 快照');
  } catch (error) {
    console.log(`FAIL  还原 4934/4928 快照失败：${error.message}`);
    failed = true;
  }
}

async function loginAndEnterGame(driver) {
  if (!await waitFor(() => driver.evaluate("typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90))
    throw new Error('WASM module did not initialize');
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30))
    throw new Error('login UI did not activate');
  // 登录页刚出现时 LazyFS 仍在后台灌资源，忙渲染进程会丢按键（密码少字符 → reason=4）。
  await waitFor(() => driver.evaluate(
    "Module.LazyFS && ['complete','failed'].includes(Module.LazyFS.itemAssetPreloadState)"), 60);
  let advanced = false;
  for (let attempt = 0; attempt < 4 && !advanced; attempt += 1) {
    await driver.clickField(340, 260); await driver.typeAscii(account, 80);
    await driver.clickField(340, 285); await driver.typeAscii(password, 80);
    await driver.key('Enter', 'Enter', 13);
    advanced = await waitFor(async () =>
      [uiState.world, uiState.characters].includes(await probe(driver, 'msui_state')), 20);
    if (!advanced && await probe(driver, 'msui_login_notice_active') === 1) {
      await driver.key('Enter', 'Enter', 13); await sleep(1500);
    }
  }
  if (!advanced) throw new Error('login never advanced past the login UI');
  await driver.retryClick(510, 216,
    async () => await probe(driver, 'msui_state') === uiState.characters);
  if (!await waitFor(async () => await probe(driver, 'msui_state') === uiState.characters, 45))
    throw new Error('character-select UI did not activate');
  if (!await waitFor(async () => await probe(driver, 'msasset_blocking') === 0, 60))
    throw new Error('char-select assets never idle');
  const center = cardCenter(cardIndex);
  let entered = false;
  for (let attempt = 0; attempt < 4 && !entered; attempt += 1) {
    await driver.doubleClick(center.x, center.y);
    entered = await waitFor(async () =>
      await probe(driver, 'msui_state') === uiState.game, 8);
  }
  if (!entered) throw new Error(`game UI did not activate (card ${cardIndex})`);
  if (!await waitFor(async () => await probe(driver, 'msasset_blocking') === 0, 120))
    throw new Error('in-game assets never idle');
  if (!await waitFor(async () => await probe(driver, 'msui_enabled') === 1, 60))
    throw new Error('game UI never enabled');
  await sleep(1000);
}

async function clickDialogButton(driver, which) {
  const px = await dialogField(driver, 0), py = await dialogField(driver, 1);
  const x = await dialogButton(driver, which, 0), y = await dialogButton(driver, which, 1);
  const w = await dialogButton(driver, which, 2), h = await dialogButton(driver, which, 3);
  if ([px, py, x, y, w, h].some(v => v <= -9000))
    throw new Error(`quest dialog button ${which} has no hitbox (px=${px} py=${py} x=${x} y=${y} w=${w} h=${h})`);
  await driver.click(px + x + (w >> 1), py + y + (h >> 1));
}

async function advanceToLastPage(driver) {
  for (let step = 0; step < 10 && await dialogField(driver, 4) !== 1; step += 1) {
    const page = await dialogField(driver, 7);
    await clickDialogButton(driver, 0);
    await waitFor(async () => await dialogField(driver, 7) !== page, 5);
  }
  return await dialogField(driver, 4) === 1;
}
async function openOffer(driver, label) {
  // 进场后 NPC/地图资源仍在流式加载，单次按键可能被丢弃；重试直到对话框出现。
  let opened = false;
  for (let attempt = 0; attempt < 5 && !opened; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    opened = await waitFor(async () => await dialogActive(driver) === 1, 3);
  }
  report(opened, `${label}：Space 打开任务对话框`);
  if (!opened) {
    await driver.screenshot(`${label}-no-dialog`);
    throw new Error(`${label}: quest dialog never opened`);
  }
  return {
    questId: await dialogField(driver, 9),
    completing: await dialogField(driver, 10),
    scripted: await dialogField(driver, 11)
  };
}

async function acceptQuest(driver) {
  const offer = await openOffer(driver, 'accept');
  report(offer.questId === QID && offer.completing === 0,
    `accept：对话框是接取 2047（实测 quest=${offer.questId} completing=${offer.completing}）`);
  if (offer.questId !== QID || offer.completing !== 0)
    throw new Error('accept: unexpected quest offer');
  const fields = await Promise.all([4, 5, 6, 7, 8].map(f => dialogField(driver, f)));
  report(fields[1] === 0 && fields[2] === -1 && fields[4] === 0,
    `accept：无选一奖励、未选、非拒绝确认（fields=${fields.join(',')}）`);
  report(await advanceToLastPage(driver), 'accept：NEXT 翻到末页');
  await driver.screenshot('accept-last-page');
  await clickDialogButton(driver, 2);
  report(await waitFor(async () => await dialogActive(driver) === 0, 8), 'accept：YES 后对话框关闭');
  await sleep(1500);
  const notice = await uiokActive(driver);
  report(notice === 0,
    `accept：服务端 mode 8 STARTED 不弹 UIOk（实测 active=${notice}）`);
  await driver.screenshot('accept-no-uiok');
}

async function completeQuest(driver) {
  const offer = await openOffer(driver, 'complete');
  report(offer.questId === QID && offer.completing === 1,
    `complete：对话框是交付 2047（实测 quest=${offer.questId} completing=${offer.completing}）`);
  if (offer.questId !== QID || offer.completing !== 1)
    throw new Error('complete: unexpected quest offer');
  report(await advanceToLastPage(driver), 'complete：翻到奖励选择页');
  const selections = await dialogField(driver, 5);
  report(selections === 2, `complete：选一奖励 2 格（实测 ${selections}）`);
  const cells = [];
  for (let i = 0; i < 2; i += 1)
    cells.push(await Promise.all([0, 1, 2, 3].map(f => rewardField(driver, i, f))));
  report(cells.every(([, , w, h]) => w === 64 && h === 40),
    `complete：奖励格几何 64x40（实测 ${JSON.stringify(cells)}）`);
  await driver.screenshot('complete-selection');
  const [rx, ry, rw, rh] = cells[1];
  const px = await dialogField(driver, 0), py = await dialogField(driver, 1);
  await driver.click(px + rx + (rw >> 1), py + ry + (rh >> 1));
  report(await waitFor(async () => await dialogField(driver, 6) === 1, 3),
    'complete：点击第二个奖励格后 selected_index=1');
  await clickDialogButton(driver, 2);
  report(await waitFor(async () => await dialogActive(driver) === 0, 8), 'complete：YES 后对话框关闭');
  const shown = await waitFor(async () => await uiokActive(driver) === 1, 10);
  report(shown, 'complete：服务端 FINISH 变体弹出 UIOk');
  const got = await uiokHash(driver);
  report(got === hash(COMPLETE_TEXT),
    `complete：UIOk 文案哈希匹配（got=${got} want=${hash(COMPLETE_TEXT)}）`);
  await driver.screenshot('complete-uiok');
  if (shown) {
    await driver.key('Enter', 'Enter', 13);
    await waitFor(async () => await uiokActive(driver) === 0, 5);
  }
  return { shown, got };
}

async function rejectQuest(driver) {
  const offer = await openOffer(driver, 'reject');
  report(offer.questId === REJECT_QID && offer.completing === 0,
    `reject：对话框是接取 4934（实测 quest=${offer.questId} completing=${offer.completing}）`);
  if (offer.questId !== REJECT_QID || offer.completing !== 0)
    throw new Error('reject: unexpected quest offer');
  report(await advanceToLastPage(driver), 'reject：NEXT 翻到末页');
  await clickDialogButton(driver, 2);
  report(await waitFor(async () => await dialogActive(driver) === 0, 8), 'reject：YES 后对话框关闭');
  const shown = await waitFor(async () => await uiokActive(driver) === 1, 10);
  report(shown, 'reject：服务端拒绝后弹出 UIOk');
  const got = await uiokHash(driver);
  report(got === hash(REJECT_TEXT),
    `reject：UIOk 是 0x0A 任务无法完成提示（got=${got} want=${hash(REJECT_TEXT)}）`);
  report(got !== hash(COMPLETE_TEXT), 'reject：未误用完成提示文案');
  await driver.screenshot('reject-uiok');
  if (shown) {
    await driver.key('Enter', 'Enter', 13);
    await waitFor(async () => await uiokActive(driver) === 0, 5);
  }
}

async function verifyDatabase() {
  if (!db) {
    console.log('INFO  QUESTFLOW_SKIP_DB=1，跳过 DB 断言');
    return;
  }
  const statusReady = await waitFor(() => {
    if (skipDb) return true;
    const row = queryRows(`SELECT status FROM queststatus
      WHERE characterid=${characterId} AND quest=${QID}`)[0];
    return row?.[0] === '2';
  }, 10);
  report(statusReady, 'DB：queststatus 2047 status=2（完成）');
  report(queryRows(`SELECT COUNT(*) FROM inventoryitems
    WHERE characterid=${characterId} AND itemid=${QUEST_ITEM}`)[0][0] === '0',
  `DB：任务道具 ${QUEST_ITEM} 已消耗`);
  const reward = queryRows(`SELECT inventorytype,quantity FROM inventoryitems
    WHERE characterid=${characterId} AND itemid=${REWARD_SELECTED}`);
  report(reward.length === 1 && reward[0][0] === '1' && reward[0][1] === '1',
    `DB：选中的奖励 ${REWARD_SELECTED} 已入 EQUIP 栏（rows=${JSON.stringify(reward)}）`);
  report(queryRows(`SELECT COUNT(*) FROM inventoryitems
    WHERE characterid=${characterId} AND itemid=${REWARD_OTHER}`)[0][0] === '0',
  `DB：未选中的奖励 ${REWARD_OTHER} 未发放`);
  report(queryRows(`SELECT COUNT(*) FROM queststatus
    WHERE characterid=${characterId} AND quest=${REJECT_QID}`)[0][0] === '0',
  'DB：4934 仍未开始（NOT_STARTED）');
}

async function main() {
  if (!skipDb) prepareDatabase();
  const driver = new BrowserDriver({
    binary: chromeBinary, debugPort, artifactDir
  });
  try {
    await driver.start();
    await driver.navigate(pageUrl);
    await loginAndEnterGame(driver);
    const map = await probe(driver, 'msmap_id');
    report(map === MAP, `登录地图 = ${MAP}（实测 ${map}）`);
    if (map !== MAP) throw new Error(`unexpected map ${map}; DB fixture map not loaded`);
    await acceptQuest(driver);
    await completeQuest(driver);
    await rejectQuest(driver);
  } catch (error) {
    report(false, `flow aborted: ${error.message}`);
    await driver.screenshot('flow-aborted').catch(() => {});
  } finally {
    if (driver.errors.length)
      fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`,
        `${driver.errors.join('\n')}\n`);
    fs.writeFileSync(`${driver.artifactDir}/browser-console.log`,
      `${driver.logs.join('\n')}\n`);
    console.log(`artifacts: ${driver.artifactDir}`);
    await driver.stop();
    if (!skipDb) {
      await waitFor(() => mysqlRun(`SELECT loggedin FROM accounts WHERE name='${account}'`) === '0', 30); // 等落库
      await verifyDatabase();
      restoreDatabase();
    }
  }
  if (failed) process.exitCode = 1;
}

if (!Number.isInteger(cardIndex) || cardIndex < 0 || cardIndex > 3) {
  console.log(`FAIL  QUESTFLOW_CARD must be 0..3, got ${process.env.QUESTFLOW_CARD}`);
  process.exitCode = 1;
} else await main();
