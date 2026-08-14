Commit: fde7b1761b51ea4cf6d9c5ff2902b63506a86088

# 系统菜单登出与 linked server 收口

## Context

System 的退出入口只停止客户端主循环，没有主动关闭会话；线上服务同时误指向仓库外的 Cosmic 工作区，无法保证客户端按 `link_repos/MapleStory-Server` 的会话语义验收。

## Change Summary

- System 的游戏退出项改为明确的登出确认，确认后发送标准 v83 `PLAYER_DC(0x0C)`，再主动关闭 WebSocket/TCP、清空客户端接收状态并结束客户端循环。
- linked server 接通原本只声明未注册的 `PLAYER_DC`，收到请求后主动关闭会话，并沿既有 `sessionClosed → MapleClient.disconnect` 路径保存角色、释放在线状态。
- 线上 Java 服务的工作目录和入口切换到 `link_repos/MapleStory-Server`；数据库凭据通过运行时环境覆盖，不写入 Git。

## Impact Surface

- 游戏内 System 菜单的登出行为。
- 客户端 Session 主动断开生命周期。
- 8484/7575 游戏服务端的构建与运行来源。

## Notes / Compatibility

- 未新增私有协议；复用标准 v83 opcode。未新增或修改数据库表，也未删除数据库数据。
- 保留 systemd 单元的历史名称，但运行路径、JDK、classpath 和单频道端口均以 linked server 为准。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [Web 基础设施](../../../agents/web/index.md)
