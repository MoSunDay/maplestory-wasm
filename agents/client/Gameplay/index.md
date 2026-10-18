Commit: e683a36c8ac98d8f13d5a15928b91366c126879d

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
- `find_player_target()`: 在 NPC 优先的前提下，把屏幕坐标和摄像机偏移交给纯命中策略选择远端玩家

Stage 内部组合了所有地图实体管理器。

### 地图实体管理器

| 类 | 职责 |
|------|------|
| `MapInfo` | 地图元数据（ID、名称、BGM、边界、复活点） |
| `MapTilesObjs` | 图块和静态对象渲染 |
| `MapBackgrounds` | 背景层渲染 |
| `MapPortals` | 传送门管理 |
| `MapChars` | 其他玩家角色管理（不含自身），向 UI 提供当前可见玩家的点击目标 |
| `MapMobs` | 怪物管理（生成、控制权、仇恨目标、攻击/技能动作、状态） |
| `MapNpcs` | NPC 管理（对话、商店） |
| `MapReactors` | 反应器管理（可交互物体） |
| `MapDrops` | 掉落物管理（物品、Meso），区分可拾取、拾取动画与已移除状态，并以服务端移除结论收敛对象生命周期 |
| `MapEffect` | 地图级特效（如天气效果） |
| `MapMists` | 服务端雾对象的生命周期与区域渲染 |

### 怪物仇恨战斗 (`MapleMap/Mob*`, `MobCombat/`)

- `MobAction` 是无运行时依赖的纯决策模块：技能建议优先于普通攻击，其次依据范围、飞行/跳跃能力选择追击、跳跃或待机；矩形和圆形范围统一在朝向局部坐标中判断。
- `Mob` 只在本客户端拥有控制权且服务端确认有仇恨时驱动 AI。地面怪水平追击并在目标位于可达高处时跳跃，飞行怪同时调整水平和垂直方向；动作期间停止主动位移。
- `MobActions.cpp` 从 `Mob.nx` 加载 `attack1..attack9`、技能绑定、动画、范围、`effectAfter` 与 `attackAfter`。缺动画或范围的声明会明确报错并禁用，不用默认动作掩盖素材缺口。
- 普通攻击在 `effectAfter` 到达时重新检查目标范围，并只产生一次本地受击意图；无敌帧仍会消费该命中，避免动作结束后延迟补伤害。远端客户端依据服务端广播播放相同动作。
- linked server 是动作权威：累计伤害决定控制者，平局保留当前目标；移动序列、控制者、WZ 朝向/范围、冷却、MP、忙碌窗口和一次性技能建议均在服务端校验。客户端预测只负责表现，拒绝的动作不能建立有效命中窗口。
- 怪物 WATK/MATK、防御、回避、速度、免疫与硬皮状态参与客户端表现计算；实际玩家伤害、反伤、技能疾病和技能效果仍以服务端结论为准。

### 宠物自动化 (`Pet/`)

- `PetAutomation::Controller` 以无 UI、无网络依赖的纯状态机判断 HP/MP 阈值、能力门禁、单次药水请求和冷却；阈值固定钳制到 1%–99%。
- `PetCapabilityPolicy` 合并当前宠物物品 ID 与已装备能力；Brown Puppy（`5000001`）内建金币磁铁、物品袋和飞行靴，自动 HP/MP 与魔法秤仍只由装备提供。
- `PetFollowPolicy` 以启停迟滞、300px 二维主人安全绳和 150px 拾取可达高度输出待机、左右移动、回主人身边或放弃拾取目标；地面移动力为 `0.16`。共享 `Physics::MoveResult` 返回当帧真实水平碰撞，纯 `should_jump` 只在有水平移动意图、仍在地面、实际撞墙且没有沿意图方向前进时允许定向跳跃；斜坡、终端边缘和距离停滞均不触发跳跃。不可达或连续三次真实受阻跳跃失败的掉落不会成为传送目的地。`PetLootPolicy` 把服务端 400ms 新掉落保护期、拾取重试间隔和最多三次尝试集中为纯时序规则。
- `PetSettingsStore` 在 WASM 下把 HP/MP 阈值保存到浏览器本地存储，读取失败回到 50%，写入失败由调用方直接提示。
- `Stage` 只驱动已召唤的首只宠物：在主人 120px 范围内选择有权限且匹配金币磁铁/物品袋能力的最近掉落，设置追踪目标并限频同步宠物移动；主人侧传送会跳过常规限频立即同步，且移动同步先于同帧拾取请求。`PetLook` 放弃目标或服务端已把目标置为不可拾取后，Stage 同帧清除对应掉落 OID。发出拾取或自动补给请求前仍以当前背包、解析后的能力和角色状态重新判断。
- `Drop` 的过期、玩家拾取、宠物拾取与立即删除由 `DropLifecycle/RemovalPolicy` 统一映射。玩家/宠物拾取先把对象置为不可再次选取，再播放 48 帧淡出；宠物位置缺失时仍原地淡出并最终从 `MapObjects` 擦除。立即删除模式直接移除容器项，不留下失活幽灵对象。

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
→ MapMobs::update()
→ MapChars::update()
→ Player::update()
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

### 宠物拾取与自动补给

```
Stage::update_pet_automation()
→ PetAutomation::Controller 判断单次 HP/MP 请求
→ MapDrops 按掉落类型、归属与宠物能力选择目标
→ PetFollowPolicy 驱动 PetLook 正常速度跟随、放弃不可达掉落或回到主人身边
→ Stage 先同步移动，再按 PetLootPolicy 等待保护期并限次重试拾取
→ linked server 复核宠物身份、距离、归属、能力与实时库存后执行
→ 客户端按服务端 mode 5 回包结束目标 pending，播放宠物拾取淡出并擦除精确掉落 OID
```

## 依赖关系

- **内部依赖**: [角色系统](../Character/index.md) (Player), [图形渲染](../Graphics/index.md) (渲染), [UI 系统](../IO/index.md) (输入)
- **外部依赖**: `link_repos/MapleStory-Server` 服务端（仇恨控制权、动作授权、MP/冷却、命中窗口、状态与雾同步）
