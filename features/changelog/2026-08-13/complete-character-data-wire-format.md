Commit: 836cbaa0879e3459cfa17a594479446e658c3d7c

# 完整角色数据线格式闭环

## Context

角色列表修复了中文角色名后，进入地图的完整角色数据及相邻功能仍存在同类 Java 定长 UTF-8 字段；中文宠物名、队友名、戒指姓名或现金礼物可能使后续字段错位。`SET_FIELD` 对非空新年贺卡也只读取数量而未消费内容。

## Change Summary

- 统一按 Java UTF-16 code unit 消费宠物、队伍、戒指和现金礼物的定长 UTF-8 字段。
- 抽出完整角色附加数据解析器，完整消费三类戒指和新年贺卡。
- Cosmic 当前固定为空的小游戏数据若变为非零，客户端返回明确协议错误，不再静默错位。
- 新增协议 fixture，覆盖 13/73 字段、中文、四字节 Unicode、非法与截断 UTF-8、三类戒指、多张新年贺卡及异常计数。

## Impact Surface

进入地图、物品与仓库宠物、队伍状态、戒指信息和现金商城礼物解析。

## Notes / Compatibility

仅修正客户端对 Cosmic 现有 producer 合约的消费；不修改服务端、数据库、配置或 `assets/`。

## Related Docs

- [网络层](../../../agents/client/Net/index.md)
- [网络协议](../../../docs/ms-network-protocol.md)
