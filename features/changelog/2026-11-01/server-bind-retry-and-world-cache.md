# 服务端启动端口 bind 重试与角色 world 缓存刷新

## 背景

FM 仓库 e2e 收尾排障（2026-11-01 晚）实录两处服务端半活缺陷：

1. **重启竞态 → 死端口**：`ms-game-server` 容器重启时新旧 Java 进程短暂
   重叠（shutdown hook + HikariCP 收尾数秒），新进程对 8484（登录口）或
   某个频道口（7575+100×world）bind 得到 "Address already in use"。
   原实现两处 catch 均只 `printStackTrace()`：登录口挂了进程仍打印
   "is now online"；world4 频道口挂了 SERVER_IP 持续指向死端口，客户端
   `Reconnecting to channel server 127.0.0.1:7975` 无限重连，登录侧
   无任何报错入口——两态都只能靠人工 `ss -tln` 对账发现。
2. **worldChars 缓存与 DB 脱节**：`Server.worldChars` 仅启动时从 DB
   载入；测试侧直接 `UPDATE characters SET world=...`（本次为绕开死端口
   所做的应急改库）后，`CharSelectedHandler` 仍按缓存 world 计算
   `getInetSocket`，继续发旧 world 端口。

## 变更（linked server，`scripts/server-patches/0008-login-channel-bind-retry-and-world-cache-refresh.patch`）

- `Server.run()` 登录口 bind → `bindLoginPortWithRetry`：1/2/4/8/15s 退避
  重试 6 次，仍失败 `System.exit(1)`（容器 `unless-stopped` 自动拉起，
  比半活进程可诊断得多）；
- `Channel` 构造器频道口 bind → `bindWithRetry`：同退避，最终失败抛
  `RuntimeException` 中止启动而非静默缺一个 world；
- 新增 `Server.getCharacterWorldRefreshed(chrid)`：单行
  `SELECT world` + 回写缓存，SQL 异常回退旧缓存；
  `CharSelectedHandler` 改用该方法——运行中改库后下一个登录会话即生效。

## 验证

- 重放补丁 → `posix-compile.sh`（本次为定向 javac 三文件入 dist）→
  `docker restart ms-game-server`：`ss -tln` 六口（8484/7575/7675/7775/
  7875/7975）全监听，日志无 "Error while binding"；
- 登录冒烟（IME 直写账号 + 键盘密码 → msui_state 离开 1）通过，
  服务端日志无 `SQLException`；
- 补丁对 pristine checkout `patch -p1` 重放输出与已应用文件逐字节一致。

排障过程与 runbook 详见
`docs/known-issues/2026-11-01-game-server-restart-bind-race-and-world-cache.md`。
