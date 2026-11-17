// 建角武器断言夹具：新职业建角后按 DB 事实核查服务端实际装备的起始武器
// （服务端 CharacterFactory 装备 position -11 武器位，E2E 屏幕像素无从断言）。
//
// 期望值（服务端 CharacterFactoryRecipe / 客户端 JobOption.weapon_pool 同源）：
//   Aran    = 1442079 长枪（单武器池钉死）
//   Pirate  = 1482000 指虎（index 0；备用 1492000 短枪由服务端放背包不装备）
//   Explorer/Cygnus 沿用基础武器池，值 0 表示跳过断言（池内容随 UI.nx 数据
//   变化，不锁死具体 itemid）。
//
// 探针复用 scripts/game_server_character_weapon.sh（口令从服务端配置运行时
// 读取），接线点在 run.mjs 建角返回选人界面之后、进图断言之前。

import { execFileSync } from 'child_process';
import { fileURLToPath } from 'url';

const repoRoot = fileURLToPath(new URL('../../../../', import.meta.url));

const expectedWeapons = Object.freeze({
  aran: 1442079,
  pirate: 1482000,
  explorer: 0,
  cygnus: 0
});

export function assertCreatedCharacterWeapon(account, characterName, charJob) {
  const expected = expectedWeapons[charJob] ?? 0;
  if (!expected) {
    console.log(`INFO  creation weapon assertion skipped for ${charJob}`);
    return;
  }
  const weaponId = Number(execFileSync('bash', [
    `${repoRoot}scripts/game_server_character_weapon.sh`,
    '--account', account,
    '--name', characterName
  ], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim());
  if (weaponId !== expected) {
    throw new Error(
      `expected ${charJob} creation weapon ${expected}, got ${weaponId}` +
      ` (account=${account} name=${characterName};` +
      ' dispatch_creation 必须发送 weapon_choices()[weapon] 而非基础池同索引元素)');
  }
  console.log(`PASS  creation weapon matches ${charJob} start (itemid ${weaponId})`);
}
