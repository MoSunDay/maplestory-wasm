Commit: 722f9f98f45bc2c7317c3758b304bb3922a3297d

# Escape System 菜单登出返回登录页

## Context

游戏内没有其他窗口时按 Escape 只会关闭状态栏弹层；System 的退出项还会结束整个 WASM 主循环，无法完成键盘快速登出后重新登录的预期流程。

## Change Summary

- Escape 在无其他可关闭窗口时打开 System 菜单，并默认选中退出游戏。
- Enter 激活默认项，发送标准 `PLAYER_DC`、关闭频道连接、重连登录服并切换到账号登录页。
- 重连失败保持为明确连接失败，不伪造已返回登录服的状态。

## Impact Surface

- 游戏内 Escape / Enter 键盘路径。
- System 菜单退出项和登录服会话生命周期。

## Notes / Compatibility

- 继续使用 linked server 已接通的标准 v83 `PLAYER_DC(0x0C)`，不新增协议或环境变量。
- 登出时清理当前 Stage；客户端主循环和页面继续运行。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
