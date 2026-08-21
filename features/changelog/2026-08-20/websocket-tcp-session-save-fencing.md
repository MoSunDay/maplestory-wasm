Commit: d484de5f30c73b3b55c1c5f1ed9816ffe133fabd

# WebSocket 退出到角色保存的端到端收口

## Context

浏览器退出后，WebSocket 与本地代理断开，但目标 Java TCP 连接可能继续存活。旧角色对象稍后执行整角色保存时，会用本次登录时加载的内存快照覆盖新会话已经写入的数据，表现为精确回退到登录时状态。

## Change Summary

- `ws-proxy` 不再只依赖拆分 TCP half 析构；任一桥接方向结束后，对目标 socket 显式执行 `Shutdown::Both`。
- 标准 WebSocket Close 帧和浏览器传输层直接消失都会立即让 linked server 收到 TCP EOF。
- linked server 以账号会话代次保护整角色保存；新会话建立后，旧异步断线保存无法进入数据库事务。
- 会话代次切换与完整保存事务互斥：合法旧保存若已经开始，新登录会等待其提交后再继续；否则新代次先建立并拒绝旧快照。

## Impact Surface

- 浏览器退出、刷新和网络断开后的游戏 TCP 生命周期。
- linked server 的账号会话唯一性与角色整行持久化。

## Validation

- Rust workspace 全量测试通过；代理集成测试覆盖 Close 帧和无 Close 握手的传输层直接断开，均在 1 秒门限内观察到目标 TCP EOF。
- linked server 全量 Java 源码通过 Java 7 source/target 兼容编译；独立 verifier 覆盖新旧代次、延迟旧会话和保存事务/新登录互斥。

## Notes / Compatibility

- 未修改网络协议、数据库表、数据库数据或环境变量。
- 保存 fencing 为单 JVM 进程内代次；当前单实例 linked server 部署与该模型一致。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
- [linked server net 模块](../../../link_repos/MapleStory-Server/agents/net/index.md)
- [linked server client 模块](../../../link_repos/MapleStory-Server/agents/client/index.md)
