Commit: 2c824078f1fb122f71fd43ec8a223fce6ba6e7ad

# 死亡墓碑固定落地

## Context

角色在空中死亡时物理坐标会被冻结，墓碑此前直接跟随该坐标绘制，因此可能停在平台上方。

## Change Summary

- 墓碑记录死亡世界坐标，并固定到正下方最近的 foothold。
- 下落动画与落地帧使用独立地面锚点，不改变死亡角色的物理状态。
- 墓碑落地后，幽灵围绕同一固定锚点运动；下落期间仍保留角色原死亡位置。

## Impact Surface

- 本地玩家与其他玩家的死亡墓碑和幽灵绘制位置。
- 回城提示、复活清理和服务端协议保持不变。

## Related Docs

- [角色系统](../../../agents/client/Character/index.md)
- [标准死亡墓碑动画](death-tomb-animation.md)
