# 任务日志窗口（新版 UI 任务系统接入）

客户端此前没有任何任务 UI：服务端下发的任务状态回包要么是
NullHandler，要么只打印日志。本轮接入任务日志窗口（Q 键 / 状态栏
Menu）与配套封包，任务状态（放弃/数据更新/完成/过期）实时反映到
窗口列表。

## 窗口：IO/UITypes/Quest/UIQuestLog

新建 `UIQuestLog.{h,cpp}`（101/400 行，`UIDragElement`，
`TYPE = QUESTLOG`、`TOGGLED`，可拖动、Q 键或再按 Q 翻转关闭）：

- **双代素材 dispatch**（`QuestLogNodes.h`）：沿用登录链的世代探测
  标记 `Login.img/WorldSelect/BtChannel`——post-Chaos UI.nx 从
  `UIWindow2.img/Quest/list`（左列表）+ `UIWindow2.img/Quest/quest_info`
  （右信息面板）取素材，v83 回退 `UIWindow.img/Quest`；两代只有根
  路径不同，信息面板始终单独解析。
- **双 Tab**：In Progress / Completed，优先 `Tab/enabled|disabled`
  双精灵按钮（TwoSpriteButton），素材缺失时回退文本标签。
- **列表**：每页 9 行 + `Slider`，按任务 id 升序；右侧信息面板显示
  选中任务的名称/分类/摘要。
- **自动刷新**：`update()` 比较角色任务日志的 `revision`，变化即
  重建行并尽量保留选中项。

## 数据层

- `Character/QuestLog.{h,cpp}` 扩展：新增 `forfeit` / `update_data` /
  `complete` / `revision` 与 `list_started/list_in_progress/
  list_completed` const 访问器，每个 mutator 递增 revision 供窗口
  廉价检测变化；并修复 `get_last_started` 在空 map 上自减迭代器的
  UB（空表返回 0）。
- 新建 `IO/UITypes/Quest/QuestLogModel.{h,cpp}` 纯函数模型：`collect_rows`
  用 set 合并 started 与 in_progress（同一任务两表都有时只显示一行），
  completed 走过滤；名称/分类读 `Quest.nx QuestInfo.img/<id>` 的
  `name`/`parent`，未知任务回退 `?id`。头文件刻意不依赖 nx，
  可独立编译进单测。

## 封包：Net/Handlers/QuestHandlers

新建 `QuestHandlers.{h,cpp}`（59/180 行）：

| opcode | 封包 | 负载 | 行为 |
|---|---|---|---|
| 49 | QUEST_CLEAR | int16 questid | 立即完成（本地时间戳） |
| 150 | SET_QUEST_CLEAR | lenient int16 | 旧版完成通知；本服务端从不发送，宽松解析并镜像 QUEST_CLEAR |
| 211 | UPDATE_QUEST_INFO | mode 分发 | 6 任务计时器表 / 7 NPC 标记表 / 8 任务 NPC 位置 / 0x0A-0x0E 通知；0x0F 过期触发 forfeit |
| 194 | SHOW_ITEM_EFFECT | int cid + int itemid | 本地/远处角色播放 `Effect.nx ItemEff.img/<itemid>` 动画 |

`MessagingHandlers.cpp` 的 `SHOW_STATUS_INFO(39)` 补 mode 1
（quest + status：0 放弃 / 1 数据字符串更新 / 2 filetime 完成）与
mode 2（道具过期提示）。

封包字段以 link_repos `MaplePacketCreator.java` 实测为准：211 mode 8
存在两种尾部不一致的变体，因此只读 quest + npc 两个前置字段，剩余
字节留给上游截断检查。

## 接线

- `UIElement::Type` 新增 `QUESTLOG` / `PROGRESSBANNER`；Q 键默认绑定
  `KeyAction::QUESTLOG` 已存在，`UIStateGame.cpp` 的 MENU 分支补上
  case，Q 键与状态栏入口共用同一条路径。
- `UIStatusBar` 的 `BT_QUEST` 按钮 + `StatusBarPopup::Action::QUESTS`
  （pre-Chaos 菜单行 "Quests" / post-Chaos `BtQuest` 图标）派发打开
  窗口；`dispatch_popup_action` 实现移入新的
  `IO/UITypes/StatusBar/StatusBarActions.cpp`，`UIStatusBar.cpp`
  799 → 754 行，解除 800 行红线。
- `UI.cpp` 的 escape_types 增加 `QUESTLOG`，ESC 可关闭窗口。

探针：`_msui_questlog_active` / `_msui_questlog_rows`（CMake 导出）。

## 验证

纯模型单测 `tools/verify/quest-log-model.cpp`（141 行，链接 nlnx +
`assets/Quest.nx`，命令见 `tools/verify/index.md`）：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function \
  -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes \
  tools/verify/quest-log-model.cpp \
  src/client/Character/QuestLog.cpp \
  src/client/IO/UITypes/Quest/QuestLogModel.cpp \
  src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp \
  src/nlnx/nx.cpp src/client/includes/lz4.c \
  -o /tmp/quest-log-model && /tmp/quest-log-model assets/Quest.nx
```

```
ALL CHECKS PASSED
```

覆盖 collect_rows 合并与过滤、QuestInfo 名称/分类解析与 `?id` 兜底、
quest_summary 摘要、forfeit/update_data/complete 状态迁移、revision
递增与空日志 `get_last_started` 归零。

浏览器 E2E `tools/verify/maplestory-webui/questlog-flow.mjs`（172 行）
全 PASS：

- PASS  登录地图 = 910000000（实测）
- PASS  Q 打开任务日志（msui_questlog_active=1）
- PASS  任务日志行数为非负整数（rows=1）
- PASS  再次按 Q 翻转关闭（TOGGLED，实测 active=0）
- PASS  第三次按 Q 重新打开任务日志
- PASS  Escape 走 escape 分支关闭（实测 active=0）

截图 questlog-open / questlog-closed。运行前置同 fm-gameplay-guides：
把测试角色放回自由市场，且 keymap 必须含 Q=16 → type 4 / action 8
（`keymap.key` 是客户端 Keytable 扫描码下标，Q=16 不是 GLFW 键码 81；
KeyType 枚举 MENU=4/ACTION=5，误写 ACTION 不会触发任何分支）：

```bash
QUESTLOG_ACCOUNT=test2 QUESTLOG_PASSWORD=test2 QUESTLOG_CARD=0 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/questlog-flow.mjs
```

## 相关文档

- [新版本地图数据源 gate 检查](new-era-maps-gate.md)（UI.nx 任务素材
  齐备性的 gate 结论由本条目消化）
- [场景机关/进度横幅/BGM 切换](field-obstacles-progress-banner.md)
  （同批接入的 `PROGRESSBANNER` 元素类型）
- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
