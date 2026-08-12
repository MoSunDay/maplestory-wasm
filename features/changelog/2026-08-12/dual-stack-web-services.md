Commit: d8304dc47fa83bd1ef6722660bd549ef89913c9c

# Web 服务 IPv4/IPv6 双栈支持

## Context

Web 页面和两个 WebSocket 服务原先默认只监听 `0.0.0.0`，通过 IPv6 地址访问页面时不可达；客户端直接拼接 IPv6 主机与端口也可能生成无效 URL。

## Change Summary

- `web-server`、`ws-proxy` 和 `assets-server` 默认监听改为显式 IPv4/IPv6 双栈的 `::` socket，同一端口继续兼容 IPv4。
- 三个服务复用 `web-common/listener.rs`，避免不同服务产生监听行为差异；显式 `--bind` 仍按指定地址监听。
- LazyFS 素材 URL 和游戏代理 URL 对裸 IPv6 地址自动添加 URI 方括号，同时保留 IPv4、域名和已带方括号地址。

## Validation

- 三个 Rust 服务共 45 个测试通过，其中双栈监听测试分别验证 IPv4 与 IPv6 loopback。
- WASM 构建成功；LazyFS 连接测试覆盖裸 IPv6、已括号 IPv6 和域名格式化。
- 运行服务通过 IPv4、IPv6 loopback 和主机全局 IPv6 地址访问 HTTP；素材与代理 WebSocket 均通过 IPv4/IPv6 实测。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
- [按需文件系统](../../../agents/client/LazyFS/index.md)
- [网络层](../../../agents/client/Net/index.md)
