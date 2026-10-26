# 自由市场 13 个组队任务：传送与单人模式全链路验证

## 背景

自由市场（910000000）玩法向导（NPC 9010000）提供 13 个组队任务的
传送招募。需验证：① 各 PQ 传送落点正确；② 单人模式（NPC 队友补位）
可从报名到进入入场图完整走通。

## 覆盖范围（13 PQ）

Kerning(21-30)、Ludi(35-50)、LudiMaze(51-70)、Orbis(51-70)、
Pirate(55-100)、Henesys(10-255)、Ellin(44-200)、MagatiaA/Z(71-85)、
Amoria(40-255)、CWKPQ(90-255)、Treasure(140-255)、BossRush(1-255)。
招募图/NPC 坐标与入口图前缀断言内置于脚本。

## 验证流程（tools/verify/maplestory-webui/）

- `fm-pq-teleports.mjs`：登录→FM 向导 9010000→逐个传送→断言落点地图
  ID。结果：**13/13 PASS**。
- `fm-pq-solo-flow.mjs`：每 PQ 独立会话（SQL 预置 test3 回 FM、等级按
  段设置）→ 走到招募 NPC（Pirate/Treasure 配近门 spawn 省长距走位）
  → Space 开对话 → 悬停扫描选「我想参加组队任务」（CWKPQ 两段式
  远征流程）→ 单人模式 sendYesNo 点 YES → 断言进入入场图。NPC 队友
  补位证据以服务端 `[SoloPQ] ... spawned N NPC teammates` 日志为准
  （`msother_character_count` 仅辅助——客户端角色计数不能区分补位
  召唤与同图玩家）。
- 结果：**13/13 终态 PASS**（Kerning soloNpc=2、Ludi 2、Orbis 2、
  Pirate 2、Henesys 2、Ellin 2、MagatiaA/Z 3、Treasure 3、Amoria 5、
  CWKPQ 5（lazy）、LudiMaze/BossRush soloOptional 入场 PASS）。
  早前 6/13 时曾归因宿主高负载，续跑后其余 7 个陆续终态；最后一个
  OrbisPQ 卡在招募 NPC 2013000 对 Space 完全无响应，根因见下节
  「NPC 生成坐标高于所在斜面 foothold」。
- 支撑 harness（新增/增强）：`pq-harness.mjs` 桶文件 + 五模块
  `pq-harness-{login,db,dialog,game,move}.mjs`（凭据运行时解析
  configuration.ini；对话按钮定位=客户端探针优先→视觉→网格三级）、
  `fm-pq-nav.mjs`（LudiMaze 跳跃滞空按↑乘隐藏门、Amoria/CWK/
  Henesys/Ludi 攀爬路线）、`png-probe.mjs`（纯像素解码定位 YES/NO
  按钮，计数阈值法替代已证伪的成对启发）、`cdp.mjs` move()。

## 验证过程中的缺陷修复（本批次）

- **minPlayers=1 致补位静默失败**：Ludi/Orbis/Pirate/Ellin 事件脚本
  minPlayers 曾为 1，`fill = min(5, minPlayers-1) = 0` → 入场后无
  NPC 队友且无任何日志。已改 3，契约测试补数值断言防回归。
- **YES/NO 按钮定位偏移**：纯像素探针曾按 v83 宽度 529 计算对话框
  几何，实际 UI.nx 为 post-Chaos 519，整体偏 ~88px；右下角 HUD 曾致
  假阳性。改为客户端探针（`msnpctalk_button_field` 导出
  UINpcTalk 按钮真实坐标）优先，视觉/网格仅兜底。
- **走位提速**：长距 hold 500→1100ms、菜单扫描步长 5→7、
  Pirate/Treasure 近门 spawn，压缩单轮时长以贴近 115s 上限。
- **NPC 生成坐标高于所在斜面 foothold（OrbisPQ Space 无响应根因）**：
  200080101 招募 NPC 2013000 的 wz life cy=-41，而其所在 fh14 斜面
  (138,-36)→(231,-45) 在 x=198 处插值 y=-41.8。客户端
  `Footholdtree::get_fhid_below` 仅接受 `ycomp >= fy` 的 foothold，
  -41 高于 -41.8 → NPC 对象被吸附到下层 fh26（y≈168），与爬绳到达
  上层的玩家层差 215px，超出 `talk_to_nearest` 的 |dy|<=80 判定 →
  Space 不发 NPC_TALK（服务端零收包，非脚本/造型/账号问题）。修复：
  life y/cy 改 -50（低于斜面最低点 -45 必命中 fh14），重启后 Space
  即开对话并单人入场 PASS。排查链：msnpc_active/位置探针 + Npc.nx
  chunk 懒加载日志证明客户端已建 NPC 对象并渲染，最终由
  `MapNpcs::talk_to_nearest` 距离窗口 + `get_y_below` 方向性锁定。

  wz 补丁全文（`wz/Map.wz/Map/Map2/200080101.img.xml` life 段）：

  ```diff
  -      <int name="y" value="-47"/>
  +      <int name="y" value="-50"/>
  -      <int name="cy" value="-41"/>
  +      <int name="cy" value="-50"/>
  ```

  防回归：`tools/verify/pq-solo-server-contract.mjs` 新增 E 节，解析
  13 张招募图 XML，断言每个招募 NPC life 的 `cy ≤ 所在 fh 在 x 处的
  斜面插值`（OrbisPQ 原缺陷值 cy=-41 会以精确坐标差直接 FAIL）；
  服务端 checkout 经 `link_repos/MapleStory-Server` 符号链接读取。

## 已知非阻塞问题（既有，非本次范围）

- MySQL `rank = 1, rankMove = 0` 保存语法错误（服务端既有 bug）。
- ws-proxy 被外部 8-12 分钟周期重建致偶发会话中断（流程按可重试设计）。
