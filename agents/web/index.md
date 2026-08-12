# Web 基础设施

Commit: fc979c0fe21730064ed2054302453773440d270d

## 职责

提供浏览器运行 WASM 客户端所需的全部 Web 服务层。三个 Rust 二进制（Cargo workspace crate）通过 WebSocket/HTTP 桥接浏览器与 Cosmic 服务端和本地资源；`web/` 目录仅存放静态页面与配置。

## 边界

- **包含**: HTTP 静态文件服务、WebSocket-TCP 代理、WebSocket 资源服务
- **不包含**: 游戏逻辑、服务端逻辑、WASM 编译

## 架构

```
浏览器
  ├── HTTP ──► web-server :8000         (WASM/JS/HTML 分发)
  ├── WebSocket ──► ws-proxy :8080       (游戏封包 → TCP → Cosmic 服务端)
  └── WebSocket ──► assets-server :8765  (按需 .nx 资源流)
```

三个 crate 共享根 `Cargo.toml` workspace；Docker 侧由 `docker/rust-web.Dockerfile` 多阶段构建同一镜像，`docker-compose.yml` 以不同 command 启动三个服务。

## 关键服务

### web-server (`web-server/`)

HTTP 服务器，默认绑定 8000 端口（`--port`/`--bind`/`--directory` 可配）。负责:
- 提供 `index.html` 入口页面、`build/JourneyClient.js`/`.wasm`、`web/config.json`、字体等静态资源
- 页面在 WASM 和 NX 素材初始化期间显示加载动画及阶段提示，首帧绘制完成后再淡出；初始化失败时原位显示错误
- 输出 WASM 所需的跨域隔离头（COOP/COEP）与 `Cache-Control`
- 支持 HTTP Range 请求（单区间），目录自动索引

### ws-proxy (`ws-proxy/`)

WebSocket-TCP 桥接代理，默认绑定 8080 端口（`--ws-port`/`--bind` 可配）。负责:
- 接收浏览器 WebSocket 连接，首帧二进制消息为目标地址 `host:port`
- 与目标建立 TCP 连接并双向转发字节（WS 二进制帧 ↔ TCP）
- `WS_PROXY_LOCALHOST_TARGET` 环境变量可将 `127.0.0.1`/`localhost` 目标重映射为其他主机（Docker 场景下为 `host.docker.internal`）

### assets-server (`assets-server/`)

LazyFS WebSocket 资源服务器，默认绑定 8765 端口（`--port`/`--bind`/`--directory` 可配）。负责:
- 接收 LazyFS 客户端的按需文件请求（`get_size` 查询大小、`get_chunks` 批量取块、`get_chunk` 单块）
- 以二进制帧 `[u32 块号][u8 文件名长度][文件名][数据]` 返回 .nx 文件块，`chunks_done` 结束批量请求

## 配置

`web/config.json` 提供客户端连接参数。由 `index.html` 在加载 WASM 前读取，仅注入非 `null` 字段；`null` 字段回落到客户端默认值（定义于 `src/client/Configuration.h`）或按页面地址自动探测:

| 字段 | 说明 | 客户端默认值 |
|------|------|--------|
| `AssetsServerIP` | 资源服务器地址 | 页面所在主机 |
| `AssetsServerPort` | 资源服务器端口 | 8765 |
| `AssetsServerProtocol` | 资源协议 (ws/wss) | ws (https 页面为 wss) |
| `ProxyIP` | 代理地址 | 页面所在主机 |
| `ProxyPort` | 代理端口 | 8080 |
| `MapleStoryServerIp` | 目标 Cosmic 服务端 IP | 127.0.0.1 |
| `MapleStoryServerPort` | 目标 Cosmic 服务端端口 | 8484 |

## 依赖关系

- **运行依赖**: Rust 工具链（本地构建）或 Docker（`docker/rust-web.Dockerfile`）
- **上游依赖**: Cosmic 服务端 (TCP)
- **下游使用者**: 浏览器 WASM 客户端
