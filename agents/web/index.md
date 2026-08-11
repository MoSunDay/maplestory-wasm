# Web 基础设施

Commit: 8f4a6f68e1353ab5ce356f4e2bb865732ce65b42

## 职责

提供浏览器运行 WASM 客户端所需的全部 Web 服务层。三个 Python 服务通过 WebSocket/HTTP 桥接浏览器与 Cosmic 服务端和本地资源。

## 边界

- **包含**: HTTP 静态文件服务、WebSocket-TCP 代理、WebSocket 资源服务
- **不包含**: 游戏逻辑、服务端逻辑、WASM 编译

## 架构

```
浏览器
  ├── HTTP ──► server.py :8000         (WASM/JS/HTML 分发)
  ├── WebSocket ──► ws_proxy.py :8080   (游戏封包 → TCP → Cosmic 服务端)
  └── WebSocket ──► assets_server.py :8765 (按需 .nx 资源流)
```

## 关键服务

### server.py

HTTP 服务器，绑定 8000 端口。负责:
- 提供 `index.html` 入口页面
- 提供 `build/JourneyClient.js` 和 `build/JourneyClient.wasm` 
- 提供 `web/config.json` 客户端配置
- 提供字体文件等其他静态资源

### ws_proxy.py

WebSocket-TCP 桥接代理，绑定 8080 端口。负责:
- 接收浏览器 WebSocket 连接
- 将 WebSocket 消息转发为 TCP 连接至 Cosmic 服务端
- 将 Cosmic 服务端响应转发回浏览器
- 支持 `--ws-port` 参数自定义端口

### assets_server.py

LazyFS WebSocket 资源服务器，绑定 8765 端口。负责:
- 接收 LazyFS 客户端的按需文件请求
- 按 WebSocket 分块协议返回 .nx 文件块（`get_size` 查询大小，`get_chunks` 批量取块，二进制帧传输）
- 支持 `--port` 和 `--directory` 参数配置端口和资源目录

### server_fast.py

`server.py` 的可选替代实现，基于 aiohttp 的异步 HTTP 服务器，同样绑定 8000 端口并支持 Range 请求与 WASM 所需的跨域隔离头。用于大文件分发性能更好的场景，二者只启动其一。

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

- **运行依赖**: Python 3.9+, `websockets` 库
- **上游依赖**: Cosmic 服务端 (TCP)
- **下游使用者**: 浏览器 WASM 客户端
