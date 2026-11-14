# 多人同步收尾：攻击 charge 回退、召唤兽迟到 owner 暂存、移动解析未知命令截断

## 背景（Context）

多人同步批次评审结论为 `not ready`，阻断项三个：

- **D1 攻击 charge 回放开倒车**：`Combat::push_attack` 把服务端广播的
  `attack.charge`（Big Bang/吐息等蓄力技能的 keydown 已耗时）钳制到
  0..2000ms 后作为远端挥砍排队延迟。攻击者本地早已起手、服务端也已结算
  命中，观察端再回放蓄力会让挥砍晚于该攻击的伤害数字/特效——与
  [2026-11-06 远端动作同步](../2026-11-06/remote-action-sync.md) 移除固定
  400ms 延迟的理由完全相同。
- **D2 召唤兽 owner 迟到丢包**：`SPAWN_SPECIAL_MAPOBJECT`(175)、
  `MOVE_SUMMON`(177)、`SPAWN_DRAGON`(181) 在 owner 尚未实例化时直接丢弃。
  owner 可能仍在 `MapChars` 的待实例化队列中（同批 SPAWN_PLAYER 晚到），
  与 2026-11-14 迟到角色事件同一竞态，导致召唤兽永久不可见。
- **D3 未知移动命令错位**：`MovementParser` 的 switch 无 `default` 分支，
  未知 command（如 23）不消费任何载荷字节却继续解析，同批后续片段全部
  错位；服务端 `MovementPacketDecoder` 遇未知命令返回 null 丢弃整包。

## 改动（Change Summary）

### D1：`Combat::push_attack` 延迟恒 0

- `attackresults.push(std::clamp(attack.charge, 0, 2000), attack)` →
  `attackresults.push(0, attack)`，注释引用
  `features/changelog/2026-11-06/remote-action-sync.md`。
- **charge 字段解析保留**：`AttackHandlers` 仍按 `has_broadcast_charge`
  白名单读取 `attack.charge`（协议完整性/后续消费），只是不再作为队列
  延迟；命中伤害时序仍由 `DamageEffect` 队列按攻击帧单独排程。
- 新源码契约 `tools/verify/charge-zero-delay-source-contract.mjs`：断言
  `push_attack` 仅以字面量 0 入队、不含 `std::clamp(attack.charge`、
  且 charge 解析与 `AttackResult::charge` 字段仍在。

### D2：召唤兽迟到 owner 暂存（复用 MapChars CharEvent 模式）

- `SummonPool` 新增 `SummonEvent{SPAWN/MOVE}` 与按 owner cid 的
  `pending_events`：`stash_event` 仅在同 owner 同类同 oid 时原位覆盖
  （两类均携带幂等状态），每 owner 封顶 `MAX_STASHED_SUMMON_EVENTS=4`
  超出丢最旧；`replay_stashed` 在 owner 已存在于地图时按到达顺序重放
  （SPAWN 实例化、MOVE 替换回放批次）；`drop_stashed` 在 REMOVE_CHARACTER
  时清该 cid；`summon::clear` 随换图一并清空。
- `SummonHandlers` 的 `owner_on_map` 布尔升级为三态
  `OwnerState{PRESENT,PENDING,GONE}`：PRESENT 立即应用、PENDING 入
  stash、GONE 丢弃；`MapChars::has_pending_spawn` 提升为公开只读查询；
  `Stage::update` 在 `chars.update(physics)`（含 instantiate_spawns）之后、
  `summon::update` 之前调用 `summon::replay_stashed`。
- 新源码契约 `tools/verify/summon-late-owner-contract.mjs`：覆盖三态
  分派、同类同 oid 覆盖、封顶 4、实例化后重放顺序、离图清理与换图清空。

### D3：移动解析未知命令整包截断

- `MovementParser` 新增 `default`：输出
  `[MovementParser] Unknown movement command <n>; dropping the whole
  fragment batch` 警告并清空已解析片段、返回空批次，对齐服务端
  return null 语义；未知命令载荷布局不可知，不做逐片段跳过以免错位。
- `movement-parser-alignment` 增断言：`[绝对(0), 未知 23 + 4 字节垃圾,
  绝对(5)]` 批次的解析结果为零片段（且不抛异常）。

## 验证（Verification）

- **门 1（10/10 快测）**：`char-movement-playback`、`mob-movement-playback`、
  `movement-parser-alignment`、`pet-playback-interpolation`、
  `remote-pet-desync-source-contract`、`foreign-buff-mask`、
  `spawn-player-parser`、`face-expression-sync`、
  `charge-zero-delay-source-contract`、`summon-late-owner-contract` 全部
  PASS。
- **门 2（双端 e2e）**：
  - `remote-pet-desync-flow`：`SUMMARY 4/4 scenarios OK`
    （base-visibility / summon-race / ghost-race / coop-regression）。
  - `social-flow` 三模式：`actions`（远端 F1→HIT、5s 复位、远端攻击
    stance/frame 断言）、`mobs`（oid 集合一致、worst maxDelta=15px ≤48px）、
    `coop`（远端同步差 13/16px ≤48px、远端 p95 步进 6/6px ≤12px、
    落地 y 差 ≤6px、双端零浏览器异常）全 PASS。
- **环境类失败记录**：首跑 `3/4`（ghost-race r3 失败）与次跑
  `base-visibility` 超时（残留浏览器会话导致 page=null），清理
  chromium 进程/调试端口并重置账号 `loggedin` 后一次通过 4/4；属
  known-issues 的 idle-reaper/重启冻结类环境噪声，非逻辑回归。
- WASM 由 `scripts/docker_build_wasm.sh`（emsdk 4.0.21）重编成功；
  linked server 已应用补丁
  [0019-expression-throttle](../../../scripts/server-patches/0019-expression-throttle.patch)
  并 `posix-compile.sh` 重编 + `docker restart ms-game-server`。

## 影响面（Impact Surface）

- 远端攻击起手时序（回到 2026-11-06 的即时语义）；本地攻击、命中结算、
  伤害数字排程未改。
- 召唤兽出生/移动的迟到竞态；攻击/伤害/收回等瞬时事件仍不缓存。
- 未知命令包从「错位污染」变为「整批丢弃」；已知命令 0..22 的字节表
  覆盖不变。

## 已知边界（2026-11-14 收尾登记）

见 [docs/roadmap.md](../../../docs/roadmap.md)「多人同步收尾遗留边界」：
Morph.nx 挂载与变身外观、召唤兽动画美术缺节点（傀儡/鹰/祭司龙）、
出生包三态 morph 消费、D4 服务端 `SpecialMoveHandler` effectid 误传
（补丁 0021 待办）、trade-server-contract 环境与 e2e 长跑稳定性。
