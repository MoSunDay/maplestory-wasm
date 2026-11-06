// 前置夹具清理：历次验收会在账号下残留同名角色（角色创建后被验收流程中断、
// 或上一轮验收本身创建成功），下次再走创建流程时因重名被服务端拒绝，卡在
// 命名一步并报 "accepted name never entered customization" 假失败。
// 复用 scripts/game_server_cleanup_test_character.sh 按账号+角色名清理残留
// 角色及其子表数据（幂等、无匹配即 no-op）；脚本非零退出时本函数直接抛错，
// 让验收入口尽快失败而非拖到建角步骤。
// 背景见 docs/known-issues/2026-11-06-ui-hit-offset-and-idle-reaper.md。

import { execFileSync } from 'child_process';
import { fileURLToPath } from 'url';

const repoRoot = fileURLToPath(new URL('../../../../', import.meta.url));

export function cleanupResidualCharacter(account, characterName) {
  const output = execFileSync('bash', [
    `${repoRoot}scripts/game_server_cleanup_test_character.sh`,
    '--account', account,
    '--name', characterName
  ], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
  const keyLines = output.split(/\r?\n/)
    .filter(line => line.includes('清理'))
    .join(' | ');
  console.log(`fixture: ${keyLines}`);
}
