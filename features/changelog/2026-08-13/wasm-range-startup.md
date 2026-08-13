Commit: e264ed06c12c5c2a122ead0b7fa18299b9467b8d

# WASM 分块重试启动

## Context

浏览器原先以单个请求下载约 9.6 MB WASM。IPv6 链路中途停止传输时，Emscripten 不会超时或续传，启动依赖会永久停留在 `wasm-instantiate`。

## Change Summary

- 启动页先查询 WASM 长度，再以 1 MiB Range 分块、最多 3 路并发下载并组装客户端二进制。
- 每块下载包含贯穿响应体读取的 15 秒超时、最多 3 次有限重试和严格范围/长度校验；失败后直接显示错误，不回退到整文件请求。
- 加载页显示下载百分比并按有效进度重置停滞计时。
- 页面不再重复下载已经嵌入 WASM 的 4 MB CJK 字体，加载提示改用系统中文字体栈。

## Impact Surface

浏览器入口页、WASM 静态文件下载方式和启动错误反馈。

## Validation

- Node 测试覆盖分块组装、并发、请求和响应体超时、重试耗尽、范围及长度错误。
- Rust Web 服务测试覆盖完整响应、Range、MIME、连接复用和错误响应。
- Chromium 启动实测得到 1 个 `HEAD`、10 个 `206` Range 响应并进入 `[runtime] Initialized`。

## Notes / Compatibility

不修改 WASM、LazyFS、Cosmic 或任何服务端协议；仍使用原有 `web-server` HTTP Range 能力，并保留反向代理路径前缀。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
