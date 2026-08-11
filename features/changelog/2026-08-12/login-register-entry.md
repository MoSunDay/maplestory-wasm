# 登录界面注册入口接线（RegisterUrl 配置 + 外部跳转）

日期: 2026-08-12

## 范围

- 登录界面 `BT_REGISTER`（`UI.nx → Login.img/Title/BtNew`，此前已渲染但点击无响应的死按钮）接线：
  - 新增配置项 `RegisterUrl`（`Configuration::StringEntry`，默认空串），注册于 `Configuration.cpp`
  - WASM 注入链路：`web/config.json → web/index.html lazyConfigKeys 白名单 → Module.LazyFS → Journey.cpp loadConfigString → Setting<RegisterUrl>`
  - `Util/Misc.h` 新增 `open_url()`：WASM 下 `EM_ASM(window.open(url, '_blank', 'noopener'))`（在点击回调内同步调用以规避弹窗拦截），桌面构建降级为控制台打印
  - `UILogin::button_pressed` 处理 `BT_REGISTER`：URL 非空 → 新标签页打开；URL 为空 → 弹 `UILoginNotice::PLEASE_SIGN_UP`（复用 `Login.img/Notice` 素材），不静默失败
- `web/config.json` 新增 `"RegisterUrl": null`（未设置 = 走 Notice 分支）

## 动机

v83/Cosmic 协议没有账号注册封包（见 `docs/ms-network-protocol.md` §7），游戏内表单注册在协议上不可行；`UI.nx` 中也无注册表单素材（已全库核对 Login.img 28 个子节点）。原版客户端 BtNew 的行为即跳转外部注册页，故注册入口只能导向可配置的外部 URL。

## 测试覆盖表（当次实跑）

| 验证 | 结果 |
|---|---|
| `./scripts/build_wasm.sh` 隔离 worktree 全量构建（HEAD + 本改动） | PASS，零代码警告（仅 clang `-Wgcc-install-dir-libstdcxx` 工具链提示，与代码无关） |
| 产物取证 | `strings JourneyClient.wasm` 含 `RegisterUrl`/`N3jrc11RegisterUrlE`；`JourneyClient.js` 含 `window.open(UTF8ToString($0),"_blank","noopener")` |
| 无头浏览器 E2E（headless Chrome + CDP，配置 `RegisterUrl=https://example.com/register`） | 点击注册按钮 → `[dbg] UILogin::button_pressed id=1` → `window.open` 捕获到 `https://example.com/register` ✅ |
| 空 URL 分支 E2E（`RegisterUrl: null`） | 点击 → 无 `window.open`，屏幕渲染 Notice 弹窗（5 万像素变化），客户端存活、无异常 ✅ |
| 登录流程回归 E2E（真实服务端 127.0.0.1:8484） | 输入账号/密码 → Enter → 服务端响应 `Login failed. reason=5`（未注册账号，预期）→ Notice 正常展示、无崩溃 ✅ |
| `cargo build --release -p web-server -p ws-proxy -p assets-server` | PASS（本次无 Rust 改动） |

## 回归风险

- 低：`button_pressed` 仅新增 `BT_REGISTER` 分支，BT_LOGIN/BT_QUIT/BT_SAVEID 路径未触碰（登录流程已 E2E 回归）。
- 低：`RegisterUrl` 为新增配置项，默认空串，不影响既有 Settings 文件解析。

## 部署须知

- 若服务端开启 `AUTOMATIC_REGISTER`（登录即建号），可将 `RegisterUrl` 保持为空（按钮显示 PLEASE_SIGN_UP 提示），或配置说明页 URL。
- Notice 文案为位图贴图（`Login.img/Notice/text/40`），选用 PLEASE_SIGN_UP 需按部署版本肉眼核对。

## 回滚

`git revert <commit>` 即可恢复死按钮状态。
