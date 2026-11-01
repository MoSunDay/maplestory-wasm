# 现金武器攻击停顿修复：攻击姿态改由真实武器驱动

## 背景

用户反馈"带了现金武器后，攻击后总是要停顿一下，不连贯"。

根因：攻击动画/时长取自 `CharLook::attack`，其武器来自外观槽
（`CharEquips::get_weapon()`）。佩戴现金武器时外观槽是 1702xxx 封面，
而现金武器 `info` 只有 `cash=1`（无 `attack`/`attackSpeed`/`afterImage`），
`WeaponData::get_attack()` 返回 0 → `regular_attack::choices(0)` 为空 →
姿态回退 **STAND1**。stand1 为 3 帧 ×500ms（共 1500ms），`attacking`
要等整套站立动画循环回帧 0 才清零（`Player::update` 的 `aniend`），期间
`PlayerStates` 全部忽略移动/攻击输入 → 每次攻击后硬直 ~1.5s。真实攻击
姿态（swingO1/stabO1 等）整组仅 ~800ms，且看起来就是攻击动作。

连带影响（同一根因）：`Char::prepare_attack_assets` 与 `set_afterimage`
也走外观武器 → 攻击姿态贴图未预热（LazyFS 首击额外卡顿）、残影名为空。

## 修复

战斗用武器数据统一从"角色真实武器"解析，现金封面只管渲染：

- `Char` 新增虚函数 `get_weaponid()`（默认取外观武器；`Player` 覆盖为
  `EQUIPPED/WEAPON` 真实武器，即原 `Player::get_weaponid` 上移为
  public override），`Char::get_weapontype()` 改经其解析，删除与基类
  重复的 `Player::get_weapontype`。
- `CharLook::attack(const WeaponData&, bool)` / `prepare_attack_assets(
  uint8_t attacktype)`：由 `Char` 传入真实武器的 `WeaponData`（姿态选择、
  攻击音效、预热集合、残影全部随之取真实武器数据）。
- `Char::prepare_attack_assets` / `set_afterimage` 同步改用 `get_weaponid()`。

数据契约：现金武器 0170*.img 姿态嵌套于数字体型分组
（如 `01702000.img/33/swingO1`），选定姿态仍可正常渲染，外观不受影响。

## 验证

- 新增探针 `tools/verify/cash-weapon-attack-stance.cpp`（9/9 通过）：
  389 张 0170*.img 全部 `cash=1` 且无 attack/attackSpeed/afterImage；
  各体型分组含可渲染攻击姿态（空占位组除外）；16 类真实武器
  `info/attack` 1..9 对应姿态集合非空（knuckle attack=8 上游即空集合，
  与本修无关）；00002000.img stand1 1500ms vs swingO1 800ms 佐证停顿量级。
- 回归：`regular-attack-stances`、`weapon-afterimage-nx` 探针通过。
- 编译门：Char/CharLook/Player/SkillAction/Combat/Stage 六个 TU 在
  gcc 与 clang 下错误集与改前逐字节一致（本地工具链预存在错误，
  零新增）。
