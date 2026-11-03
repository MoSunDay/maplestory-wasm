# 多玩家同图：怪物控制权真空窗口接管与动作区间判定

## 症状（Context）

- 关联症状"**怪冻结画面不同步**"：双账号同图打怪时，一方换图/掉线后的一小段窗口内，**全场玩家画面里的怪物集体冻结**——不追人、不出招、原地定格，往往要等有人重新走近怪物或再次进图才恢复。
- 伴随观感问题：怪物出招时各端表现不一致，有的端播普攻动作、有的端播技能动作（MobPresentation 的 begin_attack / begin_skill 选择失真），多人画面里同一只怪"打的是技能还是普攻"对不上。

## 根因（linked server 两处，行号为补丁前）

1. `src/server/life/MapleMonster.java` `aggroMoveLifeUpdate`（:1942-1954）：怪物当前无控制器（或控制器已离图/掉线）时返回 `null`；而 `src/net/server/channel/handlers/MoveLifeHandler.java:144-145` 收到 `null` 直接 `return`，该玩家的 MoveLife 上报被静默丢弃。控制器离图/掉线到其 `releaseControlledMonsters` 完成 `aggroRedirectController` 重定向之前的**控制权真空窗口**内，没有任何人转发怪物移动（`moveMonster` 广播），全场玩家画面里怪冻结，直到下一次 `aggroUpdateController`（往往要等有人走近/进图触发 AutoAggro）。
2. `MoveLifeHandler.inRangeInclusive`（:170-172）：`!(pVal < pMin) || (pVal > pMax)` 恒为真（前半 `!(pVal<pMin)` 已涵盖 `pVal>pMax` 之外的一切情况），导致 `rawActivity >= 42` 时 `isAttack`（24~41）与 `isSkill`（42~59）同时为 true，怪物技能/普攻动作转发分类失真。该方法为 private static，仅本 handler :78-79 两处调用，无其他共用者。

## 修复（Change Summary）

- `aggroMoveLifeUpdate` 增加真空接管分支：`getActiveController() == null`（无人有效控怪）且上报者活着、登录中、在同图时，经 `aggroSwitchController(player, isControllerHasAggro())` 让上报者**立即接管**并返回正常 aggro 结果（而非 null），MoveLife 继续走完 `moveMonsterResponse` + `moveMonster` 广播，全场画面不再冻结。
- 最小改动并贴合既有语义：
  - 交接本身（`aggroRemoveController` + 登录/同图复查 + `controlMonster` 下发）在 `aggroSwitchController` 内部的 `aggroUpdateLock`（MonitoredReentrantLock, tryLock）保护下完成，沿用该类现有同步风格，`aggroMoveLifeUpdate` 自身保持无锁读；
  - 第二参数传**当前** `isControllerHasAggro()` 而非硬编码 true，保持怪物仇恨状态不被交接重置（对齐 `aggroUpdatePuppetController`:1924 的保态写法），不凭空拉仇恨；
  - 接管后复查 `getActiveController()`，若他人（如并发重定向）已抢先接管则照旧返回 null——不与 `aggroAutoAggroUpdate`（AutoAggroHandler 走近触发）的自动接管逻辑重复触发：接管成功后再收到 AutoAggro 只会命中其"已是控制器 → setControllerHasAggro(true)"分支，无双重切换。
- `inRangeInclusive` 修为 `!(pVal < pMin || pVal > pMax)`，`isAttack`/`isSkill` 区间恢复互斥，rawActivity≥42 只归类为技能，普攻/技能转发分类一致。

补丁留档：`scripts/server-patches/0013-mob-controller-vacuum-and-activity-range.patch`
（重放：`cd link_repos/MapleStory-Server && patch -p1 < 本文件`，
已在干净目录对补丁前原文件干跑重放，逐字节一致）。

## Impact Surface

- linked server 怪物控制权交接（`aggroMoveLifeUpdate` / `aggroSwitchController` / 控制权真空窗口）
- linked server 怪物移动/出招转发（`MoveLifeHandler` 的 isAttack / isSkill 分类）

## Validation

- `bash posix-compile.sh` 全量 javac 编译退出码 0（仅存量 `finalize()` 弃用告警，与本次改动无关）。
- `docker restart ms-game-server` 后 `docker logs --tail 30`：5 个频道 7575/7675/7775/7875/7975 全部 Listening，登录口 8484 bind 成功，`MapleStory is now online`，无异常栈。
- `node tools/verify/party-share-server-contract.mjs` 回归 PASS——经验/金币分配契约断言未破坏。
- 干跑重放：干净目录 `patch -p1` 应用补丁后与修改后文件 `cmp` 逐字节一致。

## 遗留观察项（不在本次范围）

- 控制权真空窗口本身仍存在（ms 级），本修复是让"仍在场玩家"补位接管，不改变 `releaseControlledMonsters` 异步重定向路径。
- 客户端召唤物攻击 S→C opcode 未注册 handler 等既有观察项见 [0012 changelog](mob-hp-broadcast-and-exp-share.md)。

## Related Docs

- [同图怪物血条广播与死亡贡献者经验](mob-hp-broadcast-and-exp-share.md)（同日多玩家同图同步系列，服务端 patch 0012）
- [服务端补丁目录](../../../scripts/server-patches/README.md)
