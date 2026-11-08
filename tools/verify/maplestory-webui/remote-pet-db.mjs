// remote-pet-desync-flow.mjs 的 DB 助手：凭据运行时解析（pq-harness-db 同源约定，
// 绝不硬编码），提供账号登录位重置、角色 id 反查与测试宠物的幂等注入。
import { execFileSync } from 'child_process';
import { readDbConfig } from './pq-harness-db.mjs';

export const PET_ITEM = 5000001;
const CONFIG_PATH = process.env.MS_SERVER_CONFIG ||
  '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';

export const openDb = () => readDbConfig(CONFIG_PATH);

export function mysqlRun(db, sql) {
  return execFileSync('mysql', ['-h', db.host, '-u', db.user, db.database,
    '--batch', '--skip-column-names', '-e', sql],
  { encoding: 'utf8', env: { ...process.env, MYSQL_PWD: db.password }, stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}

export const resetLogin = (db, accounts) => mysqlRun(db,
  `UPDATE accounts SET loggedin=0 WHERE name IN (${accounts.map(a => `'${a}'`).join(',')});`);

export const characterId = (db, account, character) => Number(mysqlRun(db,
  `SELECT c.id FROM characters c JOIN accounts a ON a.id=c.accountid ` +
  `WHERE a.name='${account}' AND c.name='${character}';`));

const firstFreeSlot = used => {
  for (let slot = 1; ; slot += 1) if (!used.has(slot)) return slot;
};

// 幂等注入测试宠物：先记 petid 再删（避免孤儿 pets 行）→ 重置召唤位 →
// pets 行 → inventoryitems 行（CASH 栏首个空闲槽位）。
export function injectPet(db, charId) {
  const orphanIds = mysqlRun(db, `SELECT petid FROM inventoryitems WHERE characterid=${charId} ` +
    `AND inventorytype=5 AND itemid=${PET_ITEM};`).split(/\r?\n/).map(l => l.trim()).filter(Boolean);
  mysqlRun(db, `DELETE FROM inventoryitems WHERE characterid=${charId} AND inventorytype=5 AND itemid=${PET_ITEM};`);
  if (orphanIds.length) mysqlRun(db, `DELETE FROM pets WHERE petid IN (${orphanIds.join(',')});`);
  mysqlRun(db, `UPDATE pets SET summoned=0 WHERE petid IN ` +
    `(SELECT petid FROM inventoryitems WHERE characterid=${charId});`);
  const petId = Number(mysqlRun(db, `INSERT INTO pets (name, level, closeness, fullness, summoned, flag) ` +
    `VALUES ('DesyncPet', 1, 0, 100, 0, 0); SELECT LAST_INSERT_ID();`).split(/\r?\n/).pop());
  const used = new Set(mysqlRun(db, `SELECT position FROM inventoryitems WHERE characterid=${charId} ` +
    `AND inventorytype=5;`).split(/\r?\n/).map(l => Number(l.trim())));
  const slot = firstFreeSlot(used);
  mysqlRun(db, `INSERT INTO inventoryitems (type, characterid, itemid, inventorytype, position, ` +
    `quantity, owner, petid, flag, expiration, giftFrom) VALUES ` +
    `(1, ${charId}, ${PET_ITEM}, 5, ${slot}, 1, '', ${petId}, 0, -1, '');`);
  return { petId, slot };
}
