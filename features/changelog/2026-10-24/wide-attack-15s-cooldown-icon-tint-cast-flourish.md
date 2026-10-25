# 四转宽范围攻击技能：冷却统一 15s + 图标紫色识别 + 施放特效增强

## 背景

12 个自定义"宽范围攻击"(WIDE) 四转技能（每个四转职业一个的技能书末格大招，
如主教 `2321010 Genesis Nova` 克隆自圣光普照 `2321008`）此前的冷却参差不齐
（10s/14s/15s/50s/60s/75s/120s/180s 不等），且图标是从源技能整树克隆的，
与源技能图标一模一样、在技能书里还相邻，缺乏辨识度；施法表现也与源技能
完全相同。

本轮将冷却统一为 **15s**，为图标叠加**紫色识别色**，并在施放时叠加两重
额外特效作为统一的"大招签名"。

## 变更

### 冷却统一 15s（数据管线，restore → rerun）

- `tools/nx/wide_attack_manifest.json`：12 处 `stats.cooltime` 全部改为 `15`。
- 客户端 `assets/Skill.nx`、`assets/String.nx`：恢复 `*.pre-wide-attack-backup`
  后由 `tools/nx/inject_wide_attack_nx.py` 重注入（h1..h10 描述同步为
  `Cooldown 15s`）。
- 服务端 `link_repos/MapleStory-Server/wz/Skill.wz/{112,122,132,212,222,232,
  312,322,412,422,512,522}.img.xml` 由 `emit_wide_attack_xml.py`（读取重注入
  后的 Skill.nx）重发射；`wz/String.wz/Skill.img.xml` 由
  `inject_wide_attack_string_xml.py` 重注入。服务端 `MapleStatEffect` 从 wz 读
  cooltime，重启游戏服后生效；客户端冷却由服务端 ADD_COOLDOWN(234) 封包下发，
  无需客户端冷却代码改动。
- `tools/verify/wide-attack-nx.cpp` 硬编码期望表 12 处 cooltime 同步为 15。

### 图标着色（客户端）

- `src/client/Data/SkillData.h/.cpp`：新增自由函数
  `Color wide_attack_icon_tint(int32_t skill_id)`——宽技能返回紫色
  `Color(0.78f, 0.55f, 1.0f, 1.0f)`，其余返回不透明白（乘法调制=不着色；
  注意 `Color()` 默认构造是 alpha=0 全透明，白色必须显式构造）。
- `UISkillBook.cpp` `SkillIcon::draw`：normal/disabled/mouseover 三态均以
  `DrawArgument(pos, tint)` 绘制。
- `QuickSlotBar.cpp`：`draw_slot` 增加 `Color tint` 参数；技能槽传
  `wide_attack_icon_tint(skill_id)`，按键纹理分支传白色。
- `SkillTooltip.cpp`：悬停提示的 2 倍图标改用带 tint 的全参 `DrawArgument`。

### 施放特效增强（客户端 `Combat/Skill.h/.cpp`）

- Skill 类新增私有成员 `struct Flourish { Animation animation; int8_t z; }`、
  `std::vector<Flourish> flourish`、`bool ironbody_flash`（不新增类层次）。
- 构造函数中 `SkillId::is_wide_attack` 时按常量表
  `FLOURISH_EFFECTS = { { "230.img", "2301004" } }` 加载祝福的三层施放动画
  （`effect`/`effect0`/`effect1`，写法镜像 `SkillUseEffect.cpp` 的
  MultiUseEffect 遍历），并置 `ironbody_flash = true`。
- `apply_useeffects` 在源技能 useeffect 之后依次叠加：
  - **祝福(2301004) 金色圣光**：`show_attack_effect` 逐层播放；
  - **圣甲术剪影闪光**：`show_iron_body()`（客户端硬编码的 Iron Body
    特效，角色剪影放大+淡出闪光 500ms）。
- 特效映射考证：数据中不存在"女神祝福"此名，方案按名称最接近的
  **祝福 (Bless, 2301004)** 金色圣光实现；圣甲术即 **Iron Body (1001003)**，
  其特效为客户端硬编码而非 NX 数据。如需换成 神圣祈祷(2311003) 或
  圣灵之盾(2321005)，只改 `Skill.cpp` 中 `FLOURISH_EFFECTS` 一行常量。

## 验证

- `tools/verify/wide-attack-nx.cpp`（Skill.nx + String.nx 全量契约）：PASSED。
- `tools/verify/wide-attack-server-xml.py`（286 项检查）：PASSED。
- `tools/verify/final-attack-combo-nx.cpp`：确认重注入未丢失 combo 注入。
- `scripts/docker_build_wasm.sh` 客户端构建成功；
  `scripts/game_server_restart.sh` 游戏服已重启（新 cooltime 生效）。
- 游戏内：施放宽技能后快捷栏 15s 倒数，技能书/悬停图标呈紫色，施放时
  源技能特效 + 祝福圣光 + 剪影闪光三重叠加。

## Related Docs

- [技能系统](../../../agents/client/Character/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
