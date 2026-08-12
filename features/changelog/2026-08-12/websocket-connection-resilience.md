Commit: d8304dc47fa83bd1ef6722660bd549ef89913c9c

# WebSocket 保活、恢复与断线退出提示

## Context

素材 WebSocket 空闲或短暂抖动后断开时，LazyFS 原先只清空连接引用，未重连、未完成请求会等待到超时，用户也不知道游戏已不可继续。游戏 WebSocket 断开后主循环会静默结束，同样没有可见提示。

## Change Summary

- 素材连接每 25 秒发送只读 `get_size` 保活，并以共享 Promise 合并并发连接请求。
- 意外断线后按 1/2/4 秒有限重连；连接恢复后只重放仍未完成的幂等素材请求。
- 三次素材连接重试耗尽时停止保活与后台预取并拒绝未完成请求；它不会终止游戏 Session，当前前台素材进入可重试错误遮罩，连接可由用户重试重新建立。
- 游戏 WebSocket 每 25 秒发送零长度数据帧维持浏览器到代理的传输层活动，不改变 Cosmic 协议；非主动断线会停止主循环并显示同一退出提示。
- 页面正常卸载和用户主动退出不触发重连或断线提示。

## Impact Surface

- LazyFS WebSocket 生命周期和请求调度。
- WASM 游戏 WebSocket 封装与主循环退出判断。
- Web 页面运行时断线反馈。

## Validation

- 模拟 WebSocket 测试覆盖共享连接、保活、断线请求重放、素材重试耗尽后不退出游戏、用户重试重新建连和页面卸载抑制。
- Docker WASM 构建成功，改动后的 JavaScript 产物通过语法检查。

## Notes / Compatibility

- 不修改 `assets-server`、`ws-proxy` 或 Cosmic 服务端协议。
- `ERR_CONNECTION_REFUSED` 表示目标端口没有可用服务；客户端会有限重试并提示，但仍需部署侧确保 8765 服务可达。

## Related Docs

- [按需文件系统](../../../agents/client/LazyFS/index.md)
- [网络层](../../../agents/client/Net/index.md)
