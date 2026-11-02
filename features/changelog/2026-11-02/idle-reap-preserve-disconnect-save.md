# 修复空闲收割"幽灵会话"：断线角色不落库、残留地图

## 背景

补丁 0003 让空闲会话看门狗（`manageIdleSessions`，10s tick）对登记满
15s 的连接确定性调用 `MapleClient.testPing`：无响应连接在
~45-75s（MINA BOTH_IDLE 30s 登记 + 15s 阈值 + ≤10s tick 对齐）被收割。
但收割路径本身有缺陷，构成"幽灵会话"。

## 根因

`MapleClient.testPing` 原实现（本次补丁前的字节码实证，见下）在收割时
手工执行三步清理：

```java
MapleSessionCoordinator.getInstance().closeSession(session, false);
updateLoginState(MapleClient.LOGIN_NOTLOGGEDIN);
session.removeAttribute(MapleClient.CLIENT_KEY);   // ← 致命一步
```

`closeSession(session, false)` 内部 `session.close(false)` 关闭传输层，
而 MINA 的 `sessionClosed` 回调（`MapleServerHandler.sessionClosed →
closeMapleSession`）是**异步**触发的——回调执行时
`session.getAttribute(CLIENT_KEY)` 已被 `removeAttribute` 清空 →
`client == null` → 跳过 `client.disconnect()` → `disconnectInternal`
从不执行：

- 游戏内玩家走不到 `saveCharToDB` / `MapleMap.removePlayer`——角色状态
  不落库、地图上残留"幽灵"；
- 事故证据：`logs/<日期>/players/SaveChar.txt` 曾出现 22:47:08→23:01:38
  断档（浏览器粗暴死亡后无任何落库行）。

触发条件：浏览器"静默死亡"——WebSocket 无 close frame 且无 TCP FIN
（断电/拔线/NAT 丢弃；本地 headless 场景用 SIGSTOP 整个 chrome 进程
复现）。此时 ws-proxy（liveness 90s）晚于服务端空闲收割（45-75s），
`testPing` 是第一个也是唯一的收割执行者，缺陷窗口必然命中。浏览器干净
关闭（WS close frame 或 TCP FIN 及时到达 ws-proxy）走的是正常
`sessionClosed` 路径，不受影响——这也是该缺陷长期潜伏的原因。

## 变更

### linked server（`scripts/server-patches/0010-idle-reap-preserve-disconnect-save.patch`）

`MapleClient.testPing` 主体改为单一 `session.close(true)`，删除收割时
提前清理的三行，交由既有下线链路接管：

```
sessionClosed → closeMapleSession（closeSession/closeLoginSession +
从 CLIENT_KEY 取回 client）→ disconnect → disconnectInternal
```

- 游戏内玩家：`disconnectInternal` 主分支 `removePlayer` +
  `saveCharToDB(true)`（角色落库、离开地图）；
- 登录态会话（player==null）：`disconnectInternal` 尾部本就有等价的
  `closeSession / updateLoginState(LOGIN_NOTLOGGEDIN) / removeAttribute
  (CLIENT_KEY)` 分支，账号不会卡在已登录态；
- 不改任何锁顺序（补丁 0003 已验证 testPing 的 close 路径与 idleLock
  无重入）；`MapleSessionCoordinator` import 文件他处仍在用，保留。

## 验证（2026-11-02）

- **编译**：`javac`（等价 `posix-compile.sh` 全量源集，输出至临时目录，
  不触碰运行中 dist）exit 0；产物 `MapleClient.class` 与 live dist
  逐字节相同（`cmp`），且运行容器 `ms-game-server`（StartedAt 20:38:37）
  晚于 dist 写入（20:38:31）——运行中服务端即补丁后构建。
- **字节码断言**：`javap -c` 对比收割前后——修复前（2026-10-18 dist
  快照）：`closeSession(session,false)` → `updateLoginState(0)` →
  `removeAttribute("CLIENT")`；修复后：仅 `IoSession.close:(Z)`。
- **静默死亡收割（核心场景）**：`systemd-run --unit=mslreap10
  --property=PrivateTmp=no google-chrome-stable --headless`（CDP harness
  `tools/verify/maplestory-webui/cdp.mjs` + `pq-harness-login`）登录
  test1hold 进入游戏（test1Pala，charid 216，map 40001），走动改变坐标
  （x −4→−84）后 `systemctl kill -s SIGSTOP --kill-who=all mslreap10`
  （进程全部进入 T 态：无 WS close frame、无 TCP FIN、连接保持
  ESTABLISHED）：
  - **t+49s**（21:37:19 冻结 → 21:38:08，落在 45-75s 设计窗口）
    `SaveChar.txt` 出现新行
    `Attempting to save test1Pala at Mon Nov 02 08:38:08 GMT-05:00 2026`
    （冻结前最后一条为 21:19:42，中间无该角色任何落库）；
  - MySQL：`SELECT id,name,map FROM characters WHERE id=216` →
    map=40001，与冻结时刻客户端地图（`msmap_id` 探针）一致；
  - `accounts`：test1hold `loggedin` 归零（收割链路走通
    updateLoginState 分支）；
  - 随后 `systemctl kill -s SIGKILL` 清掉冻结浏览器。
- **冒烟（收割后重登）**：全新浏览器实例重新登录 test1hold → 正常进入
  游戏（uiState=5, map 40001）——账号未被收割卡在已登录态；收尾
  `Browser.close` 干净关闭（WS close frame）→ 21:39:42 再现一条
  test1Pala 落库行，正常退出路径无回归。

补丁：`scripts/server-patches/0010-idle-reap-preserve-disconnect-save.patch`
