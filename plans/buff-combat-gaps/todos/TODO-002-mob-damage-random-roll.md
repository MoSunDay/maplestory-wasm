# TODO-002：受击/动作伤害随机系数

- 状态：待启动
- 优先级：P2
- 需求目标：客户端本地计算受击伤害时引入 v83 怪攻随机系数（约 [0.8,1.0]×PAD），与正式服手感一致。
- 核心实现：`CharStats::calculate_damage`（`src/client/Character/CharStats.cpp:230`）目前对同一 mobatk 输出恒定值，无 roll；需在调用侧（`Player::damage`/动作攻击路径）注入随机因子。注意受击数字最终以服务端 `TakeDamagePacket` 为准（客户端权威上报，`TakeDamageHandler.java:70` 直读），本地 roll 仅影响展示层。
- 关键要素：随机范围与 v83 参考（HeavenMS/DamageParse）对齐；不影响防御抵扣修复的回归断言（`player-defense-damage.cpp`、`equip-stat-flow.mjs` 期望 345/367 均基于无 roll 的确定输入，改动时需同步）。
- 前置依赖：无。
- 当前进展：未动工。
- 验证手段：原生审计抽样分布落在 [0.8,1.0] 区间；e2e 回归期望值更新。
- 下一步：对照 HeavenMS `AbstractDealDamage`/playerDamage 随机项确定精确区间。
- 备注：源自圣甲术修复评审；展示层问题，不改变服务端权威链路。
