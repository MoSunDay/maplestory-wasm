# 登录账号归属异常修复（空名 AUTOMATIC_REGISTER 建号根因）

## Context

现象：test1/test1 登录偶发进入陌生账号（曾出现 test1test1 等归属
错乱），数据库出现空用户名账号 id 27/29（后被人工删除），角色列表
"previews 0/0"。排查出三层根因链条：

1. `src/client/IO/ImeBridge.cpp` `focus_field()` 切换字段时未先失焦
   旧字段，DOM textarea 焦点与游戏侧 Textfield 状态脱钩；
2. `web/index.html` `#ime-input` 未设 `pointer-events:none`，点击
   落到 textarea 而非 canvas；
3. 点击后浏览器把焦点抢给 canvas，textarea 已 blur 但
   `ImeBridge::is_active()` 仍为真，`src/client/IO/Window.cpp`
   `key_callback` 的死区闸门把后续按键全部丢弃 → 用户名实际为空
   → 服务端 `LoginPasswordHandler.java` loginok==5 走
   AUTOMATIC_REGISTER，INSERT 了空名新账号。

## Change Summary

- `src/client/IO/ImeBridge.cpp`：`focus_field()` 先调用
  `blur_field()` 清理旧焦点，DOM 焦点与游戏侧状态重新对齐。
- `web/index.html`：`#ime-input` 加 `pointer-events:none`，点击
  不再被透明 textarea 截走。
- `src/client/IO/Window.cpp`：闸门改为仅当
  `document.activeElement.id === 'ime-input'` 时才拦 GLFW 转发；
  canvas 抢焦点后不再吞键。
- `src/client/IO/UI.h` / `src/client/IO/UI.cpp`：新增
  `has_focused_textfield()`；`src/client/CMakeLists.txt` 导出
  `_msime_field_focused` 供 e2e 轮询焦点真态。
- `src/client/IO/UITypes/UIWorldSelect.cpp`：世界绑定改
  `worlds.back()`——serverlist 5 包可分包或 TCP 合并送达，
  `UIStateLogin::pre_add` 先 remove 再 add，`back()` 在两种送达序下
  都解析到最后通告的世界，消除 charlist 按错 world 字节过滤。
- `tools/verify/maplestory-webui/cdp.mjs`：
  - 新增 `waitFieldFocused`/`clickField`：点击后轮询
    `msime_field_focused`，3 次重试；
  - 新增 `retryClick`：谓词轮询的状态变迁点击，防资产流高负载下
    CDP 整击丢失；
  - `doubleClick` 两击间 80ms（低于 0.35s 双击窗口）。
- `tools/verify/maplestory-webui/run.mjs`：
  - 世界选择门改为容忍 worldSelect/charSelect 双态——web 构建的
    DOM GO 按钮桥接可能让客户端越过 state 2，1Hz 轮询漏采；
  - 建卡名称提交点击放宽为 8 次 × 4s（第二波定制素材冷流期间
    点击易丢）。

## Verification

- `E2E_CREATE_CHARACTER=0`（/tmp/run7.log）与 `E2E_CREATE_CHARACTER=1`
  （/tmp/run9.log，需先
  `delete from characters where accountid=25 and name='测试一'`）
  双模式全绿。
- test1/test1 稳定登入账号 25，不再出现空名建号与归属错乱。
