# 组队击杀经验：无条件同图全额 1:1 分享

## 症状（Context）

- 队友（击杀者）击杀怪物时，与击杀者等级差 ≥ 40 的同图队员经验为 0。
- 队员等级低于怪物等级 − 20（BOSS 为 − 40）时，同样拿 0 经验——哪怕整队同图协作输出。

## 根因（linked server `src/server/life/MapleMonster.java#distributeExperience`，行号为补丁前）

- 击杀者队伍全额经验块（`killer.getPartyMembersOnSameMap()` 循环）保留了两道防 leech 门槛：
  1. `mc.getLevel() >= minThresholdLevel`（`ServerConstants.USE_ENFORCE_MOB_LEVEL_RANGE` 派生的怪物−20 / BOSS−40 下限）；
  2. `Math.abs(killerPartyLevel - mc.getLevel()) < ServerConstants.MIN_RANGELEVEL_TO_EXP_LEECH`（常量 40 的等级差限制）。
- 任一门槛不满足即走 `underleveled.add(mc)` 分支，该成员拿 0 经验，只有 `showUnderleveledInfo` 提示。

## 修复（Change Summary）

- 删除击杀者队伍全额经验块内的两道等级门槛与 `underleveled` 分支：同图全队成员无条件各得怪物基础经验的 100%（1:1）。
- 其余路径一律不动：按伤害归属的经验拆分、其他队伍的 `distributeExperienceToParty` 伤害加权加成（含 Boss 翻倍门槛）、个人经验倍率（祝福/圣火等 buff）仍分别生效。

补丁留档：`scripts/server-patches/0014-party-exp-unconditional-full-share.patch`
（重放：`cd link_repos/MapleStory-Server && patch -p1 < 本文件`）。

## Impact Surface

- linked server 怪物经验分配（`distributeExperience` 击杀者队伍路径）
- 组队经验契约测试 `tools/verify/party-share-server-contract.mjs`（断言升级为无门槛全额 1:1）

## Validation

由父 agent 执行：

- `bash posix-compile.sh` 全量 javac 编译通过。
- `docker restart ms-game-server` 后服务正常在线。
- `node tools/verify/party-share-server-contract.mjs` 输出 PASS（unconditional 1:1 party EXP sharing and even meso split server contract）。

## Related Docs

- [组队收益前作](../2026-10-22/party-share-trade-layout.md)（击杀者队伍全额经验定制与金币平分的初始版本）
- [怪物血条广播与死亡贡献者经验](../2026-11-03/mob-hp-broadcast-and-exp-share.md)（同文件经验路径的上一轮改动）
