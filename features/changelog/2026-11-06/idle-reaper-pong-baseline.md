# 空闲收割 PONG 基准错杀修复（登录连接长空闲被误踢）

## 症状（Context）

- run.mjs UI 验收在角色创建 "character name entered" 后必现
  `Runtime.evaluate` 超时楔死（基线同样挂点）。
- 浏览器 stderr 出现 `[disconnect-watchdog] connection lost: websocket
  close event`，ws-proxy 出现 `[TCP→WS] TCP connection closed by server`。

## 根因

- 服务端空闲收割（0003 引入）以 `testPing(timeThen = now-15s)` 为基准：
  10s 周期的检查 tick 落在 PING 登记后 15~25s 时，ping 后数百毫秒内
  正常应答的 PONG 也满足 `lastPong < timeThen` 被判过期 → 健康登录
  连接 ~45-55s 空闲即被错杀（角色创建素材等待是该时长的第一个场景）。
- 客户端 `MapleConnectionLost.show()` 用阻塞 `window.alert` 通知断线，
  冻结渲染主线程 → CDP `Runtime.evaluate` 全部超时，表现为"验收 flake"。

## 修复

- 服务端：`scripts/server-patches/0015-idle-reaper-pong-baseline.patch`
  （基准改为登记时刻 `mc.getValue()`，"本次 ping 后无 PONG 才关闭"，
  收割节奏不变；已重编 dist 并重启容器）。
- 客户端页面：`web/index.html` 断线提示改非阻塞 `#ms-disconnect-banner`
  （随 8f3ff27 提交），CDP/自动化与真实用户均不再被 alert 冻结。
- 根因与复跑注意事项：docs/known-issues/2026-11-06-ui-hit-offset-and-idle-reaper.md。

## 验收

- `E2E_URL=http://127.0.0.1:8000/web/index.html node tools/verify/maplestory-webui/run.mjs`
  全绿：44 PASS / 0 FAIL / exit 0（含角色创建、物品栏冷开、技能书、
  IME 聊天、资产性能门 p95=16.7ms）。
