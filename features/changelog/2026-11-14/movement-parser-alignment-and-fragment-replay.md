# 移动解析逐命令对齐服务端字节表 + 瞬移 0 时长 waypoint + 本地发包真实 duration

## 背景（Context）

- 三条远端回放链路（`OtherChar`/`MapMobs`/`PetLook`）共用
  `MovementParser::parse_movements` 的产物，但解析器与 linked server
  （v83 `MovementPacketDecoder`）的字节表存在系统性脱节，任何一处字段宽度或
  字段序读错都会让**同批后续所有片段**错位。
- 本地玩家移动发包 `duration` 硬编码 1ms：远端按 duration 线性插值，1ms 意味着
  每个片段即达端点，观感是"逐格跳"而非平滑跟随。

## 根因

1. 命令族归类与服务端不一致：
   - 18/19/20/22 未按相对位移族（x y state duration，7B）解析；
   - 3/4/7/8/9/11（瞬移/Assaulter/Rush/椅子落座类）被当作带 fh+duration 的
     绝对位移读取——线上根本没有 duration 字段，多读 3 字节；
   - 14（无 fh 下跳）/21（Aran 弧线）服务端 parse 后**直接丢弃**，客户端却把
     它们塞进回放；10（换装）外观另有广播，同样不该入列；
   - case 15 的 fh 与 origin fh 字段序错位，回放保留了 origin foothold。
2. `Movement` 枚举没有"瞬移"类型：0 时长片段无法与"普通绝对位移"区分，
   `MobMovementPlayback` 也不支持 0 时长即达 waypoint 与 NONE 末尾状态。
3. `Player::send_movement` 构造片段时 `Movement(phobj, stance, cmd)` 的
   duration 默认恒 1。

## 改动（Change Summary）

- **A. MovementParser 逐命令对齐字节表**
  （`src/client/Net/Handlers/Helpers/MovementParser.cpp`）：
  - 绝对位移族 `0/5/17`：x y lastx lasty fh state duration（13B）；
  - 相对位移族 `1/2/6/12/13/16/18/19/20/22`：x y state duration（7B），
    新增 18/19/20/22 归族；
  - 瞬移族 `3/4/7/8/9/11`：x y lastx lasty state（9B，无 duration），
    解析为 `Movement::TELEPORT` 且 `duration=0`；
  - case 15：x y lastx lasty **fh** origin-fh（skip 2B）state duration，
    保留目标 fh、丢弃 origin fh；
  - `14`（skip 9B）/`21`（skip 3B）/`10`（skip 1B）读后丢弃，不入回放列表。
- **B. Movement 枚举与构造**（`src/client/Gameplay/Movement.h`）：新增
  `TELEPORT` 类型（注释明确：瞬移/Assaulter/Rush/椅子落座，线上无 duration，
  以 0 时长 waypoint 回放）；`Movement(phobj, s, cmd, d)` 的 duration 形参
  直传，**0 是合法瞬移时长而非哨兵值**。
- **C. MobMovementPlayback**（`src/client/Gameplay/MapleMap/MobMovementPlayback.h`）：
  `positioned()` 改 switch——TELEPORT 计为定位片段；default 分支带坐标或
  state 的未知类型片段也计为定位（末尾 state-only 片段不丢）；`step()` 对
  `duration<=0` 的片段零耗时即达端点，不再按普通时长消耗。
- **D. 本地发包真实 duration**（新文件 `src/client/Character/PlayerMovement.h`，
  纯函数头文件）：`movement_duration()` 把累计耗时钳制进
  **8..250ms**（下限一个 TIMESTEP、上限防远端看起来停滞）；
  `send_player_movement()` 每 step 累计 `movesince` 作为新片段 duration、
  发包后清零（久站后再起步从单 tick 起算，不携带陈旧长间隔）；
  `Player::send_movement` 改为调用上述函数（`Player.h` 新增 `movesince` 字段）。

## 验证

- 全量语法扫描（`clang++ -std=c++17 -Wno-c++11-narrowing -Isrc/client -Isrc
  -fsyntax-only`）：`MovementParser.cpp`/`Movement.h` 相关编译单元 exit 0。
- 新增 `tools/verify/movement-parser-alignment.cpp`（已注册 index.md）：单包
  覆盖命令 0..22 全表，断言字节完全消费、10/14/21 不入列（20 条入列）、
  case15 保留目标 fh（7）而非 origin fh（9）、瞬移族 duration=0、各族字段
  round-trip。
- `tools/verify/mob-movement-playback.cpp` 更新：TELEPORT 0 时长即达端点、
  后继片段从其端点续插值；全零 NONE 片段不占时（原"无位置片段不占时"断言的
  场景自解析器丢弃 14 后已不可达，改为全零形态）。
- `tools/verify/char-movement-playback.cpp`、`rush-movement.cpp` 回归 PASS。

## 已知边界

- `hasmoved` 仍不含 fh： foothold 变化（如走上新平台）不单独触发重发，需伴随
  位置/姿态变化才会发包——本轮计划中的"hasmoved 含 fh"未落地（代码与计划项
  不一致，见收口报告）。
- `positioned()` 的 default 分支：带 state 的 NONE 片段会计为 waypoint，耗尽后
  `last_positioned()` 返回其**自身端点**（NONE 片段通常为 0,0），存在理论上的
  "归零回拉"。当前解析器已丢弃 10/14/21、且无本地代码构造 NONE 片段入回放，
  属防御分支的潜在坑，暂不触达。
- duration 钳制上限 250ms：本地卡顿超过 250ms 时远端看到的是慢放而非位置跳变
  （选择平滑优先）；下限 8ms 保证片段永不为 0 时长（本地玩家片段不是瞬移）。
