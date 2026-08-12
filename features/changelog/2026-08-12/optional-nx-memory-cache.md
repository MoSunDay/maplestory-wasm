Commit: 25a0dc2b2db29b81f6ffb3419b08123cf60f3f0e

# 可选的全量 NX 服务端内存缓存

日期: 2026-08-12

## 变更

- `assets-server` 增加 `--cache-all-nx` / `ASSETS_CACHE_ALL_NX=true` 开关，默认关闭。
- 开启后，服务启动时按既有目录优先级扫描并载入全部 `.nx`，所有连接共享同一份只读数据，块请求不再反复打开和读取磁盘文件。
- 关闭时保留按需磁盘块读取，不承担全量 NX 的常驻内存成本。
- Docker Compose 接入同名环境变量，可用 `ASSETS_CACHE_ALL_NX=true ./scripts/run_all.sh` 启用。

## 约束

- 内存缓存是启动快照；替换 NX 文件后需重启 `assets-server`。
- 当前仓库可服务 NX 合计约 15.29 GiB，启用前应预留对应内存和启动读取时间。

## 验证

- 磁盘模式与内存模式针对同一文件返回完全一致的分块数据，越界及乘法溢出请求安全返回空块。
- assets-server 单元测试与真实 WebSocket 协议集成测试通过。
- 严格 Clippy（`-D warnings`）和 release 构建通过。
