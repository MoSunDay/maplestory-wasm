# game-server 崩溃重启期间：WASM 客户端登录流冻结（排查记录）

> 状态：2026-10-29 观测，环境侧事件，客户端代码无需改动（但可考虑加断线看门狗）。
> 目击者：圣甲术修复会话。若已过时请直接删除本文件。

## 症状

真实浏览器 e2e（iron-body-* / equip-stat-flow 等）在 login UI 阶段随机冻结：
CDP `Runtime.evaluate` 45s 超时、渲染器 CPU 降至个位数（活死）、chromium 偶发
直接崩溃。冻结点在页面加载后 40~55s 墙钟，与交互阶段无关。

## 根因链

1. ms-game-server 被反复热重启（本会话观测到至少 3 次：21:5x / 22:05 / 22:50）。
2. 客户端页面加载即 WS 连接登录服；服务端中途断流后，wasm 封包解析在
   Asyncify 等待中永久挂起——主线程 parked、无异常、无网络错误。
3. CDP 侧表现为 evaluate 超时；换 chrome 实例/端口/账号/Map.nx 版本均无效。

## 对照实验（已排除项）

| 排除项 | 手段 | 结果 |
|---|---|---|
| 新生 Map.nx (23:08 再生) | 旁路 assets-server:8766 供 10/17 旧版 | 仍在 t≈45s 冻结 |
| assets-server | 协议层直连 get_size/get_chunks | 0-1ms 全部正常 |
| ws-proxy | 握手 + 日志 | 正常 |
| 客户端二进制/负载 | `--host-resolver-rules` 屏蔽 127.0.0.1:8090 | **72s+ 全程 1-2ms 响应** |

## 给 e2e 作者的提示

- 服务端重启窗口内浏览器 e2e 必挂，先 `docker ps` 确认 ms-game-server
  Up 时长 > 5 分钟、`docker logs --since 5m` 无新 Exception 再跑。
- ws-proxy 日志出现 `Connection refused ... 8484` 即服务端不可用。
- 僵尸 chromium 会持有调试端口；cdp.mjs 启动时已自动 fuser -k 清理。

## 23:17 重启后的新阶段：WORLD_LIST 消失（23:40 复查）

- 服务端 23:17 再次重启后进入**稳定但不完整**状态：`docker logs` 全量统计
  `LOGIN_PASSWORD` 78 次、`Sent Packet: WORLD_LIST` **0 次**（含对端会话自己的
  登录循环）。所有新会话停在 world select，与账号/chromium 端口无关。
- 同期 `build/JourneyClient.{js,wasm}` 被对端会话于 22:42 重建（源含
  d550890，探针 `_mscombat_last_buffcast` 胶水已在产物中验证存在）。
- 判定：共享环境正在经历登录协议改造的中间破损态（客户端/服务端两侧同时
  被对端迭代），**非客户端回归**。待对端收尾后重试即可。
- 复跑判据（升级版）：除 Up>5min / 无新 Exception 外，还需
  `docker logs ms-game-server --since 10m | grep -c "WORLD_LIST"` > 0。
- 附带澄清：`--virtual-time-budget` 会让 LazyFS 胶水的 setTimeout 立即到期，
  控制台出现假阳性 `LazyFS request timed out`；抓 console 时勿用虚拟时间。
