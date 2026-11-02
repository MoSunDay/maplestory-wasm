# 任务日志（Q 键）彻底可用 + 登录期 null function 崩溃修复

## 背景

任务日志窗口（Q 键）此前完全不可用：按键无响应或窗口空白、点击无高亮、
滚轮即关窗、信息面板无正文。修复过程中还连带揭出两个环境级问题：
登录阶段必现 `RuntimeError: null function`（所有 2026-10-30 之后的构建）
与造数导致的 `SET_FIELD` 解析漂移，一并收口。

## 客户端修复（f1db03b）

- `IO/UIQuestLog`：Q 键/状态栏菜单打开（TOGGLED，ESC 关闭），双 Tab
  （In Progress/Completed）+ 9 行列表 + Slider，右侧信息面板；
- `Quest/QuestLogModel`：纯函数行集模型，随 `Questlog` revision 刷新；
- 点击行高亮选中、滚轮翻页不再误关窗、正文按 NX `Q` 节点渲染；
- 探针 `msui_questlog_{active,rows,selected}` 与单测
  `tools/verify/questlog-nx.cpp`（双 NX 全绿）、E2E
  `tools/verify/maplestory-webui/questlog-flow.mjs`（8 断言全绿：
  开窗/行数/点击选中/滚轮/Q 翻转/Escape）。
- NX 文本补丁 `scripts/nx/patch_quest_info_text.py`：为任务摘要补
  中文本地化文本（Quest.nx md5 `a57687a812111fdeae73e89bf7410d23`，
  备份 `assets/Quest.nx.pre-quest-l10n-backup`）。

## null function 崩溃（8b5cde5 引入，本次修复）

**症状**：10-30 之后任意提交全量重建后，登录界面必现
`RuntimeError: null function`，随后 wasm OOB 风暴；`build/backup-*`
（10-30 21:05）正常。`039520c/cc80da1/b6dcaa4` 为不可构建中间提交，
无法直接二分。

**根因**：LazyFS 同步快路径与 Asyncify rewind 重放互斥缺失。首读
`(Base.nx, off, len)` cache miss → `handleSleep` 挂起等 chunk → chunk
到达 `wakeUp` → rewind **重放整个函数**，EM_ASM 二次执行时
`readCachedSync` 已命中 → 快路径在到达记录的恢复点**之前提前 return**
→ rewind 状态机损坏 → 此后任意 `dynCall`（keepalive 的
`dynCall_vi`、WebSocket `dynCall_iiii`，见 `Net/SockInternal.cpp`）
查空表崩溃。插桩实证同一读执行两次，第二次命中即崩。

**修复**（`LazyFS/LazyFileBackend.cpp` EM_ASM 快路径）：
`Asyncify.state` 非 Normal 时禁用快路径，rewind 重放落回
`handleSleep` 按记录值恢复。`LazyFS/lazyfs.js` 的 readFileData 早前
已有同款守卫，本次对齐。

## assets-server 陈旧缓存陷阱

`assets/Quest.nx` 更新后容器 `maplestory-wasm-assets-server-1` 若未
重启，仍按旧 md5/内容应答，症状为"补丁明明存在但客户端读不到新文本"。
改 NX 后必须重启 assets-server 并核对 md5 与 size（本例
`6120296`）。

## 造数 wire-compat 约束（SET_FIELD 漂移）

带 `infoNumber` 的任务（`Check.img`：20000→20022、20010→20022、
29005→27010、29007→27012）服务端 `addQuestInfo` 会内联追加
`short infoNumber + mapleAsciiString(data)`，客户端
`SetfieldHandlers.cpp parse_questlog` 以 `is_started(qid)+i--` 配对，
要求父任务先出现。**人工种入带 infoNumber 的父任务而不种子任务**会
多出未消费字节 → completed 计数读到任务号 → 垃圾解析 →
`PacketError: Unsupported non-empty minigame info`
（CharacterDataParser.cpp:23）→ SET_FIELD 中断 → UI 永卡 state 3
（"game UI did not activate"）。

**种子规范**：只种无 `infoNumber` 的任务（本例 `20001-20008`，
`queststatus` 8 条 status=1）；原生 29005/29007 与 completed 任务不受
影响。详见
[roadmap「服务端种子与反馈事项」陷阱六](../../../docs/roadmap.md)
（原 known-issues 2026-10-31 SQL seeding 陷阱文档已删除并入）。

## 排障备忘

- worktree 构建必须 `COMPOSE_PROJECT_NAME=maplestory-wasm` 复用热
  emscripten cache；切 checkout 后必须清 worktree build 的 CMake
  状态，否则陈旧 CMakeCache 会让链接用错 EXPORTED_FUNCTIONS；
- emsdk cache 锁残留：alpine 容器 `rm /cache/cache.lock`；
- 服务端仅 .class，`javap -p -c` 反汇编 `MaplePacketCreator` 核对
  编码；进行中任务表为 `queststatus`（非 questprogress）。
