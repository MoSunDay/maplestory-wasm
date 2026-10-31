# 现金武器外观不再影响战斗属性（外观/数值隔离收尾）

## 背景

现金（NX）装备定位是纯外观。背包侧此前已保证 worn cash 装备落
`EQUIP` 100+ 隐藏槽、`Inventory::recalc_stats` 只统计 `EQUIPPED`，
但武器维度的战斗数据仍取自 look：`CharLook` 把武器外观（maskedequips）
盖在真武器之上，导致戴上武器外观后 look 的 WEAPON 槽变成 cover
（170 段），`Weapon::by_value` 只能得出 `Weapon::CASH`，进而污染：

- `CharStats` 伤害倍率 / 主副属性 / `attack_stat::of`（recalc_stats 链）；
- 子弹前缀选择（2060/2061/2070/2330 → 子弹 WATK）；
- 技能武器类型校验（can_use）、攻击类型判定（prepare_attack）；
- 攻速（get_integer_attackspeed 读了外观武器 speed）。

## 变更

- `src/client/Character/Char.h`：`get_weapontype()` 改 virtual，远程角色
  保留 look 实现（仅剩外观用途）；
- `src/client/Character/Player.h/.cpp`：新增 `get_weaponid()`（读
  `EQUIPPED[WEAPON]`，该库存永不含 cash 装备）与 `get_weapontype()`
  override；攻速改用真武器 id。外观链（cover 显示、持械姿态、残影）
  全部保持 look 驱动不变。

## 验收

- `tools/verify/cash-equip-policy.cpp` 新增第 8 节（31 断言全绿）：真武器
  只落 `EQUIPPED[WEAPON]`、cover 落 `EQUIP[100+WEAPON]`、170 段 cover
  经 `Weapon::by_value` 仅得 `Weapon::CASH`、cover WATK 不入属性总和；
- 回归：`player-defense-damage`、`second-job-attack-nx`、
  `weapon-afterimage-nx` 全部通过；
- WASM 全量重编（docker wasm-builder，vtable 变更波及面全量重建）通过；
- e2e（`cash-shop-take-equip-flow`）暂缓补跑：验证窗口内共享测试床被
  并行任务持续变更（quest WIP 混入构建、游戏服务端 21:08 重启、干净
  HEAD 因缺 `UIQuestLog::selected_id` 定义无法链接），登录阶段超时
  无法归因到本修复；环境稳定后按 runbook 补跑。

## 已知边界

本地玩家攻击姿态现随真武器（SkillAction 虚派发），cover 与真武器
类别差异大时姿态与贴图可能错配，且与远程角色（look 驱动）表现可能
不一致——纯外观层问题，不影响属性，待 e2e 目测确认后再定姿态取值。
