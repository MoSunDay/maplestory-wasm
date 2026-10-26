// PQ e2e harness —— 数据库预置层（2026-10-26 自 pq-harness.mjs 按职责拆出）。
// 与 scripts/test_accounts/create_accounts.py 的 read_db_config 相同：凭据
// 运行时从服务端 configuration.ini 解析，绝不硬编码。
import { execFileSync } from 'child_process';
import fs from 'fs';
import { sleep } from './cdp.mjs';

export const CONFIG_INI =
  '/data/maplestory-wasm/link_repos/MapleStory-Server/configuration.ini';

// 与 scripts/test_accounts/create_accounts.py 的 read_db_config 相同：凭据
// 运行时从服务端 configuration.ini 解析，绝不硬编码。
export function readDbConfig(iniPath = CONFIG_INI) {
  const values = {};
  for (const raw of fs.readFileSync(iniPath, 'utf8').split(/\r?\n/)) {
    const at = raw.indexOf('=');
    if (at <= 0) continue;
    const key = raw.slice(0, at).trim();
    if (!(key in values)) values[key] = raw.slice(at + 1).trim();
  }
  let host = '127.0.0.1';
  let database = 'maplestory';
  const url = values.URL || '';
  if (url) {
    const rest = url.split('//')[1] || '';
    const [hostPart, tail = ''] = rest.split('/');
    if (hostPart.includes(':')) host = hostPart.split(':')[0];
    if (tail.split('?')[0]) database = tail.split('?')[0];
  }
  return {
    host, database,
    user: values.DB_USER || 'root',
    password: values.DB_PASS || '',
  };
}

export function sqlExec(sql, config = readDbConfig()) {
  return execFileSync(
    'mysql',
    ['-h', config.host, '-u', config.user, config.database],
    { input: sql, encoding: 'utf8', env: { ...process.env, MYSQL_PWD: config.password } },
  );
}

export function sqlQuery(sql, config = readDbConfig()) {
  return sqlExec(sql, config);
}

// 上一会话浏览器被杀后，服务端需要几秒处理断线并把账号 loggedin 归零；
// 连续两次观测到 0 才继续，避免下一轮登录撞上“角色仍在游戏中”。
export async function waitAccountFree(account = 'test2', seconds = 40) {
  const loggedin = () => {
    try {
      const out = sqlQuery(
        `select loggedin from accounts where name='${account}';`);
      return Number((out.split('\n')[1] || '').trim());
    } catch (error) { return -1; }
  };
  let zeroes = 0;
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (loggedin() === 0) zeroes += 1; else zeroes = 0;
    if (zeroes >= 2) return true;
    await sleep(1200);
  }
  return false;
}

// 每轮浏览器会话之间的标准 SQL 预处理（角色必须离线）：清登录态、重置到
// 自由市场、按 PQ 需要调整等级。keymap 需包含 Space(57→ACTION/NPCCHAT)、
// Alt(56→JUMP)、Ctrl(29→ATTACK)（见 fm-gameplay-guides.mjs 头注；曾发生过
// 被整表重建抹掉的情况，因此每次预写回）。
export function prepCharacter({ level = null, account = 'test2', charId = 219, map = 910000000, spawnpoint = 0 } = {}) {
  const levelSql = Number.isInteger(level)
    ? `update characters set level=${level} where id=${charId};\n` : '';
  sqlExec(
    `update accounts set loggedin=0 where name='${account}';\n` +
    `update characters set map=${map}, spawnpoint=${spawnpoint} where id=${charId};\n` +
    `delete from keymap where characterid=${charId} and \`key\` in (29, 56, 57);\n` +
    `insert into keymap (characterid, \`key\`, type, action) values ` +
    `(${charId}, 57, 5, 54), (${charId}, 56, 5, 53), (${charId}, 29, 5, 52);\n` +
    levelSql,
  );
}
