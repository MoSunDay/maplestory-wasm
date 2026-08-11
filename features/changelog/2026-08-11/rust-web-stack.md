# Rust Web 服务栈替换 Python 实现 + 客户端协议修正

日期: 2026-08-11

## 范围

- 新增 Cargo workspace（根 `Cargo.toml`/`Cargo.lock`）与三个 crate：
  - `web-server/`：静态文件服务（端口 8000），Range 请求、keep-alive、COOP/COEP 跨域隔离头
  - `ws-proxy/`：WebSocket→TCP 游戏代理（端口 8080），首消息 `host:port` 目标解析、`WS_PROXY_LOCALHOST_TARGET` Docker 环回重映射
  - `assets-server/`：LazyFS 按需资源服务（端口 8765），`get_size`/`get_chunks`/`get_chunk` 协议，二进制帧 `[u32 块号][u8 文件名长][文件名][数据]`，与 `src/client/LazyFS/lazyfs.js` 线格式兼容
- 删除 Python 实现：`web/server.py`、`web/server_fast.py`、`web/ws_proxy.py`、`web/assets_server.py`、`web/requirements.txt`、`docker/web.Dockerfile`
- Docker：新增 `docker/rust-web.Dockerfile`（多阶段构建），`docker-compose.yml` 三个 web 服务改用 Rust 镜像（`wasm-builder` 服务保持不变）
- 客户端协议修正（配合 custom-client 模式服务端）：
  - `LoginParser.cpp`：角色 stats 中 `level` 按 short 读取（原按 byte 导致后续字段错位）
  - `CharCreationPackets.h`：创建角色时 `hair + hairColor` 合并为单个 int 写入
- 文档/记忆同步：`AGENTS.md`、`README.md`、`agents.md`、`agents/web/index.md`、`agents/client/LazyFS/index.md`、`web/README.md`、`docs/ms-network-protocol.md`
- `.gitignore`：新增 `/target/`、`*.nx`、`link_repos`

## 动机

Python 服务无类型检查、依赖手动 pip 安装且性能受限；Rust 单二进制便于 Docker 多阶段构建分发，且 ws-proxy/assets-server 的协议逻辑可用集成测试覆盖。客户端两处解析/打包字段与运行中服务端的 custom-client 模式不一致，导致 CHARLIST/角色创建流程数据错位。

## 测试覆盖表（当次实跑）

| 命令 | 结果 |
|---|---|
| `cargo build --workspace` | PASS（Finished dev profile） |
| `cargo test --workspace` | 37 passed; 0 failed（assets-server 4 单测 + 8 集成；web-server 5 单测 + 10 集成；ws-proxy 3 单测 + 7 集成） |
| `cargo clippy --workspace --all-targets -- -D warnings` | PASS，零警告 |
| `./scripts/build_wasm.sh`（emsdk 本地） | PASS，`Build finished successfully`，产物 `build/JourneyClient.{js,wasm}` 已含客户端修改 |

## 回归风险

- 低：web 服务对外契约（端口、请求/响应格式）按原 Python 行为实现并有集成测试锁定；LazyFS 线格式与 `lazyfs.js` 注释逐条对照。
- 中：客户端 `level` 读取宽度变更仅适配 custom-client 模式服务端；对接标准 v83 服务端需评估（`docs/ms-network-protocol.md` 已注明）。

## 回滚

`git revert <commit>` 即可恢复 Python 服务栈与旧解析逻辑。

## 未随本次提交的本地改动

- `web/config.json`（`MapleStoryServerIp: 127.0.0.1`、`ProxyPort: "8090"`）：疑似本地调试遗留，`ProxyPort 8090` 与全部文档及 ws-proxy 默认端口 8080 不一致，未暂存、待用户确认。
