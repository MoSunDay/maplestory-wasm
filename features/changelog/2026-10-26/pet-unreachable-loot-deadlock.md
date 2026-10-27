# 宠物拾取死锁：停驻在够不到的掉落旁彻底罢工（不动也不捡）

## 症状

用户报告：宠物有概率一动不动地站在原地，既不跟随主人，也完全停止
拾取任何掉落；只有主人走出 300px 牵引距离触发瞬移时才"解冻"。

## 根因

追根链路（`PetLook::update` → `pet_follow::decide` →
`Stage::update_pet_automation`）：

- 宠物追拾取目标时水平对齐即停（|dx| ≤ 14 停、≤ 32 站立），而拾取请求
  只在宠物与目标欧氏距离 ≤ 50px（`pet_loot::BATCH_RANGE`）时才发。
- `pet_follow::decide` 原来只在三种情况下放弃目标：高于 88px 单跳上限、
  深于 240px 下落保护、连续 3 次受阻跳跃失败。
- 死锁几何：掉落与宠物水平对齐但垂直相差约 48..88px（上方平台无墙可
  跳）或 48..240px（坑内掉不进去）时，宠物停在目标正下方/正上方——
  永远进不了 50px 拾取半径，也永远不满足任何放弃条件，`followpos` 被
  拾取目标占据后连主人也不跟随，只能靠 300px 牵引瞬移解围。
- 次生问题：即使放弃，`MapDrops::find_pet_loot` 永远选最近的同一件
  不可达掉落，45 帧冷却一过立即重新锁定——逼近/放弃循环饿死其余全部
  掉落，表现即为"宠物不动、也不拣东西"。

## 变更

- `PetFollowPolicy`：`Inputs` 追加 `grounded` 与 `pickup_range`（保持
  原有聚合初始化语义不变）；`decide` 在高度/下落/跳跃失败放弃判定之后
  新增"停驻却超出拾取距离"放弃——在地面（`grounded`）且水平已对齐
  （|dx| ≤ 停驻阈值）但欧氏距离 > `pickup_range` 时返回
  `ABANDON_LOOT`；空中不触发，避免下落中途被打断。
- `PetLook::update`：`decide` 调用传入 `phobj.onground` 与
  `pet_loot::BATCH_RANGE`。
- `MapDrops`：`PetLootQuery` 追加 `exclude_oid`，`find_pet_loot` 跳过
  该 oid（`find_pet_loot_batch` 不变，它只扫已在拾取半径内的掉落）。
- `Stage`：新增 `pet_loot_blocked_oid`；宠物放弃目标时先登记该 oid 再
  清理目标（保留冷却），`find_pet_loot` 持续排除它直至掉落消失
  （`has_lootable_drop` 判定后自动解除）；`clear()` 复位。

## 验证

- 单测：`c++ -std=c++17 -Wall -Wextra -Werror -Isrc tools/verify/pet-automation.cpp src/client/Gameplay/Pet/PetAutomation.cpp src/client/Gameplay/Pet/PetCapabilityPolicy.cpp src/client/Gameplay/Pet/PetFollowPolicy.cpp src/client/Gameplay/Pet/PetLootPolicy.cpp -o /tmp/pet-automation && /tmp/pet-automation`
  PASS（新增用例：停驻超距放弃-上下沿/站立姿态、50px 内仍追、空中继续
  追-深落/高跳、远距离行走不受干扰）。
- 宠物物理 NX 回归：`c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function -Isrc/client -Isrc -Isrc/nlnx tools/verify/pet-physics-nx.cpp src/client/Gameplay/Pet/PetFollowPolicy.cpp src/client/Gameplay/Physics/Physics.cpp src/client/Gameplay/Physics/FootholdTree.cpp src/client/Gameplay/Physics/Foothold.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/pet-physics-nx && /tmp/pet-physics-nx assets/Map.nx`
  PASS（策略改动对真实 foothold 坡度/墙体/末端物理仍成立）。
- 触及的翻译单元（Stage.cpp / MapDrops.cpp / PetLook.cpp /
  PetFollowPolicy.cpp）经 wasm-builder 容器内 em++ `-fsyntax-only`
  全部通过。

## Related Docs

- [永久新手宠物与自动化](../../pet-automation/index.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)

## 第二轮迭代（用户复报：停在物品旁不动也不拣取）

### 残余根因

- 上一轮的“停驻却超出拾取距离立即放弃”只看宠物与（捕获时的）目标位置
  的欧氏距离：掉落从怪物尸体高度下落途中，宠物停在正下方时距离瞬时
  超过 50px，规则在下落中途触发放弃并把 oid 拉黑——掉落随后落进拾取
  范围也永远不会再被选中，主人原地挂机时宠物就站在物品旁“罢工”。
- 目标位置只在锁定时捕获一次（`Stage::pet_loot_target_position`），而
  掉落生成后还要经历约 48 帧的下落与水平散落（与玩家拾取 80px 散落
  修复同源），宠物会停在过时坐标上，扫拾批次（以宠物实时位置计算）
  反而装不进该掉落，造成空尝试与追逐错位。
- 服务端 `PetLootHandler` 实际不做宠物-掉落距离校验（旧注释里的 60px
  校验不存在），但客户端 50px 圆形门槛把“正下方一层平台、垂直差
  50~96px”这种视觉上紧邻的掉落也判为不可拾取。

### 变更

- `Stage::update_pet_automation`：目标存活期间每帧用
  `MapDrops::pet_loot_position` 刷新实时图标中心，并同步
  `PetLook::set_loot_target`，宠物始终追实时位置。
- `PetLootPolicy`：新增可达窗口 `in_reach`（水平 `BATCH_RANGE` 50px、
  向下 `REACH_DROP_LIMIT` 96px、向上不超过追击高度上限）与
  `should_abandon`（`ABANDON_FRAMES` 240 帧兜底）；拾取发起门槛与
  `MapDrops::find_pet_loot_batch` 扫拾窗口统一改用该窗口（批次签名
  去掉半径参数，仍按最近优先、每轮 4 件）。
- `PetFollowPolicy`：移除上一轮加入的停驻欧氏距离立即放弃规则及
  `Inputs::grounded/pickup_range` 字段（由 Stage 侧带掉落状态感知的
  时间兜底取代），`PetLook::update` 调用恢复 7 字段。
- 高度/下落/连续受阻跳跃放弃、300px 牵引瞬移、45 帧冷却、oid 拉黑
  直至掉落消失等既有语义不变。

### 验证

- `tools/verify/pet-automation.cpp`：更新停驻场景期望（窗口内/窗口外
  停驻均为 IDLE，交由 Stage 兜底），新增 `in_reach` 六边界与
  `should_abandon` 三态断言；`/tmp/pet-automation` 全绿。
- docker WASM 全量构建（`WARNINGS_AS_ERRORS=ON`）通过。
