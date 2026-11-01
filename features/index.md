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

### 游戏世界

- **地图加载与渲染**: 加载 .nx 地图数据，渲染背景、图块、对象；进入地图前阻塞到全图已实例化素材驻留、首屏与当前实体素材进入图集，期间服务端仍保持过图保护
- **角色移动**: 行走、跳跃、攀爬、游泳等物理移动
- **[角色自然恢复](character-recovery/)**: 按 v83 站立、攀爬、技能、地图倍率和椅子规则恢复 HP/MP，并与 linked server 的恢复校验及椅子状态同步
- **传送门**: 地图间瞬移
- **NPC 交互**: 对话、商店、仓库等
- **[自由市场入口 BGM 修复](changelog/2026-10-27/fm-map-info-fallback-bgm.md)**: /gohome 回自由市场后 BGM 不刷新——079 注入的 910000000.img 属性平铺无 info 包装，MapInfo 回退平铺读取，恢复 BGM/VR 边界/town 等字段
- **[自由市场地图名恢复](changelog/2026-10-26/fm-map-name-restore.md)**: 910000000 街道名/地图名恢复原版「自由市场/自由市场入口」，注入管线加覆盖防回归，小地图实显取证通过
- **[自由市场玩法向导 NPC](changelog/2026-10-25/fm-gameplay-guide-npcs.md)**: 910000000 内的 PQ 招募/BOSS 入口向导（9010000/9010005），一键传送到 13 个组队任务招募点与 12 个 BOSS 入口，危险地图先二次确认
- **[自由市场科皮药水价格](changelog/2026-10-25/fm-kopee-potion-prices.md)**: 超级药水 5 万、特殊药水 2 万（shopid 1339 shopitems，重启生效）
- **[场景机关/进度横幅/BGM 切换](changelog/2026-10-25/field-obstacles-progress-banner.md)**: 服务端可开/关/批量/重置具名地图机关对象（OrbisPQ 陷阱、CWKPQ 石阵等），下发 PQ 进度横幅并运行时切换 BGM
- **反应器**: 可交互物体（如采集物）
- **掉落物**: 物品/meso 的拾取
- **小地图/世界地图**: 地图导航

相关模块: [游戏世界](../agents/client/Gameplay/index.md), [图形渲染](../agents/client/Graphics/index.md)

### 任务系统

- **[任务日志窗口](changelog/2026-10-25/quest-log-window.md)**: Q 键或状态栏菜单打开的 In Progress/Completed 双页任务列表（双代 UI.nx 素材自动适配），实时反映服务端任务状态：放弃、进度数据更新、完成与过期；任务相关的道具特效与状态提示同步接入
- **新版本地图数据源 gate**: 252/262/271-273/300-305 等新版本地图区段在现有 079 wz 数据源中不存在，注入整体暂缓（[结论记录](changelog/2026-10-25/new-era-maps-gate.md)），不阻塞任务 UI 与玩法枢纽

### 战斗系统

- **普通攻击**: 近战/远程攻击
- **技能使用**: 各职业技能施放
- **攻击特效**: 支持多段/多目标命中、斗气终结和属性充能特效；一次按键只发起一次攻击
- **伤害数字显示**: 伤害数值渲染
- **Buff 管理**: 增益/减益效果
- **[地图机关伤害](changelog/2026-11-01/map-obstacle-damage.md)**: 地刺/陷阱等地面机关恢复伤害判定——客户端从 Obj 定义的 `obstacle`/`damage` 字段收集机关足迹，玩家踏入即受固定伤害（地铁跳跳图地刺条、忍苦树林尖刺、椰子赛陷阱洞、CWKPQ 石柱等 224 个机关定义），`TAKE_DAMAGE -2` 上报，服务端不再丢弃无怪物来源的受击
- **[怪物仇恨战斗](mob-combat/)**: 怪物受击后按累计伤害锁定目标并持续追击，在范围内使用普通攻击或服务端授权技能，完整呈现状态、疾病和雾效果

相关模块: [游戏世界](../agents/client/Gameplay/index.md), [角色系统](../agents/client/Character/index.md)

### 物品与装备

- **物品栏管理**: 消耗品、装备、其他物品
- **装备系统**: 穿戴/卸下装备，属性变更
- **商店交易**: NPC 商店购买/单件出售；出售数量超过当前堆叠时立即回填上限，可二次确认后一键售卖当前分类的全部非现金物品，现金物品不进入售卖列表
- **[玩家面对面交易](player-trade/)**: 点击同地图玩家发起双人交易，交换最多 9 格物品和金币，支持交易内聊天与双方确认
- **仓库存储**: 物品存储
- **[现金商城](cash-shop/)**: 查询真实 NX 余额和在售目录，购买单品/礼包，并在商城仓库与角色物品栏之间转移现金物品；现金装备取出落 CASH 栏、双击以纯装饰外观覆盖穿戴（不占真实装备槽、不影响属性），右栏可购买背包 +4 格扩容（[2026-10-29 改动](changelog/2026-10-29/cash-shop-decorative-equips-and-slotbuy.md)）
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
- **[终极攻击缺失变体补齐](changelog/2026-11-01/final-attack-variant-grafts.md)**: 从 v83 归档 graft 1100003/1200002/1300003（三张图标 + 原生兄弟的整级数据：lv30 60%/350% + `lt`/`rb` 判定向量），3100001/3200001/11101002/13101002 全级 `attackCount=2`/`mobCount=8`，11001002/11001003 补回 `finalAttack`（`11101002` 0=30/1=40）；同一契约由 `tools/nx/patch_final_attack_variants_xml.py` 幂等同步到服务端 `wz/Skill.wz/*.img.xml`（level 按数值序插入，`--check` 一键自检）

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
