import assert from "node:assert/strict";
import fs from "node:fs";

const readSource = (path) => fs.readFileSync(path, "utf8");

const sliceBetween = (source, start, end) => {
  const startAt = source.indexOf(start);
  assert.ok(startAt >= 0, `source must contain: ${start}`);
  const endAt = source.indexOf(end, startAt);
  assert.ok(endAt > startAt, `source must contain ${end} after: ${start}`);
  return source.slice(startAt, endAt);
};

const occurrences = (haystack, needle) => haystack.split(needle).length - 1;

// ---------------------------------------------------------------------------
// A. 出站封包（QuestPackets.h）——四个 QUEST_ACTION 上行封包的布局契约。
// 为什么：客户端与服务端 QuestActionHandler 的读包顺序必须逐字段对齐
// （[byte action][short qid][int npc][short x][short y]，isNpcNearby 靠
// available>=4 探测尾随坐标）；action 枚举 1/2/4/5 对应标准起/完与脚本
// 起/完，一旦错位服务端会把 qid 读成 npc，任务上行整体报废。
// ---------------------------------------------------------------------------
const packets = readSource("src/client/Net/Packets/QuestPackets.h");

const uplinkPackets = [
  { cls: "QuestStartPacket", next: "QuestCompletePacket(int16_t",
    mode: "START", value: "START = 1" },
  { cls: "QuestCompletePacket", next: "QuestScriptStartPacket(int16_t",
    mode: "COMPLETE", value: "COMPLETE = 2" },
  { cls: "QuestScriptStartPacket", next: "QuestScriptEndPacket(int16_t",
    mode: "SCRIPT_START", value: "SCRIPT_START = 4" },
  { cls: "QuestScriptEndPacket", next: "QuestForfeitPacket",
    mode: "SCRIPT_END", value: "SCRIPT_END = 5" },
];

for (const { cls, next, mode, value } of uplinkPackets) {
  const body = sliceBetween(packets, `${cls}(int16_t quest_id`, next);
  assert.ok(body.includes(`write_byte(${mode});`),
    `${cls} must write its ${mode} action byte`);
  assert.ok(body.includes(value),
    `${cls} enum must pin ${value} (server action id)`);
  assert.ok(body.includes(": OutPacket(QUEST_ACTION)"),
    `${cls} must target the QUEST_ACTION opcode`);
  const order = [
    `write_byte(${mode});`,
    "write_short(quest_id);",
    "write_int(npc_id);",
    "write_short(position.x());",
    "write_short(position.y());",
  ];
  let prev = -1;
  for (const field of order) {
    const at = body.indexOf(field);
    assert.ok(at > prev,
      `${cls} field order must be [action][qid][npc][x][y]: ${field}`);
    prev = at;
  }
}

// Action 2 选择奖励变体：4 参构造必须在 [x][y] 后追加 [short selection]，
// 服务端 QuestActionHandler 在尾随 >=2 字节时按 selection 索引 prop=-1
// 奖励表；缺这一分支，选择奖励任务只能靠 0020 的缺省 0 兜底，玩家选择
// 永远送不到服务端。
const completePacket = sliceBetween(
  packets, "class QuestCompletePacket", "QuestScriptStartPacket(int16_t");
assert.ok(completePacket.includes("int16_t selection)"),
  "QuestCompletePacket must expose a selection constructor overload");
const selectionCtor = sliceBetween(
  completePacket, "Point<int16_t> position, int16_t selection)",
  "private:");
assert.equal(occurrences(selectionCtor, "write_short(selection);"), 1,
  "selection overload must append exactly one trailing [short selection]");
assert.ok(occurrences(selectionCtor, "write_short(position.x());") === 1
    && occurrences(selectionCtor, "write_short(position.y());") === 1,
  "selection overload must keep the base [x][y] layout");

// ---------------------------------------------------------------------------
// B. 判定模块（QuestCheck）——quest_offer_at 用 bool 返回值 + 出参。
// 为什么：判定函数必须可重入：若用函数内 static 持有槽返回"上次命中"，
// 连续调用两次（第二个 NPC 无任务）会复用第一个 NPC 的旧结果，兜底对话框
// 弹错任务。值语义出参（QuestOffer& out）让每次调用独立成帧。
// ---------------------------------------------------------------------------
const questCheckHeader = readSource("src/client/Character/QuestCheck.h");
assert.ok(questCheckHeader.includes("bool quest_offer_at("),
  "QuestCheck.h must declare bool quest_offer_at (value-semantics out param)");
const questCheck = readSource("src/client/Character/QuestCheck.cpp");
assert.ok(!questCheck.includes("static QuestOffer result"),
  "QuestCheck.cpp must not keep a static QuestOffer result slot: repeated "
  + "calls would alias the previous NPC's offer");

// ---------------------------------------------------------------------------
// C. 触发接线——arm/cancel/update 三端各就各位。
// 为什么：arm_npc_talk 恰好 2 处（鼠标点击 + 键盘最近 NPC），cancel 恰好
// 2 处（NPC 对话 + 商店打开），多一处就会把服务端对话误判成超时兜底；
// update() 必须挂在 UIStateGame 帧循环，QUESTDIALOG 在 UIStateGame 输入
// 阻断列表与 UI.cpp ESC 列表里，缺一个都会漏按/漏关。
// ---------------------------------------------------------------------------
const mapNpcs = readSource("src/client/Gameplay/MapleMap/MapNpcs.cpp");
assert.equal(occurrences(mapNpcs, "quest_interaction::arm_npc_talk("), 2,
  "MapNpcs must arm the fallback exactly twice (cursor click + nearest key)");

const npcHandlers =
  readSource("src/client/Net/Handlers/NpcInteractionHandlers.cpp");
assert.equal(occurrences(npcHandlers, "quest_interaction::cancel_npc_talk()"), 2,
  "NpcDialogue/OpenNpcShop handlers must each cancel the armed fallback");

const uiStateGame = readSource("src/client/IO/UIStateGame.cpp");
assert.ok(uiStateGame.includes("quest_interaction::update();"),
  "UIStateGame::update must drive the 600ms fallback timer");
assert.ok(uiStateGame.includes("get(UIElement::QUESTDIALOG)"),
  "UIStateGame must block game-layer input while the quest dialog is up");

const ui = readSource("src/client/IO/UI.cpp");
assert.ok(ui.includes("UIElement::QUESTDIALOG,"),
  "UI.cpp ESC-close list must include QUESTDIALOG");

// ---------------------------------------------------------------------------
// D. 竞态收口（QuestInteraction）——cancel 时撤除已弹出的兜底框。
// 为什么：慢网络下 600ms 兜底先弹、服务端对话框后到，不撤除就会双框叠层；
// 撤除必须以 is_active() 守卫，避免误杀玩家刚用 YES 接受后（已非活跃）
// 的历史框。超时常量必须保持 600ms。
// ---------------------------------------------------------------------------
const questInteraction = readSource("src/client/Character/QuestInteraction.cpp");
assert.ok(
  questInteraction.includes(
    "constexpr auto NPC_TALK_TIMEOUT = std::chrono::milliseconds(600);"),
  "NPC_TALK_TIMEOUT must stay 600ms (server dialog vs fallback window)");

const cancelFn = sliceBetween(
  questInteraction,
  "void cancel_npc_talk()",
  "void update()");
assert.ok(cancelFn.includes("UI::get().remove(UIElement::QUESTDIALOG);"),
  "cancel_npc_talk must drop a raced-in fallback QUESTDIALOG");
const guardAt = cancelFn.indexOf("dialog->is_active()");
assert.ok(guardAt >= 0,
  "the removal must be guarded by is_active()");
assert.ok(
  cancelFn.indexOf("UI::get().remove(UIElement::QUESTDIALOG);") > guardAt,
  "remove(QUESTDIALOG) must sit inside the is_active() guard");

// ---------------------------------------------------------------------------
// E. 入站呈现（QuestHandlers）——mode 8 双变体与失败码提示。
// 为什么：接取任务时服务端 STARTED 分支发的 mode 8 变体
// （[short][int][int 0]，10B）若被当成 FINISH 变体（[short][int]
// [short nextquest]，8B），每次接取都会误弹「任务完成！」；反之
// 0x0A/0x0B/0x0C/0x0D/0x0E 失败码只打 Console 会让"提交被拒"零感知。
// ---------------------------------------------------------------------------
const questHandlers = readSource("src/client/Net/Handlers/QuestHandlers.cpp");

const finishCase = sliceBetween(questHandlers, "case 8:", "case 0x0A:");
assert.ok(finishCase.includes("if (recv.length() >= 4)"),
  "case 8 must branch on >=4 remaining bytes (STARTED int vs FINISH short)");
assert.ok(finishCase.includes("recv.read_int();"),
  "STARTED variant must consume the trailing int");
assert.ok(finishCase.includes("const int16_t nextquest = recv.read_short();"),
  "FINISH variant must read the trailing nextquest short");
assert.equal(occurrences(finishCase, "emplace<UIOk>"), 1,
  "only the FINISH variant may raise the completion UIOk");
const startedArm = sliceBetween(
  finishCase, "if (recv.length() >= 4)", "const int16_t nextquest");
assert.ok(!startedArm.includes("emplace<UIOk>"),
  "STARTED variant must never toast 完成 (quest accept regression)");

const failureBlock = sliceBetween(questHandlers, "case 0x0A:", "case 0x0F:");
for (const label of
  ["case 0x0A:", "case 0x0B:", "case 0x0C:", "case 0x0D:", "case 0x0E:"]) {
  assert.ok(failureBlock.includes(label),
    `failure block must keep ${label}`);
}
assert.equal(occurrences(failureBlock, "emplace<UIOk>"), 5,
  "each failure code (0x0A/0x0B/0x0C/0x0D/0x0E) must raise a UIOk notice");
assert.ok(failureBlock.includes("背包空间不足"),
  "mode 0x0C must explain the full-inventory failure in Chinese");

// 任务计时器：mode 6 写入 deadline、mode 7 移除；只打日志不算接通。
const timerBlock = sliceBetween(questHandlers, "case 6:", "case 8:");
assert.ok(timerBlock.includes("quests.add_timer("),
  "mode 6 must store quest deadlines via Questlog::add_timer");
assert.ok(timerBlock.includes("quests.remove_timer("),
  "mode 7 must clear quest deadlines via Questlog::remove_timer");

// 完成期特效 mode 9（showSpecialEffect(9)）必须显式吞掉：无 Quest.img
// 素材可渲染，但绝不能落进 buff 兜底反复刷截断日志。
const messagingHandlers =
  readSource("src/client/Net/Handlers/MessagingHandlers.cpp");
assert.ok(messagingHandlers.includes("if (mode1 == 9)"),
  "SHOW_ITEM_GAIN_INCHAT mode 9 must have an explicit gated branch");
const mode9 = sliceBetween(messagingHandlers, "if (mode1 == 9)", "mode_fallback_");
assert.ok(!mode9.includes("add_effect"),
  "mode 9 must not render before the quest effect asset exists");

// ---------------------------------------------------------------------------
// F. 服务端补丁——0018 action 2/5 拒绝反馈 + 0020 上行加固六项。
// 为什么：补丁文件是 linked checkout 的唯一持久化载体；0020 的每一项
// 都对应一个已验证故障（冗余 mode-8 / selection NPE / 接取静默拒绝 /
// 脚本缺失卡死 / 条件失败静默），缺一项上述客户端修复就无法闭环。
// ---------------------------------------------------------------------------
const patchPath =
  "scripts/server-patches/0018-quest-complete-reject-feedback.patch";
assert.ok(fs.existsSync(patchPath), `missing patch: ${patchPath}`);
const patch = readSource(patchPath);
const addedFeedback = patch
  .split("\n")
  .filter((line) => line.startsWith("+") && line.includes("questError(questid)"));
assert.equal(addedFeedback.length, 2,
  "patch 0018 must add questError feedback to both action 2 and action 5");

const hardeningPath =
  "scripts/server-patches/0020-quest-uplink-hardening.patch";
assert.ok(fs.existsSync(hardeningPath), `missing patch: ${hardeningPath}`);
const hardening = readSource(hardeningPath);
const addedLines = hardening.split("\n").filter((line) => line.startsWith("+"));
const removedLines = hardening.split("\n").filter((line) => line.startsWith("-"));

// ① STARTED 分支停发冗余 mode-8（10B 变体）。
assert.ok(removedLines.some((line) =>
    line.includes("updateQuestInfo((short) quest.getQuest().getId()")),
  "0020 must remove the redundant STARTED mode-8 announce");
assert.ok(hardening.includes("MapleCharacter.java"),
  "0020 must touch MapleCharacter.java (①)");

// ② ItemAction 选择奖励 null 缺省 + 越界钳制。
assert.ok(addedLines.some((line) =>
    line.includes("extSelection != null ? extSelection : 0")),
  "0020 must default a missing reward selection to 0");
assert.ok(addedLines.some((line) => line.includes("selectList.size() - 1")),
  "0020 must clamp the reward selection into [0, size-1]");

// ③ action 1/4 接取拒绝回 questError。
const addedQuestError =
  addedLines.filter((line) => line.includes("questError(questid)"));
assert.equal(addedQuestError.length, 2,
  "0020 must add questError feedback to actions 1 and 4");

// ④ 脚本缺失降级标准 start/complete。
assert.ok(addedLines.some((line) =>
    line.includes("quest.start(c.getPlayer(), npc)")),
  "0020 must fall back to quest.start when the script is missing");
assert.ok(addedLines.some((line) =>
    line.includes("quest.complete(c.getPlayer(), npc)")),
  "0020 must fall back to quest.complete when the script is missing");
assert.ok(hardening.includes("QuestScriptManager.java"),
  "0020 must touch QuestScriptManager.java (④)");

// ⑤ 条件失败封包接线（meso 0x0B / 缺物品 0x0E）。
assert.ok(hardening.includes("questFailure((byte) 0x0B)"),
  "0020 must wire the not-enough-meso failure (0x0B)");
assert.ok(hardening.includes("questFailure((byte) 0x0E)"),
  "0020 must wire the missing-item failure (0x0E)");

// ⑥ README 编号规则与实际最大编号对齐。
const patchReadme = readSource("scripts/server-patches/README.md");
assert.ok(patchReadme.includes("当前最大 0020"),
  "patch README must record 0020 as the current maximum");
assert.ok(patchReadme.includes("0020-quest-uplink-hardening.patch"),
  "patch README must list the 0020 patch row");

// ---------------------------------------------------------------------------
// G. 模态对话守卫（UIStateGame）——键盘与鼠标共用同一谓词。
// 为什么：NPCTALK 与 QUESTDIALOG 都是模态对话；只拦键盘会让点击窗外
// NPC 落穿重发 TalkToNPC，叠出第二个对话框。
// ---------------------------------------------------------------------------
const uiStateHeader = readSource("src/client/IO/UIStateGame.h");
assert.ok(uiStateHeader.includes("bool modal_dialog_open();"),
  "UIStateGame.h must declare the shared modal_dialog_open predicate");
const uiStateGameModal = readSource("src/client/IO/UIStateGame.cpp");
assert.ok(occurrences(uiStateGameModal, "modal_dialog_open()") >= 3,
  "modal_dialog_open must be defined and used by both input guards");
const modalFn = sliceBetween(
  uiStateGameModal, "bool UIStateGame::modal_dialog_open()", "}");
assert.ok(modalFn.includes("UIElement::NPCTALK")
    && modalFn.includes("UIElement::QUESTDIALOG"),
  "modal_dialog_open must treat NPCTALK and QUESTDIALOG as modal");

// ---------------------------------------------------------------------------
// H. 任务计时器与奖励选择（QuestLog / QuestDialogModel）。
// 为什么：计时器必须有 deadline 存储与倒计时读取；选择奖励必须按
// prop==-1 且保持服务端 selectList 的数值节点顺序，否则 selection
// 索引与奖励错位。
// ---------------------------------------------------------------------------
const questLogHeader = readSource("src/client/Character/QuestLog.h");
assert.ok(questLogHeader.includes("add_timer(")
    && questLogHeader.includes("remove_timer(")
    && questLogHeader.includes("timer_remaining_ms("),
  "Questlog must expose add/remove/remaining timer APIs");
assert.ok(questLogHeader.includes("forfeited_info_numbers"),
  "Questlog must guard forfeited info echoes with a set, not a single slot");

const dialogModelHeader =
  readSource("src/client/IO/UITypes/Quest/QuestDialogModel.h");
assert.ok(dialogModelHeader.includes("selectable_rewards(int16_t"),
  "QuestDialogModel must expose selectable_rewards for prop=-1 rewards");
const dialogModel =
  readSource("src/client/IO/UITypes/Quest/QuestDialogModel.cpp");
assert.ok(dialogModel.includes("prop") && dialogModel.includes("-1"),
  "selectable_rewards must filter prop == -1 items");

console.log("PASS quest uplink source contract (QUEST_ACTION packets + selection, QuestCheck offer, arm/cancel wiring, race close, mode-8 dual variant, 0x0A-0x0E notices, timers, modal guard, mode 9 gate, patches 0018/0020)");
