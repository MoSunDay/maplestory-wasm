# NPC 对话 toggle 失步：对话中途消失后无法再交流

## 背景

玩家反馈二转教官（武术教练）"又无法对话"：与 NPC 交流时对话框凭空消失，
之后按交互键/点击 NPC 均无响应，无法完成二转流程。二转教官对话由服务器
连续下发多页 `NPC_DIALOGUE(304)`，是触发该失步的高频场景。

## 根因（两处叠加）

1. `NpcDialogueHandler` 每收一页都 `emplace<UINpcTalk>()`；该元素是
   `UIStateGame` 构造时的常驻单例且 `TOGGLED=true`，`pre_add` 对已存在
   元素只做 `toggle_active()`（取反开关）。窗口已打开时再来一页会被翻成
   关闭，客户端 active 状态从此与服务器会话失步，后续开关交替。
2. 对话期间输入泄漏到游戏层：`UIStateGame::send_key` 把 ACTION 键直接
   转发 `Stage::send_key`（空格=KeyAction::NPCCHAT 会再发一次
   TalkToNPC）；`send_cursor` 未命中 UI 元素时落到 `Stage::send_cursor`
   点击 NPC 同样重发 TalkToNPC。服务器视为重新开始对话、重发一页
   NPC_DIALOGUE，触发 1 的翻转，还可能重置二转脚本的服务端状态。

## 变更

- `src/client/Net/Handlers/NpcInteractionHandlers.cpp`
  `NpcDialogueHandler::handle`：`change_text` 前显式 `makeactive()`，
  每页对话包确定性打开窗口，不再依赖 toggle 语义。
- `src/client/IO/UIStateGame.cpp` `send_key`：NPCTALK 激活期间
  ACTION/FACE/ITEM/SKILL 键不再转发 Stage（对话锁输入，符合原版行为；
  ESC 关闭路径在 `UI::send_key` 更早分支，不受影响）。
- `src/client/IO/UIStateGame.cpp` `send_cursor`：NPCTALK 激活期间点击
  对话窗外不落空到 `Stage::send_cursor`，避免点击 NPC 重复发起对话。

## 验证

- 单编 `IO/UIStateGame.cpp.o`、`Net/Handlers/NpcInteractionHandlers.cpp.o`
  通过；全量 wasm 已随 2026-10-30 构建链接并部署（ms-web-server 挂载
  仓库目录直出产物，md5 与 build/ 一致）。
- 新增浏览器 e2e `tools/verify/maplestory-webui/npc-dialog-reopen-flow.mjs`
  （射手村 100000000 对照 NPC 1012000）：首次 Space 弹对话 → 对话中再按
  Space 4s 采样内保持打开（无二次 TalkToNPC→toggle 关窗）→ Escape 关闭 →
  再次 Space 可重新弹出（「无法再对话」症状消失）→ 无残留、无浏览器错误。
  2026-10-30 实测 ALL PASS（exit=0）。
- 回归对照：`henesys-cindy-gone-flow.mjs` 正向对照（Space 弹对话）与负向
  断言（辛迪缺席）同日均 ALL PASS，无环境性回归。
- 排障记录：e2e 期间发现 test1Hero 的服务端 keymap 被
  skill-keymap-prep.py 整表覆写，SPACE(57)→ACTION/NPCCHAT 被抹掉导致
  Space 无动作（并非代码回归）；恢复 `keymap(57,5,54)` 后一切正常。
  e2e 脚本头部已固化该前置。
