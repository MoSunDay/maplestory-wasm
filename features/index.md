Commit: b2922f3eef7e2afa16fe954cee477d1ca3831e00

# 功能能力索引

## 概述

MapleStory WASM 是一个运行于浏览器中的 MapleStory v83 客户端。所有功能通过 WebAssembly 在浏览器中实现，经由 WebSocket 代理与仓库 `link_repos/MapleStory-Server` 服务端通信。

## 功能分组

### 登录与认证

- **账号登录**: 用户名/密码认证
- **重复登录接管**: 同一账号再次用正确密码登录时，服务端强制断开该账号此前的在线会话（包括游戏内角色），由新会话接管；错误密码不会踢掉旧会话
- **世界选择**: 使用唯一的频道 1 入口进入服务端首个可用频道；确认按钮或双击频道均可继续到角色选择
- **角色创建/删除**: 创建新角色或删除已有角色；新建冒险家、骑士团、战神的 STR/DEX/INT/LUK 均初始化为 20
- **角色选择**: 选择角色进入游戏世界
- **PIC 认证**: 二次密码验证
- **游戏内账号注册**: 登录界面注册按钮打开 NX 原生风格表单；账号限 4–12 位 ASCII 字母数字，密码限 4–12 位非空白可打印 ASCII，确认一致后通过现有登录协议自动建号、接受 TOS 并继续登录

相关模块: [网络层](../agents/client/Net/index.md), [UI 系统](../agents/client/IO/index.md)

### 输入与本地化

- **[中文/Unicode 输入与渲染](input-localization/)**: CJK 回退字体与 UTF-8 排版；WASM 下由浏览器原生输入法处理候选，拼音 preedit 不进入游戏文本框，只同步选中后的最终文本，密码字段除外
- **[回车打开输入框不再带入多余空格](changelog/2026-10-25/enter-key-stray-space-in-textfield.md)**: 修复按 Enter 聚焦聊天输入框时浏览器默认动作向 IME 镜像 textarea 插入换行、渲染成多余空格的问题，单行字段一律拦截换行插入
- **[默认键位对齐官方 v83](changelog/2026-11-15/default-keymap-official-v83.md)**: 运行时预置与键位设置“默认”按钮共用的默认键表逐条对齐服务器 GameConstants 官方 v83 键表；修正 F4 空绑导致的表情键整体右移一位、C/S/B 错绑与 T/Y/V 错加大乱改动作，补回 S/F/O/数字6 的属性/家族/组队搜索/夫妻聊天官方绑定

### 游戏世界

- **地图加载与渲染**: 加载 .nx 地图数据，渲染背景、图块、对象；进入地图前阻塞到全图已实例化素材驻留、首屏与当前实体素材进入图集，期间服务端仍保持过图保护
- **角色移动**: 行走、跳跃、攀爬、游泳等物理移动
- **[水下世界游泳](changelog/2026-11-01/aqua-road-swim.md)**: swim 水图四方向游泳、跳跃键向上划水、水物理从落水首帧生效、移动封包用官方 swim 片段（command 5）；修复非水图坐姿按↑误入漂浮态、进水图预热水下动画
- **[雪地滑行](changelog/2026-11-01/snow-map-slide.md)**: fs 雪图未穿防滑鞋松键后惯性滑行一小段、行走速度与普通地图不变，斜坡顺势下滑保留；穿防滑鞋完全免疫
- **[冰封雪域滑动 1:1 修正](changelog/2026-11-08/snow-slide-official-fs.md)**: 按官方语义重写 fs 物理——fs(0.2) 缩放整个地面摩擦（松键滑行 1.5 倍距离）、新增斜坡恒定下滑力（静止下溜/上坡滑回/坡上站不住），滑行全程保持走路姿态；行走步速与防滑鞋免疫不变
- **[同图人物移动片段回放与重复刷新](changelog/2026-11-03/char-remote-movement-playback.md)**: 远端玩家位置同步——MOVE_PLAYER 整包片段复用 mobplayback 内核按 duration 线性回放（位置直写 phobj、力清零、stance 随片段切换、耗尽保持末态），重复 SPAWN_PLAYER 经 respawn(position,stance) 幂等重锚不再被空 if 丢弃
- **[同图远端表情与攻击动作同步](changelog/2026-11-06/remote-action-sync.md)**: FACE1..FACE7（默认 F1/F2/F3/F5/F6/F7/F8）表情走 v83 FACE_EXPRESSION(0x33) 上行、FACIAL_EXPRESSION(193) 下行，服务端 5 秒回退默认；远端攻击广播不再额外延迟 400ms，挥砍与命中特效同步
- **[带宠物远端角色可见性与怪物位置/动作同步](changelog/2026-11-06/remote-visibility-mob-sync.md)**: SPAWN_PLAYER 全量解析（宠物块无 byte2、尾 0 终止、条件 buff/ring/announce box/new-year card 跳过），修复带宠物角色整包丢弃导致的「隐身」；怪移动片段带上一网络锚点+实际耗时（钳制 1..1000ms）、观察端 y+2、重复生成 respawn 恢复位置/姿态/foothold、击杀动画直进死亡，双账号 27 怪位置/动作一致且残余 ≤15px 有界
- **[远端宠物可见性与角色待实例化队列同步](changelog/2026-11-08/remote-pets-and-chars-queue-desync.md)**: MapChars remove/clear 补清 spawns 待实例化队列与暂存宠物表（换图跨图幽灵、同批 SPAWN+REMOVE 永久幽灵根除）、SPAWN_PLAYER 宠物块解析后直挂 OtherChar 三槽（对方进图自带宠物首帧可见）、SPAWN_PET 在 owner 未实例化时按 cid 暂存重放、respawn 复位攻击门控；双客户端 e2e 4 场景（基础可见/召唤竞态/幽灵竞态/协作回归）验收通过
- **[迟到角色事件暂存重放 + 坠落伤害垫层 + SHOW_FOREIGN_EFFECT 分派](changelog/2026-11-14/remote-late-event-stash-and-damage-effect-parsing.md)**: MOVE_PLAYER/MOVE_PET/UPDATE_CHARLOOK/FACIAL_EXPRESSION/SHOW_CHAIR 在 cid 未实例化时按 cid 暂存（仅 pending spawn、同类留最新、每 cid 封顶 4 条）实例化时按序重放；DAMAGE_PLAYER skill==-3 跳 4 字节垫层（坠落伤害不再读成 0）；SHOW_FOREIGN_EFFECT 按 effect 值精确分派（0/8/10/11/13/15/16 各自载荷、skill 变体仅 1/2/5），裸包不再被 length() 探测错读
- **[移动解析逐命令对齐服务端字节表](changelog/2026-11-14/movement-parser-alignment-and-fragment-replay.md)**: MovementParser 按服务端字节表逐命令解析（绝对 0/5/17、相对 1/2/6/12/13/16/18/19/20/22、瞬移 3/4/7/8/9/11 无 duration、case15 fh/origin-fh 字段序、14/21/10 读后丢弃）；Movement 增 TELEPORT 类型、MobMovementPlayback 支持 0 时长即达 waypoint 与 NONE 末尾状态；本地玩家移动发包携带真实耗时（钳制 8..250ms，新 PlayerMovement.h），远端按发送端节奏插值
- **[远端宠物移动片段插值回放](changelog/2026-11-14/remote-pet-movement-playback.md)**: MOVE_PET 首个定位片段直接落地锚定、其余片段复用 mobplayback 按 duration 线性插值（回放期间本地跟随冻结、片段 newstate 拥有 stance），耗尽停末点并回落本地跟随；Char::move_pet 整包透传
- **[召唤兽同步 + 外显 BUFF/粉笔板/椅子/空中出生落地](changelog/2026-11-14/summons-foreign-buffs-and-cosmetics.md)**: SPAWN/REMOVE/MOVE/ATTACK/DAMAGE_SUMMON 与 SPAWN_DRAGON 六类封包落地为远端召唤兽实体（STATIONARY/FOLLOW/CIRCLE_FOLLOW 移动策略、动画节点按技能解析、缺失节点保持逻辑同步但不渲染）；DARKSIGHT/SHADOWPARTNER 外显、GIVE/CANCEL_FOREIGN_BUFF 疾病位宽修正、粉笔板与椅子同步、空中出生按地面落地
- **[多人同步收尾：charge 回退 + 召唤迟到 owner 暂存 + 未知命令截断](changelog/2026-11-14/multiplayer-closeout-charge-revert-late-owner-parser-hardening.md)**: 远端攻击 charge 不再作为排队延迟（回到 2026-11-06 即时语义，字段解析保留）；召唤兽 SPAWN/MOVE 在 owner 仍待实例化时按 cid 暂存（同类同 oid 覆盖、封顶 4、实例化时重放、离图/换图清理、REMOVE 按 oid 取消暂存避免幽灵召唤兽）；MovementParser 未知 command 警告并整包截断，对齐服务端 return null；10 项快测 + e2e 4/4 + social 三模式全绿
- **[同图幽灵怪/画面不同步/表情卡闭眼修复](changelog/2026-11-06/ghost-mob-desync-and-expression-stuck.md)**: 切图清空待实例化 spawns 队列、remove 同步过滤同 oid 队列（同批次 SPAWN+KILL 不再丢 KILL）、respawn 复位 dying/dead/fading/hppercent（复活体可选中）；CharLook 动画/表情计时钳制无符号下溢（表情不再卡闭眼帧）；linked server killMonster 击杀广播提前到 removeKilledMonsterObject 之后立即执行（服务端 patch 0017），结算异常不再产生打不死的服务端幽灵怪
- **[角色自然恢复](character-recovery/)**: 按 v83 站立、攀爬、技能、地图倍率和椅子规则恢复 HP/MP，并与 linked server 的恢复校验及椅子状态同步
- **传送门**: 地图间瞬移
- **NPC 交互**: 对话、商店、仓库等
- **[自由市场入口 BGM 修复](changelog/2026-10-27/fm-map-info-fallback-bgm.md)**: /gohome 回自由市场后 BGM 不刷新——079 注入的 910000000.img 属性平铺无 info 包装，MapInfo 回退平铺读取，恢复 BGM/VR 边界/town 等字段
- **[自由市场地图名恢复](changelog/2026-10-26/fm-map-name-restore.md)**: 910000000 街道名/地图名恢复原版「自由市场/自由市场入口」，注入管线加覆盖防回归，小地图实显取证通过
- **[自由市场玩法向导 NPC](changelog/2026-10-25/fm-gameplay-guide-npcs.md)**: 910000000 内的 PQ 招募/BOSS 入口向导（9010000/9010005），一键传送到 13 个组队任务招募点与 12 个 BOSS 入口，危险地图先二次确认
- **[自由市场科皮药水价格](changelog/2026-10-25/fm-kopee-potion-prices.md)**: 超级药水 5 万、特殊药水 2 万（shopid 1339 shopitems，重启生效）
- **[自由市场科皮商店换货](changelog/2026-11-01/fm-kopee-shop-pet-food.md)**: 下架射手村回城券（2030004），上架宠物饲料 2120000（200 金）、棒棒冰 2001001（2300 金，HP+2000）与红豆刨冰 2001002（4000 金，MP+2000），货架 32→34 项（shopid 1339 shopitems，改后需重启服务端）
- **[自由市场世界旅行 NPC 与佩里海外目的地](changelog/2026-11-14/fm-world-tour-npcs-and-perry-destinations.md)**: 910000000 移除导游妮妮（9000020）与工作人员G（9000021），life 重编号 0..6；佩里 9110000「东方都市与海外」补齐泰国水上市场 500000000、上海外滩 701000000、嵩山镇 702000000、大雄宝殿 702100000（分类共 13 项），六目的地（射手村/蚂蚁广场/泰国/上海/嵩山/大雄宝殿）浏览器 e2e 全部落图通过
- **[场景机关/进度横幅/BGM 切换](changelog/2026-10-25/field-obstacles-progress-banner.md)**: 服务端可开/关/批量/重置具名地图机关对象（OrbisPQ 陷阱、CWKPQ 石阵等），下发 PQ 进度横幅并运行时切换 BGM
- **反应器**: 可交互物体（如采集物）
- **掉落物**: 物品/meso 的拾取
- **小地图/世界地图**: 地图导航
- **[东方神州与泰国地图素材](changelog/2026-11-14/orient-thailand-maps-and-canvas-format.md)**: 079MAX4 年包补齐 37 张海外地图（泰国 500xxx 9 张、上海 701 16 张、嵩山 702 12 张）与 Tile/ShanghaiNJL、Tile/thai、Back/YuyuanCN 三个 library、63 个 NPC 与 13 个 Mob；客户端 Map/Npc/Mob/String.nx 与服务端 wz 双侧落地，破解 2023 新 canvas 载荷后 miniMap/画布逐像素校验一致，进图 e2e 全部通过
- **[东方神州与泰国商店 NPC](changelog/2026-11-15/orient-shanghai-thai-shop-npcs.md)**: 上海（9310001 李女士武器/9310002 张老板防具/9310003 金女士杂货）与泰国水上市场（9250007 丹武器/9250008 帕恩杂货/9250009 诺伊防具）六家商店 NPC 落地，shops/shopitems 按 079 模板补 319 件商品（价格沿用、双侧物品存在性校验），shopid==npcid 命中 MapleShop 工厂；浏览器 e2e 六店全开、货架与首屏商品全部核验通过

相关模块: [游戏世界](../agents/client/Gameplay/index.md), [图形渲染](../agents/client/Graphics/index.md)

### 任务系统

- **[任务日志窗口](changelog/2026-10-25/quest-log-window.md)**: Q 键或状态栏菜单打开的 In Progress/Completed 双页任务列表（双代 UI.nx 素材自动适配），实时反映服务端任务状态：放弃、进度数据更新、完成与过期；任务相关的道具特效与状态提示同步接入
- **[任务系统接取/完成闭环](changelog/2026-11-08/quest-system-restore.md)**: 客户端驱动的任务全链路——QUEST_ACTION 1/2/4/5 出站封包（接取/完成/脚本接取/脚本交付）、Check.img 可用性判定（lvmin/前置/可重复/击杀与物品进度）、Say.img 任务对话滚动框（文本已从服务端 wz XML 回填，15,103 节点）、无脚本 NPC 的 600ms 兜底触发（服务端脚本/商店优先零冲突）、任务完成链与失败原因的玩家可见提示
- **[任务系统加固](changelog/2026-11-14/quest-system-hardening.md)**: 任务链收尾——UPDATE_QUEST_INFO mode-8 双变体区分（接取不再误弹完成提示）、0x0A–0x0E 失败原因玩家可见、Check.img 全量判定（lvmin/lvmax/职业/meso/精确前置状态/重复冷却）、任务计时器与日志倒计时、选一奖励（Act.img prop=-1）图标格与 `selection` 上行、NPC conditionN 姿态/隐藏；服务端补丁 0020 补齐接取失败反馈/选择奖励 NPE/缺脚本降级/条件失败封包，并以 e2e 验证接取→选奖励完成→拒绝全链路
- **新版本地图数据源 gate**: 252/262/271-273/300-305 等新版本地图区段在现有 079 wz 数据源中不存在，注入整体暂缓（[结论记录](changelog/2026-10-25/new-era-maps-gate.md)），不阻塞任务 UI 与玩法枢纽

### 战斗系统

- **普通攻击**: 近战/远程攻击
- **技能使用**: 各职业技能施放
- **攻击特效**: 支持多段/多目标命中、斗气终结和属性充能特效；一次按键只发起一次攻击
- **伤害数字显示**: 伤害数值渲染
- **Buff 管理**: 增益/减益效果
- **[地图机关伤害](changelog/2026-11-01/map-obstacle-damage.md)**: 地刺/陷阱等地面机关恢复伤害判定——客户端从 Obj 定义的 `obstacle`/`damage` 字段收集机关足迹，玩家踏入即受固定伤害（地铁跳跳图地刺条、忍苦树林尖刺、椰子赛陷阱洞、CWKPQ 石柱等 224 个机关定义），`TAKE_DAMAGE -2` 上报，服务端不再丢弃无怪物来源的受击；迷宫喷气/蒸汽陷阱（trap.img/steam/0/*）定义无 `damage` 字段，按 `DEFAULT_OBSTACLE_DAMAGE=100` 落伤，足迹优先取帧 `lt`/`rb` 碰撞盒
- **[怪物仇恨战斗](mob-combat/)**: 怪物受击后按累计伤害锁定目标并持续追击，在范围内使用普通攻击或服务端授权技能，完整呈现状态、疾病和雾效果
- **[怪物远程片段回放](changelog/2026-11-03/mob-remote-fragment-playback.md)**: 非控制端怪物改为服务器移动片段回放——整批片段按 duration 线性插值、片段 0 从包内 start 重锚、stance/fh 随片段切换、队列耗尽保持末位静止，消灭收包硬拉与本地外推分叉造成的回跳/瞬移
- **[同图怪物血条广播与死亡贡献者经验](changelog/2026-11-03/mob-hp-broadcast-and-exp-share.md)**: linked server 非 Boss 怪 `SHOW_MONSTER_HP` 改为全图广播（非组队同图玩家血条同步）；`giveExpToCharacter` 去掉 `isAlive` 门槛，打怪途中死亡的伤害贡献者照常结算伤害经验（伤害 80%+击杀 20% 分配、击杀者队伍全额经验与防 leech 等级门槛不动；服务端 patch 0012）
- **[怪物控制权真空窗口接管](changelog/2026-11-03/mob-controller-vacuum-fix.md)**: 控制器离图/掉线到服务端重定向完成前的窗口内，同场上报者经 `aggroSwitchController` 立即接管怪物控制权（MoveLife 不再被丢弃返回 null），全场玩家画面里怪不再冻结；`MoveLifeHandler.inRangeInclusive` 恒真表达式修正后 rawActivity≥42 只归类技能，普攻/技能动作转发分类一致（服务端 patch 0013）

相关模块: [游戏世界](../agents/client/Gameplay/index.md), [角色系统](../agents/client/Character/index.md)

### 物品与装备

- **物品栏管理**: 消耗品、装备、其他物品；超过栏位上限（slotmax）的格子以半透明遮罩+X 叉锁定，不可点击/拖放（[2026-11-03 改动](changelog/2026-11-03/cash-cover-look.md)）
- **装备系统**: 穿戴/卸下装备，属性变更
- **商店交易**: NPC 商店购买/单件出售；出售数量超过当前堆叠时立即回填上限，可二次确认后一键售卖当前分类的全部非现金物品，现金物品不进入售卖列表
- **[玩家面对面交易](player-trade/)**: 点击同地图玩家发起双人交易，交换最多 9 格物品和金币，支持交易内聊天与双方确认
- **仓库存储**: 物品存储；自由市场 910000000 等 v83 Trunk 列表窗按 NX 真值落位，整理按钮与两侧物品图标不再错位（[2026-11-14 改动](changelog/2026-11-14/storage-legacy-buttons-and-icon-alignment.md)）
- **仓库/装备窗口落位**: v83 仓库的取出/存入/整理/关闭按钮上移到空置的羊皮纸头部带（y9..58，不再压底部纸张），装备窗口默认位下移 40px 到 (250,190)，开仓时不再压住页签条与 CAP 插槽（[2026-11-15 改动](changelog/2026-11-15/storage-header-buttons-and-equip-window-offset.md)）
- **[现金商城](cash-shop/)**: 查询真实 NX 余额和在售目录，购买单品/礼包，并在商城仓库与角色物品栏之间转移现金物品；现金装备取出落 CASH 栏、双击以纯装饰外观覆盖穿戴（不占真实装备槽、不影响属性；覆盖外观重登/他人视角保持与现金帽整帽 capOverHair 渲染已修复，见 [2026-11-03 改动](changelog/2026-11-03/cash-cover-look.md)），右栏可购买背包 +4 格扩容（[2026-10-29 改动](changelog/2026-10-29/cash-shop-decorative-equips-and-slotbuy.md)）
- **[永久新手宠物与自动化](pet-automation/)**: 每个角色获得永久 Brown Puppy，宠物也可在现金商城购买；任意召唤的宠物内建磁铁/物品袋/自动 HP/MP 药水袋能力，可自动拾取并按本地可配置阈值使用角色选择的 HP/MP 药水

相关模块: [角色系统](../agents/client/Character/index.md), [UI 系统](../agents/client/IO/index.md)

### 技能系统

- **技能书管理**: 技能学习、升级；Explorer、Cygnus、Aran 可切换全部已到达的转职阶段，未来阶段保持禁用
- **紧凑技能窗口**: 四行技能布局使用匹配尺寸的紧凑面板，不显示无功能的空白扩展区域
- **技能冷却**: 冷却计时
- **被动技能**: 被动效果计算
- **[圣甲术 buff 修复](changelog/2026-10-25/iron-body-buff-skill-id-fix.md)**: 服务端技能常量 1000003→1001003，圣甲术正确下发 WDEF 增益（GIVE_BUFF、图标、属性面板与到期移除）
- **[技能名消失修复](changelog/2026-11-01/skillbook-names-and-final-attack-hit-counts.md)**: Asyncify 重放期间禁止走 LazyFS 同步快路径（挂起读必须重放到记录帧，其它读同步返回），技能窗口不再整列空白、启动不再 `null function` 崩溃
- **[终极攻击 2 段 8 只](changelog/2026-11-01/skillbook-names-and-final-attack-hit-counts.md)**: 1100002/1200003/1300002 的 1-29 级补齐 `attackCount=2`/`mobCount=8`（NX 归档 + 服务端 wz XML 双侧），非满级终极攻击也打满两段
- **[终极攻击缺失变体补齐](changelog/2026-11-01/final-attack-variant-grafts.md)**: 从 v83 归档 graft 1100003/1200002/1300003（三张图标 + 原生兄弟的整级数据：lv30 60%/350% + `lt`/`rb` 判定向量），3100001/3200001/11101002/13101002 全级 `attackCount=2`/`mobCount=8`，其中远程的 3100001/3200001/13101002 另加 `bulletCount=2`（客户端 `Attack::RANGED` 的段数取 `bulletCount`，`11101002` 为魂骑士剑 FA 不加），11001002/11001003 补回 `finalAttack`（`11101002` 0=30/1=40）；同一契约由 `tools/nx/patch_final_attack_variants_xml.py` 幂等同步到服务端 `wz/Skill.wz/*.img.xml`（level 按数值序插入，`--check` 一键自检）

相关模块: [角色系统](../agents/client/Character/index.md)

### 社交

- **[好友与组队](social/)**: 独立好友窗口、好友请求/接受/拒绝/删除、上下线同步与好友聊天；队伍创建、邀请、拒绝、加入、成员/HP 同步、队聊、转让队长、踢人、退出与解散
- **聊天系统**: 全局、私聊、频道消息，以及好友/队伍目标聊天
- **[组队任务与单人模式](changelog/2026-10-26/pq-solo-npc-teammates.md)**: 自由市场玩法向导可传送 13 个组队任务（废弃小组/露台迷宫/天空/海盗/月妙/艾琳/玛加提亚A·Z/结婚/CWK/宝物/BOSS冲锋，13/13 传送验证通过）；无队伍时 NPC 队友（特工小队）补位协同进行副本——跟随走位、平台站位计数、协同攻击，全 13 本单人进图流程 PASS（[验证记录](changelog/2026-10-26/fm-pq-teleport-and-solo-verification.md)）

相关模块: [UI 系统](../agents/client/IO/index.md), [网络层](../agents/client/Net/index.md)

### 资源流式加载

- **LazyFS**: 按需通过 WebSocket 分块请求加载 .nx 文件
- **浏览器缓存**: 本地 IndexedDB 持久化缓存已加载的文件块，减少重复下载
- **物品素材后台预载**: 进入游戏后异步缓存 Item/String 和 Character 元数据；实际出现的装备图片按需永久缓存，下载期间不阻塞渲染
- **服务端全量内存缓存**: 可选在 assets-server 启动时把全部可服务 `.nx` 载入共享只读内存，消除后续块请求的磁盘打开与读取；默认关闭

相关模块: [按需文件系统](../agents/client/LazyFS/index.md)

### 音频

- **BGM 播放**: 背景音乐；进图设定之外还支持服务端 FIELD_EFFECT 运行时切换
- **音效**: 攻击、技能等音效

## 变更记录

变更日志入口: [changelog/](changelog/)
