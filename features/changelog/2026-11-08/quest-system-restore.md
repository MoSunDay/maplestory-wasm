# 任务系统彻底恢复：客户端驱动的接取/完成闭环

## 背景

任务系统此前是单向链路：下行（SET_FIELD 初始化、SHOW_STATUS_INFO 起/更/完/弃、
UPDATE_QUEST_INFO 过期、QUEST_CLEAR、任务日志窗口）完整，但上行只有"放弃"
（QUEST_ACTION mode 3）。后果：

- QUEST_ACTION 1/2（标准接取/完成）从未实现，客户端永远无法主动接取/提交任务；
- Check.img 里带 `startscript`/`endscript` 的脚本任务（action 4/5）不可达，
  服务端 `scripts/quest/*.js`（q20000s、q21000s 等）全部是死路径；
- UPDATE_QUEST_INFO 的完成链（mode 8）、失败提示（0x0A/0x0B/0x0D/0x0E）
  只打 Console 日志，玩家不可见；
- Quest.nx 的 Say.img（任务对话文本）因韩文重打包掩码全部损坏（'?????' 串）。

## 恢复内容

### 1. 上行封包（QuestPackets.h）

新增 QuestStartPacket(1) / QuestCompletePacket(2) / QuestScriptStartPacket(4) /
QuestScriptEndPacket(5)，布局按服务端 `QuestActionHandler.java` 契约：
`[action][qid][int npc][short x][short y]`（isNpcNearby 的 available>=4 探测顺序；
action 2 不带 selection，走服务端无选择分支）。

### 2. 任务可用性判定（Character/QuestCheck.{h,cpp}，纯函数）

- Check.img 反向索引（npc → 起始/交付 qid，惰性构建一次）；
- 交付判定：进度串按服务端 `MapleQuestStatus.registerMobs/getQuestData` 的
  3 位零填充编码逐怪解码，对照 Check/1/mob 的 id/count；Check/1/item 用背包计数核对；
- 接取判定：未接取（或已完成且 Check/0/interval 可重复）、非 autoStart、
  `lvmin`（wz 实际节点名，1973 处，非 lv）、任务前置（quest state 1/2）达标；
- 交付优先于接取，同类取最小 qid。
- `tools/verify/quest-check.cpp`：30 项断言全过（含 1000/1001/1016/10002/21000
  真实数据用例）。

### 3. 任务对话框（IO/UITypes/Quest/UIQuestDialog + QuestDialogModel）

- 镜像 UINpcTalk 的 UtilDlgEx 框架与按钮（NEXT/PREV 翻页，末页 YES/NO 接受/拒绝，
  拒绝且有 Say/<qid>/<stage>/no 文本时展示回应页）；
- 文本来自 Say.img/<qid>/0|1 数字页，空则回退 QuestInfo 摘要，再回退中文兜底；
  #b/#p/#t 等 marker 走既有 npctalk_text 管线；
- 接受时按 scripted × completing 四选一分发上述封包（附玩家坐标供服务端
  isNpcNearby 校验）。

### 4. 触发接线（Character/QuestInteraction.{h,cpp} + 接线点）

设计原则：**服务端 NPC 脚本/商店永远优先，任务对话框只做无脚本地板的兜底**——
与既有脚本流（cm.startQuest 强制起任务，如二转教官）零冲突：

- 点击 NPC 发 TalkToNPCPacket 时 `arm_npc_talk(npcid)`；
- 服务端真的回对话（NpcDialogueHandler）或开商店（OpenNpcShopHandler）即取消；
- 600ms 兜底超时（steady_clock，UIStateGame::update 驱动）且无对话框时，
  客户端 QuestCheck 判定该 NPC 有可接/可交互任务 → 弹 UIQuestDialog。
- 对话期间阻断游戏层输入（UIStateGame/ESC 处理与 NPCTALK 同列）。

### 5. 入站呈现补齐（QuestHandlers.cpp）

- mode 8（NextQuestAction 的 updateQuestFinish）：UIOk「任务【名】完成！
  （新任务【名】可以接取了。）」，含尾接 nextquest；
- 0x0A 任务条件不满足、0x0B 金币不足、0x0D 物品正被装备、0x0E 缺道具：
  均改为玩家可见中文提示（语义对照 MaplePacketCreator.questFailure）。

### 6. 数据修复：Say.img 文本回填（scripts/nx/patch_quest_info_text.py 扩展）

沿用 QuestInfo 回填的"重指节点字符串索引"机制：Say.img 全部 string 节点按
`wz/Quest.wz/Say.img.xml`（英文，marker 保留）对齐回填——15,103 节点重指、
1,275 已匹配、1,746 纯拉丁保留、6 处 nx 缺失路径跳过（quest 3927/1 子树）。
幂等可重跑，备份 `.pre-say-l10n-backup`；QuestInfo 既有断言不变。
`assets/Quest.nx` 与 `desktop/resources/msweb/assets/Quest.nx` 双文件已更新。

## 收尾修复（2026-11-09）

- **竞态收口**：`quest_interaction::cancel_npc_talk`（服务端 NPC 对话/商店
  真正打开时调用）现在会撤除已弹出的兜底 UIQuestDialog——慢网络下 600ms
  兜底先弹、服务端对话框后到时不再双框叠层；撤除以 `is_active()` 守卫，
  不影响 YES 接受流程中已非活跃的历史框；
- **quest_offer_at 改 bool + 出参**：消除函数内 `static QuestOffer result`
  持有槽——判定函数改为值语义（命中写 `QuestOffer& out` 并返回 true），
  连续调用（上一个 NPC 有任务、下一个没有）不再复用同一持有槽弹错任务；
- **服务端补丁 0018**：
  `scripts/server-patches/0018-quest-complete-reject-feedback.patch`——
  action 2（标准完成）/action 5（脚本完成）在 `quest.canComplete` 为 false
  时服务端回 UPDATE_QUEST_INFO 0x0A，配合客户端既有 UIOk 提示，玩家可见
  「任务【名】暂时无法完成，请确认任务条件。」（已重编 dist 并重启
  ms-game-server 容器）；
- **quest-uplink-source-contract.mjs 契约验证器**：上行封包布局/枚举、判定
  模块可重入（无静态槽）、arm/cancel/update 接线、竞态收口、入站提示、
  补丁 0018 六节源码契约断言（`tools/verify/quest-uplink-source-contract.mjs`）。

## 验证

- WASM 全量构建通过（docker_build_wasm.sh，-O3 Release）；
- `tools/verify/quest-check` 30 项断言、`tools/verify/quest-log-model` 回归全过；
- QuestDialogModel 对真实数据冒烟：1000 起始 3 页+拒绝回应页、21000 交付单页、
  20000（空 Say）正确回退 QuestInfo 摘要；
- `node tools/verify/quest-uplink-source-contract.mjs` 任务上行闭环源码契约全过。

## 已知边界（后续可做）

- NPC 头顶任务气泡（!/问号/书）与任务完成特效：现有 Npc.nx/Effect.nx 无对应
  素材（无 QuestBalloon、Effect 无 Quest.img），需素材注入后再接入；
- UPDATE_QUEST_INFO mode 6/7（任务计时器表/NPC 标记）仍仅解析打印；
- action 2 的奖励选择（selection）未做 UI——绝大多数任务无选择项，
  服务端无 selection 走默认奖励分支；
- 拒绝回应页按钮复用 YES 槽位（视觉与接受相同，注释已说明）。

以下两条边界已在收尾修复中处理（见上文"收尾修复（2026-11-09）"）：

- 慢网络下兜底任务框与服务端 NPC 对话/商店的双框竞态：**已收口**——
  `cancel_npc_talk` 撤除仍活跃的兜底 UIQuestDialog（is_active 守卫）；
- action 2/5 完成条件不满足时服务端静默拒绝、客户端无反馈：**已修复**——
  服务端补丁 0018 回 UPDATE_QUEST_INFO 0x0A，客户端弹 UIOk 提示。
