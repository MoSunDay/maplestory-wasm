Commit: 62348ef7079246e1eedc88edecad4a310b337420

# 输入法候选阶段隔离拼音 preedit

## Context

文本框的 `input` 事件已在 composition 期间停止同步，但浏览器输入法更新拼音和光标时还会触发 `selectionchange`。该路径没有检查 composition 状态，会在候选词确认前将 `zhongwen` 之类的 preedit 通过 `msime_input` 写入游戏文本框。

## Change Summary

- 把浏览器 IME 桥从超过 800 行的 `web/index.html` 拆分到 `web/ime_input.js`，同步决策由可独立验证的纯函数提供。
- composition 期间同时隔离 `input`、`selectionchange` 和输入法物理键，preedit 只保留在浏览器 textarea 与原生候选窗中。
- 兼容浏览器在 `compositionend` 前后派发最终 `input` 的不同顺序：优先在非合成 `input` 上提交，并在下一任务保留兼容回退。
- 记录已同步文本和 UTF-16 光标快照，避免 composition 结束事件与 C++ 回显重复写入。

## Impact Surface

- WASM 浏览器文本框，包括登录账号、角色名和游戏聊天框。
- WebUI 自动验收的 composition 事件和真实聊天流程。
- 不改变 C++ `ImeBridge`、游戏封包、linked server 或 WASM 导出接口。

## Validation

- `node tools/verify/ime-input.cjs` 通过纯同步策略回归。
- `scripts/e2e_ime.mjs` 在 Chromium 中 13/13 通过：候选前的 `zhongwen` 没有调用 `msime_input`，确认后中文、限长、字形、退格、密码排除和失焦路径均通过。
- 使用现有 `amoshe` 角色通过真实 WebUI 进入游戏：拼音键未泄漏到窗口，选中「中文候选词验证」后聊天气泡和聊天记录正常显示，服务端收到 `GENERAL_CHAT` 并回发 `CHATTEXT`，浏览器异常为 0。

## Notes / Compatibility

- 密码字段仍不经 DOM IME 桥接。
- 不新增数据库字段、环境变量或构建制品。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [Web 基础设施](../../../agents/web/index.md)
- [中文/Unicode 输入与渲染](../../input-localization/index.md)
