# 召唤兽 owner 离场收尾：GONE 早退回归修复、龙 182/183、离场广播补丁 0022

## 背景（Context）

多人同步收尾（`120a3a1`）评审结论为 `not ready`，发布阻断集中在
「owner 离场后召唤兽残留」一族：

- **P1 静止傀儡幽灵（收尾新回归）**：服务端 owner 离图时先广播
  REMOVE_CHAR、后经 `cancelEffectFromBuffStat(PUPPET)` 广播
  REMOVE_SUMMON；客户端 `RemoveCharHandler` 已把 owner 移除，随后
  `RemoveSummonHandler` 看到 GONE owner 直接 `return`，跳过幂等
  `summon::remove`，傀儡残留到换图。收尾前的旧实现是无条件 remove，
  因此这是 GONE 三态分派引入的回归。
- **P1 非静止召唤无离场清理**：服务端 `MapleMap.removePlayer` 对跟随型
  召唤只做本地 `removeMapObject` 不广播，客户端也没有按 owner 删除
  实体的接口，鸟/鹰/章鱼在 owner 离图后冻结残留。
- **P1 龙 MOVE/REMOVE 未注册**：SPAWN_DRAGON(181) 已接，但
  MOVE_DRAGON(0xB6/182)、REMOVE_DRAGON(0xB7/183) 落入 unhandled warn，
  龙在主人离图后永久残留且每次移动刷 `Opcode: 182`。
- **P2 owner 可见性误门控**：主人出视野时 SUMMON 实体仍在（服务端不按
  距离销毁召唤），但 MOVE/ATTACK/DAMAGE 按 owner 可见性丢弃，召唤冻结。
- **P2 暂存溢出可能淘汰 SPAWN**：上限 4 溢出时无差别丢最旧，多召唤+龙
  同 owner 的边缘会把 SPAWN 挤掉，余下 MOVE 永远 no-op。
- **P3**：`HIT_LENGTH=7`（实际每 hit 9 字节）；协议文档把 MOVE_DRAGON 误
  标 0xB5 且缺 0xB7；roadmap D4 补丁编号 0021 与并行 orient 批次冲突。

## 改动（Change Summary）

### A1：`RemoveSummonHandler` 去掉 GONE 早退（回归修复）

- REMOVE 不再按 `OwnerState` 分派：无条件先
  `drop_stashed_oid(owner_cid, oid)` 取消暂存 SPAWN/MOVE（覆盖 PENDING
  同批竞态、REMOVE_CHAR 先到、LOADING 窗口三种时序），再执行幂等
  `summon::remove`。GONE owner 时的静止傀儡 REMOVE_SUMMON 因此恢复消费，
  幽灵不再残留。
- 该修复同时覆盖「LOADING 期 REMOVE 不清 stash」：`update_loading` 不重放
  stash，但 REMOVE 侧已无条件取消暂存，首帧 ACTIVE 重放不会复活实体。

### A2：服务端补丁 0022（owner 离场跟随召唤广播）

- [0022-summon-owner-leave-broadcast.patch](../../../scripts/server-patches/0022-summon-owner-leave-broadcast.patch)：
  `MapleMap.removePlayer` 非静止分支在 `removeMapObject` 之外按 owner
  隐藏状态广播 `removeSummon(summon, true)`（mode 4 死亡动画，排除
  离图者本人）；静止傀儡保持既有 `cancelEffectFromBuffStat(PUPPET)`
  广播链不重复发送。roadmap D4 effectid 待办编号顺延为 0023
  （0021 已被 orient NPC 兜底占用、0022 为本补丁）。

### A3：龙 MOVE_DRAGON(182)/REMOVE_DRAGON(183) 注册

- `PacketSwitch` 补 `MOVE_DRAGON=182`、`REMOVE_DRAGON=183` 与两个
  handler 注册；`SummonPayloads` 新增
  `DragonMovePayload{cid,start,movements}`（9 字节头+片段表）与
  `DragonRemovePayload{cid}`（4 字节）。
- `MoveDragonHandler` 复用与 MOVE_SUMMON 相同的 `apply_move`：龙的
  oid 即 owner cid；`RemoveDragonHandler` 先
  `drop_stashed_oid(cid, cid)` 取消迟到龙的暂存，再幂等 remove。
- `HIT_LENGTH` 7→9（oid 4 + marker 1 + damage 4）。

### A4：MOVE/ATTACK/DAMAGE 与 owner 可见性解耦

- 新增自由函数 `apply_move(owner_cid, oid, start, movements)`：实体存在
  即应用移动（主人出视野不再冻结）；实体不存在才按三态决定
  stash/丢弃。`SummonAttackHandler`/`DamageSummonHandler` 改以
  `summon::find(pool, oid)` 的实体存在性为准，不再查 owner。
- SPAWN 类（SPAWN_SUMMON/SPAWN_DRAGON）仍按三态分派（无实体时只有
  PENDING 需要暂存）。

### A5：暂存溢出保护 SPAWN

- `stash_event` 溢出改为循环淘汰最旧 MOVE，仅在全是 SPAWN 时才退化为
  丢最旧：被淘汰的 MOVE 丢失的只是插值细节，被淘汰的 SPAWN 会让召唤
  整体缺席，两者代价不对称。

### 配套探针/契约/e2e

- 新增 CDP 探针 `GameplayProbesSummon.cpp`（`ms_summon_count`、
  `ms_summon_skill_at`，oid 排序保证确定性），导出于 CMakeLists。
- `summon-late-owner-contract.mjs` Section F 重写为「无条件先取消暂存再
  remove、不得按 owner 早退」，新增 F2（apply_move 实体优先、
  attack/damage 实体门控）与 G（dragon 182/183 注册与路由），
  Section B 改锁「溢出优先淘汰 MOVE」。
- 新增双客户端 e2e
  [`tools/verify/maplestory-webui/summon-owner-leave-flow.mjs`](../../../tools/verify/maplestory-webui/summon-owner-leave-flow.mjs)：
  A=test2BM 经 DB 注入 Puppet(3111002)/Phoenix(3121006) 至 J/K 键位，
  场景 1 静止傀儡（登出后 B 侧计数归零，验收 A1）、场景 2 跟随召唤
  Phoenix（登出后归零，验收 A2 服务端广播）、场景 3 双端控制台无
  `Opcode: 182/183` unhandled 警告且无浏览器异常（验收 A3）。

## 验证（Verification）

- 门 1（10/10 快测）在改动后重跑全绿（含更新后的
  `summon-late-owner-contract.mjs`）。
- 门 2/门 3 与干净构建复编记录见批次执行注记（本文件随提交后补记
  构建日志与 md5）。

## 影响面（Impact Surface）

- 客户端：`SummonHandlers.*`、`SummonStash.cpp`、`SummonPayloads.*`、
  `PacketSwitch.cpp`、`GameplayProbesSummon.cpp`（新）、`CMakeLists.txt`
  （导出追加）；契约与 e2e 各一。
- 服务端：补丁 0022（`MapleMap.removePlayer`），重放后需
  `bash posix-compile.sh && docker restart ms-game-server`。
- 行为变化：owner 离图后静止傀儡/跟随召唤/龙均按各自链路消失；主人
  出视野期间召唤继续按服务端广播移动；此前基线 changelog
  `summons-foreign-buffs-and-cosmetics.md` 记录的「owner 离图则保持」
  从已知边界升级为已修复。

## 收尾实录（2026-11-15 下午：门 3 三场景转绿 + 宠物拾取环境修复）

上文 e2e 首跑仅 1/3：傀儡场景 B 侧计数恒 0。逐项定位出六个叠加根因，
全部修复后 `summon-owner-leave-flow.mjs` 3/3 全绿，顺带修复同日
`pet-automation-flow.mjs`（E2E_PET_OPTIONS=pickup）的环境性失败。

### 客户端/数据根因与修复

1. **Skill.nx 缺 3111002（Puppet）节点**：v83 数据集根本不含该技能，
   `SkillData::get(3111002)` 为空、is_summon 判否。新增
   `tools/nx/inject_puppet_summon_nx.py` 参照 Phoenix(3121006) 结构把
   20 级傀儡数据移植进 `assets/Skill.nx`（备份
   `Skill.nx.pre-puppet-summon-backup`）。
2. **bullet 字段缺失**：数据集剥离了各级 bulletConsume/bulletCount，
   `SkillData.cpp` bulletcost 缺省取 0，与服务端扣弹逻辑对齐。
3. **Asyncify 双重展开崩溃**：召唤首帧在挂起读上再次 handleSleep 触发
   wasm 双重展开 abort。`LazyFileBackend.cpp` 预置 busy-guard
   （`!replayingSuspendedRead && (currData || state)` 时同步返回 0），
   `lazyfs.js` 配套 `warmRangeInBackground` 预热，消除交互期挂起读。
4. **UseSkillPacket 不带坐标**：服务端 `SpecialMoveHandler` 仅在
   `slea.available() == 5` 时刷出召唤兽。`AttackAndSkillPackets.h`
   新增坐标重载（write_point + write_byte(0)），`Combat.cpp` apply_move
   对 `is_summon()` 技能改发坐标版。
5. **首登冷缓存丢键**：`Stage::send_key` 在 Stage 非 ACTIVE（LOADING）
   时静默丢键，首登傀儡按键被吞（重登热缓存才成功）。e2e 的
   castSummon 先等 `uiState==game && msstage_loading==0` 再点击+按键。
6. **StageInput.cpp / PacketSwitch.cpp** 诊断后还原为与 HEAD 字节一致。

### 宠物拾取门（pet-automation-flow.mjs pickup）环境根因

- 门入口固定**双击第一张角色卡**：test1 首卡是 test1Hero（等级 200、
  HP 30000、map 40001），并非当轮新建的「测试一」——门实际一直跑在
  test1Hero 上（历史全绿记录用 accept1/accept1Hero 同理）。
- test1Hero 现金栏幼犬被 `remote-pet-desync-flow` 的 injectPet 重绑到
  pets 行 50000024「DesyncPet」，温饱度随全量 worker 衰减到 25 <
  客户端 HUNGRY_THRESHOLD(40) → `update_pet_automation` 饥饿门每帧
  `clear_pet_loot_target`，targetSeen 恒 false、服务端确认拾取恒 0。
- 修复：`UPDATE pets SET fullness=100, summoned=0 WHERE petid=50000024`
  （恢复验证过的健康形态：门自己走现金栏双击召唤路径）。复跑全绿：
  服务端确认拾取 4 次（meso +4）、精确 OID 1000000031 1ms 移除、
  跟随恢复达标（distance 34 / 0 warp / recovered）。

### 附带环境结论（复现排查途中定位）

- 服务端 `accountChars` 内存计数**不会遗忘 SQL 直删的角色**：门收尾
  脚本按 SQL 清理同名角色后，内存仍占满 test1 的 4 槽，
  `CreateCharHandler` 槽位 -3 **静默无回包**，客户端卡建角界面 →
  `Timed out: character creation returned to selection`。处理：清理
  残留角色后 `docker restart ms-game-server` 同步内存即可复跑。
- 测试库 `innodb_flush_log_at_trx_commit=2`、`sync_binlog=0`（建角
  82–107s → 57ms），为测试环境调优，保留；生产不适用。

### 回归验证（全部通过）

- `summon-owner-leave-flow.mjs` 3/3（puppet/phoenix/owner-leave 归零、
  0×182/183、0 浏览器异常）。
- `node tools/verify/summon-late-owner-contract.mjs`。
- 纯函数门 chatbar-geometry / pet-buff-policy / drag-session。
- `E2E_PET_OPTIONS=pickup E2E_REQUIRE_MOBS=1 E2E_ROUTE_TUTORIAL_MOB_MAP=1
  node tools/verify/maplestory-webui/run.mjs` 全绿（含建角、入场、
  现金栏召唤、能力掩码、拾取、跟随）。
- snow-slide-flow 于当日最终构建后已复验 ALL CHECKS PASSED（本轮无
  源码改动，未重跑）。
