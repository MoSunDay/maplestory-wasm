# 空闲会话收割确定性生效修复（服务端 manageIdleSessions）

## 症状

无响应的空闲连接（连上登录服 8484 后既不读也不写）不会被服务端按设计及时
收割——实测需 2-3 分钟才被关闭，且时刻不稳定（概率性）。设计意图是
~45-75s（30s MINA idle 登记 + ≥15s 判定阈值 + ≤10s 看门狗 tick）确定性
关闭，将账号从 loggedin 状态释放、回收会话资源。此前只能依赖 ws-proxy
liveness（已上线）从外部兜底清理半开连接。

## 根因（link_repos/MapleStory-Server src/net/MapleServerHandler.java）

- MINA `BOTH_IDLE` 30s（登录服 `net/server/Server.java:1081`、频道服
  `net/server/channel/Channel.java:160`）触发 `sessionIdle` →
  `registerIdleSession`：登记 `Map<MapleClient, Long> idleSessions`（当前
  时间戳）并下发 PING；
- 看门狗 `manageIdleSessions` 每 10000ms 运行（`TimerManager.register`），
  意图对登记满 15000ms 的连接调 `MapleClient.testPing`（lastPong 过期 →
  `closeSession` + `updateLoginState(LOGIN_NOTLOGGEDIN)`）。

缺陷：tick 处理完后在锁内**无条件 `idleSessions.clear()`**，而登记只发生在
30s 一次的 idle 事件上 → 任何条目在它经历的首个 tick 时年龄只有 0-10s
（< 15s 阈值），必然在未做 testPing 检测的情况下被整体清除；正常路径的
testPing 永远不会触发。唯一能存活到阈值的是 `registerIdleSession` 中
`idleLock.tryLock()` 恰好失败而落入 `tempIdleSessions`、经下个 tick 合并
进 `idleSessions` 的条目（时间戳多经历一个 tick 周期），这才是修复前
2-3 分钟"侥幸收割"的来源——一个 bug 掩盖了另一个 bug 的效果。

## 修复（补丁 0003）

`manageIdleSessions` 改为显式 `Iterator` 遍历：

- 条目年龄 ≥ 15000ms → `testPing(timeThen)` 并 `idleIterator.remove()`；
- 条目 session 已断开（`getSession() == null || !isConnected()`）→ 直接
  移除，防泄漏；
- 未达阈值 → 保留到下个 tick，不再被无条件清除。

`tempIdleSessions` 合并逻辑与锁序（`idleLock` → `tempLock`）保持不变；
`testPing` 关闭路径（`MapleSessionCoordinator.closeSession` → MINA I/O 线程
回调 `sessionClosed`）不回取 idleLock，无新增死锁面。修复后设计值确定性
生效：idle 事件（30s）登记 → 满 15s 后的首个 tick（每 10s）testPing →
不回 PONG 即关闭。

## 验证（2026-10-26）

- `bash posix-compile.sh` 全量重编 `dist/`（javac 21），`javap -c` 抽查
  `manageIdleSessions` 字节码：`Iterator.remove` 出现在 testPing 分支与
  断连分支，`idleSessions` 的无条件 `Map.clear` 已消失；
- `bash scripts/game_server_restart.sh` 重启，8484 + 5 个频道口全部就绪；
- 空闲收割时间线（python3 裸 socket 连 `127.0.0.1:8484` 后完全不读写）：
  `t=0.001s` 收到 HELLO（16B）→ `t=30.032s` 收到 PING（6B，idle 登记）→
  `t=50.728s` recv EOF（服务端关闭）。50.7s 落在 45-75s 设计窗口内
  （30s idle + 20.7s 阈值等待/tick 对齐），修复前同场景需 2-3 分钟；
- 回归：`storage-flow.mjs` 8 项 ALL CHECKS PASSED（test2 / charid 219 /
  map 130000200 前置，跑完恢复 map=910000000、loggedin=0）。

补丁：`scripts/server-patches/0003-deterministic-idle-session-reaping.patch`
