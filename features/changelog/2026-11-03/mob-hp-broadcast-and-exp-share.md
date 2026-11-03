# 多玩家同图：怪物血条同步与死亡贡献者经验结算

## 症状（Context）

- 双账号（非组队）同图打怪时，只有攻击者本人能看到怪物血条扣减，另一名玩家的画面血条不动——除非两人组队或目标是有 Boss 血条的对象。
- 战斗中途死亡的伤害贡献者拿 0 经验且不补发，表现为"打怪经验不共享"：A 与 B 轮流输出同一只怪，先死的一方哪怕伤害占比更高也颗粒无收。

## 根因（均在 linked server `src/server/life/MapleMonster.java`，行号为补丁前）

1. `broadcastMobHpBar()`（:362-371）：非 Boss 分支的 `SHOW_MONSTER_HP` 包只发给攻击者本人（无队伍时 `from.announce(packet)`）或其队友（有队伍时按 `getParty().getMembers()` 循环 `getCharacterById`），非组队的同图玩家永远收不到。Boss 分支本就是全图广播（`broadcastBossHpMessage`），不受影响。
2. `giveExpToCharacter()`（:670）：整个方法体包在 `if (attacker.isAlive())` 里，死亡贡献者直接跳过 `gainExp`/`increaseEquipExp`/`updateQuestMobCount`，且无任何补发路径。

经验分配骨架本身健全：`distributeExperience()` 按每玩家伤害归属 `takenDamage` 以 80% 比例分配 + 20% 击杀者加成，另有击杀者队伍全额经验定制（2026-10-22，本次未动）。

## 修复（Change Summary）

- 非 Boss 分支改为遍历 `map.getCharacters()` 全图广播，每个接收者 `announce(packet.clone())`（保留原 "clone it just in case of crypto" 注释语义，风格对齐原 party 分支）。
- `giveExpToCharacter()` 去掉 `isAlive` 门槛，方法体平铺、内部逻辑不变，并加注释说明死亡贡献者也结算伤害经验（multi-client co-op）。
- 等级门槛等防 leech 设计、击杀者队伍全额经验定制一律保留不动。

补丁留档：`scripts/server-patches/0012-mob-hp-broadcast-and-dead-damager-exp.patch`
（重放：`cd link_repos/MapleStory-Server && patch -p1 < 本文件`，
已在 checkout 根对补丁前原文件干跑验证逐字节一致）。

## Impact Surface

- linked server 怪物血条广播（`SHOW_MONSTER_HP`）
- linked server 怪物经验分配（死亡贡献者路径）

## Validation

- `bash posix-compile.sh` 全量 javac 编译退出码 0（仅存量 `finalize()` 弃用告警，与本次改动无关）。
- `docker restart ms-game-server` 后 `docker logs --tail 50`：5 个频道 7575/7675/7775/7875/7975 全部 Listening，登录口 8484 bind 成功，`MapleStory is now online`，无异常栈。
- `node tools/verify/party-share-server-contract.mjs` PASS——击杀者队伍全额经验、同图金币平分契约断言未破坏。

## 遗留观察项（不在本次范围）

- 客户端召唤物/龙攻击 S→C opcode 0xAF~0xB7 未注册 handler，他人召唤物输出不可见。
- TOUCH_MONSTER_ATTACK 无攻击动作广播包（客户端无对应格式）。
- `killMonster(chr==null)` 脚本击杀路径不结算经验。

## Related Docs

- [组队收益前作](../2026-10-22/party-share-trade-layout.md)（击杀者队伍全额经验定制，本次未动）
- [服务端补丁目录](../../../scripts/server-patches/README.md)
