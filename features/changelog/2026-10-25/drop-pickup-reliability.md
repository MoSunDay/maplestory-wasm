# 掉落捡取可靠性：拒绝重试、幽灵掉落与状态提示

## 背景

掉落物有概率无法捡取：按住拾取键反复请求却"没反应"，或掉落一直躺在
地上永远捡不走。排查发现是服务端规则性拒绝叠加客户端三处缺陷的复合
问题。

## 根因

三层因素叠加：

1. **服务端规则性拒绝（近静默）**：新掉落有 400ms 拾取锁
   （`MapleCharacter.pickupItem` 的 dropTime 检查，仅回
   `enableActions()`）；他人掉落有 15 秒归属期
   （`MapleMapItem.canBePickedBy`）；背包满/任务物品不可拾取时仅回
   `SHOW_STATUS_INFO` mode2 0xff（背包满）/0xfe（物品不可用）。此前
   客户端完全忽略这些状态包，表现为"按了没反应"。
2. **客户端每次只锁定最近的一个掉落**：被服务端拒绝的掉落在客户端
   侧仍可捡（不会收到移除包），按住拾取键会永远重复请求同一个被拒
   oid，同点位的其他掉落被饿死，永远轮不到。
3. **幽灵掉落**：移除包先于排队中的 spawn 实例化到达时丢失（进图
   瞬间他人拾取，`MapDrops::remove` 只查已实例化的掉落）；以及
   `MapDrops::clear` 不清空 spawn 队列，旧地图掉落被带进新地图实例
   化。服务端 `getMapObject(oid)==null` 时静默返回，幽灵永远捡不走。

## 变更

- 新增纯函数模块 `src/client/Gameplay/MapleMap/DropLifecycle/
  LootSelection.h`：32x32 图标重叠或中心 30px 邻近判定可及、最近优先
  选择、上次尝试 oid 让位但仍可兜底重试（`skip_oid` 仅降优先级，唯一
  可及时仍返回，保证 400ms 锁/归属期过后能重试而不是永久饿死）。
- `MapDrops::find_loot_at` 改用该选择器并记录 `last_loot_oid`，删除
  原 `try_pickup`；同点位多个掉落时被拒 oid 会自动让位给下一个。
- `MapDrops::remove` 记录 `pending_removals`、`instantiate_spawns`
  消费（丢弃被提前移除的排队 spawn，队列排空后残余清零）、`clear`
  清空 spawn 队列、pending 集合与 `last_loot_oid`。
- `ShowStatusInfoHandler`（`MessagingHandlers.cpp`）增加 0xff/0xfe
  分支：显示"背包已满"/"无法拾取该物品"提示（服务端
  `getShowInventoryStatus` 尾随两个零 int，已跳过）。

## 验证

- `tools/verify/drop-pickup-selection.cpp`：选择契约单测（空列表、
  最近优先、越界/不可捡忽略、重叠可及、让位与兜底、并列取先），命令
  行见 `tools/verify/index.md`。

## Related Docs

- [客户端网络层](../../../agents/client/Net/index.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)
