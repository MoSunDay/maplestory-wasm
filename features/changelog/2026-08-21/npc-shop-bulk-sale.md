Commit: 1f11c638eaf870a61a60ddb8094b0c8f80d6d75b

# NPC 商店批量售卖

## Context

逐件输入和确认售卖大量背包物品成本较高；同时现金物品不应进入 NPC 售卖链路。

## Change Summary

- 单件售卖数量输入超过当前堆叠时，输入框立即回填当前最大数量。
- NPC 商店增加“全部售卖”入口，二次确认后出售当前 Tab 的全部可售堆叠。
- 客户端保留现金 Tab 的入口但使其可售列表为空，并过滤其他 Tab 中标记为现金的条目；服务端再次校验并拒绝现金物品。
- 批量请求只携带物品栏分类，由服务端在背包锁内按实时快照计算和执行。
- 注册商店交易结果回包，失败时给出可见提示。

## Impact Surface

- NPC 商店 UI、数字输入弹窗和背包现金标志查询。
- NPC 商店请求/结果协议。
- linked server 的 NPC 售卖策略与批量事务。

## Validation

- WASM 客户端全量编译和售卖纯策略验证通过。
- 真实浏览器角色流程确认超量输入即时回填、当前 Tab 二次确认和现金 Tab 空态；确认均取消，未改变角色物品与金币。

## Notes / Compatibility

- 购买和仓库数字输入保持原行为。
- 不新增数据库表、环境变量或 NX 素材。
- 未执行真实角色物品售卖或发布操作。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [协议参考](../../../docs/ms-network-protocol.md)
