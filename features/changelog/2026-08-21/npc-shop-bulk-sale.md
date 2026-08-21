Commit: d484de5f30c73b3b55c1c5f1ed9816ffe133fabd

# NPC 商店批量售卖

## Context

逐件输入和确认售卖大量背包物品成本较高；同时现金物品不应进入 NPC 售卖链路。

## Change Summary

- 单件售卖数量输入超过当前堆叠时，输入框立即回填当前最大数量。
- NPC 商店增加“全部售卖”入口，二次确认后出售当前 Tab 的全部可售堆叠。
- 客户端隐藏现金 Tab 和其他 Tab 中标记为现金的可售条目；服务端再次校验并拒绝现金物品。
- 批量请求只携带物品栏分类，由服务端在背包锁内按实时快照计算和执行。
- 注册商店交易结果回包，失败时给出可见提示。

## Impact Surface

- NPC 商店 UI、数字输入弹窗和背包现金标志查询。
- NPC 商店请求/结果协议。
- linked server 的 NPC 售卖策略与批量事务。

## Notes / Compatibility

- 购买和仓库数字输入保持原行为。
- 不新增数据库表、环境变量或 NX 素材。
- 未执行真实角色物品售卖或发布操作。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [协议参考](../../../docs/ms-network-protocol.md)
