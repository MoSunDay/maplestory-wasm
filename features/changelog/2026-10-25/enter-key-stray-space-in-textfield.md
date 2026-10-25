# 回车打开输入框不再带入多余空格

## 背景

用户反馈「输入回车后，输入框内有空格，预期应该不要追加这个空格」。
复现路径：聊天框未聚焦时按 Enter，游戏打开聊天输入框，框内出现一个
看似空格的占位字符。

## 根因

WASM 端文本输入由隐藏的 `<textarea id="ime-input">` 镜像（IME 桥）。
按 Enter 时事件链为：

1. Emscripten GLFW 在 window 捕获阶段派发 keydown，C++ 同步执行
   `UIStateGame` 的 RETURN 分支 → `focus_chatfield()` →
   `ImeBridge::focus_field` → `textarea.focus()`——此时 keydown 仍在
   派发中；
2. Emscripten 的 `GLFW.onKeydown` 只对 Backspace/Tab 调
   `preventDefault()`，Enter 的浏览器默认行为未被取消；
3. 派发结束后浏览器对该 keydown 执行默认动作——向**此刻已聚焦**的
   textarea 插入换行符 `\n`，触发 `input` → `msime_input("\n")`；
4. 游戏字段是单行文本，字体对 `\n` 无字形，按「不可见占位符 + 前进
   宽度」渲染，视觉上就是一个空格。

textarea 自身的 keydown 处理（`FORWARD_KEYS`）只覆盖事件目标已是
textarea 的场景，无法拦截这种"聚焦发生在 keydown 派发中途"的路径。

## 修复

`web/ime_input.js`：textarea 增加 `beforeinput` 监听，取消
`insertLineBreak` / `insertParagraph` 两类换行插入。游戏字段恒为单行，
镜像值中永远不应出现换行，该拦截与聚焦来源无关，可覆盖所有把换行
塞进 textarea 的路径。普通文本插入、拼音合成不受影响。

## 验证

- 新增 `web/tests/ime_input.test.mjs`（`node --test`）：换行插入被
  取消、普通文本不受影响、聚焦后无多余同步；
- `node --test web/tests/*.test.mjs` 全部 24 项通过。
