# 召唤兽同步全家桶 + 外显 BUFF/粉笔板/椅子/空中出生落地

## 背景（Context）

- **召唤兽完全不可见**：`SPAWN_SPECIAL_MAPOBJECT`（0xAF）等六个封包客户端从未
  注册 handler——对方的木偶/鸟/龙/章鱼/贝希摩斯在观察端既不出现也不移动，
  `SUMMON_ATTACK` 的伤害数字与 `DAMAGE_SUMMON` 的掉血也无从显示。
- **外显 BUFF 无视觉**：`GIVE_FOREIGN_BUFF`（0xC7）只驱动 COMBO/WK_CHARGE，
  DARKSIGHT（隐身）与 SHADOWPARTNER（影子搭档）落地无任何画面差异；
  `CANCEL_FOREIGN_BUFF` 同样不清除。
- **粉笔板/椅子**：CHALKBOARD（0xA4）无 handler；`SPAWN_PLAYER` 的 chair id 与
  chalkboard 文本被解析后丢弃——对方坐着进图/写字进图，观察端一概不知。
- **空中出生悬空**：服务端让进图角色出生在 foothold 上方约 42px、姿态 FALL；
  远端角色不做本地物理，直接悬在空中。
- **疾病位宽错读**：非 POISON 疾病位按 6B（value+skillid+skilllevel）读，实际
  服务端 `giveForeignDebuff` 非 POISON 只写 4B（skillid+skilllevel），错位后
  同包后续疾病全读歪。
- **表情节流**：服务端 emote 广播节流 2000ms，连续表情远端只看到零星几个
  （服务端补丁 0019 一并处理）。

## 根因

1. 客户端没有召唤兽实体模型：无 pool、无动画解析、无 handler、Stage 无
   update/draw 挂点。
2. `GiveForeignBuffHandler` 的疾病值区按 POISON 布局统一读取；DARKSIGHT/
   SHADOWPARTNER 是 whole-body 外观状态，无专属落地路径。
3. `CharSpawn` 无 chair/chalkboard 字段；`CosmeticHandlers` 缺失；
   出生位置不查询地面。
4. linked server `MapleCharacter.changeFaceExpression` 节流阈值 2000ms 过粗。

## 改动（Change Summary）

- **A. 召唤兽同步全家桶（新文件×8，纯数据+自由函数风格）**：
  - `src/client/Gameplay/Summon/SummonAnims.h/.cpp`：从 `Skill/<book>.img/
    skill/<id>/summon/{summoned,stand,move|fly,attack1,die}` 解析动画集，按
    skillid 缓存；**无 summon 节点的技能返回 invalid 集合——实体保持不可见但
    位置与生命周期照常同步（优雅降级）**；`dragon_skillid(job)` 把
    SPAWN_DRAGON 的职业映射到渲染技能（priest 龙等，未映射职业返回 0=不渲染）。
  - `src/client/Gameplay/Summon/SummonPool.h/.cpp`：spawn/remove/move/attack/
    show_damage/clear/draw/update；movementType 0=stationary/1=follow/
    3=circle-follow（未知值回落 stationary）；follow 型向主人位置缓动、
    owner 离图（Stage 回调查 cid）则保持；攻击播放 attack1 并对每 hit 出
    伤害数字；animated 模式走 summoned→stand、remove mode 4 走 die 动画。
  - `src/client/Net/Protocol/SummonPayloads.h/.cpp`：SPAWN/REMOVE/MOVE/
    ATTACK/DAMAGE/DRAGON 六个 payload 的纯解析函数，长度前置校验、越界抛
    `PacketError`。
  - `src/client/Net/Handlers/SummonHandlers.h/.cpp`：六个 handler，owner 不在
    图上则忽略；MOVE_SUMMON 复用 `MovementParser` 片段回放。
  - `PacketSwitch` 注册 `0xAF/0xB0/0xB1/0xB2/0xB3/0xB5`；`Stage` 持有
    `summon::SummonPool`（clear/draw/update 挂进主循环，update 回调解析本地
    player 与 OtherChar 的主人位置）、`get_summons()` 访问器。
- **B. GIVE/CANCEL_FOREIGN_BUFF 外显落地**
  （`src/client/Net/Handlers/MapObjectHandlers.cpp`、`Char.cpp/.h`）：
  - DARKSIGHT → `OtherChar::set_darksight`：整身精灵 alpha 0.4（名字标签同步
    变淡）；SHADOWPARTNER → `set_shadowpartner`：面向反侧 30px 偏移、
    alpha 0.5 的**镜像二次绘制**（仅外观，不影响移动）；CANCEL 时清标志。
    （GHOST/MORPH 视觉见同日变更，由并行工作流处理。）
  - 疾病值区修正：**非 POISON 疾病 4B**（mob skillid+skilllevel）、**POISON
    6B**（value short 在前），逐 flag 按 mask 位序消费，不再错位。
- **C. 粉笔板 + 椅子 + 空中出生**
  （新 `src/client/Net/Handlers/CosmeticHandlers.h/.cpp`、`Spawn.cpp/.h`、
  `SpawnPlayerPayload`、`Char.cpp`）：
  - `ChalkboardHandler`（PacketSwitch 注册 164/0xA4）：open(1)+len+ASCII 文本
    /close(0)，经 MapChars 路由到 `Char::set_chalkboard`；头顶文本以名牌锚点
    上方一个名牌高度绘制（A13M 居中黄色，隐身时随 alpha 变淡），与迟到事件
    stash 的 CHALKBOARD 分支联动。
  - `CharSpawn` 增加 `chair_id`/`chalkboard`，`SPAWN_PLAYER` 解析落地；实例化
    时 `set_chair/set_chalkboard`。
  - `resolve_position(physics)`：FALL 姿态的出生点经 `physics.get_y_below()`
    落地，落差钳制 1..200px（过小忽略、过大视为真实空中状态不拉地面）。
- **D. 服务端补丁 0019**（`scripts/server-patches/0019-expression-throttle.patch`）：
  emote 广播节流 2000→300ms——本地表情即时播放而远端被 2s 节流大量吞掉的
  "动作丢失"消除；5s 的 emote 0 回落广播不受影响。

## 验证

- 全量语法扫描：Summon/SummonHandlers/SummonPayloads/CosmeticHandlers/
  PacketSwitch/MapObjectHandlers/Spawn/SpawnPlayerPayload/Char 全部 exit 0
  （Stage.cpp/StageLoading.cpp 仅存量宿主 freetype 头缺失噪声，HEAD 同样失败）。
- `node tools/verify/remote-pet-desync-source-contract.mjs`（含 chair/chalkboard
  落地断言）PASS；`foreign-buff-mask.cpp`、`spawn-player-parser.cpp` PASS。
- 联机建议：双客户端验证 木偶/鸟/龙 召唤-移动-攻击-消散、暗影/影子搭档开关、
  粉笔板书写与关闭、坐椅进图、高处进图落地、连续表情同步（需先应用补丁 0019
  并重编重启服务端容器）。

## 已知边界

- 无 `summon` 动画节点的召唤兽技能保持**不可见**（位置/生命周期仍同步），
  不做占位渲染；dragon_skillid 未映射职业同样不渲染。
- SUMMON_ATTACK 仅呈现攻击动画与伤害数字，不做命中判定（判定在服务端）。
- GIVE_FOREIGN_BUFF 的 MONSTER_RIDING 变体（writeInt×2 布局）不在本 handler
  消费范围；GHOST/MORPH 视觉与远端攻击 charge 消费由并行代理同日处理中，
  见各自变更记录。
