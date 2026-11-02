# game-server 重启端口竞态与 worldChars 缓存脱节（2026-11-01）

## 现象

FM 仓库 e2e（fm-storage-flow）连续 30+ 轮失败，三种表层症状交替：

1. 客户端 console 无限刷
   `Reconnecting to channel server 127.0.0.1:7975` / `Failed to connect`，
   登录-选角全通，就是进不了场（PLAYER_LOGGEDIN 永远到不了服务端）；
2. 服务端日志 `Received CHAR_SELECT → Sent SERVER_IP → Sent SET_FIELD`
   看似正常，但客户端 msmap_id 恒 40001、Map.nx chunk 拉到 ~20 个停住，
   200s 超时（地图加载挂起，非资源缺失）；
3. 更早一轮：登录完全无法建立——8484 bind 失败但进程打印
   "MapleStory is now online" 继续跑。

共同根因：**容器重启竞态**。`docker restart` 时旧 Java 进程（shutdown
hook + HikariCP 收尾）仍持有端口数秒，新进程 bind 得到
`Error while binding on 0.0.0.0/0.0.0.0:8484/7975 ... Address already in
use`，而两处 catch 块只 `printStackTrace()`：

- 登录口 8484 挂 → 半活进程（频道全在、登录必死）；
- 某个 world 频道口挂 → `Server.getInetSocket(world, ch)` 返回死端口，
  SERVER_IP 照发，客户端重连循环。

叠加因素：**Server.worldChars 是启动时一次性缓存**。为绕开死端口把
`characters.world 4→3` 直接改库后，服务端仍按缓存 world=4 发 7975——
改库不重启等于没改，极易误判为「客户端/网络问题」。

## 定位手段（对账清单）

- `ss -tln | grep -E '8484|7575|7675|7775|7875|7975'`：六个口应全监听，
  缺谁查谁；端口公式 `7575 + (channel-1) + 100×world`；
- `docker logs ms-game-server | grep -A3 "Error while binding"`：bind
  竞态的直接证据（注意日志是全量的，看重启时段）；
- 客户端 console `Reconnecting to channel server 127.0.0.1:<port>`：
  port 即 SERVER_IP 下发值，反推服务端认为角色在哪个 world；
- `mysql ... SELECT id,name,world FROM characters WHERE id=<cid>` 与
  服务端内存缓存可能脱节——运行中改库必须配合重启才生效。

## 修复（0008 补丁，已应用）

`scripts/server-patches/0008-login-channel-bind-retry-and-world-cache-refresh.patch`：

- 登录口/频道口 bind 失败改为 1/2/4/8/15s 退避重试 6 次，仍失败
  exit(1)（登录口）/RuntimeException（频道口）中止启动，交给容器
  `unless-stopped` 干净拉起——半活进程从此不可达；
- `getCharacterWorldRefreshed` 在 CharSelectedHandler 按单行 SQL 刷新
  worldChars，运行中改库下一会话即生效。

## Runbook（遇到重启后异常）

1. 六口对账 `ss -tln`；缺口 → 看 bind 报错 → **再 restart 一次**
   （无旧进程重叠时即恢复；0008 之后理论上不再需要这一步）；
2. 客户端重连循环里的端口 → 反查 world → 与 DB characters.world 对账；
3. 改过 characters.world 的，重启前先确认目标 world 端口活着，避免
   缓存与 DB 双重错位。
