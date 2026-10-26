# proto 回归管线三层自愈（warp / levels / ammo）

## 背景

`verify_all.sh proto` 全量跑时出现三类与技能本身无关的失败，全部源于共享测试角色被并发流程（FM/PQ/仓库等 E2E）改动状态。

## 根因链（NL "no response" 7/7 全挂）

1. 现象：test3NL 登录正常、地图 40001 有 27 只活怪、爪已装备，但所有远程攻击发出后服务端 **0 帧静默**（不结算、不踢线、无 enableActions）。
2. 原始封包对照（NL vs CS 同 opcode 0x2D、同图同频道）：CS 立即回 0x1F+0xFA；NL 无任何回包。
3. 定位 `RangedAttackHandler.java:144`：扫描 USE 栏找投射物时，`2070018` 飞标有 `level>=70` 门槛，而 **test3NL 被并发流程改成 60 级**（其余 11 角色均 200）→ `projectile=0` → 尾部 `if (projectile != 0 || soulArrow || ...)` 不成立 → **静默 return**。
4. v83 无 Shadow Claw（4121003 是 Taunt），弓弩有 Soul Arrow 预 buff 而爪系没有——探针的 soul-arrow 预处理只覆盖 31x/32x 前缀，NL 必须真实飞标。
5. 另证：`--topup-ammo` 两次执行间 2070018 行再次消失——并发流程持续在动 NL 的库存/等级，单次修复不可靠，必须在管线内自愈。

## ILA "no living mob"

角色被搬到 910000000（无怪图）。SQL 传送在角色在线时会被登出存回覆盖——自愈需在角色离线时落库（phase 开始 + 收尾各一次，收敛）。

## 修复（`skill-keymap-prep.py` + `verify_all.sh`）

| 手段 | 说明 |
|------|------|
| `--warp-map 40001` | 复位角色到练级图，不动 keymap |
| `--restore-levels 200` | 只升不降，修复被改低的等级（星标门槛依赖） |
| `--topup-ammo` | 补满 NL 飞标 2070018×800、CS 子弹 2330005×3000，缺行则 INSERT；NL 额外注入无等级门槛的备用星标 2070000(Subi)×800 |
| `phase_proto` self_heal | 开跑前 + 收尾（此时全部角色离线，基本避开登出存回竞态（仍可能有毫秒级晚到写回，由下一轮自愈收敛））各执行一轮 |

## 验证

- NL：修复前 7 fired 0 settled（全部 no response）→ 修复后 **7/7 settled**。
- ILA：warp 生效后 **11/11 settled**。
- 全量 report：TOTAL proto 112 settled / 2 failed，剩余 2 个为 Hero 斧系变体（`Panic : Axe`/`Coma : Axe` 需配斧，历史基线）。

## 残留与后续

- 服务端日志见 `CloseRangeDamageHandler` 抛 `IndexOutOfBoundsException: Index -1, length 30`（两次），与本次 NL 问题无关，待单独排查。
- ~~Hero 斧系 2 fail~~ 已修复（真根因是 skills 表缺行打崩 parseDamage，见 `unlearned-skill-attack-crash.md`，非「需配斧」）；Pala 旧 visual 2 fail 仍待空闲窗口刷新。
- 若并发流程再次压低等级，Subi 备用星标兜底；收尾自愈会在下一轮 phase 前恢复 200。

## 追记：飞标堆去重（幂等修正）

首版 `--topup-ammo` 缺行即 INSERT——但**飞标/子弹不可叠加合并，每一堆是独立物品行**，每轮跑都新增一堆（3 轮后 NL 出现 2×2070018 + 3×2070000）。修正为「每种弹药恰好一堆」：补满 position 最小的一堆，删除同 itemid 多余堆。连续两轮管线输出全部 `refilled`（无 inserted / 无 dup removed），幂等成立；CS 的子弹重复堆同步清理。
