Commit: adb311ccfef22025c0673927d91738a9b3d1ca26

# 背面手套图层与角色预览失败收敛

## Context

装备 `1082003` 的 Character NX 外观包含合法 `backGlove` 图层，Base zmap 同时定义了 `backGloveWrist`。旧映射只覆盖小写 `glove*` 图层并对其他值抛异常；角色选择又把单个预览构造异常提升为整个界面失败，使预览完成数无法收敛，角色选择持续不可操作。

## Change Summary

- 手套外观补齐 `backGlove` 与 `backGloveWrist` 的背面层级；未知图层只警告并跳过对应位图。
- 角色预览采用 `PENDING`、`READY`、`FAILED` 三态，成功与失败都结束 pending；单个角色失败后继续处理其他角色。
- 全部预览收敛时只在完成边沿进入一次 `READY` 并恢复 UI；后续帧不会覆盖选角、PIC 或删除请求持有的输入锁，缺少预览的角色仍可正常选择并进入游戏。
- NX 审计覆盖 Base 的 16 个前后手套层和 `1082003`；浏览器验收要求预览完成数最终等于角色总数、加载期间保持输入锁，并验证选角请求等待期不会被重新解锁。

## Impact Surface

角色外观的手套分层、角色选择预览生命周期和 CHARLIST 运行时验收。登录协议、服务端、数据库、环境变量与 NX 原始数据不变。

## Related Docs

- [角色系统](../../../agents/client/Character/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
