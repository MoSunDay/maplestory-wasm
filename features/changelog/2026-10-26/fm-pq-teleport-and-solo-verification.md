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
  段设置）→ 走到招募 NPC → Space 开对话 → 悬停扫描选「我想参加组队
  任务」（CWKPQ 两段式远征流程）→ 单人模式 sendYesNo 点 YES → 断言
  进入入场图 + `msother_character_count` 读取 NPC 队友数。结果：
  **13/13 PASS**（证据：`/tmp/fm-pq-solo-flow/<NN>-PQ>/summary.txt`，
  含每人入口图截图）。
- 支撑 harness（新增/增强）：`pq-harness.mjs`（登录/对话/走位/DB 预置
  通用层，凭据运行时解析 configuration.ini）、`fm-pq-nav.mjs`
  （LudiMaze 跳跃滞空按↑乘隐藏门、Amoria/CWK/Henesys/Ludi 攀爬路线）、
  `png-probe.mjs`（纯像素解码定位 YES/NO 按钮）、`cdp.mjs` move()。

## 已知非阻塞问题（既有，非本次范围）

- MySQL `rank = 1, rankMove = 0` 保存语法错误（服务端既有 bug）。
- ws-proxy 被外部 8-12 分钟周期重建致偶发会话中断（流程按可重试设计）。
