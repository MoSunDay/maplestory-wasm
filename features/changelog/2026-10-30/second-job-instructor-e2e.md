# 二转教官（1072000）全流程 e2e：打通真实二转链路

## 背景

`npc-dialogue-toggle-desync` 修复（cc80da1）已由对照 NPC e2e 验证，但触发
该 bug 的原始场景是二转教官多页对话。本条补充真实二转全流程 e2e 并将其
跑通（ALL PASS），过程中连环排障出 5 个与修复正交的环境/服务端问题，
一并记录如下。

## 新增 e2e（ALL PASS）

`tools/verify/maplestory-webui/second-job-instructor-flow.mjs`（默认端口
9264）：

1. 登录 test1 选 test1Warr 卡（level30 战士，map 102020300 sp0）。
2. 走到武术教练 1072000：Space 弹对话 → 对话中再按 Space 保持打开
   （修复点回归）→ Escape 关闭 → 再 Space 重开（症状回归）。
3. 探针点 NEXT×3 + YES + NEXT：服务端 1072000.js 完成 100003、开启
   100004、回收信件并 warp 108000300。
4. 考验图走到同事 NPC 1072004 验证对话可再开，Escape 收尾。

完整前置 runbook（含全部踩坑 SQL）已固化在脚本头部注释。

## 排障过程发现的问题（均与 cc80da1 修复正交）

1. **服务端 quest 大 id 崩角色加载**（已修）：`MapleCharacter.java:1354`
   `rs.getShort("quest")` 遇 quest>32767（如 100003）抛
   `MySQLDataException`，角色完全无法进图。已改 `rs.getInt` 并重编译
   ms-game-server（该源码树在 link_repos/MapleStory-Server，非本仓 git
   管辖，仅在此记录）。
2. **plife 自定义 NPC 刷点三要素**（本 e2e 的关键前置）：
   - `world` 列必须与角色所在 world 一致（本环境真实世界是 world=4
     "Reboot"，非 0），否则 `loadLifeFromDb` 查不到、NPC 不生成；
   - `y` 必须高于目标 foothold 顶部——客户端 `NpcSpawn::instantiate` 用
     `physics.get_y_below` 向下吸附，y=2385 低于地面 fh83 顶 2382 时
     NPC 一路掉进 y=2805 深坑，永远超出 120/80 对话射程；
   - `fh` 填该图真实 fh id（fh=0 在 102020300 不存在，属无效值）。
3. **SQL 造数必须重启 ms-game-server**：`Server.java
   isFirstAccountLogin` 只缓存启动后首次登录的账号→角色集合（SQL 插入
   的角色不在缓存 → `CharSelectedHandler` 静默 closeSession）；地图对象
   另有进程内缓存（plife 改动同样不生效）。
4. **账号/角色数据格式坑**：`characters.sp` 必须逗号分隔（分号触发服务端
   NumberFormatException → 无 WORLD_LIST → 客户端 Asyncify park）；
   `accounts.characterslots` 必须 ≥ 角色数（否则 CharlistHandler 校验
   异常同样 park）；选角 UI PAGESIZE=4，账号只留 1 张卡最稳。
5. **重启偶发 world4 端口绑定失败**：docker restart 后 7975 可能因
   TIME_WAIT 绑定失败，登录服仍引导客户端连 7975 → 页面 park，再重启
   一次即恢复（复跑判据见
   docs/known-issues/2026-10-29-game-server-restart-client-freeze.md）。

## e2e 健壮性修正（脚本侧）

- `mapId` 断言存在双取值竞态（条件先取可为 -1、模板重取已恢复），
  改为单次取值；warp 断言在切图瞬间等待 mapId 稳定后再读。
- `openDialog` 每次尝试前重新走回目标 NPC：地图怪物击退会把角色推出
  120/80 对话射程，Space 落空被误判为"无法再对话"。
- 测试角色 maxhp 拉高到 2500：默认 600 会在走位途中被怪打死回城
  （102000000）断流程。
