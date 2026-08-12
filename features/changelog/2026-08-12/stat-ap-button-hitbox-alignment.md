Commit: 02e256356be8552379c7d3b93068441ab9ebd485

# 属性加点按钮命中区域对齐

## Context

属性窗口的加点按钮额外叠加了手工坐标，而当前 NX 按钮素材的 `origin` 已包含对应属性行的位置，造成可见加号和点击区域偏离属性。

## Change Summary

- HP、MP、STR、DEX、INT、LUK 加点按钮直接使用 NX 内置位置。
- 启用、悬浮、按下和禁用状态只切换纹理，不改变按钮坐标。
- 属性与加点封包映射保持不变。

## Impact Surface

游戏内角色属性窗口的加点按钮绘制与鼠标命中区域。

## Notes / Compatibility

不修改 NX 素材、网络协议、服务端属性规则或数据库结构。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [功能索引](../../index.md)
