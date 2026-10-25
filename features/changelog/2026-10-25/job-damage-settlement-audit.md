# 全职业伤害结算前置审计：宽技能双版本契约固化与魔法死数据清理

## 背景

四转末格大招存在**双版本**：原版技能（普通版本）与 12 个自定义
WIDE_*"更大范围伤害版本"（每个四转职业末格一个，如主教
`2321010 Genesis Nova` 克隆自 `2321008`）。本轮曾把原版终极也误注入
15s 冷却，随后回滚：数据从 `*.pre-wide-attack-backup` 基线按 wide-attack
注入管线重建，已字节级还原，并新增审计器把契约固化下来。

**契约澄清：仅 12 个更大范围伤害版本（WIDE_*）享有 15s 冷却 +
紫色图标 + 施放金光/圣甲闪光特效；原版终极 Genesis(2321008) /
Blizzard(2221007) / Meteor Shower(2121007) 属普通版本，保持无冷却。**

同时排查发现 3 个魔法 WIDE_* 的 level 里同时携带 `damage` 与 `mad`，
而魔法分支只消费 `mad%`——`damage` 属死数据且违反互斥契约，一并清理。

## 变更

### 新增全职业结算审计器 `tools/verify/job-damage-settlement-nx.cpp`

源码驱动（写法镜像 `second-job-attack-nx.cpp`）：从
`SkillData::flags_of` 解析 104 个 ATTACK 技能白名单，从 `SkillId.h`
解析 `SkillId::NAME` → id 与 5 职业 39 分组，逐技能逐级断言 Skill.nx，
共 **586 断言 / 0 失败**（`skills 104, groups 39, assertions 586,
failures 0`）。覆盖：

- 每技能 NX 节点存在、每级伤害源 damage/mad/fixdamage/x > 0；
  例外：SNIPE 3221007 / MESO_EXPLOSION 4211006 走代码兜底、
  ARMOR_CRASH 1111007 无伤害、BIG_BANG 2321001 仅满级 mad。
- damage 与 mad 互斥（魔法只消费 mad%，宽范围魔法
  2121009/2221009/2321010 不携带 damage）。
- mobCount/attackCount ≤ 15 半字节界；MESO_EXPLOSION 原版
  attackCount 16..20 例外，但审计器必须在 `Skill.cpp` 中找到
  `std::min` 钳制的文本断言。
- RANGED 技能 hitcount 走 bulletCount 且 1..15。
- 宽技能契约：`is_wide_attack` 集合恰为 12 个 WIDE_* 且每级
  cooltime == 15；原版终极 GENESIS 2321008 / BLIZZARD 2221007 /
  METEOR_SHOWER 2121007 为普通版本、全部 level 无 cooltime。
- 差异化特效源码断言：`wide_attack_icon_tint` 对 is_wide_attack
  返回 `{0.78f, 0.55f, 1.0f, 1.0f}`；`FLOURISH_EFFECTS` 非空且以
  `is_wide_attack(skillid)` 为开关。

### 数据清理：魔法 WIDE_* 移除死数据 damage（从基线重建）

- `tools/nx/wide_attack_manifest.json`：3 个 magic 条目
  （2121009 Inferno Sphere / 2221009 Frozen Orb / 2321010 Genesis Nova）
  stats 删除 `damage`，仅保留 `mad`。
- 同步注入管线：`wide_attack_common.py` `load_manifest`、
  `inject_wide_attack_nx.py` `expected_entries`、
  `emit_wide_attack_xml.py`（magic 条目发射 mad 而非 damage）、
  `inject_wide_attack_string_xml.py`（magic 文案
  `Deals %d%% magic damage`）。
- 三个验证器契约同步：`wide-attack-nx.cpp`（magic level 携带
  damage 即失败）、`wide-attack-server-xml.py`（magic 无 damage
  断言）、`magic-damage-nx.cpp`（2121009 damage 节点缺失 +
  mad 定标走 `magic_damage::scale`）。

### 修复入口记录

- 客户端 `assets/Skill.nx`、`assets/String.nx`：恢复
  `*.pre-wide-attack-backup` 基线后由 `tools/nx/inject_wide_attack_nx.py`
  重注入。
- 服务端 `link_repos/MapleStory-Server/wz/Skill.wz` 12 个
  `{112,122,132,212,222,232,312,322,412,422,512,522}.img.xml` 由
  `emit_wide_attack_xml.py` 重发射；`wz/String.wz/Skill.img.xml` 由
  `inject_wide_attack_string_xml.py` 重注入。
- `scripts/game_server_restart.sh`：游戏服重启，6/6 就绪。

### 产品代码改动

- 仅 `src/client/Character/SkillId.h` 中 `is_wide_attack` 的注释措辞
  （明确"更大范围伤害版本"与普通版本无冷却的契约）；集合保持
  12 个 WIDE_*，曾误加入的三个终极已回滚出集合。
- `scripts/docker_build_wasm.sh -j 4`：WASM 重编通过。

## 验证（全部 PASS）

- `tools/verify/job-damage-settlement-nx.cpp`：586 断言 / 0 失败。
- `tools/verify/wide-attack-nx.cpp`：PASSED。
- `tools/verify/wide-attack-server-xml.py`：286 checks PASSED。
- `tools/verify/second-job-attack-nx.cpp`：PASSED。
- `tools/verify/attack-gap-skills-nx.cpp`：28 checks PASSED。
- `tools/verify/magic-damage-nx.cpp`：PASSED。
- `tools/verify/damage-encoding.cpp`：PASSED。

## Related Docs

- [技能系统](../../../agents/client/Character/index.md)
- [验证工具索引](../../../tools/verify/index.md)
