# 受击伤害恢复正常量级：移除 PAD/2 砍半基线，保留 v83 0.7 防御抵扣

## 背景

d5508905 把受击伤害改为 `(mobatk - 0.7×DEF) / 2`，其中 `/2` 是
LibreMaple 基线残留而非 v83 系数，会把全部受击再砍半；与 0.7 防御
抵扣叠加后，1320 只怪中 706 只对成装角色恒打 1（`max(1, ...)` 兜底），
受击量级明显偏低，防御收益被放大到失真。

## 变更

- `CharStats::calculate_damage`：基线改为 `max(1, mobatk - DEF×7/10)`，
  即 v83 系数 0.7 抵扣全额怪攻，去掉 `/2`。`mobatk` 本身已是
  [0.8,1.0]×PAD 的随机值，随机性不受影响。
- Achilles / 圣甲术减伤链路（`reducedamage` 二次扣减）不变。

## 数值对照（mobatk=1000, Achilles 20%）

| 场景 | 旧 /2 公式 | 新公式 |
|------|-----------|--------|
| 裸装 DEF≈4 | 399 | 799 |
| WDEF=198 物理 | 345 | 690 |
| MDEF=120 魔法 | 367 | 733 |

## 验证

- `tools/verify/player-defense-damage.cpp` 原生回归通过。
- `tools/verify/maplestory-webui/equip-stat-flow.mjs` 期望已同步为
  690/733，需在有游戏服务端的环境重跑。
