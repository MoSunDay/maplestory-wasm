Commit: d484de5f30c73b3b55c1c5f1ed9816ffe133fabd

# 角色创建 DB Endpoint 恢复

## Context

创建角色最终提交后弹出 `An error occurred while trying to connect to the server`。线上 Cosmic 日志显示 `SQL Driver refused to give a connection ... Communications link failure`，服务单元仍固定使用 `jdbc:mysql://127.0.0.1:3306/maplestory`，但当前宿主机回环 3306 已不可用，真实 `maplestory` MySQL 容器在 Docker 网络 IP 上提供 3306。

## Change Summary

- 新增 `scripts/maplestory_db_endpoint.sh`，启动时从运行中的 `mysqld`/`mariadbd` 进程里定位 `MYSQL_DATABASE=maplestory` 的实例，解析容器 IPv4，并为 Cosmic 注入 `MAPLE_DB_URL`、`MAPLE_DB_USER`、`MAPLE_DB_PASS` 后执行 linked server 的 `run.sh`。
- 生产 `maplestory-cosmic.service` 改为调用 `scripts/maplestory_db_endpoint.sh run-cosmic`，不再固定连接失效的 `127.0.0.1:3306`。
- 生产 `maplestory-mysql-public.service` 改为调用 `scripts/maplestory_db_endpoint.sh proxy`，避免公网 IPv6 DB 代理继续转发到空回环端口。
- 客户端 `Configuration` 显式注册 `MapleStoryServerPort`、`ProxyIP`、`ProxyPort`，保证 `web/config.json` 覆盖项进入统一配置生命周期。

## Impact Surface

- 角色创建恢复依赖数据库插入、角色列表更新和 `ADD_NEW_CHAR_ENTRY` 回包。
- Web 服务端、ws-proxy、assets-server 运行端口不变：8001、8090、8765。
- 不新增环境变量，不修改数据库 schema，不删除或清理生产数据。

## Verification

- `scripts/maplestory_db_endpoint.sh endpoint` 返回当前 `maplestory` MySQL 容器 `172.17.0.24:3306`。
- `mysqladmin --protocol=TCP -h172.17.0.24 -P3306 -umaplestory ... ping` 通过；只读查询 `accounts=40`、`characters=25`。
- 重启后 `maplestory-cosmic.service` 使用 `jdbc:mysql://172.17.0.24:3306/maplestory?...`，并监听 8484/7575。
- `node scripts/e2e_utf8_protocol.mjs` 使用 ws-proxy 创建唯一中文角色 `角3509`，收到 `ADD_NEW_CHAR_ENTRY`，服务端日志记录 `Received Packet: CREATE_CHAR` / `Sent Packet: ADD_NEW_CHAR_ENTRY`。
