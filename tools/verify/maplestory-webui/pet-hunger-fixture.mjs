// 宠物饥饿 E2E 数据夹具：把选定角色现金栏第 1 格新手宠（5000001）的温饱度写为
// 指定值（默认 10，低于客户端饥饿阈值 40），并向 USE 栏注入定量宠物食品
// 2120000，供 E2E_PET_OPTIONS=hunger 验收复用；结束后按业务键还原温饱度并
// 清理注入的食品行 —— 服务端存档会重写 inventoryitems 主键，清理必须走业务键
// （characterid + inventorytype=2 + itemid），而非复用旧主键；仅当注入前 USE 栏
// 不存在该食品时才整组删除，避免误删玩家已有食品。
// 环境：E2E_PET_HUNGER="fullness,foodQty"（如 "10,10"）/ MS_SERVER_CONFIG。
// Run: E2E_PET_OPTIONS=hunger E2E_PET_HUNGER="10,10" node tools/verify/maplestory-webui/run.mjs

import { execFileSync } from 'child_process';
import { readFileSync } from 'fs';

const defaultConfigPath = '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';
const STARTER_PUPPY = 5000001;
const PET_FOOD = 2120000;

function readDbConfig(file) {
  const values = Object.fromEntries(readFileSync(file, 'utf-8').split(/\r?\n/)
    .filter(line => line.indexOf('=') > 0)
    .map(line => [line.slice(0, line.indexOf('=')).trim(), line.slice(line.indexOf('=') + 1).trim()]));
  const match = /\/\/([^/:]+)(?::(\d+))?\/([^?]+)/.exec(values.URL || '');
  return { host: match?.[1] || '127.0.0.1', port: match?.[2] || '3306',
    database: match?.[3] || 'maplestory', user: values.DB_USER || 'root', password: values.DB_PASS || '' };
}

function mysqlRun(db, sql) {
  return execFileSync('mysql', ['-h', db.host, '-P', db.port, '-u', db.user, db.database,
    '--batch', '--skip-column-names', '-e', sql],
  { encoding: 'utf8', env: { ...process.env, MYSQL_PWD: db.password }, stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}

function queryRows(db, sql) {
  return mysqlRun(db, sql).split('\n').filter(line => line.length > 0).map(line => line.split('\t'));
}

function assertSafeText(value, label) {
  if (`${value}`.includes("'")) throw new Error(`${label} contains a single quote: ${value}`);
  return `${value}`;
}

// "10,5" -> {fullness:10, foodQty:5}；缺失/非字符串 -> null；非空但格式错误 -> throw
export function parsePetHungerEnv(value) {
  if (typeof value !== 'string' || value.trim() === '') return null;
  const match = /^(\d+),(\d+)$/.exec(value.trim());
  if (!match)
    throw new Error(`E2E_PET_HUNGER malformed: '${value}' (expected "fullness,foodQty", e.g. "10,10")`);
  const fullness = Number(match[1]);
  const foodQty = Number(match[2]);
  if (fullness > 99)
    throw new Error(`E2E_PET_HUNGER fullness must stay <= 99 to remain hungry (got ${fullness})`);
  return { fullness, foodQty };
}

function firstCharacter(db, account) {
  assertSafeText(account, 'account');
  const rows = queryRows(db, `SELECT c.id FROM characters c JOIN accounts a ON a.id=c.accountid WHERE a.name='${account}' ORDER BY c.id LIMIT 1;`);
  if (rows.length < 1) throw new Error(`no character found for account ${account}`);
  return Number(rows[0][0]);
}

// 客户端召唤现金栏最靠前的新手宠；夹具写同一行才对得上 E2E 里的召唤对象
function locateStarterPet(db, characterId) {
  const rows = queryRows(db, `SELECT inventoryitemid, petid FROM inventoryitems WHERE characterid=${characterId} AND inventorytype=5 AND itemid=${STARTER_PUPPY} ORDER BY position LIMIT 1;`);
  if (rows.length !== 1)
    throw new Error(`starter puppy for character ${characterId}: expected exactly 1 cash row, got ${rows.length}`);
  return { inventoryitemid: Number(rows[0][0]), petid: Number(rows[0][1]) };
}

export function preparePetHunger({ configPath = defaultConfigPath, account, fullness, foodQty }) {
  if (!Number.isInteger(fullness) || fullness < 0)
    throw new Error(`fullness must be a non-negative integer, got ${fullness}`);
  if (!Number.isInteger(foodQty) || foodQty < 1)
    throw new Error(`foodQty must be a positive integer, got ${foodQty}`);
  const db = readDbConfig(configPath);
  const characterId = firstCharacter(db, account);
  const pet = locateStarterPet(db, characterId);

  const previous = queryRows(db, `SELECT fullness FROM pets WHERE petid=${pet.petid};`);
  if (previous.length !== 1)
    throw new Error(`pets row for petid=${pet.petid}: expected 1, got ${previous.length}`);
  const previousFullness = Number(previous[0][0]);

  mysqlRun(db, `UPDATE pets SET fullness=${fullness} WHERE petid=${pet.petid};`);

  const typeRows = queryRows(db, `SELECT type FROM inventoryitems WHERE characterid=${characterId} LIMIT 1;`);
  if (typeRows.length !== 1)
    throw new Error(`cannot infer inventoryitems.type for character ${characterId}`);
  const typeValue = Number(typeRows[0][0]);

  const preExistingFood = queryRows(db,
    `SELECT COUNT(*) FROM inventoryitems WHERE characterid=${characterId} AND inventorytype=2 AND itemid=${PET_FOOD};`);

  const positionRows = queryRows(db,
    `SELECT COALESCE(MAX(position), 0) + 1 FROM inventoryitems WHERE characterid=${characterId} AND inventorytype=2;`);
  const position = Number(positionRows[0][0]);

  mysqlRun(db, `INSERT INTO inventoryitems (type, characterid, accountid, itemid, inventorytype, position, quantity, owner, petid, flag, expiration, giftFrom) VALUES (${typeValue}, ${characterId}, NULL, ${PET_FOOD}, 2, ${position}, ${foodQty}, '', -1, 0, -1, '');`);

  const inserted = queryRows(db,
    `SELECT inventoryitemid FROM inventoryitems WHERE characterid=${characterId} AND inventorytype=2 AND itemid=${PET_FOOD} AND position=${position};`);
  const evidence = {
    characterId, petid: pet.petid, fullness, foodQty,
    previousFullness, insertedFoodRowId: inserted.length ? Number(inserted[0][0]) : 0
  };
  console.log(`INFO  pet hunger fixture ${JSON.stringify(evidence)}`);

  // 服务端登出存档会整表重写 inventoryitems：食品行若未被消耗会被赋予新主键，
  // 清理必须走业务键；温饱度走 pets 表按 petid 还原。
  const restore = () => {
    mysqlRun(db, `UPDATE pets SET fullness=${previousFullness} WHERE petid=${pet.petid};`);
    const restored = queryRows(db, `SELECT fullness FROM pets WHERE petid=${pet.petid};`);
    if (restored.length !== 1 || Number(restored[0][0]) !== previousFullness)
      throw new Error(`pet fullness restore mismatch for petid=${pet.petid}: ${JSON.stringify(restored)}`);
    if (Number(preExistingFood[0][0]) === 0)
      mysqlRun(db, `DELETE FROM inventoryitems WHERE characterid=${characterId} AND inventorytype=2 AND itemid=${PET_FOOD};`);
    const leftover = queryRows(db,
      `SELECT COUNT(*) FROM inventoryitems WHERE characterid=${characterId} AND inventorytype=2 AND itemid=${PET_FOOD};`);
    return { ...evidence, leftoverFoodRows: Number(leftover[0][0]) };
  };
  return { restore };
}
