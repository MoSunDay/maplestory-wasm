# 冲锋改为按朝向冲刺，距离取技能前向范围并封顶半屏

## 背景

战士三职业的冲锋（Hero 1121006 / Paladin 1221007 / Dark Knight 1321003）
此前终点完全依赖被命中的怪物：

- `Combat::apply_rush(const AttackResult&)` 开头就是
  `if (result.mobcount == 0) return;`，身前没有怪时冲锋一动不动。
- 有命中时用 `mobs.get_mob_position(result.last_oid).x()` 当终点，等于朝
  「最后一只被打中的怪」滑过去：怪站在身后会被反向拉扯，怪离得很近就几乎
  不位移，冲刺距离完全随怪物站位飘忽。
- `AttackResult::last_oid`（以及 `first_oid`）在 `Attack.h` 里没有默认值，
  `MapMobs::send_attack` 只在命中数恰好等于 `mobcount`（打满）时才写
  `last_oid`。没打满时它是未初始化的栈上值，`get_mob_position` 拿这个
  随机 oid 查不到怪就返回默认点，冲锋随之跳到无意义的位置。

## 变更

- 新增 `src/client/Gameplay/Combat/RushMovement.h`（header-only，纯函数，
  namespace `jrc::rush_movement`），把冲锋几何从怪物与战斗状态里剥离：
  - `forward_reach(range, hrange)`：返回 `-range.l() * hrange`。技能 hitbox
    的 `lt.x` 位于身侧/身后为负（前向 extent 的相反数），取反即前向距离，
    与 `MapMobs::send_attack` 用同一个 `hrange` 缩放系数；结果 ≤0 返回 0，
    最大钳到 32767。
  - `half_view_distance(view_width)`：`view_width / 2`，非正宽度返回 0。
  - `dash_distance(range, hrange, max_distance)`：上限 ≤0 返回 0；否则取
    `forward_reach` 并钳到上限；extent ≤0（空 range 或异常 NX 数据）回退
    为上限，保证冲刺长度可预期。
  - `target_x(origin_x, facing_right, distance)`：朝右加、朝左减，结果钳在
    `INT16_MIN..INT16_MAX`。
  - `forward_target(origin_x, facing_right, range, hrange, view_width)`：
    上述三步的组合，也是冲锋的唯一终点来源。
- `Combat::apply_result_movement` 改为
  `(const SpecialMove& move, const Attack& attack)`，`apply_move` 调用点改为
  `apply_result_movement(move, attack)`；冲锋分支调用 `apply_rush(attack)`。
- `Combat::apply_rush(const Attack& attack)` 用 `attack.origin.x()`、
  `!attack.toleft`、`attack.range`、`attack.hrange` 和
  `Constants::viewwidth()` 计算终点后交给 `player.rush(targetx)`。删除
  `mobcount` 提前返回与 `last_oid` 查表，未命中任何怪物也照常冲锋。
  朝向来源说明：`prepare_attack` 中 `attack.toleft = !flip`，而 `flip == true`
  表示朝右（`walkforce` 为 `flip ? +force : -force`），所以 `!attack.toleft`
  即「朝右」。
- `src/client/Gameplay/Combat/Attack.h`：`first_oid` / `last_oid` 默认初始化为
  0，未命中时不再带出栈上垃圾值。
- `src/client/Gameplay/MapleMap/MapMobs.cpp` 两个 `send_attack` 重载：首个
  命中仍写 `first_oid`，但每次有效命中都写 `last_oid`，去掉「打满
  `mobcount` 才写」的条件，顺带修掉旧的未初始化/陈旧 oid 问题。

## 行为

前向距离 = `min(-range.l() * hrange, viewwidth / 2)`：

| 场景 | 前向 extent | 800 宽视口冲刺距离 | origin=1000 朝右终点 |
|---|---|---|---|
| Hero 1121006（测试同型 range `(-250,0,-50,0)`，hrange 1） | 250 | 250（未触上限 400） | 1250 |
| Paladin 1221007 / Dark Knight 1321003（对称 range `(-500,500,-500,500)`） | 500 | 400（半屏上限截断） | 1400 |
| 同上，1200 宽视口 | 500 | 500（上限 600 未触及） | 1500 |
| 视口宽 W | reach | `min(reach, W/2)` | `1000 ± 该值` |

- 朝左时终点对称：origin 1000、距离 250 → 750；Paladin/DK 在 800 宽下
  → 600。公式只做加法/减法，不做方向翻转以外的推导。
- 对称 range 只看左边界 `lt.x`（500），不取 `width()`（1000）。
- 空 range、`lt.x >= 0` 的异常 range 或 `hrange = 0` 时，前向距离退化为
  `W/2`，仍能冲刺半屏而不是原地不动。
- 视口宽为 0 或负数时不产生位移（`dash_distance` 上限为 0）。
- 终点钳在 `INT16_MIN..INT16_MAX`，地图边缘或极端坐标不会溢出回绕。
- 撞墙/台阶仍由 `Player::rush` 的 `PhysicsObject::TURNATEDGES` 处理，实际
  位移可能小于上述距离。

## 验证

- 新增 `tools/verify/rush-movement.cpp`（纯 `assert`，无 NX 依赖）：覆盖
  Hero 型 250、Paladin/DK 型 500 在 800/1200/1600 宽下的封顶行为、
  `hrange` 1.5 倍得 375 与 2 倍被封顶、空的/异常的 range 回退半屏、
  `hrange = 0`、左右朝向目标（1000→1250/750、Paladin 1000→1400/600）、
  `half_view_distance`（800→400、1200→600、1→0、0 与负数→0）、上限为 0
  或负数时零位移、两端 INT16 溢出钳制、对称 range 只用左边界 500。
  命令：
  `c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function -Isrc/client -Isrc -Isrc/nlnx tools/verify/rush-movement.cpp -o /tmp/rush-movement && /tmp/rush-movement`
  本地输出 `rush-movement: PASS`。
- `clang++ -std=c++17 -Wno-narrowing -fsyntax-only -Isrc/client -Isrc -Isrc/nlnx`
  分别检查过 `Combat.cpp` 与 `MapMobs.cpp`，无编译错误。
- 父任务已执行 `./scripts/docker_build_wasm.sh`（Release，完整 WASM 构建），
  构建通过；仍未做浏览器内 E2E。

## 限制

- 表里的终点是**上限**而非精确落点：`Player::rush` 仍走
  `phobj.movexuntil(targetx, get_attackdelay(1))`，受攻击动作时长与地面摩擦
  影响，且 `TURNATEDGES` 会在墙体/台阶处提前停下，短于计算距离属正常。
- 距离优先取 `range` 的 `lt.x` 前向 extent 乘 `hrange`（`-lt.x * hrange`）；
  仅当传入的 hitbox 没有正向前向 extent 时，纯函数 helper 才回退到半屏上限。
  集成路径里 `Attack::range` 在技能缺少 `range` 节点时会沿用玩家默认攻击范围；
  当前三个冲锋技能 1–30 级都带 `lt`，该回退实际未被触发。
- 未做浏览器内交互 E2E，朝向与距离只经纯函数回归与代码路径核对。
