Commit: a5b3b864bffe31f118e579ac357b5efb3957e627

# 浏览器恢复前台时避免自然恢复误封

## Context

浏览器主循环会在标签页或主线程长时间暂停后补跑全部固定帧。角色 HP 未满时，多个 10 秒自然恢复周期会在同一轮补跑中连续上报，linked server 因短时间收到过多 `HEAL_OVER_TIME` 而将正常账号自动封禁。

## Change Summary

- 固定步长主循环将单帧可补跑时间限制为 250ms，超出部分不再重放。
- WASM 与原生运行路径使用同一纯函数时间边界。
- 增加 160 秒暂停回归场景，验证恢复后最多产生一个恢复封包。

## Impact Surface

- 浏览器标签页恢复前台、系统休眠恢复和主线程长任务后的游戏更新。
- 客户端自然恢复封包的发送节奏。

## Notes / Compatibility

- 不修改自然恢复数值、网络协议、服务端反作弊阈值、数据库结构或环境变量。
- 短于 250ms 的正常帧耗时仍按原有固定步长完整推进。

## Related Docs

- [WASM 客户端](../../../agents/client/index.md)
- [角色自然恢复](../../character-recovery/index.md)
