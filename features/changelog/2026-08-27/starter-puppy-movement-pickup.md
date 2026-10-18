Commit: 36bf867ec97ebe38af632a4bb48f7e2fcf8ac88e

# 新手小狗正常跟随与内建拾取

## Context

旧宠物跟随力高于普通角色，追踪掉落时还可能直接传送到物品坐标；同时新手 Brown Puppy 的拾取依赖隐藏能力装备，部分已发放小狗无法稳定触发客户端与服务端的能力门禁。

## Change Summary

- Brown Puppy（`5000001`）按物品 ID 内建金币磁铁、物品袋和飞行靴，自动药水与魔法秤保持装备门禁。
- 地面移动力调整为 `0.16`；主人距离超过 150px 时只瞬移到主人左侧，不再瞬移到掉落物。
- 不可达或卡住的拾取目标会被双端状态机放弃，掉落搜索范围收紧到主人 120px。
- 真实 WASM + linked server 验收固定断言宠物 ID、能力掩码、移动 P95、主人侧 WARP 和服务端确认拾取。

## Impact Surface

宠物能力显示、地面/飞行跟随、自动拾取目标选择、移动同步和 WebUI 宠物验收。

## Notes / Compatibility

不改变 v83 opcode、数据库 schema、既有礼包版本或宠物来源数据；所有 `5000001` 都获得相同内建能力。

## Related Docs

- [永久新手宠物与自动化](../../pet-automation/)
- [游戏世界](../../../agents/client/Gameplay/index.md)
- [角色系统](../../../agents/client/Character/index.md)
