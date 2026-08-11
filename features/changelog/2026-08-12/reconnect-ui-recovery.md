# 登录→频道重连路径的 UI 卡死恢复与封包流保护

日期: 2026-08-12

## 范围

修复「选择角色进入游戏」时，登录服务器经 `SERVER_IP` 封包切换到频道服务端这条重连路径上的多个卡死/数据错乱缺陷。全部改动在客户端 Net 模块内，未触碰服务端。

- **B1 — 重连失败时恢复 UI（`Handlers/LoginHandlers.cpp` `ServerIPHandler`）**：原代码无条件调用 `Session::get().reconnect()` 后立即 `PlayerLoginPacket(cid).dispatch()`。若频道重连未成功，`dispatch()` 会把封包写进一个未建立的连接，角色选择界面仍处于禁用态 → 永久卡死。现改为重连后检查 `Session::get().is_connected()`：成功才发登录封包；失败则 `Console::get().print` 诊断并 `UI::get().enable()` 解锁界面。
- **B2 — 握手接收超时（`SocketWinsock.cpp` `open()`）**：WASM 握手阶段 `ws_recv(sock, buffer, 32, timeout)` 原 `timeout=(size_t)-1`，在 `ws_recv` 的 5ms 轮询循环里等价于无限等待。当频道服务端不可达/已关闭时，握手字节永不到达 → `open()` 永久挂起 → 整个客户端冻结。改为 `15000`（15 秒）：超时后 `ws_recv` 返回 0，`open()` 返回 false，`init()` 置 `connected=false`，配合 B1 恢复 UI。注意：正常读循环 `ws_recv(..., 0)`（`timeout=0` 即非阻塞轮询）不受影响。
- **B3 — `set_field` 恢复路径（`Handlers/SetfieldHandlers.cpp` `set_field`）**：进入游戏前若角色选择界面已消失（重连竞态）或本地无该 `cid`，原代码直接 `return`，界面停留在禁用态 → 卡死。现两处早退分支均 `Console::get().print` 诊断并 `UI::get().enable()` 恢复操作。
- **B4 — 重连后丢弃旧连接残留字节（`Session.{h,cpp}`）**：`reconnect()` 会替换 socket/加密上下文。若 `reconnect()` 恰好在 `forward()`（由 `process()` 调用）处理同一批读入字节期间触发，`process()` 后续仍会用新加密上下文继续解密属于旧连接的尾部字节 → 封包错位/解密错误。新增 `bool connection_changed` 成员：`reconnect()` 末尾置位，`process()` 在解出一个封包后检查该标志，若已置位则丢弃当前缓冲区剩余字节、复位标志并返回，避免跨连接污染。正常多封包处理（无重连）标志恒为 false，行为不变。

诊断字符串（标准 `Console::get().print` 日志，非调试残留）：
`"Reconnecting to channel server <addr>:<port>"`、`"Reconnect to channel server failed (<addr>:<port>)"`、`"set_field: character-select UI not found"`、`"set_field: cid mismatch, cannot enter game"`。

## 动机

`SERVER_IP`→`reconnect()` 是登录到游戏世界的必经切换点（登录服与频道服是不同 TCP 连接）。这条路径此前没有任何失败处理：频道服务端不可达、重连与 `set_field` 发生竞态、或 `set_field` 引用本地不存在的 cid 时，都会让角色选择界面卡在禁用态（`disable()` 已禁用输入但无人重新 `enable()`），玩家只能刷新页面。更隐蔽的是，若服务端在 `SERVER_IP` 之后还塞入了属于旧登录连接的尾部字节，`process()` 会用频道连接的新加密上下文错误解密，引发连串封包解析异常。本次为该路径补齐超时、失败恢复与跨连接隔离。

## 测试覆盖表（当次实跑）

| 验证 | 结果 |
|---|---|
| `./scripts/build_wasm.sh --jobs 4` + 对 5 个改动翻译单元强制重编（source emsdk，`-Werror` 生效） | PASS，`[100%] Built target JourneyClient`，5 个改动单元（LoginHandlers/SetfieldHandlers/Session/SocketWinsock 等）重编+重链，零代码警告（仅 clang `-Wgcc-install-dir-libstdcxx` 工具链提示，与本仓库历次构建一致） |
| 产物取证 `strings build/JourneyClient.wasm` | 4 条诊断字符串全部命中：`Reconnecting to channel server`、`Reconnect to channel server failed`、`set_field: character-select UI not found`、`set_field: cid mismatch` ✅ |
| 防修绿 diff 扫描 | 无 `#[ignore]`/删除测试/弱断言/硬编码密钥/`TODO`/`FIXME`/`dbg!`/`todo!`；4 处 `Console::get().print` 为本仓库既有日志机制（同 `init()`/`open()`/`close()`）✅ |
| 客户端启动 + 登录连接回归（headless Chrome + CDP，web 栈 :8000/:8765/:8080，Cosmic :8484 实运行） | 4/4 PASS，详见「运行时验证」 |

> 注：本仓库 WASM 客户端无 C++ 单元测试框架（CMakeLists 未引入 gtest/catch2），客户端验证依赖构建门禁 + 浏览器 E2E，与既有 changelog 约定一致。Rust web crates（web-server/ws-proxy/assets-server）本次未改动。

### 运行时验证

- **环境就绪性**：web 栈（`web-server :8000`、`assets-server :8765`、`ws-proxy :8080`）与 Cosmic 服务端（java `:8484`，工作目录 `/root/MapleStory-Server`）均在运行；`web/config.json` 的 `MapleStoryServerIp=127.0.0.1`，本机可达——原评审担心的不可达局域网 IP（`192.168.31.159`）已不存在。
- **覆盖范围说明**：B1/B2 的「失败恢复」分支需在频道服不可达时触发，但本环境服务端在线且不能停服/改库（安全约束），故无法在该会话内真实触发失败路径；B1 的「成功」分支与 B4 的正常路径需以已注册账号 + 角色登录到「选择角色 → 进入游戏」全程验证，而本仓库既有 E2E（`e2e_ime.mjs`、`login-register-entry`）均止于登录界面（无可用已注册账号，且禁止直接写库建号）。
- **本会话已验证（headless Chrome + CDP，4/4 PASS）**：
  1. 客户端正常启动（`Module.LazyFS` ready）
  2. 登录 WS 连接建立——控制台出现 `[stdout] Opening connection: ...`，且后续登录握手（16 字节 IV）正常完成（回归 B2 的 15s 超时未误杀正常握手，B4 的 `connection_changed`/`process()` 新增路径未破坏正常封包处理）
  3. 启动/连接全程无运行时异常
  4. 正常启动不触发任何重连诊断字符串（4 条仅在实际重连/恢复时打印）
  — 登录界面正常渲染（截图像素高度变化，非冻结空白屏）
- **未能在本会话覆盖**：B1/B2/B3 的失败恢复分支需真实触发重连失败——要么停掉频道服（本环境服务端在线且禁止停服/改库），要么以已注册账号完整登录到「选角色 → 进入游戏」。本仓库既有 E2E 均止于登录界面（无可用已注册账号；账号字段经 `#ime-input` textarea 桥输入、密码字段为 crypt 字段不经桥接，完整登录输入链路在本会话未稳定打通）。
- **建议的人工/E2E 后续**：在已有已注册账号 + 角色的环境，验证（1）正常路径：选角色 → Start → 进入游戏或明确报错而非卡死；（2）失败路径：停掉频道服 → Start → 控制台打印 `Reconnect to channel server failed` 且界面恢复可点击。

## 回归风险

- **低（B4 `connection_changed`）**：仅当 `reconnect()` 在 `forward()` 期间被调用时置位，而 `reconnect()` 的唯一调用方是 `SERVER_IP` 路径的 `ServerIPHandler`；正常多封包处理不重连，标志恒 false，行为不变。`process()` 的唯一调用方是 `read()`。
- **低（B2 15s 超时）**：仅替换原先的无限挂起；最坏情况是极端缓慢的握手被误判为断连而提前恢复 UI——这严格优于永久冻结。正常读循环 `ws_recv(..., 0)` 非阻塞，不受影响。
- **低（B1/B3）**：仅在新增的失败/竞态早退分支内调用 `UI::get().enable()`，不改变正常登录进游戏的成功路径。

## 部署须知

- `web/config.json` 的 `MapleStoryServerIp` 必须指向客户端（浏览器/容器）可路由到的登录服地址；频道服地址由服务端在 `SERVER_IP` 封包中下发，`resolve_channel_address` 会对本地地址做 `MapleStoryServerIp` 替换（便于容器/穿透部署）。

## 回滚

`git revert <commit>` 即可恢复重连路径无恢复/无限握手的旧行为。
