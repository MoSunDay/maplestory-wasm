# 2026-11-06 UI 命中偏移 10px + 服务端空闲收割错杀（阻断 UI 验收）

## 现象

- 物品栏格子/页签点击命中错位（点击比可见像素高 10px）。
- run.mjs UI 验收在角色创建"character name entered"后必现
  `Runtime.evaluate` 超时楔死（基线同样挂在此处）。

## 根因一：输入未补偿 VIEWYOFFSET

渲染管线把整帧下移 `Constants::VIEWYOFFSET`（GraphicsGL yoffset
uniform，见 StatusBar 等处的 ±10 补偿），而 `Window.cpp` 的 GLFW
鼠标回调直接用原始坐标 → 所有 UI 点击落点高 10px。NX 取证证明
`UIItemInventoryNodes.h` 的网格几何常量（rows y=51/36/35 pitch、
cols x=10/46/82/118）全部正确，勿改。

修复：`to_design_position`（cursor_callback + event_cursor_position）
统一 `y - VIEWYOFFSET`；CDP 驱动侧 `cdp.mjs` 的 click/move/drag
映射统一 `+VIEW_Y_OFFSET`（questlog/cash-shop 本地映射同步）。
提交 8f3ff27。

## 根因二：服务端空闲收割错杀 + 阻塞 alert 楔死 CDP

`MapleServerHandler.testPing` 原以 `now-15s` 为基准比较 lastPong，
检查 tick（10s 周期）落在 ping 后 15~25s 时，数百毫秒内正常应答的
PONG 也被判过期 → 登录连接在 ~45-55s 空闲（角色创建等长等待）后被
错杀。客户端 `MapleConnectionLost.show()` 的 `window.alert` 同步
阻塞渲染主线程 → CDP evaluate 全部超时。

修复（link_repos/MapleStory-Server，非 git 管理）：
`manageIdleSessions` 改传 ping 时刻（`mc.getValue()`）；
`web/index.html` 断线提示改为非阻塞 `#ms-disconnect-banner`
（复用 SockInternal 看门狗元素），重编
`dist/net/MapleServerHandler.class` 后 `scripts/game_server_restart.sh`。

## 复跑注意

- 验收前需清理账号 63（test1）的残留 `测试一` 角色，否则默认建角
  流程因重名在 "accepted name never entered customization" 失败。
- 资产性能门（p95≤34ms、longtask≤100ms）对共享主机负载敏感：
  load5 >7 时可能超预算，等负载回落重跑即可（2026-11-06 全绿时
  p95=16.7ms、longtask=0）。
- 后台进程跨工具调用必被回收（见 2026-10-31 文档）；长命令跑满
  150s 由 harness 自动转后台监管。
