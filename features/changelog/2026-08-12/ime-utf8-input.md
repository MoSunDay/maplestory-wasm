# WASM 客户端中文/Unicode 输入与渲染（CJK 回退字体 + UTF-8 排版 + 浏览器 IME 桥）

日期: 2026-08-12

## 范围

- **字体**: 内嵌 `DroidSansFallbackFull.ttf`（Apache-2.0，`src/client/fonts/DroidSansFallback/`，随 `--embed-file` 进 WASM）；新增 `Graphics/FontCache`：主 face（Arial）+ 共享回退 face 的按码点惰性光栅化，ASCII 32-127 仍启动预烘焙，缺字写占位字形（有 advance、不可见）
- **排版/渲染**: `GraphicsGL` 移除按字节固定表，改为码点感知的 `createlayout`/`drawtext`；`Layout::advances` 保持字节索引（延续字节复用引导字节 advance），Textfield 光标语义不变；字体区改为自图集底边向上分配，着色器条件翻转为 `texpos.y >= fontregion`，`fontregion` 随 `upload_glyph` 动态更新；排版/绘制实现拆于 `Graphics/GraphicsGLText.cpp`（满足文件行数限制）
- **工具**: 新增 `Util/Utf8.h`（decode/序列长/UTF-16↔字节偏移/CJK 判断）
- **编辑**: `Textfield` 左移/右移/退格/粘贴按码点处理；限长仍按字节（login/account/password 12 字节、角色名 12 字节、聊天按宽度），保证协议安全
- **IME 桥**: 新增 `IO/ImeBridge`（WASM 真实现 + 其他平台空实现）。焦点字段时经 `EM_ASM` 驱动 `web/index.html` 的隐藏 textarea `#ime-input`，浏览器原生 IME 候选窗随字段定位；JS 经导出函数 `msime_input`（整段文本 + UTF-16 光标）与 `msime_key`（Enter/Tab/Esc/Up/Down 白名单）回传；`Window::key_callback` 在桥激活时让位，`MapleWasmInputGuard` 跳过 `#ime-input` 事件；密码字段（crypt>0）不走桥接
- **web 胶水**: `web/index.html` 新增 `MapleWasmIME`（composition 处理、手动 Backspace——Emscripten GLFW port 会在 window capture 阶段 preventDefault、手动退格防 Enter 确认误提交（compositionend 后 100ms 内吞掉裸 Enter）、失焦回收）

## 动机

原版渲染器把 ASCII 32-127 预烘焙进按字节索引的固定表，任何非拉丁文字（中文、日文、韩文）既无法渲染也无法输入；浏览器 WASM 环境没有直接可用的 IME，必须借宿主 textarea。

## 有意不做（本轮）

- 游戏内 preedit 下划线渲染（候选窗由浏览器提供，提交在 compositionend 一次性完成）
- 密码字段的 IME（保持纯键盘路径，避免密文经 DOM 暴露）

## 测试覆盖表（当次实跑）

| 验证 | 结果 |
|---|---|
| 隔离目录全量构建（emcmake + emmake，Release，-Werror 生效） | PASS，零代码警告（仅 clang `-Wgcc-install-dir-libstdcxx` 工具链提示，与本仓库历次构建一致） |
| 产物取证 | `JourneyClient.wasm` 增大 ~4MB（内嵌字体）；二进制含 `msime_input`/`msime_key` 导出与 `DroidSansFallback` 路径；`JourneyClient.js` 含 `msime_input` |
| headless Chrome + CDP E2E（`scripts/e2e_ime.mjs`，web 栈 :8000/:8765/:8080 + Cosmic :8484 实运行） | 12/12 PASS ×5 连续轮次（backspace 项前置状态固定后；固定前该项间歇 flaky） |
| — 点击账号框 | IME 桥激活、隐藏 textarea 获得焦点 ✅ |
| — 模拟输入法提交「中文测试」 | C++ 接受并经 `sync_field` 回写 textarea ✅ |
| — 超限提交 8 个汉字（24 字节） | 按 12 字节限长截断 ✅ |
| — 字段区域像素对比 | 2686 像素变化（远超光标量级）= CJK 字形真实渲染 ✅ |
| — CDP rawKeyDown Backspace | 删除一个码点 ✅（测试前置固定焦点+caret 于文末，隔离胶水回显竞态；失败时打印桥接全状态取证） |
| — Tab 到密码框 | IME 桥不激活（crypt 路径保持） ✅ |
| — 点击他处 | 桥正确失活 ✅ |

## 回归风险

- 排版核心重写：ASCII 文本走相同预烘焙字形，观感不变；`advances` 字节索引语义保留，依赖 `belowlimit`/光标的既有调用不受影响
- 图集字体区改为自底向上分配并与位图区互查边界（`upload_glyph` 与位图 wrap 都检查 `fontregion`）；极端大量缺字可能耗尽字体区，此时打占位字形并一次性告警
- `web/index.html` 仅增量改动；`RegisterUrl` 相关改动属并行任务，未随本提交
- 已知遗留（非本提交引入）：`syncToGame`（selectionchange 触发）与 `onText` 回显在 Asyncify 挂起下存在 caret 竞态（13b70ae 胶水遗留），E2E 经前置固定规避；胶水侧串行化留待独立跟进
