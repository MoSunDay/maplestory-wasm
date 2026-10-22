# 终极追加攻击（Final Attack）+ 斗气球渲染 + 毒/冰变色与玩家受击数字

## 背景

用户报告：1) 剑客二转攻击技能不出伤害；2) 三转斗气（Combo Attack 珠子）不显示；
3) 毒伤害等攻击特效缺失。

排查结论（Skill.nx / 服务端双源比对）：

- **终极不出伤害的根因**：079MAX4 数据集里 `110.img` 剑客二转只有
  1100000/1100002/1100004/1100006/1100007，**没有主动攻击技**——用户感知的
  "二转攻击"实际是被动 终极(1100002, prop 2%→60%, damage 105%→350%)。
  服务端 `AbstractDealDamageHandler` 对 1100002/1100003/1200002/1200003/
  1300002/1300003 特判 `mobCount = 15;//:(`，即期望客户端在主攻击命中后
  **主动追加发送 skillid=终极 的 CLOSE_RANGE_ATTACK 包**；客户端从未实现 → 无伤害。
- **斗气不显示的根因**：封包→状态链路（GIVE_BUFF/GIVE_FOREIGN_BUFF/CANCEL_*、
  `Player::give_buff`、`OtherChar::visual_buffs`、虚接口 `get_visual_buff_value/
  source`）全部就绪，唯独**没有任何绘制代码**。数据：`111.img/skill/1111002/state`
  与 `1120003/state` 各 6 帧——帧 "0" 111x112 静态光环、帧 "1".."5" 是 5 种**不同**的
  42x42 珠图案（个别帧宽 43；origin 21,21 居中，delay=300，
  a0=64→a1=255 脉动）。COMBO 值 N 含未激活中心珠，环绕珠数 = N-1（基础最多 5，
  进阶最多 10，进阶复用这 5 种图案循环）。
- **毒/受击特效缺口**：DAMAGE_PLAYER(0xC0=192) 未在 PacketSwitch 注册
  （他人受击/自己 DoT 掉血无数字）；怪物 POISON/FREEZE 状态掩码已存于
  `Mob::status_first_mask` 但无变色；玩家 POISON buff 已存无变色；
  SHOW_FOREIGN_EFFECT 的 effect 0(升级)/8(转职) 落在 `// todo`；
  GIVE_FOREIGN_BUFF 的 disease 段（每种病 [POISON 多一个 value short]+skillId
  +skillLevel 三个 short）未解析，旧按位单 short 读取必然错位。

## 变更

### 终极追加攻击（客户端发起的第二次攻击）

- 新增 `Gameplay/Combat/FinalAttack.h/.cpp`（namespace `final_attack`，自由函数）：
  - `find_skill(主技能, 技能簿, 武器类型)`：读 `skill/<img>/skill/<id>/finalAttack`
    子表（本数据集仅 1001005 横斩持有），按 **已学习等级>0 + 武器族匹配
    (1100002/1200003 剑、1100003 斧、1200002 锤、1300002 矛、1300003 枪) +
    FA 技能节点存在**（1100003/1200002/1300003 在本数据集缺失，自动过滤）筛选。
  - `roll(fa_skill, 技能簿)`：读 `level/<n>/prop` 掷百分比。
- `Gameplay/Combat/Combat`：攻击分支命中非空且 `move.is_skill() && type==CLOSE`
  时 `schedule_final_attack()` 掷prop；`update()` 中主挥砍动画结束
  （`!player.is_attacking() && player.can_attack()`）后 `perform_final_attack()`：
  镜像主攻击流程（prepare_attack→apply_useeffects→player.attack(false) 常规挥砍
  →set_afterimage(fa_id)→apply_stats→手动补 stance/range（终极非 attack 技能，
  apply_stats 不写这两项）→send_attack→extract_effects→AttackPacket.dispatch）。
  `clear()`（换图）重置。终极包本身不再链式触发终极。

### 斗气球渲染

- 新增 `Character/Effects/ComboOrbs.h/.cpp`（namespace `combo_orbs`，自由函数+
  纯数据 struct）：按进阶外观判定选 1111002/1120003 的
  state 节点惰性加载；`draw_aura` 在 look 之前画静态光环，`draw_orbs` 在残影之后
  画 N-1 颗珠。光环与环绕共用同一个中心 `absp + (0,-14)`，环绕是半径 34 的圆周，
  正好压在光环那一圈上。
- 珠子必须**真的绕人转**：角度相位取 `steady_clock` 毫秒数，
  `phase = now % 2400 / 2400 * 2π`，2.4s 顺时针转完一圈、均布在圆周上；
  前后半圈按 `sin(angle)` 判定，远半圈明暗系数 0.65，先画远半圈再画近半圈，
  于是珠子分别落在角色身后/身前。
- 每颗珠的帧时钟再按环绕序号错开：`local_time = now + duration * i / n`，
  按 delay 序列取帧（时间回绕）并插值 a0→a1 得到脉动透明度。因此同一时刻
  基础 5 珠展示的是 5 种不同图案，而非共用一帧；进阶 10 珠则循环复用这 5 种图案。
- 绘制保持无状态：整段 draw 只读时间戳与帧数据，不修改任何状态。
- 进阶外观判定改为 `skilllevel(1120003) > 0 || value > 6`——服务端
  `GIVE_BUFF` 的 source 恒为 1111002，单看 source 永远进不了进阶分支。
- `Character/Char.cpp`：存活分支挂 `draw_aura`/`draw_orbs`；颜色链在 is_hidden 后
  增加 POISON 分支 `{0.62, 1.0, 0.55, 1}`（自己与他人共用该虚接口路径）。

### 怪物状态变色

- 新增 `Gameplay/MapleMap/MobStatus.h`（header-only `mob_status::tint(first_mask)`：
  POISON(0x200) 绿、FREEZE(0x100) 蓝，掩码与服务端 MonsterStatus 一致）；
  `Mob.h` 补 `get_status_first_mask()`；`MobPresentation.cpp` 用全参 DrawArgument
  把 tint 与原有 flip/透明度合成进 Mob::draw。

### 斗气伤害镜像与终结技门控

- 新增 `Gameplay/Combat/ComboAttack.h/.cpp`（namespace `combo_attack`，自由函数）：
  `combo_factor` 逐行镜像服务端 `AbstractDealDamageHandler#parseDamage`：
  普通（value≤6）`(damage+50)/100 + orbs*(lv/6)/100`，进阶（value>6）
  `(advDamage+50)/100 + 0.20 + (value-5)*0.04`；终结技再按珠数
  ×1.2/×1.54/×2.0/×2.5（orbs 2/3/4/≥5）。lv30 理论值：普通 5 珠 2.75、
  10 珠 3.00，进阶 11 点 3.44，满珠 Coma 8.60。
- `Combat::apply_move` 与 `Combat::perform_final_attack` 在 `apply_stats` 之后
  调 `apply_damage`，本地预测的伤害范围与服务端校验值同源（1.5× 容差内）。
- `Player::can_use`：Coma/Panic 在 `COMBO ≤ 1` 时返回 `SpecialMove::FBR_NOCOMBO`
  （新增枚举 + `Messages::SKILL_NOCOMBO`），不再白扣 MP/冷却。
- 服务端权威性保持：客户端**绝不自增**珠数，只消费 `GIVE_BUFF/GIVE_FOREIGN_BUFF`
  下发的 value；重登/切频道回到 1 的行为与服端一致。

### 079 数据集缺口补全（Panic/Axe Coma）

- 079MAX4 的 `111.img` 只有 1111002/1111005，**缺 1111003/1111004/1111006**，
  而 v83 客户端两者都有。新增 `tools/nx/inject_skill_nx.py`：从
  `assets/Skill.nx.v83-backup` graft 1111003/1111004（attackCount/mobCount
  规整为 6/1）与 1111006（复制 1111005 结构），写回 `assets/Skill.nx`
  （+4.8MB，幂等 + 自动备份 `Skill.nx.pre-combo-backup`）。
- 新增 `tools/nx/emit_skill_xml.py`：从同一 NX 子树生成服务端
  `wz/Skill.wz/111.img.xml`（69KB→154KB），保证客户端/服端数值同源；
  `docker restart ms-game-server` 后加载 0.372s 正常，Panic/Coma 可用。

### 终极命中判定框修复

- 首版 `perform_final_attack` 把技能自带的 ±300 判定框覆盖成武器残影的
  15px 窄条，`MapMobs::send_attack` 一只怪都接不上（attached=0）→ 出伤 0。
  删除覆盖后沿用 `Skill::apply_stats` 的判定框，实测 `attached=1 lines=1`。

### 网络（受击数字 / 升级转职特效 / 疾病解析）

- 新增 `DamagePlayerHandler`（DAMAGE_PLAYER=192，注册进 PacketSwitch）：
  只解析 cid/skill/damage（-4 时 DoT 尾随 damage 不再读）；自己仅 skill==-4
  显示（接触伤害已由本地预测显示，避免双数字），他人全类型经
  `chars.get_char(cid)->show_damage` 显示。
- `ShowForeignEffectHandler`：`// todo` 分支补 effect 0→LEVELUP、8→JOBCHANGE。
- `GiveForeignBuffHandler`：second mask 改用 `buff_mask::ordered_ids` 按位序解析，
  disease 位（SLOW/SEDUCE/ZOMBIFY/CONFUSE/STUN/POISON/SEAL/DARKNESS/WEAKEN/CURSE）
  按三 short 布局（POISON 额外前置 value short）读入 visual buffs，其余位保持
  单 value short（COMBO/WK_CHARGE 不变）；`CancelForeignBuffHandler` 同步取消全部
  disease 位。他人中毒即可见绿色 tint。

## 验证

- 新增 `tools/verify/final-attack-combo-nx.cpp`（登记于 tools/verify/index.md，
  58 项全过）：1001005 finalAttack 触发表含 6 变体；1100002/1200003/1300002
  存在且 lv1 prop=2/damage>100/无 attackCount/mobCount（默认 1）、lv30 prop=60/
  damage≥350/2 击 8 怪；1100003/1200002/1300003 缺失回归；1111002/1120003
  state 六帧位图、帧 0 无 delay（静态光环）、帧 1-5 带 delay/a0/a1；补全后的
  1111003/1111004（6 击单体 + prop）与 1111006（8 击 15 怪、hpCon 25）在位。
- `bash scripts/docker_build_wasm.sh` 全量编译链接通过；
  `bash scripts/game_server_restart.sh` 6/6 端口监听完整。
- 新增 `tools/verify/maplestory-webui/hero-combo-e2e.mjs`（CDP 全链路）：
  登录 test1Hero → 40001 蜗牛图 → 依次验证 give 1→3→5→7→9→11 的珠数、
  倍率 2.50/2.60/2.70/3.28/3.36/3.44、Coma 满珠 8.60×、终结后回到 1、
  `finisher blocked value=1`、Panic(1111003) 3.12×、终极 `attached=1 lines=1`；
  38 张截图、0 浏览器错误。
- 调试探针（Combat/Player/ComboAttack 的 printf）在收尾时全部移除并重建，
  重跑同一 e2e：38 张截图、0 错误、全相位通过。
- 运行时手测路径：剑客带 1100002 学习等级，用 1001005 砍怪 → 按等级 prop 概率
  在挥砍结束后自动追加一次终极挥砍（服务端 mobCount=15 特判通道）；三转 1111002
  每次命中 giveBuff(COMBO,n) → 脚下光环 + N-1 颗脉动橙珠；怪物中毒/冰冻变绿/蓝，
  玩家中毒角色泛绿；他人受击/升级/转职特效可见。

## 未做（后续项）

- 怪物 STUN/SEAL 状态仅有掩码无图标/动作表现；玩家疾病状态图标（STUN 眩晕星等）
  与 `BasicEff.img` 数字集无 NoGreen（怪物毒 tick 保持红色数字，与原版一致）。
- WK 蓄力系 1211004/1211006/1211008/1221004 白名单缺口（attack_skills.json 列为
  攻击技但 flags_of 未登记）为另一独立问题，未在本条目混入。
