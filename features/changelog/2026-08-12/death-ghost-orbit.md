Commit: d8304dc47fa83bd1ef6722660bd549ef89913c9c

# 死亡幽灵环绕墓碑

## Context

死亡表现已有墓碑下落和落地状态，但角色只停留在原地，缺少经典死亡动画中幽灵围绕墓碑移动的过程。

## Change Summary

- 角色死亡时使用 Character 资源中的 `dead` 姿态呈现角色头部与幽灵身体。
- 墓碑落地后，幽灵沿顺时针椭圆轨迹持续环绕墓碑。
- 幽灵经过椭圆前后半圈时切换绘制层级，形成绕到墓碑前方和后方的遮挡关系。
- 复活或离开死亡状态时同步清除墓碑和环绕状态。

## Impact Surface

- 本地玩家与其他玩家的死亡状态渲染。
- 墓碑落地后的回城提示时机保持不变。

## Notes / Compatibility

- 复用现有 Character 与 `Effect.nx/Tomb.img` 资源，不修改 `assets/`。
- 未修改服务端行为或网络协议。

## Related Docs

- [角色系统](../../../agents/client/Character/index.md)
- [前置墓碑动画修复](death-tomb-animation.md)
