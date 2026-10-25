# 掉落捡取可靠性：拾取半径过窄为主因（终版）

## 背景

用户二次报告：自己击杀掉落的物品有一定概率无法捡取。第一轮修复
（选择器轮换/幽灵掉落/状态提示）后仍有残留，本轮通过浏览器端到端
实测（CDP 驱动真实客户端）+ 服务端插桩日志完成定位与闭环验证。

## 根因

实测诊断链（新增事实）：

- `tools/verify/maplestory-webui/pickup-stress-e2e.mjs`（击杀→拾取
  压测，10 轮统计）复现：修复前 3/6 轮卡死；
  `drop-reach-diagnose.mjs`（玩家/最近掉落几何采样）量化出关键
  几何：掉落图标中心恒在玩家脚点 y-16（32x32 图标 center=+16,+16），
  服务端掉落散布在击杀点 ±26~76px（服务端日志实测）。
- 服务端插桩（见"变更"末条）证实：客户端拾取请求在 30px 半径窗口
  外根本不发包（一轮 15 分钟 0 个 ITEM_PICKUP）；窗口内必发且服务端
  800x600 容差内全数放行、无一条拒绝日志（EXPLOITS 为空、locked 仅
  <400ms 瞬时）。=> 根因为主：客户端 PICKUP_RADIUS=30 过窄，
  |dx|≤25 的偏置窗口远小于掉落自然散布，"看起来就在脚边"的掉落
  捡不到。
- 次要发现：400ms 新掉落锁真实生效（服务端合成时钟经 force-snap 后
  约超前 80ms，有效锁 ~480ms）；ghost-oid 请求源于 skip_oid 兜底在
  移除包到达前的百毫秒竞态，良性；宠物 loot range 150px 但不主动
  跑图拾取（另案）。
- 第一轮定位的三处客户端缺陷仍成立但为辅：服务端规则性拒绝（400ms
  锁/归属期/背包满）被静默忽略、单目标锁死饿死同点位其他掉落、
  移除包早于排队 spawn 到达产生的幽灵掉落。

## 变更

- `LootSelection.h` PICKUP_RADIUS 30→80：服务端容差 800x600，客户端
  只需"走近"；80px 覆盖掉落散布与 y-16 偏置（dy=16 时 |dx|≤78），
  保持近身拾取语义。注释已写明依据。
- 其余保持第一轮内容：`MapDrops::find_loot_at` 改用 select_nearest +
  `last_loot_oid` 轮换（被拒 oid 让位、仍可兜底重试）；
  `MapDrops::remove` 记录 `pending_removals`、`instantiate_spawns`
  消费、`clear` 清空队列；`ShowStatusInfoHandler` 0xff/0xfe 显示
  "背包已满"/"无法拾取该物品"状态提示。
- 服务端（link_repos/MapleStory-Server，仅 println 插桩、零逻辑
  变更）：ItemPickupHandler 记录 ghost-oid/range-reject；
  MapleCharacter.pickupItem 记录 locked/already-picked/quest-gated/
  no-space/addFromDrop-failed/ok；MapleMap 记录 [DROP] spawn/expire/
  picked/removed。经 javac 增量编译进 dist/ 并随容器重启生效，
  `docker logs ms-game-server | grep -E "\[PICKUP\]|\[DROP\]"`
  即可观测。

## 验证

- 单测 `tools/verify/drop-pickup-selection.cpp` PASS（80px 边界用例
  已更新）。
- wasm 重建（docker compose wasm-builder 增量 ~1 分钟）后
  `pickup-stress-e2e.mjs` 连续两轮共 6/6 轮全部拾取成功（含坡道
  y=317~326 场景，直捡 0.9~3.1s），修复前同场景 3/6 卡死。
- `drop-reach-diagnose.mjs` 采样确认拾取前几何可达。
- 运行环境注意：e2e 前需 `scripts/test_accounts/skill-keymap-prep.py`
  恢复键位与 40001 出生点（keymap 会被服务端存档覆盖为空）；保留
  chrome-profile 缓存资产否则登录超时。
- 服务端插桩开关：拾取/掉落生命周期日志（`[PICKUP]`/`[DROP]`）由
  `ServerConstants.DROP_LIFECYCLE_LOG` 控制，默认开；生产可用
  `-DdropLifecycleLog=false` 静默（写入容器 JVM 启动参数即可）。

## Related Docs

- [客户端网络层](../../../agents/client/Net/index.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)
