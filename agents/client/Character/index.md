Commit: e683a36c8ac98d8f13d5a15928b91366c126879d

# 角色系统

## 职责

管理玩家角色的所有数据和状态：属性、装备、技能、外观、Buff。`Player` 是角色系统的聚合根，继承自 `Char` 基类并实现 `Playable` 接口。

## 边界

- **包含**: 角色属性/状态、装备系统、技能管理、Buff 管理、角色外观、角色死亡效果、任务日志、怪物书、瞬移石
- **不包含**: 渲染逻辑、物理计算、网络通信

## 关键抽象

### Player (`Player.h`)

玩家角色类，组合以下子系统:
- `CharStats`: 角色属性（力量、敏捷、智力、运气、HP、MP 等）
- `Inventory`: 物品栏（装备/消耗/其他）
- `Skillbook`: 技能书
- `Questlog`: 任务日志
- `Telerock`: 瞬移石（收藏地图）
- `Monsterbook`: 怪物书
- `ActiveBuffs` / `PassiveBuffs`: Buff 管理系统

Player 继承 `Playable`（可操控）和 `Char`（角色基类）。

`JobProgression` 以纯函数统一 Explorer、Cygnus、Aran 的新手到四转阶段、各阶段职业 ID、新手技能族和主副属性原型。`Job` 与技能窗口共用该映射，因此当前角色可切换所有已经到达的技能页签，尚未转职的页签保持禁用。

`reset_progress(StatsEntry)` 在服务端发送完整角色快照时重建属性、物品栏、技能、任务、瞬移石、怪物书、Buff 和冷却，避免商城回服后继续使用旧进度数据。`Inventory` 对现金物品额外保留服务端 `cash_id`，供商城仓库转移稳定定位；已装备现金物品不会直接暴露给仓库转移入口。

### Char (`Char.h`)

角色基类，定义:
- 角色状态枚举 (`State`): `WALK`, `STAND`, `FALL`, `PRONE`, `LADDER`, `ROPE`, `SIT`, `DIED` 等
- 外观渲染入口
- 动画更新
- 碰撞边界

进入 `DIED` 时，`Char` 启动 `DeathTomb` 并切换到 Character 资源的 `dead` 姿态。墓碑以死亡位置正下方最近的 foothold 为固定锚点，先播放 `Effect.nx/Tomb.img/fall`，落地后持续绘制 `land/0`；幽灵在墓碑落地后沿顺时针椭圆轨迹循环，并根据前后半圈切换与墓碑的绘制顺序。离开死亡状态时同时清除墓碑和环绕状态。该流程由 `Char` 统一持有，因此本地玩家和其他玩家采用相同表现；本地玩家仅在落地后额外显示回城确认框。

供 `Player` 和 `OtherChar` 继承。

### CharStats (`CharStats.h`)

角色属性容器。管理:
- 基础属性和总属性（装备+Buff 加成）
- `recalc_stats(equipchanged)`: 重新计算总属性
- 过期 HP/MP 跟踪
- 怪物物理/魔法攻击分别使用 WDEF/MDEF 计算受击预测，防御为零时保持安全下限；服务端仍负责最终 HP/MP 结算。

### 怪物疾病与 ActiveBuffs

- `SLOW` 和 `DARKNESS` 分别进入速度、命中属性重算；其余疾病由 `Stage` 或 linked server 执行行为约束。
- `STUN`/`SEDUCE` 阻止动作和技能，`SEAL` 阻止技能，`CONFUSE` 反转左右输入，`WEAKEN` 阻止跳跃，`FEAR` 缩小可见区域。混乱方向在 key-down 时锁存到对应 key-up，疾病中途解除不会抬起错误方向；切图会释放并清空持续动作状态。
- `POTION_LOCK` 由 linked server 同时拦截恢复/净化消耗品与宠物自动补给；`SHADOW` 由服务端按每个完整静止秒造成伤害；`ZOMBIFY`、`CURSE` 和既有疾病沿用服务端规则。
- 新疾病追加在服务端 `MapleDisease` 枚举末尾，避免改变持久化 ordinal；客户端 Buff 掩码与服务端 first/second mask 保持一致。

### NaturalRecovery (`Recovery/NaturalRecovery.h`)

无副作用的自然恢复规则模块。`Player::update` 将当前姿态、HP/MP、等级、地图 `recovery` 倍率、技能 NX 数值和椅子 NX 数值组装为 `natural_recovery::Context`，按固定帧推进计时状态，再把产生的 HP/MP 增量交给网络封包发送。站立和坐下可恢复 HP，攀爬仅在 Endure 有等级时恢复 HP；存活状态持续计算 MP。技能与椅子只改变单次恢复量或攀爬间隔，HP 发送值按 linked server 的地图倍率动态校验上限约束，MP 发送值限制为 999。

### JobProgression (`JobProgression.h`)

无 UI 依赖的职业映射模块统一解析冒险家、骑士团和战神的转职阶段、新手职业、阶段职业 ID、新手技能族与主副属性职业原型。骑士团职业去除 1000 前缀后按冒险家原型取属性；战神 2000–2112 归入战士原型，避免用 `jobId / 100` 直接计算时落入未知分支。

### CharLook (`Look/`)

角色外观系统。根据装备、发型、脸型组合渲染角色外观:
- `Body`: 身体各部位图层
- `Clothing`: 装备外观
- `Hair` / `Face`: 发型/脸型
- `Stance`: 姿态
- `Afterimage`: 残影效果
- `PetLook`: 宠物外观

`Afterimage` 按当前攻击的精确姿态选择 Character/Skill NX 残影；武器等级超过素材最高档时，回退到仍包含该姿态的最近低档，不会用其他动作掩盖素材缺口。地图进入安全门会预热当前武器全部可能的普通攻击姿态、装备图层和残影，并在 `Char` 中保留每种姿态的未播放模板，普通攻击复制后显式重置动画、触发阶段和呈现状态，不重复遍历可能被 LRU 淘汰的 NX 元数据。运行时残影走瞬态优先加载，不冻结玩法；姿态更新后同时观察前后身体帧锁存终止帧触发，单帧动画只有在非零透明贴图实际提交渲染后才允许推进。本地玩家与其他角色均经 `Char` 共用这套生命周期。

`Clothing` 对 Base `zmap.img` 定义的 16 个手套/护腕层逐一建模，包括位于身体背面的 `backGlove` 与 `backGloveWrist`；各部件按自己的 `z` 标签和身体、头部、衣袖、武器、头发的相对顺序绘制。Character NX 若出现未建模的手套层，会记录装备、部件和图层警告并只跳过该位图，不会退回默认层造成重影，也不会终止整个角色外观构造。

`CharLook`、`Body`、`Hair`、`Face` 和 `Clothing` 可按后台或延迟模式建立外观。角色选择按帧建立延迟预览时只保留位图引用，绘制到的当前姿态再请求可见贴图；若同一缓存对象随后被游戏角色以默认后台模式复用，会显式补入全部后台位图，避免“首次由预览建立缓存”改变其他场景的既有预取语义。

`PetLook` 在构造完成动画表后初始化姿态，并在远端姿态变化时重置对应动画。地面跟随消费 `PetFollowPolicy` 的启停迟滞，水平力固定为普通角色满基础速度对应的 `0.16`；待机和传送会清空残留速度/加速度。普通移动不设置 `TURNATEDGES`，通过人物共用的 foothold 更新贴合连续斜坡，并像人物一样从平台末端自然下落；只有当次水平移动实际被墙体限制且完全没有前进时，才在下一物理帧定向跳跃。宠物与主人二维距离只有大于 300px 时才传送到主人左侧 35px；150px 垂直差仅用于拾取路径可达性，连续三次真实受阻跳跃失败的拾取目标会被放弃，绝不传送到掉落物坐标。地面与飞行宠物共用主人安全绳，传送会标记一次紧急移动同步。

## 子目录

| 目录 | 职责 |
|------|------|
| `Effects/` | 角色状态特效（死亡墓碑的下落、落地、幽灵环绕和清除状态） |
| `Inventory/` | 物品栏系统 (Inventory, Equip, Item, Pet, Weapon) |
| `Look/` | 角色外观渲染 (CharLook, Body, Clothing, Hair, Face, Stance, PetLook, Afterimage) |
| `Recovery/` | HP/MP 自然恢复的纯函数规则、周期状态和服务端安全上限 |

## 依赖关系

- **内部依赖**: [游戏世界](../Gameplay/index.md) (Physics, Playable 接口), [图形渲染](../Graphics/index.md) (渲染), Data 模块 (物品/技能静态数据)
- **外部依赖**: `link_repos/MapleStory-Server` 服务端 (属性/装备/技能同步)
