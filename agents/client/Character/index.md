# 角色系统

Commit: c8a11b8a07579923276950416e6c11a68ada35a2

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

### Char (`Char.h`)

角色基类，定义:
- 角色状态枚举 (`State`): `WALK`, `STAND`, `FALL`, `PRONE`, `LADDER`, `ROPE`, `SIT`, `DIED` 等
- 外观渲染入口
- 动画更新
- 碰撞边界

进入 `DIED` 时，`Char` 启动 `DeathTomb`：按顺序播放 `Effect.nx/Tomb.img/fall`，落地后持续绘制 `land/0`，离开死亡状态时清除。该流程由 `Char` 统一持有，因此本地玩家和其他玩家采用相同的墓碑表现；本地玩家仅在落地后额外显示回城确认框。

供 `Player` 和 `OtherChar` 继承。

### CharStats (`CharStats.h`)

角色属性容器。管理:
- 基础属性和总属性（装备+Buff 加成）
- `recalc_stats(equipchanged)`: 重新计算总属性
- 过期 HP/MP 跟踪

### CharLook (`Look/`)

角色外观系统。根据装备、发型、脸型组合渲染角色外观:
- `Body`: 身体各部位图层
- `Clothing`: 装备外观
- `Hair` / `Face`: 发型/脸型
- `Stance`: 姿态
- `Afterimage`: 残影效果
- `PetLook`: 宠物外观

## 子目录

| 目录 | 职责 |
|------|------|
| `Effects/` | 角色状态特效（标准死亡墓碑的下落、落地和清除状态） |
| `Inventory/` | 物品栏系统 (Inventory, Equip, Item, Pet, Weapon) |
| `Look/` | 角色外观渲染 (CharLook, Body, Clothing, Hair, Face, Stance, PetLook, Afterimage) |

## 依赖关系

- **内部依赖**: [游戏世界](../Gameplay/index.md) (Physics, Playable 接口), [图形渲染](../Graphics/index.md) (渲染), Data 模块 (物品/技能静态数据)
- **外部依赖**: Cosmic 服务端 (属性/装备/技能同步)
