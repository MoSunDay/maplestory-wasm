Commit: b6cde97b087a8c94bf9ace910bb726776bec6d23

# 角色数据对齐 linked server 线格式

## Context

线上已切换到 `link_repos/MapleStory-Server`，客户端仍按旧 Cosmic 规则读取角色等级和 Java 定长字符串。玩家在世界选择确认后收到 `CHARLIST`，解析从第一个角色起错位，最终抛出 `Stack underflow` 并停留在禁用状态。

## Change Summary

- `LoginParser::parse_stats` 按 linked server 的 custom-client 合约读取 short 等级和固定 13 字节角色名。
- 宠物、队伍、戒指与现金礼物等同类 Java 定长字段统一按固定字节宽度消费。
- 原生协议夹具覆盖中文角色名、普通职业与 Evan 多池 SP，并校验角色列表尾字段完整消费。

## Impact Surface

- 世界选择到角色选择的 `CHARLIST` 流程。
- 新建角色回执、进入地图角色快照及包含定长姓名的相邻功能。

## Notes / Compatibility

- 以 `link_repos/MapleStory-Server` 的 `addCharStats`、`writeFixedString` 和 `USE_CUSTOM_CLIENT=true` 为唯一线上合约。
- 未修改数据库、环境变量、服务端端口或私有协议。

## Related Docs

- [网络层](../../../agents/client/Net/index.md)
