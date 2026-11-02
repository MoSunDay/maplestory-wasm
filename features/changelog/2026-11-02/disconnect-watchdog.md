# 客户端断线看门狗（最小安全版：检测 + 通知，不改传输控制流）

排查背景与冻结证据链见
`docs/known-issues/2026-10-29-game-server-restart-client-freeze.md`（该 issue
随本条落地关闭）。这是 P1 残余缺陷 #3 的客户端侧补全：此前 ms-game-server
崩溃/重启期间，WASM 客户端登录流在 Asyncify 等待中**静默楔死**——主线程
parked、无异常、无网络错误、用户界面零提示，只能手动刷新页面。

## 触发条件（用户可见行为）

改动只新增两条**一次性**通知路径，均以浏览器顶部横幅呈现（fixed 定位、
内联样式、中文文案、点击即 `location.reload()`，按元素 id 幂等）：

| 触发点 | 条件 | 文案 |
|---|---|---|
| `onclose` 回调 | WebSocket 被关闭，且该 socket **非**客户端主动关闭 | 「与服务器的连接已中断，请检查服务器后点击重新加载」 |
| `onerror` 回调 | WebSocket 错误事件，且该 socket **非**客户端主动关闭 | 同上（stderr 日志区分 `websocket close event` / `websocket error event`） |
| `ws_recv` 等待循环 | 某 socket **从未收到任何字节**且累计空转等待 ≥ 30s | 「与服务器的连接长时间无响应，请检查服务器后点击重新加载」 |

每次触发同时输出一行 `std::cerr`（`[disconnect-watchdog] connection lost/stalled: ...`），
两类通知各自以静态 bool 幂等（首次触发即定格，横幅本体按 DOM id 幂等，
重复触发零副作用）。

## 变更

- `src/client/Net/SockInternal.cpp`（全部为匿名命名空间自由函数 + 静态
  变量，**未新增任何 class**）：
  - `sock_inject_reload_banner`：经 `emscripten_run_script` 注入一行式
    DOM 横幅（`id=ms-disconnect-banner`，幂等）。
  - `sock_notify_disconnect(reason)` / `sock_notify_stall(reason)`：各自
    静态 bool 幂等；一行 cerr + 横幅。
  - `sock_note_transport_data` / `sock_accumulate_recv_wait`：按 socket id
    记录"曾收到字节"与"累计空转等待"，供 30s 判定。
  - `sock_mark_manual_close(socket)`（外部链接，声明于 `SockInternal.h`）：
    客户端主动断开前置位；按 socket id 记录，换频道（close 后立刻
    reconnect）时旧 socket 迟到的 close 事件只会命中自己的置位，不会误报。
  - `sock_handle_transport_closed(socket, reason)`：onclose/onerror 回调
    头部调用——manual-close 置位则静默放行，否则通知；两种情况都清掉该
    socket 的看门狗状态。
- `src/client/Net/SocketWinsock.cpp`：`close()` 的 WASM 分支在
  `ws_closesocket` 前调用 `sock_mark_manual_close(sock)`。grep 确认这是
  客户端主动关闭的**唯一汇聚点**（`Session::reconnect` / `Session::disconnect`
  均经 `socket.close()`；`SocketAsio` 仅用于非 WASM 构建，不含 ws 链路）。
- `src/client/Net/SockInternal.h`：补 `sock_mark_manual_close` 声明。

## 红线遵守（为何这是"最小安全版"）

- **未触碰 `ws_connect`/`ws_recv` 的任何循环条件、返回值、sleep 步长与
  回调时序**：等待循环内只插入一行纯记账调用 `sock_accumulate_recv_wait`
  （map/set 查找，每次 5ms 迭代一次，不改变挂起语义）。其前身热补丁曾因
  动挂起/自旋控制流引发 boot 期 `null function` 回归，本版明确只做
  "检测 + 通知"叠加（见 `docs/roadmap.md` P1#2 警示）。
- 通知路径不抛异常、不改变数据流、不重排回调；横幅注入为幂等 DOM 追加。
- 无任何敏感信息注入（横幅为固定中文文案与内联样式，不依赖外部资源）。

## 验证

- 宿主机 `g++ -std=c++17 -fsyntax-only`（stub emscripten 宏/头，方法同
  8689c62）：`SockInternal.cpp`、`SocketWinsock.cpp` 均通过。
- 注入 JS（断线/无响应两份 payload）`node --check` 通过；DOM stub 行为
  验证：双通知只创建一个横幅、id/文案正确、点击恰触发一次
  `location.reload()`。
- grep 确认客户端主动关闭点全部前置标记（唯一通道
  `SocketWinsock::close()`）。
- 未跑完整 wasm 构建（父流程统一重建）。

## 后续 roadmap

**完整自动重连 / 会话恢复**（断线后自动重登重进，而非一键手动重载）仍为
后续项，见 `docs/roadmap.md` P1#3（服务端侧三段链路已就位：ws-proxy
liveness、空闲收割、幽灵会话落库）。本条只承诺"冻结不再无声"。
