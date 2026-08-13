Commit: 3370abf1c8767b470de950e7c975cc09ffa72acd

# Cosmic 原生部署与 UTF-8 中文取名

## Context

部署机曾用 Docker 运行 Cosmic（GMS v83 模拟器）但实例丢失；同时服务端仅支持 ASCII 角色名/账号名，中文取名在服务端被正则拒绝。

## Change Summary

- Cosmic 改为宿主机原生 systemd 部署：Amazon Corretto 21（`/data00/tiger/jdk/corretto-21.0.12`）+ Maven 3.9.11（`/opt/maven`）构建 `target/Cosmic.jar`，`maplestory-cosmic.service` 常驻运行（端口 8484 / 7575-7577）。
- 修复旧库 schema 与 Liquibase changelog 差异（补 7 张表、3 个列，37 个 changeset 标记 EXECUTED）。
- 服务端开启 UTF-8：`CharsetConstants` 新增 UTF-8 枚举、`Character.canCreateChar` 正则改为 `[\p{L}\p{N}]{3,12}`、`config.yaml` 设 `CHARSET: UTF-8` 并追加 JDBC UTF-8 参数、全部 78 张表转 utf8mb4。
- 新增协议级 E2E `scripts/e2e_utf8_protocol.mjs`：Node 直连（或经 ws-proxy）实现 v83 握手与 AES-256 OFB 加密，验证中文用户名自动注册登录、中文角色名检查与创建、UTF-8 回显。

## Impact Surface

- 部署机服务端运行方式（Docker → systemd）。
- 服务端接受中文账号名与中文角色名（3-12 个 Unicode 字母/数字）。

## Notes / Compatibility

- 协议要点：hello 16 字节中 sendiv=bytes[7..10]、recviv=bytes[11..14]；包头必须在加密**之前**用 sendiv 计算；AES 密钥 32 字节（AES-256）；opcode 为 2 字节小端；登录后状态 23 需先发 `ACCEPT_TOS(0x07)` 带字节 `1`。
- 服务端属外部上游（`/root/MapleStory-Server`），客户端仓库不包含其源码改动。
- 客户端输入桥的 IME 字节截断问题（24 bytes vs 期望 12）仍未定位，不影响服务端 UTF-8 能力。

## Related Docs

- `agents/web/index.md`（Cosmic 部署段）
