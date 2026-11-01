# 任务日志 info 子任务配对修复、进度展示与放弃功能

2026-10-25 落地的任务日志核心链路存在配对缺陷与功能缺口，本轮分三段
修复。

## 数据层：info 子任务配对（QuestLog / SetfieldHandlers）

- 服务端 `addQuestInfo` 对 infoNumber>0 的任务会紧随父任务追加一条
  以 infoNumber 为 id 的条目；旧解析只在"同 qid 重复"时配对，判定
  永远不成立，带进度的任务被拆成两行 started，进度串落入错误桶。
- infoNumber 真实数据源是 `Quest.nx Check.img/<qid>/0|1/infoNumber`
  （QuestInfo.img 全量扫描为 0 个，勿走该路径），新增
  `jrc::quest_info_number(qid)` 读取。
- `Questlog::add_questlog_entry`：条目 id 匹配上一条 started 的
  infoNumber 时并入 `in_progress` 并移除父任务；同 qid 重复启发式
  保留为回退。`parse_questlog` 改为"读一条、决策一条"。
- `update_data` 支持 infoNumber 反查折叠回父桶（服务端
  `updateQuest(q, infoUpdate=true)` 以 infoNumber 为 id 下发进度），
  直查 started/in_progress 桶优先。
- `is_started`/`get_last_started` 补 const；删除未使用的 completed
  局部变量死代码。

## 展示层：进度、完成时间、省略与汉化（QuestLogModel / UIQuestLog）

- `QuestRow` 新增 `progress` 与 `completed_time`；行文本追加
  `(3/10)` 式进度。
- 信息面板状态行按行数据显示 `进行中 · 3/10` / `已完成 · 日期`；
  completed 时间兼容 setfield FILETIME 与运行时 epoch 秒两种来源
  （`format_completed_time`，UTC `YYYY-MM-DD`）。
- 分类行接入 `quest_summary`（消灭死代码），未知任务回退"任务"。
- 故事段落按面板高度预截断并补 `……` 省略行（原为硬截断无提示）。
  收尾修复：截断判定抽为 `visible_paragraphs` 纯函数（溢出段不入列，
  省略号占据/挤占溢出槽保证必被绘制），并补确定性单测。
- 文案汉化：进行中 / 已完成 / 未选择任务。

## 放弃任务：QUEST_ACTION(0x6B) 出站封包

- `OutPacket.h` 新增 `QUEST_ACTION = 0x6B`；新建
  `Net/Packets/QuestPackets.h`（`QuestForfeitPacket`，action=3 +
  questId short）。
- 信息面板右下角接入 `BtGiveup` 双代素材按钮（v83
  `UIWindow.img/Quest/BtGiveup` 57x17、post-Chaos
  `quest_info/BtGiveup` 60x16），仅对进行中任务启用；点击发包，
  服务端 SHOW_STATUS_INFO mode1 status0 → `forfeit()` → revision
  bump 自动刷新列表。
- 新探针 `msui_questlog_forfeit_armed`（ImeBridge，导出清单已登记）；
  E2E `questlog-flow.mjs` 补放弃按钮可见性断言。

## 验证

- 单测 `tools/verify/quest-log-model.cpp`：info 配对（10015→10024
  真实数据）、update_data 三路映射、进度串与双时间源格式化断言。
- E2E `maplestory-webui/questlog-flow.mjs`：全流程 11 项断言通过
  （rows=10、行选中切换、armed=1、滚轮/翻转/Escape）。
