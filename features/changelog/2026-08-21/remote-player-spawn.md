Commit: d484de5f30c73b3b55c1c5f1ed9816ffe133fabd

# 远端玩家出生包等级宽度对齐

## Context

同一地图内能观察到怪物被攻击、掉落被拾取，却看不到执行动作的其他玩家。说明地图事件流正常，但远端玩家出生包没有成功落到角色实体。

## Change Summary

- 将 `SPAWN_PLAYER` 头部提取为无应用状态依赖的解析模块。
- 按 linked server custom-client 合约读取 16 位等级，再读取角色名，避免一个字节的游标错位导致整包被丢弃。
- 将出生数据中的等级类型贯通为 `uint16_t`，覆盖 255 以上等级。
- 增加包含中文名、300 级、游标耗尽和截断拒绝的独立回归验证。

## Impact Surface

- 同地图其他玩家的生成与可见性。
- `SPAWN_PLAYER` 后续公会、外观、位置和状态字段的解析对齐。

## Notes / Compatibility

- 不修改 opcode 或服务端封包；客户端与当前 `USE_CUSTOM_CLIENT=true` 合约对齐。
- 不涉及数据库、环境变量或线上发布。

## Related Docs

- [网络层](../../../agents/client/Net/index.md)
- [协议参考](../../../docs/ms-network-protocol.md)
