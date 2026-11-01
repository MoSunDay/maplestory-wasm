# 服务端 NPC 脚本错包修复：`constants.game.*` → `constants.*`

## 背景

用户反馈"点击武术教练（102000003 图 1022000）无法对话"。客户端修复
（cc80da1）与 e2e（8ef1264）均绿，但该 NPC 服务端脚本 1022000.js 引用
`Packages.constants.game.GameConstants`，而本服类在 `constants.GameConstants`
（src/constants/GameConstants.java:444）。nashorn 每次点击抛
`ClassNotFoundException` → 脚本死 → NPC_TALK 无回包 → 对话永不弹出。
1072000.js 不受影响，故 e2e 未暴露。

## 修复

link_repos/MapleStory-Server 下 18 个文件（15 处全限定引用 + 10 处
`importPackage`，去重后 18 文件）统一改为 `Packages.constants`。
`cm.getJob()` 返回 `MapleJob`，形参匹配无需适配。脚本引擎每次
`engine.eval` 读盘无缓存，但 SQL 造数需 `docker restart ms-game-server`。

## 验证

- **nashorn 直跑**（/tmp/npscript/ScriptProbe.java，服务端 dist 类路径 +
  同款 nashorn-core 15.4）：`Packages.constants.GameConstants.getHallOfFameMapid`
  成功返回真实名人堂 mapid（102000004），脚本走完 job=100/持信 4031008
  分支 `sendOk("去见#b#p1072000##k吧。")`，无 ClassNotFoundException。
- **生产日志**：修复部署（11:58Z 重启）后 ClassNotFound 绝迹；12:40Z 起
  观测到真实玩家 `NPC_TALK → Sent NPC_TALK → NPC_TALK_MORE → Sent`
  完整往返（对话弹出且可翻页）。
- 浏览器 e2e 本次未复跑：build/JourneyClient.wasm 为共享产物，被并行
  会话于 21:20 重建（UI 汉化大改），无头探针在该构建上登录流程漂移，
  不适合作为本修复的验证载体。

另注：102000003 地图 VR=(-400,400,-300,300) 恰等于视口 800×600，相机
完全钳死，NPC 屏幕坐标 = world+(400,300)+VIEWYOFFSET(10)；高台 1022000
（fh44，脚点 y=16，地面 y≈150-197）对键盘交互键天然超程，鼠标点击路径
（MapNpcs::send_cursor，无距离限制）是唯一触发方式。
