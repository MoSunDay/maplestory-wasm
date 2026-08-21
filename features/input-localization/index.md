Commit: 62348ef7079246e1eedc88edecad4a310b337420

# 中文/Unicode 输入与渲染

## 能力

- 账号、角色名、普通文本框和聊天框可输入、编辑和渲染中文等 Unicode 文本。
- CJK 回退字体与 UTF-8 按码点排版保证非 ASCII 文本可见；光标移动和退格按完整码点处理。
- WASM 文本框使用浏览器原生输入法候选窗，候选窗位置跟随游戏内字段。

## 合成输入规则

1. `compositionstart` 后的拼音 preedit、候选键和光标变化只存在于浏览器 textarea 和原生候选窗。
2. preedit 期间不更新游戏文本框，也不把物理拼音键交给 GLFW。
3. 选中候选词后，最终非合成 `input` 一次性替换游戏字段全文并同步 UTF-16 光标。
4. 浏览器若不派发最终 `input`，桥接在 `compositionend` 的下一任务同步已提交的 DOM 值，不在事件当下冒险同步旧拼音。

## 约束与边界

- 密码字段不启用 DOM IME 桥接，避免密文经页面 DOM 暴露。
- 文本框限长仍按 UTF-8 字节执行，以保持 v83 封包宽度与已有服务端规则。
- 角色名最终合法性和唯一性由服务端校验；拒绝或超时后保留已输入文本并恢复焦点。
- 游戏内不自行绘制 preedit 下划线，合成过程由浏览器候选窗表达。

## 相关逻辑

- [UI 系统](../../agents/client/IO/index.md)
- [图形渲染](../../agents/client/Graphics/index.md)
- [Web 基础设施](../../agents/web/index.md)
