Commit: a5b3b864bffe31f118e579ac357b5efb3957e627

# 角色自然恢复

## 能力

角色进入游戏后由客户端按 MapleStory v83 规则定时上报 HP/MP 自然恢复。恢复不直接改写本地属性，最终数值以 linked server 校验并回传的属性为准。

## 规则

- 站立或坐下时，HP 每 10 秒恢复一次；普通基础值为 10。
- MP 在角色存活时每 10 秒恢复一次；普通基础值为 3。
- 战士 `Improved HP Recovery`、准骑士 `Improving MP Recovery` 使用 Skill.nx 当前等级的 `hp`/`mp` 数值。
- 魔法师 `Improved MP Recovery` 使用 `3 + 技能等级 × 角色等级 / 10`。
- 攀爬时仅有 `Endure` 才恢复 HP，周期使用 Skill.nx 当前等级的 `time` 秒。
- 地图 `info/recovery` 倍率作用于单次恢复量；负值按 0 处理。
- 物品椅子读取 Item.nx 的 `recoveryHP`/`recoveryMP`，与自然/技能恢复取较大值，不叠加。
- HP 单次上报按 linked server 的 `floor(77 × 地图恢复倍率 × 1.5)` 校验上限动态约束，MP 单次最多上报 999；满值时不发送对应增量，死亡时清空计时。

## 椅子交互

双击 SETUP 栏中的 301xxxx 椅子会发送使用请求并立即进入本地坐姿；地图座位会携带 NX 节点中的座位 ID 请求服务端占用。移动或跳跃离开任一坐姿时发送取消。其他角色的椅子素材和坐姿由服务端广播同步。

## 约束与错误面

- NX 缺少技能、地图或椅子字段时使用安全默认值，不放大恢复。
- 服务端拒绝不合法或过快的恢复封包时，客户端不做降级写值，等待服务端属性同步。
- 切图、复活和死亡都会重置可能跨场景遗留的恢复或椅子状态。
- 浏览器后台挂起或主线程长时间暂停后不补播错过的恢复周期；恢复前台时最多补跑 250ms，避免把多个正常周期压缩成恢复封包突发。

## 相关逻辑

- [角色系统](../../agents/client/Character/index.md)
- [网络层](../../agents/client/Net/index.md)
- [网络协议](../../docs/ms-network-protocol.md)
