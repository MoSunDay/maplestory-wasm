# 水下世界：官方水下游泳行为 1:1 复刻

地图 `info` 的 `swim` 标志整图生效（数据实测 45 张：水下世界 230 系
21 张、922/923 水下区 23 张、天空塔水下层 200082300）。此前四方向游
泳、缓沉、水底行走、水下梯子、宠物飞行姿态已可用，本轮补齐与官方客
户端的其余行为差异。

## 官方行为对照

| 官方水下行为 | 实现 |
|---|---|
| 方向键四方向游泳，松手缓沉（水阻+弱重力） | `Physics::move_swimming`（SWIMGRAVFORCE 0.03 / SWIMFRICTION 0.08，上游常量） |
| **跳跃键（Alt）= 向上划水**，与↑同效 | `PlayerFlyState::update`：水下 `JUMP` 按住即 `vforce = -flyforce` |
| 落水/水底起跳从第一帧就是水物理（漂浮小跳约 25px） | `PlayerFallState::initialize`、`Player::respawn`：水下直接设 `SWIMMING` 类型 |
| 游泳移动片段用官方 swim command（type 5） | `Movement` 构造增加 `cmd` 参，SWIM 态发送 `CMD_SWIM(5)`；服务端 `AbstractMovementPacketHandler` 将 0/5/17 同按 AbsoluteLifeMovement 解析，透传 stance 12/13 |
| 水底可行走/卧倒（DOWN→PRONE），走离边缘落回游泳 | 既有 `PlayerFlyState::update_state` / `PlayerFallState` |
| 水下可攀爬梯子、过传送门（↑就近触发） | 既有 `Stage::check_ladders` / `check_portals`（不区分状态） |
| 游泳动画复用 fly 动作，stance 12/13（+1 朝左） | 既有 `Stance::by_state`；其他玩家经 `MovementParser` 还原 |
| 宠物切飞行姿态 | 既有 `Char::update` SWIM→`PetLook::FLY` |
| 水下无氧气条 | 与服务端一致：v83 无 O2 机制，水下伤害走地图 `decHP`（每 5 秒扣血，2022040 水下呼吸药水/1102061 免疫），服务端已有 |

## 修复的遗留缺陷

- **非水下地图坐椅子按↑会切 SWIM**：会拿到 FLYING 物理在普通地图漂
  浮（上游 Journey 遗留）。现在仅水下地图进 SWIM，其余起身 STAND。
- **进水图首次游泳卡顿**：`CharLook::prepare_movement_assets` 只预热
  STAND/ALERT/WALK；新增 `prepare_water_assets()` 在 `Stage::respawn`
  对 swim 地图预热 FLY stance（身体/头发/装备）。

## 改动位置

- `src/client/Character/PlayerStates.cpp`：FALL 初始化水下用 SWIMMING；
  游泳态 JUMP 键上浮；坐姿 UP 分水域处理。
- `src/client/Character/Player.cpp`：respawn 水下直接 SWIMMING 物理；
  移动封包 SWIM 态发 command 5。
- `src/client/Gameplay/Movement.h`：`Movement` 构造支持 command 字节，
  新增 `CMD_SWIM = 5`。
- `src/client/Character/Look/CharLook.{h,cpp}`：`prepare_water_assets()`。
- `src/client/Gameplay/Stage.cpp`：respawn 时按 `mapinfo.is_underwater()`
  预热水下动画。

## 实测记录

- 改动翻译单元 g++ 语法检查通过（仅宿主缺 freetype 头的既有噪声）。
- Docker WASM 全量构建通过（`--jobs 16`，`[100%] Built target JourneyClient`）。
- 数值核验：按住方向键游泳末速 ≈ flyforce/SWIMFRICTION ≈ 3.1 px/帧；
  松手后以 0.03 重力缓沉；水底跳跃上升 ≈ 25px 后漂浮回落。
- e2e 实测（`tools/verify/maplestory-webui/aqua-swim-flow.mjs`，Aqua Road
  230000000，fmflow1Hero lv60）全部断言通过：水底站立(21/22, onground=1)；
  小跳起跳并回落（stance 5→3 链路正确）；水底按↑直接起游（无需先跳）：
  900ms 上升 293px、|vspeed| 平台 2.74 px/帧（理论 (flyforce-重差
  0.03)/SWIMFRICTION=2.75）、physicstype=2(SWIMMING)、stance=3(FLY)；
  Alt 键上浮等效（Δy -353 vs -293，差值来自跳跃初始力，60px 容差内）；
  松键缓沉降终速 0.370 px/帧（理论 0.375）且 stance 保持 3；水平游
  Δx 336px、|hspeed| 平台 3.12（理论 3.125）；沉底后行走 stance 34。
  产物 `/tmp/aqua-swim-flow/`。
- 本次补齐：STAND/WALK 水下按↑直接起游（此前必须先跳才能离地），
  梯子优先不受影响（`PlayerStates.cpp` 两处 UP case）。
- 遗留人工待办：多人同图 command 5 移动封包往返、游泳手感主观体验。
