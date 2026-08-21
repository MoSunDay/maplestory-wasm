Commit: d484de5f30c73b3b55c1c5f1ed9816ffe133fabd

# Web 基础设施

## 职责

提供浏览器运行 WASM 客户端所需的全部 Web 服务层。三个 Rust 二进制（Cargo workspace crate）通过 WebSocket/HTTP 桥接浏览器与 linked MapleStory 服务端和本地资源；`web/` 目录仅存放静态页面与配置。

## 边界

- **包含**: HTTP 静态文件服务、WebSocket-TCP 代理、WebSocket 资源服务
- **不包含**: 游戏逻辑、服务端逻辑、WASM 编译

## 架构

```
浏览器
  ├── HTTP ──► web-server :8000         (WASM/JS/HTML 分发)
  ├── WebSocket ──► ws-proxy :8080       (游戏封包 → TCP → linked 服务端)
  └── WebSocket ──► assets-server :8765  (按需 .nx 资源流)
```

三个 crate 共享根 `Cargo.toml` workspace 和 `web-common/listener.rs` 双栈监听实现；Docker 侧由 `docker/rust-web.Dockerfile` 多阶段构建同一镜像，`docker-compose.yml` 以不同 command 启动三个服务。

三个服务默认绑定 IPv6 通配地址 `::`，并显式关闭 `IPV6_V6ONLY`，因此同一端口同时接受 IPv4 与 IPv6；显式传入其他 `--bind` 地址时维持单地址族监听。

对外制品下载另由 `nx-nginx-dl` 提供 `:48562`，配置基线为 `docker/artifact-nginx.conf`：站点根目录固定为 `/data00/maplestory-wasm-deploy/public-artifacts`，只列出 `JourneyClient.js`、`JourneyClient.wasm`、`MapleStory-Server.jar` 和 Linux x86-64 `ws-proxy`，不暴露源码仓库。WASM 两项通过只读符号链接指向 `current/build`，会跟随 release 原子切换；Java 与代理制品由全量源码编译和 release 构建显式发布到稳定目录，不受 `current` 切换影响。

## 关键服务

### web-server (`web-server/`)

HTTP 服务器，默认绑定 8000 端口（`--port`/`--bind`/`--directory` 可配）。负责:
- 提供 `index.html` 入口页面、`build/JourneyClient.js`/`.wasm`、`web/config.json`、字体等静态资源
- `web/index.html` 以页面目录为基准使用相对资源路径；部署在反向代理路径前缀下时，配置、字体和 WASM 产物请求会保留该前缀
- 页面先读取 WASM 长度，再以 1 MiB HTTP Range、最多 3 路并发下载；每块请求 15 秒超时并最多尝试 3 次，完整组装后通过 `Module.wasmBinary` 交给 Emscripten，避免单个大请求中断后永远卡在 `wasm-instantiate`
- 页面在 WASM 下载和 NX 素材初始化期间显示进度及阶段提示；有效进度会重置 45 秒停滞计时，首帧绘制完成后再淡出，重试耗尽或初始化失败时原位显示错误
- 运行时前台素材网络缺块或可见贴图尚未进入图集时会显示玻璃化遮罩并冻结输入和模拟；地图切换使用同一遮罩报告安全集进度，失败后保留错误信息及重试按钮，静默后台预取不触发遮罩
- `web/world_select_input.js` 把缩放后的 Canvas 坐标还原为 800×600 游戏坐标，并为确认按钮单击及频道双击提供浏览器原生事件兼容路径；C++ 侧负责状态校验和重复请求抑制
- 输出 WASM 所需的跨域隔离头（COOP/COEP）与 `Cache-Control`
- 支持 HTTP Range 请求（单区间），目录自动索引
- HTTP header 不限制字段数量，仅限制整个请求头块最多 10 MiB（包含结尾的 `CRLFCRLF`）；恰好达到上限可接收，超出后返回 `400 Bad Request` 并关闭连接

### ws-proxy (`ws-proxy/`)

WebSocket-TCP 桥接代理，默认绑定 8080 端口（`--ws-port`/`--bind` 可配）。负责:
- 接收浏览器 WebSocket 连接，首帧二进制消息为目标地址 `host:port`
- 与目标建立 TCP 连接并双向转发字节（WS 二进制帧 ↔ TCP）
- 任一转发方向结束时显式对目标 TCP 执行双向 shutdown；浏览器发送 Close 帧或直接消失后，linked server 会立即收到 EOF，而不依赖拆分 socket half 的延迟析构
- `WS_PROXY_LOCALHOST_TARGET` 环境变量可将 `127.0.0.1`/`localhost` 目标重映射为其他主机（Docker 场景下为 `host.docker.internal`）

### assets-server (`assets-server/`)

LazyFS WebSocket 资源服务器，默认绑定 8765 端口（`--port`/`--bind`/`--directory` 可配）。负责:
- 接收 LazyFS 客户端的按需文件请求（`get_size` 查询大小、`get_chunks` 批量取块、`get_chunk` 单块）
- 以二进制帧 `[u32 块号][u8 文件名长度][文件名][数据]` 返回 .nx 文件块，`chunks_done` 结束批量请求
- 默认按需从磁盘读取；传入 `--cache-all-nx` 或设置 `ASSETS_CACHE_ALL_NX=true` 时，启动阶段把搜索目录下全部 `.nx` 文件载入共享只读内存，后续请求仅从内存切片。内存模式是启动快照，NX 更新后须重启服务

## 配置

`web/config.json` 提供客户端连接参数。由 `index.html` 在加载 WASM 前读取，仅注入非 `null` 字段；`null` 字段回落到客户端默认值（定义于 `src/client/Configuration.h`）或按页面地址自动探测:

| 字段 | 说明 | 客户端默认值 |
|------|------|--------|
| `AssetsServerIP` | 资源服务器地址 | 页面所在主机 |
| `AssetsServerPort` | 资源服务器端口 | 8765 |
| `AssetsServerProtocol` | 资源协议 (ws/wss) | ws (https 页面为 wss) |
| `ProxyIP` | 代理地址 | 页面所在主机 |
| `ProxyPort` | 代理端口 | 8080 |
| `MapleStoryServerIp` | 目标 linked 服务端 IP | 127.0.0.1 |
| `MapleStoryServerPort` | 目标 linked 服务端端口 | 8484 |

Docker 全量 NX 内存缓存开关：`ASSETS_CACHE_ALL_NX=true ./scripts/run_all.sh`。默认值为 `false`，不会占用全量 NX 对应的常驻内存。


## Linked 服务端部署（上游依赖）

游戏服务端源码固定使用仓库内的 `link_repos/MapleStory-Server`，以**宿主机原生 systemd** 方式部署（非 Docker）：

- 源码、`wz` 和 `scripts` 位于 `link_repos/MapleStory-Server`
- 使用宿主机 ByteOpenJDK 11；`bash posix-compile.sh` 把 Java 类编译到 `dist/`
- 运行：systemd 单元 `maplestory-cosmic.service` 的历史名称保留，但 `WorkingDirectory` 必须是 linked server，入口为 `net.server.Server`；登录端口 8484、世界 0 单频道 7575
- 数据库 URL、用户和密码通过 `MAPLE_DB_URL`、`MAPLE_DB_USER`、`MAPLE_DB_PASS` 覆盖，避免把凭据写入 Git；`maplestory-cosmic.service` 通过仓库脚本 `scripts/maplestory_db_endpoint.sh run-cosmic` 在启动时定位当前声明 `MYSQL_DATABASE=maplestory` 的 MySQL 进程，读取其容器 IP、`MYSQL_USER` 与 `MYSQL_PASSWORD` 后再执行 linked server 的 `run.sh`
- 数据依赖 `maplestory-mysql` 容器（`maplestory` 库，全部表 utf8mb4）；容器 IP 可能变化，生产服务不得固定使用 `127.0.0.1:3306`。`maplestory-mysql-public.socket` 与 `maplestory-mysql-public.service` 仅在宿主机指定公网 IPv6 的 `3306` 上监听，并由同一 helper 的 `proxy` 模式把 `systemd-socket-proxyd` 指向当前容器 IP
- 管理：`systemctl restart|status maplestory-cosmic`；日志使用 `journalctl -u maplestory-cosmic -f` 和 linked server 的 `logs/`
- 恢复验证：`systemctl show maplestory-cosmic -p WorkingDirectory` 必须指向 `link_repos/MapleStory-Server`，端口 8484/7575 由同一 Java PID 监听，日志出现 `MapleStory is now online`
- 协议级 E2E（无浏览器）：`node scripts/e2e_utf8_protocol.mjs`（v83 握手+AES-256 OFB 加密实现，中文用户名自动注册登录 + 中文角色名创建）

## 依赖关系

- **运行依赖**: Rust 工具链（本地构建）或 Docker（`docker/rust-web.Dockerfile`）
- **上游依赖**: `link_repos/MapleStory-Server` 服务端 (TCP)
- **下游使用者**: 浏览器 WASM 客户端
