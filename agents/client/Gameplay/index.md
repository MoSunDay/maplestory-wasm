Commit: c2cd3682d8a2cf5216e4be0890c6f1712e81d031

# 游戏世界

## 职责

管理游戏内地图、实体、物理和战斗系统。`Stage` 是游戏运行时核心，协调所有地图实体和游戏逻辑。

## 边界

- **包含**: 地图加载/渲染、角色/怪物/NPC 管理、物理碰撞、战斗执行、掉落物、传送门、反应器
- **不包含**: UI 元素、网络通信、角色属性系统

## 关键抽象

### Stage (`Stage.h`)

游戏阶段单例，管理整个游戏运行时。状态机:
- `INACTIVE`: 未进入游戏
- `LOADING`: 已构造地图但素材安全门尚未释放；只实例化服务端初始实体，不推进物理、AI、战斗或碰撞
- `ACTIVE`: 正常游戏

核心职责:
- `load(mapid, portalid, onready)`: 切换地图；当前地图素材、当前武器全部普通攻击姿态和可复用残影模板已准备后调用 `onready`
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
- 攻击输入按按键按下沿触发；固定帧循环只维持跳跃等持续动作，不重复发起攻击。
- 技能特效按 NX 的施放层、命中段/目标、角色等级、斗气层数和属性充能状态选择；远端角色通过可见 Buff 封包保持相同选择。
- 输入边沿和特效分支判断位于无运行时状态的纯函数模块，NX 读取与动画播放只消费判断结果。
- 普通攻击姿态集合由纯函数模块统一供随机选姿和地图预热消费；攻击残影与动态掉落物采用瞬态优先加载，素材冷读不暂停怪物、物理或战斗更新。

## 主要流程

### 进入游戏
```
SetfieldHandlers → Stage::load(mapid, portalid) → MapInfo::load()
→ 预热当前武器攻击姿态/残影 → 构造地图/实体并异步预取全图 NX 范围 → 可见静态动画全帧进入图集
→ 250ms 资源集合稳定窗 → 发送 PLAYER_UPDATE、启用输入并进入 ACTIVE
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
→ LOADING 安全门（服务端仍保持 mapTransitioning）
→ PLAYER_UPDATE → ACTIVE
```

## 依赖关系

- **内部依赖**: [角色系统](../Character/index.md) (Player), [图形渲染](../Graphics/index.md) (渲染), [UI 系统](../IO/index.md) (输入)
- **外部依赖**: `link_repos/MapleStory-Server` 服务端 (实体状态同步)
