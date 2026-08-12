Commit: 25a0dc2b2db29b81f6ffb3419b08123cf60f3f0e

# 游戏内账号注册

日期: 2026-08-12

## 变更

- 登录界面注册按钮改为打开 `UIRegister`，使用 `UI.nx` 的 `Basic.img/Notice6` 面板、输入框及确认/取消按钮素材。
- 表单包含账号、密码、确认密码，支持 Tab、Enter、Esc 和中文状态提示；账号和密码均为 4–12 位，账号限定 ASCII 字母数字，密码限定非空白可打印 ASCII，并兼容验收账号 `test1/test1`。
- 注册复用 `LOGIN_PASSWORD`：服务端自动建号返回 TOS 状态后，客户端自动发送 `ACCEPT_TOS` 并继续正常登录。
- 删除已被游戏内表单替代的 `RegisterUrl` 配置与外部网页跳转逻辑。本记录取代同日“登录界面注册入口接线”中描述的外部注册方案。

## 验证

- Docker WASM 构建通过，`UIRegister.cpp`、`UILogin.cpp`、`LoginHandlers.cpp` 均成功编译链接。
- 对真实 Cosmic 登录服完成 `LOGIN_PASSWORD → LOGIN_STATUS(23) → ACCEPT_TOS → LOGIN_STATUS(0)`，数据库确认账号唯一、TOS 已接受且密码为 BCrypt 哈希。
- 测试账号 `e2e514526313` 按验收约定保留，未执行数据库删除。
