Commit: d484de5f30c73b3b55c1c5f1ed9816ffe133fabd

# 对外提供 Java Server 与 WebSocket 代理制品

## Context

`:48562` 原先只公开当前 release 的 `JourneyClient.js` 和 `JourneyClient.wasm`。外部部署还需要服务端 JAR 与 WebSocket-to-TCP 代理，但不能为此暴露源码仓库、配置或凭据。

## Change Summary

- nginx 下载根切换到稳定的 `/data00/maplestory-wasm-deploy/public-artifacts`，仓库配置基线为 `docker/artifact-nginx.conf`。
- 保留 `JourneyClient.js` 与 `JourneyClient.wasm`，以符号链接继续跟随 `current/build`。
- 新增全量 Java 源码构建的 `MapleStory-Server.jar`，manifest 入口为 `net.server.Server`。
- 新增当前 Rust 源码 release 构建的 Linux x86-64 `ws-proxy` 可执行文件。

## Impact Surface

- IPv4/IPv6 `:48562` 制品目录与直接下载 URL。
- linked server 和 WebSocket 代理的外部分发。

## Validation

- nginx 配置检查和热重载成功。
- 使用公网 IPv6 URL 检查目录、两个文件的 HTTP 200、Content-Length、Range 支持及完整下载 SHA-256。
- JAR manifest、关键会话 fencing class 和 `ws-proxy --help` 均验证通过。

## Notes / Compatibility

- 下载目录仍为只读 nginx 挂载；未暴露源码、数据库配置或凭据。
- JAR 是服务端代码制品，运行仍需要仓库既有的 `cores/` 依赖、`wz/`、`scripts/` 和数据库配置。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
