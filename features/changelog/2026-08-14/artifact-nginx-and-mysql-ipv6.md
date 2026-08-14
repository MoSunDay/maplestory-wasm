Commit: 9e754ab7d1a558ddfa11b800297a5f82071425f1

# 收紧制品 nginx 并开放 MySQL IPv6

## Context

原 `nx-nginx-dl` 将整个源码仓库作为自动索引根目录，而 MySQL 只映射到宿主机回环地址。外部使用者需要下载当前 WASM 制品并通过指定 IPv6 连接数据库，但不应读取仓库中的源码和配置文件。

## Change Summary

- `nx-nginx-dl :48562` 的站点根目录改为当前 release 的 `build/`，仅公开 `JourneyClient.js` 与 `JourneyClient.wasm`。
- nginx 部署挂载保持只读，并通过 `/data00/maplestory-wasm-deploy/current` 自动跟随 release 切换。
- 新增 systemd socket proxy，只在 `2605:340:cd50:1301:975a:e1c3:a8b9:d386:3306` 接收连接并转发到 `127.0.0.1:3306`；MySQL 容器和数据卷不重建。

## Impact Surface

- 对外 WASM 制品下载端点 `:48562`。
- MySQL 公网 IPv6 连接端点 `:3306`。

## Notes / Compatibility

- nginx 对仓库中的 `README.md` 等路径返回 404，WASM Range 请求返回 206。
- MySQL 协议握手和既有应用账号认证均已通过指定 IPv6 验证；未修改表、记录、账号密码或数据库容器数据卷。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
