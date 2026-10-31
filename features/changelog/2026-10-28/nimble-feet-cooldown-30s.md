# 新手疾风步（Nimble Feet）冷却统一 30 秒

## 背景

新手疾风步（客户端名 `疾风步`，Nimble Feet）是三类可创建种族的新手位移
buff：冒险家 `0001002`、骑士团 `10001002`、战神 `20001002`（各 3 级）。
三处冷却不一致：

| 技能 | 现状 cooltime | 说明 |
|------|--------------|------|
| 0001002 | 3600（1 小时） | 早期运营定制（同批把 time 拉长为 7200 的超长速度带） |
| 10001002 | 60 | 原版 |
| 20001002 | 60 | 原版 |

冒险家 1 小时的冷却对新手跑图体验过差，本次统一为 **30 秒**。buff 时长
（服务端 time 字段，冒险家 7200s 长效速度带）保持不变，仅改冷却。

冷却为服务端驱动：`MapleStatEffect` 从 wz 每级 `cooltime` 读取，
`SpecialMoveHandler` 施放后下发 `ADD_COOLDOWN(234)`，客户端
`PlayerCooldown` 泛化处理（图标倒数 + `FBR_COOLDOWN` 拦截），无需客户端
代码改动。

## 变更（新工具 `tools/nx/patch_nimble_feet_cooldown_nx.py`，幂等，双侧四份）

- `assets/Skill.nx`：`{000,1000,2000}.img/skill/<sid>/level/{1,2,3}/cooltime`
  就地重写 8 字节 int64 union（尺寸不变，assets-server 按 mtime 自动重载）。
- `assets/String.nx`：三处 `desc` 的 `再次使用时间间隔 ： 1分` → `30秒`
  （经 `scripts/nx-l10n/apply.py` 的 rebuild 重写字符串 blob；
  10001002/20001002 desc 共享同一字符串表项，一次替换两处生效）。
- 服务端 `wz/Skill.wz/{000,1000,2000}.img.xml`：仅目标技能块内
  `<int name="cooltime" .../>` 值改 30，其余字节不动。
- 服务端 `wz/String.wz/Skill.img.xml`：英文 desc 提示
  `1 min.` → `30 sec.`（Terms between skills / Takes about 两式）。
- 备份一次性留存：`*.pre-nimble-feet-cd-backup`（String.nx 与四个 XML；
  Skill.nx 为 8 字节就地写，沿用 patch_nx_int.py 惯例不留副本）。
- `tools/verify/skill-2023-port-nx.cpp`：硬编码期望
  `0001002 lv1 cooltime == 3600 (1h, ops override)` 同步为
  `== 30 (30s beginner rebalance)`（沿用宽技能改 CD 时同步校验器的惯例）。

范围说明：战神系英文数据名 `Agile Body`(20001002) 客户端中文名同为
`疾风步`，一并覆盖；Evan `20011002` 无 wz 数据且建角不可选，不在范围；
被动系 `疾风步`（3110000/3210000/13100004，加移动速度的被动）非新手
位移技，不改。

## 验证

- `patch_nimble_feet_cooldown_nx.py`：首跑 9+9 处 cooltime、2+3 处 desc
  写入并自校验 9/9、9/9、3/3、3/3；重跑全量 no-op（幂等）。
- `tools/verify/attack-gap-skills-nx.cpp assets/Skill.nx`：28 checks, 0 failures。
- `tools/verify/skill-2023-port-nx.cpp assets/Skill.nx`：RESULT: PASSED。
- `scripts/game_server_restart.sh`：游戏服重启，7575/7675/7775/7875/7975/8484
  全部监听恢复，服务端新 cooltime 生效；客户端 Skill.nx/String.nx 由
  assets-server 按 mtime 热重载，无需重建 WASM。
- 游戏内：施放疾风步后快捷栏图标 30 秒倒数，冷却中再按提示冷却未完。

## 重放

```bash
python3 tools/nx/patch_nimble_feet_cooldown_nx.py          # 双侧四份数据，幂等
bash scripts/game_server_restart.sh                        # 服务端重载 wz
```

## Related Docs

- [技能系统](../../../agents/client/Character/index.md)
- [WZ 变更管线](../../index.md)
