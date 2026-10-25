// 宠物提速背书数据夹具：把绝对 speed/jump 装备值写入选定角色 -7 鞋位（已装备鞋），
// 供 E2E_PET_OPTIONS（pickup/follow）提速验收复用；结束按业务键（characterid +
// inventorytype=-1 + position=-7）重新定位并还原原值、断言命中恰好 1 行 —— 服务端
// 存档会重写 inventoryitemid，还原必须走新鲜定位而非复用旧主键。
// 环境：E2E_PET_SHOE_STATS="speed,jump"（绝对装备值，写入 -7 鞋位；需 E2E_PET_OPTIONS 同用；按业务键还原并断言命中 1 行）/ MS_SERVER_CONFIG。
// Run: E2E_PET_OPTIONS=pickup E2E_PET_SHOE_STATS="40,27" node tools/verify/maplestory-webui/run.mjs

import { execFileSync } from 'child_process';
import { readFileSync } from 'fs';

const defaultConfigPath = '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';

// DB 访问（cash-shop-modal-guards 同款解析，仅重置/写回装备数值，不硬编码凭据）
function readDbConfig(file) {
  const values = Object.fromEntries(readFileSync(file, 'utf8').split(/\r?\n/)
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
// 账号串由 harness 控制；仍拒绝单引号防注入
function assertSafeText(value, label) {
  if (`${value}`.includes("'")) throw new Error(`${label} contains a single quote: ${value}`);
  return `${value}`;
}

// "40,27" -> {speed:40,jump:27}；缺失/非字符串 -> null；非空但格式错误 -> throw
export function parseShoeStatsEnv(value) {
  if (typeof value !== 'string' || value.trim() === '') return null;
  const match = /^(\d+),(\d+)$/.exec(value.trim());
  if (!match) {
    throw new Error(`E2E_PET_SHOE_STATS malformed: '${value}' (expected "speed,jump" absolute values, e.g. "40,27")`);
  }
  return { speed: Number(match[1]), jump: Number(match[2]) };
}

// 与 harness card-0 选择一致：取账号下 id 最小的角色
export function firstCharacterCard(db, account) {
  assertSafeText(account, 'account');
  const rows = queryRows(db, `SELECT c.id, c.name FROM characters c JOIN accounts a ON a.id=c.accountid WHERE a.name='${account}' ORDER BY c.id LIMIT 1;`);
  if (rows.length < 1) throw new Error(`no character found for account ${account}`);
  return { id: Number(rows[0][0]), name: rows[0][1] };
}

// -7 鞋位业务键定位；服务端存档会重写 inventoryitemid，故不缓存主键
export function locateShoe(db, characterId) {
  const rows = queryRows(db, `SELECT i.inventoryitemid, e.speed, e.jump FROM inventoryitems i JOIN inventoryequipment e ON e.inventoryitemid=i.inventoryitemid WHERE i.characterid=${characterId} AND i.inventorytype=-1 AND i.position=-7;`);
  if (rows.length !== 1) {
    throw new Error(`pet shoe slot -7 for character ${characterId}: expected exactly 1 row, got ${rows.length}`);
  }
  return { inventoryitemid: Number(rows[0][0]), speed: Number(rows[0][1]), jump: Number(rows[0][2]) };
}

export function boostShoeStats({ configPath, account, speed, jump }) {
  assertSafeText(account, 'account');
  const db = readDbConfig(configPath || defaultConfigPath);
  const card = firstCharacterCard(db, account);
  const before = locateShoe(db, card.id);
  const applied = { speed: Number(speed), jump: Number(jump) };
  mysqlRun(db, `UPDATE inventoryequipment SET speed=${applied.speed}, jump=${applied.jump} WHERE inventoryitemid=${before.inventoryitemid};`);
  const written = locateShoe(db, card.id);
  if (written.speed !== applied.speed || written.jump !== applied.jump) {
    throw new Error(`pet shoe boost not applied: expected ${applied.speed}/${applied.jump}, got ${written.speed}/${written.jump}`);
  }
  console.log(`INFO  pet shoe boost ${card.name}#${card.id} slot=${before.inventoryitemid} ${before.speed}/${before.jump} -> ${applied.speed}/${applied.jump}`);
  const restore = () => {
    const current = locateShoe(db, card.id);
    if (current.speed !== applied.speed || current.jump !== applied.jump) {
      console.log(`WARN  pet shoe values drifted ${current.speed}/${current.jump}, restoring anyway`);
    }
    mysqlRun(db, `UPDATE inventoryequipment SET speed=${before.speed}, jump=${before.jump} WHERE inventoryitemid=${current.inventoryitemid};`);
    const restored = locateShoe(db, card.id);
    if (restored.speed !== before.speed || restored.jump !== before.jump) {
      throw new Error(`pet shoe restore failed: expected ${before.speed}/${before.jump}, got ${restored.speed}/${restored.jump}`);
    }
    return { restoredTo: before, via: current.inventoryitemid };
  };
  return { character: card, before, applied, restore };
}
