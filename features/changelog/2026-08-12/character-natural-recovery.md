Commit: 2c824078f1fb122f71fd43ec8a223fce6ba6e7ad

# 恢复角色静止时的 HP/MP 自动回复

## Context

客户端缺少 v83 的 `HEAL_OVER_TIME` 上报，角色即使长时间静止也不会触发服务端认可的自然恢复；SETUP 椅子也没有使用入口和外观同步。

## Change Summary

- 增加独立纯函数恢复模块，覆盖基础恢复、职业被动、Endure、地图倍率和椅子恢复。
- HP 恢复量按 Cosmic 的地图倍率动态校验上限约束，MP 上限为 999，并由服务端决定最终属性。
- 接入 `HEAL_OVER_TIME`、`USE_CHAIR`、`CANCEL_CHAIR`、`SHOW_CHAIR` 与本地椅子回执。
- 双击 SETUP 椅子可坐下，地图座位按 NX 节点 ID 请求占用，离开坐姿时取消；本地和其他角色显示对应 Item.nx 椅子动画。
- 增加可执行的周期、公式、姿态、满值、死亡和封包上限回归验证。

## Impact Surface

- 玩家静止、攀爬、坐椅子时的 HP/MP 恢复。
- SETUP 物品双击交互、角色姿态和椅子绘制。
- v83 客户端与 Cosmic 的恢复及椅子协议。

## Related Docs

- [角色自然恢复](../../character-recovery/index.md)
- [角色系统](../../../agents/client/Character/index.md)
- [网络层](../../../agents/client/Net/index.md)
