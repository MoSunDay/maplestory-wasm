# 任务系统加固：mode-8 双变体、失败反馈、选一奖励与计时器闭环

## 背景

上一轮[任务系统恢复](../2026-11-08/quest-system-restore.md)打通了客户端驱动的
接取/完成链路，但沿路留下六类收尾缺口：

- `UPDATE_QUEST_INFO` mode 8 同时承载「接取记录」与「完成提示」两种包体，
  客户端只按 FINISH 解析，接取时会误弹完成文案；
- mode 6/7 的任务计时器表与移除包只解析打印，任务日志无倒计时；
- `questError`/`questFailure` 的 0x0A/0x0B/0x0C/0x0D/0x0E 多数只打 Console，
  玩家看不到失败原因；
- `QuestCheck` 只校验 `lvmin`、部分前置与击杀/物品，缺 `lvmax`、职业、
  meso、精确前置状态与重复冷却；
- 完成对话框没有选一奖励 UI，`QuestCompletePacket` 也没有 selection 变体；
- 服务端在脚本缺失、接取失败、选择奖励缺省等路径上仍会静默失败或 NPE。

本批次把上述链路一次性收口，并补上可重复执行的探针与端到端验收。

## 根因

1. **mode 8 歧义**：服务端两处构造器共用 mode 8——接取侧是
   `[short quest][int npc][int 0]`（10 字节），完成侧是
   `[short quest][int npc][short nextquest]`（8 字节）；只按短整型读尾会
   把接取包剩余的 int 0 当成 `nextquest`，从而误弹「任务完成」。
2. **判定数据没接全**：Quest.nx 的 `Check.img/<qid>/0` 除 `lvmin` 外还有
   `lvmax`/`job`/`quest`/`interval`，完成侧还有 `meso`；旧实现只覆盖了
   其中一部分，导致本地兜底框与服务端判定不一致。
3. **状态容器信息不足**：`Questlog::started` 是 key 排序的 map，无法表达
   「哪个前置任务最后接取」；`forfeit` 只记一个 info 号，连续放弃时旧的
   info 回声会复活成任务行；计时器根本没有存储。
4. **输入门控重复**：NPC 对话框与本地任务对话框各自维护一份「是否阻断
   游戏层输入」的逻辑，任何新对话源都要改两处，容易漏。
5. **服务端静默路径**：`QuestActionHandler` action 1/4 在 `canStart` 失败时
   直接 return；`ItemAction` 的 `prop=-1` 选择奖励在 action 2 不带 selection
   时会自动拆箱 NPE；任务脚本缺失时 `QuestScriptManager` 仅打日志。

## 改动

### 1. 入站封包与玩家提示（Net/Handlers）

- `QuestHandlers.cpp`
  - mode 6：`count x (quest, remaining ms)` 写入 `Questlog::add_timer`，
    转成 steady-clock 绝对 deadline；mode 7 改为任务计时器移除
    （`remove_timer`），与 linked server 的 `removeQuestTimeLimit` 对齐。
  - mode 8：按剩余字节数区分两种变体——`length() >= 4` 为 STARTED
    （多一个 int 0，只记录接取 NPC，不弹提示），否则为 FINISH
    （读 `nextquest` 并弹「任务【…】完成！ 新任务【…】可以接取了。」）。
  - 0x0A 带 questid 的「暂时无法完成」；0x0B 金币不足；新增 0x0C 背包
    空间不足；0x0D 任务物品正穿在身上；0x0E 没有任务所需物品。全部走
    `UIOk`，与既有 UINotice 的模态语义一致。
- `MessagingHandlers.cpp`：`ShowItemGainInChat` mode 9（任务完成特效）
  显式门控——`Effect.nx` 尚无 `Quest.img`，只打一次日志，避免每帧刷
  missing-asset。
- `SetfieldHandlers.cpp`：每次 `parse_questlog` 前 `clear_forfeited()`，
  防止上一张地图的放弃守卫压制新地图的同 info 行。

### 2. 判定与任务状态（Character）

- `QuestCheck.{h,cpp}`
  - 新增完成侧 `meso` 判定（`context.meso >= Check/1/meso`）；
  - 新增 `lvmax`、职业表（`decoded_job_list`，数值子节点；GM 侧 id 2000
    本地放行、由服务端最终裁决）；
  - 前置任务改为精确状态匹配（0/1/2，缺省 0），「进行中」同时覆盖
    `started` 与 in-progress 配对；
  - 重复冷却 `interval_elapsed`：兼容 setfield 的 Windows FILETIME、
    本地 epoch 秒与毫秒三种完成时间，统一折算到 wall-clock ms 后比较
    `interval` 分钟；无时钟（`now_ms <= 0`）时放行，服务端保持权威；
  - `decoded_progress`：按服务端 `registerMobs` 的三位零填充计数器逐怪
    解码，返回 `mobid/killed/required`，格式不符时返回空表由调用方保留
    原始串。
- `QuestInteraction.{h,cpp}`：新增 `make_check_context()`（等级/职业/meso/
  墙钟/背包计数），兜底框与 NPC 标记共用同一判定上下文；`arm_npc_talk`
  记录地图 id，`update` 发现换图即丢弃过期 arm，避免在新地图误开旧 NPC
  的任务框。
- `QuestLog.{h,cpp}`
  - `started_order/started_seq` 记录接取顺序，info 行与「最近接取」的
    前置配对，不再依赖 id 排序；
  - `forfeited_info_numbers` 改为 set，每次放弃占一个槽位，连续放弃的
    info 回声不会再复活任务行；
  - `add_timer/remove_timer/timer_remaining_ms` 以 steady clock 存绝对
    deadline；放弃与完成都会清掉计时器。
- `UIQuestLog.{h,cpp}`：双页签在旧版 `UI.nx` 无页签美术时用程序化标签 +
  透明 `AreaButton` 兜底；进行中行显示 `剩余 M:SS` 倒计时；击杀进度经
  `decoded_progress` 转成「击杀 <String.nx Mob.img 名称> x/y」；重开窗口
  时 `toggle_active` 立即折叠 revision 并重建行，避免关闭期间漏更新。
- `Npc.{h,cpp}` / `MapNpcs.cpp`：解析 `conditionN` 的任务状态门（1/2/3）
  与替代 stance/`hide`，按 condition 数字序取首个命中；新增纯函数
  `npc_has_quest_marker` 作为头顶标记的判定缝（素材缺失，暂不绘制）。

### 3. 对话框、选择奖励与输入门控（IO）

- `UIStateGame.{h,cpp}`：新增 `modal_dialog_open()`，统一把 NPCTALK 与
  QUESTDIALOG 视为模态；游戏层按键与鼠标落点都经它短路，新增对话源不再
  需要改两处。
- `QuestPackets.h`：`QuestCompletePacket` 增加 4 参重载，在 `[x][y]` 后
  追加 `[short selection]`；无 selection 的旧重载保持不变。
- `QuestDialogModel.{h,cpp}`：`selectable_rewards(qid)` 读取
  `Act.img/<qid>/1/item`（兼容 `yes/item` 包装），只保留 `prop == -1`
  的条目，按数值节点名排序，保持服务端 `selectList` 的索引顺序；id/count
  原样保留（支持 10272 这类负数「收取物品」选项）。
- `UIQuestDialog.{h,cpp}`：完成页末页渲染选择奖励行——提示语 + 每行 4 格
  64x40 图标格，选中高亮；点击命中 `reward_at`，YES 提交时走带 selection
  的封包；布局高度把奖励块计入填充高度计算。
- `UINotice.h` / `ImeBridge.cpp`：UIOk 增加 `probe_is_ok`，UINotice 暴露
  `probe_question`；ImeBridge 新增 6 个 e2e 导出
  （`msui_uiok_active/_message_hash`、`msui_questdialog_active/_button_field/
  _element_field/_reward_field`），只在 `MS_PLATFORM_WASM` 下编译。

### 4. 服务端补丁与验收工具

- `scripts/server-patches/0020-quest-uplink-hardening.patch` 五项加固：
  1. `MapleCharacter.updateQuest` STARTED 分支不再补发 mode 8 STARTED
     announce（客户端已区分两种变体，任务日志由 SHOW_STATUS_INFO 更新）；
  2. `ItemAction` 的 `prop=-1` 选择奖励在 action 2 不带 selection 时
     `null` 缺省 0，`check()` 把索引夹进 `[0, size-1]`，消除拆箱 NPE；
  3. `QuestActionHandler` action 1/4 在 `canStart` 失败时补
     `questError`（0x0A），与补丁 0018 的 action 2/5 对齐；
  4. `QuestScriptManager.start/end` 在脚本文件缺失（含 medal 兜底）时
     降级走标准 `quest.start/complete`，任务不再卡死；
  5. `questFailure`：接取侧 meso 不足回 0x0B，`ItemRequirement` 类型
     UNDEFINED/数量不足回 0x0E。
- `tools/verify/quest-check.cpp`：新增 lvmax/职业/精确前置/interval 冷却
  用例（1201、20010、4014、1000→1001、3205），连同既有 30 项断言全过。
- `tools/verify/quest-dialog-model.cpp`：Say 页、QuestInfo 回退、中文兜底
  与 `selectable_rewards`（2047 两件、2094 十六件、10272 负数、空表）全过。
- `tools/verify/quest-log-model.cpp`：新增插入序配对、多槽放弃守卫、
  deadline/移除/放弃清理用例。
- `tools/verify/quest-uplink-source-contract.mjs`：新增 selection 构造、
  mode-8 双变体、0x0A–0x0E、计时器 API、`modal_dialog_open`、mode 9 门控
  与补丁 0018/0020 契约断言。
- `tools/verify/maplestory-webui/quest-accept-complete-flow.mjs`：正式 e2e
  （见下）。

## 验证

- **e2e（连续 3 次 ALL PASS）**：`test2`/card0（character 281，map
  102000000，NPC 1022002）——
  - 接取 2047：Space 打开，`completing=0`/`selected_index=-1`；NEXT 到
    末页 YES 后对话框关闭且**不弹 UIOk**（证明 mode 8 STARTED 变体未被
    误读成完成）；
  - 完成 2047：交付对话框 `completing=1`、2 个 64x40 奖励格；点第二格
    `selected_index=1`，YES 后 UIOk 文案哈希与
    「任务【Hero's Gladius】完成！ 新任务【Rewakening the Gladius】可以接取了。」
    的 FNV-1a 一致；
  - 拒绝 4934：本地 `acceptable` 不校验 start 侧道具，服务端先 0x0E
    再 0x0A，最终 UIOk 文案哈希等于「任务【Manji's Approval】暂时无法完成，
    请确认任务条件。」且不等于完成文案；4934 无 queststatus 行；
  - DB：2047 `status=2`；4031144 被扣；1032012 入 EQUIP 栏（invtype=1，
    qty=1）；未选中的 1302015 未发放；跑前快照、跑后复原 4934/4928 行。
- `quest-check`、`quest-log-model`、`quest-dialog-model` 编译零警告并
  `ALL CHECKS PASSED`。
- `node tools/verify/quest-uplink-source-contract.mjs` PASS。
- WASM 全量构建与 boot/运行时回归见下方「回归记录」。

## 回归记录（2026-11-14）

- `bash scripts/docker_build_wasm.sh`：全量构建通过（`[100%] Built target
  JourneyClient`，无新增警告）。
- 运行时冒烟：`tools/verify/maplestory-webui/questlog-flow.mjs` PASS
  （任务日志开合、页签、放弃链路未被计时器/双页签改动破坏）。
- 任务 e2e `quest-accept-complete-flow.mjs` 在最终构建上再次 PASS。
- Boot 冒烟：`tools/verify/maplestory-webui/run.mjs`（`E2E_STOP_AT_GAME=1`，
  自由市场 910000000）PASS——登录/选角/进图、资源门限释放、站立动画与
  自然资源流式加载全部通过，浏览器异常 0。
- 冷启动观察：在 102000000 上直接跑同一 boot 冒烟时，Map/Npc/UI 的
  2MB 分块流式加载超过该脚本固定 60s 资源门限；任务 e2e 本身在该地图
  通过，属冷缓存加载预算而非功能回归（必要时给大地图单独放宽门限）。
- DB 已复原：281 map=102000000/spawn=7；queststatus 2047=2、4928=2、
  4934=2；`accounts.test2.loggedin=0`；1032012 仍为 EQUIP x1。

## 已知边界

- **NPC 头顶任务标记/完成特效**：判定缝 `npc_has_quest_marker` 已就绪，
  但 `Effect.nx` 没有 `Quest.img`，mode 9 完成特效与标记均未绘制；需素材
  注入后接绘制层。
- **接取侧物品/meso 不做本地预检**：0x0E/0x0B/0x0A 由服务端权威返回；
  本地提前拦截会改变 4934 这类「能开框、提交被拒」的既定语义，暂不做。
- **脚本任务内容**：0020 只保证脚本缺失时任务可完成；脚本内的自定义演出/
  对话仍依赖服务端 `scripts/quest/*.js`。
- **奖励展示范围**：UI 只展示 `prop == -1` 的选一奖励；随机奖励与固定
  奖励仍由服务端 roll，客户端不预览。
- **GM 豁免**：服务端 `chr.isGM()` 客户端不可见；职业表列 2000 时本地
  放行，最终仍由服务端拒绝非 GM 请求。
- **旧版 UI.nx**：任务日志页签是程序化 fallback；换用带页签美术的
  UI.nx 后可移除 `AreaButton` 命中区。
- **计时器休眠**：deadline 基于 steady clock，客户端挂起恢复后按剩余时间
  续显；重进地图由服务端重发 mode 6 重新同步。

## 相关文档

- [任务系统接取/完成闭环（2026-11-08）](../2026-11-08/quest-system-restore.md)
- [任务日志窗口（2026-10-25）](../2026-10-25/quest-log-window.md)
- [验证工具索引](../../../tools/verify/index.md)
- [服务端补丁说明](../../../scripts/server-patches/README.md)
