# 重复登录接管（顶号）

## 背景

用户要求：账号已在线时，新的合法登录应当踢掉旧会话并接管，而不是以
status 7（已登录）失败。此前 linked server 在密码校验前就拒绝重复登录，
游戏内角色还会一直挂着旧连接。

## 变更（linked server `link_repos/MapleStory-Server`）

- `src/client/MapleClient.java`：从 `login()` 与 `loginWithEmail()` 移除
  密码校验前的"账号已登录"拒绝分支；`finishLogin()` 移除 status 7 拒绝，
  改为始终调用 `updateLoginState(LOGIN_LOGGEDIN)`。
- `updateLoginState(LOGIN_LOGGEDIN)` 已有逻辑调用
  `MapleSessionCoordinator.updateOnlineSession()`：对该账号此前的
  `MapleClient`（登录阶段或游戏内）执行 `forceDisconnect`，再把新会话登记
  为在线会话。
- 旧客户端走既有 `disconnectInternal`：保存角色并写入 `loggedin=0`；随后
  新会话的 `loggedin=2` 覆盖该值，账号状态以新会话为准。
- linked server 文档 `features/game-login/index.md` 同步补充该行为说明。

## 行为

- 正确密码：新会话接管并关闭旧会话（含游戏内角色），旧连接收到断开。
- 错误密码：返回失败码 4，旧会话保持在线，不受影响。
- 无协议、数据库表或配置变更。

## 验证

- linked server 源码经 `./posix-compile.sh` 通过 javac 编译。
- 两个 v83 会话协议核对：
  - 登录阶段接管：A 在线，B 同账号 status 0，A 的 socket 被关闭。
  - 错误密码保护：B 返回 status 4，A 保持在线。
  - 游戏内接管：A 以种子角色进入 world 4 / channel 7975，B 同账号
    status 0，A 的频道 socket 被关闭。
  - 清理后 `accounts.loggedin` 回到 0。
