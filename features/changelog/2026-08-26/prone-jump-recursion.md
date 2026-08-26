Commit: c156e60cffec5b46bfc147dc22855189fcc37b3e

# 卧姿攻击期间跳跃递归修复

## Context

角色处于卧姿并仍在攻击时，跳跃输入会尝试切换到站立。攻击状态会拒绝姿态切换，原卧姿处理随后重新分发同一个跳跃动作，持续回到自身并最终触发 WASM `Maximum call stack size exceeded`。

## Change Summary

- 输入层以纯函数区分攻击阻止、向下穿越平台和正常上跳三种卧姿跳跃决策。
- 攻击期间的卧姿跳跃不再改变姿态、位置或速度，也不播放跳跃音效。
- 正常上跳直接完成站立切换和起跳，下跳只执行一次既有穿越平台逻辑；状态处理器不再递归分发跳跃动作。

## Impact Surface

- 本地玩家卧姿跳跃输入
- 攻击动作期间的移动门禁
- 浏览器 WASM 运行稳定性

## Validation

- 纯函数回归覆盖攻击状态、下跳开关和 DOWN 按键的全部组合。
- Emscripten 4.0.21 Release 客户端完整编译和链接通过。

## Notes / Compatibility

不改变网络协议、服务端行为、数据库结构、环境变量或现有站立/行走跳跃语义。

## Related Docs

- [角色系统](../../../agents/client/Character/index.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)
