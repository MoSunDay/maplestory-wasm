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
// E. 入站呈现（QuestHandlers）——UPDATE_QUEST_INFO 各提示玩家可见。
// 为什么：mode 8 完成链与 0x0A/0x0B/0x0D/0x0E 失败码若只打 Console 日志，
// 玩家对"提交被拒"零感知（补丁 0018 的服务端反馈因此才可见）。
// ---------------------------------------------------------------------------
const questHandlers = readSource("src/client/Net/Handlers/QuestHandlers.cpp");

const finishCase = sliceBetween(questHandlers, "case 8:", "case 0x0A:");
assert.ok(finishCase.includes("const int16_t nextquest = recv.read_short();"),
  "case 8 must read the trailing nextquest short before rendering");
assert.ok(finishCase.includes("emplace<UIOk>"),
  "case 8 must show a player-visible UIOk");

const failureBlock = sliceBetween(questHandlers, "case 0x0A:", "case 0x0F:");
for (const label of ["case 0x0A:", "case 0x0B:", "case 0x0D:", "case 0x0E:"]) {
  assert.ok(failureBlock.includes(label),
    `failure block must keep ${label}`);
}
assert.equal(occurrences(failureBlock, "emplace<UIOk>"), 4,
  "each failure code (0x0A/0x0B/0x0D/0x0E) must raise a UIOk notice");

// ---------------------------------------------------------------------------
// F. 服务端补丁 0018——action 2/5 拒绝时必须回 questError。
// 为什么：客户端 0x0A 提示 UI 只有在服务端真的下发 questError 时才触发；
// 补丁文件是对 linked checkout 的唯一持久化载体，必须在档且两处齐全。
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

console.log("PASS quest uplink source contract (QUEST_ACTION packets, QuestCheck offer, arm/cancel wiring, race close, inbound notices, server patch 0018)");
