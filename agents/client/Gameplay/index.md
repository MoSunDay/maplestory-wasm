# 游戏世界

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

管理游戏内地图、实体、物理和战斗系统。`Stage` 是游戏运行时核心，协调所有地图实体和游戏逻辑。

## 边界

- **包含**: 地图加载/渲染、角色/怪物/NPC 管理、物理碰撞、战斗执行、掉落物、传送门、反应器
- **不包含**: UI 元素、网络通信、角色属性系统

## 关键抽象

### Stage (`Stage.h`)

游戏阶段单例，管理整个游戏运行时。状态机:
- `INACTIVE`: 未进入游戏
- `TRANSITION`: 过场/地图切换中
- `ACTIVE`: 正常游戏

核心职责:
- `load(mapid, portalid)`: 切换地图
- `loadplayer(entry)`: 从角色数据构造玩家
- `update()`: 每帧更新所有实体
- `draw(alpha)`: 渲染所有实体
- `send_key()`: 分发键盘输入

Stage 内部组合了所有地图实体管理器。

### 地图实体管理器

| 类 | 职责 |
|------|------|
| `MapInfo` | 地图元数据（ID、名称、BGM、边界、复活点） |
| `MapTilesObjs` | 图块和静态对象渲染 |
| `MapBackgrounds` | 背景层渲染 |
| `MapPortals` | 传送门管理 |
| `MapChars` | 其他玩家角色管理（不含自身） |
| `MapMobs` | 怪物管理（生成、移动、状态） |
| `MapNpcs` | NPC 管理（对话、商店） |
| `MapReactors` | 反应器管理（可交互物体） |
| `MapDrops` | 掉落物管理（物品、Meso） |
| `MapEffect` | 地图级特效（如天气效果） |

### Physics (`Physics.h`)

物理引擎。管理:
- `FootholdTree`: 地形的空间索引（四叉树）
- `Foothold`: 单条平台/地面碰撞检测
- 角色与平台之间的重力、移动、跳跃计算

### Combat (`Combat/`)

战斗系统。包含:
- `Combat`: 战斗协调器
- `Attack` / `RegularAttack`: 攻击定义
- `Skill` / `SkillAction` / `SkillBullet`: 技能执行
- `DamageNumber`: 伤害数字显示
- `Bullet`: 弹道投射物

## 主要流程

### 进入游戏
```
SetfieldHandlers → Stage::load(mapid, portalid) → MapInfo::load()
→ MapTilesObjs, MapBackgrounds, MapPortals 加载
→ Stage::loadplayer() → Player 构造
```

### 每帧更新
```
Journey::update()
→ Stage::update()
→ Physics::move() (玩家物理)
→ Player::update()
→ MapMobs::update()
→ MapChars::update()
→ ...
```

### 地图切换
```
Stage::load(new_mapid)
→ Stage::clear()
→ load_map(new_mapid)
→ respawn(portalid)
```

## 依赖关系

- **内部依赖**: [角色系统](../Character/index.md) (Player), [图形渲染](../Graphics/index.md) (渲染), [UI 系统](../IO/index.md) (输入)
- **外部依赖**: Cosmic 服务端 (实体状态同步)
