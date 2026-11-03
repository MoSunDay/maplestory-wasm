# 多玩家同图：非受控怪改为服务器移动片段回放（位置不同步 P0）

## 症状（Context）

双账号/多客户端同图时，**非控制端**的怪物位置与控制端不一致：各端怪
物漂移、方向对不上，且每收到一个 `MOVE_MONSTER` 广播（MOB_MOVED=239）
就被硬拉回服务器坐标，肉眼可见"回跳/瞬移"。

## 根因（客户端，改动前）

- `Mob::send_movement`（MobPresentation.cpp）：收包即
  `set_position(start)` 把怪钉到片段列表的末位置，片段的
  xpos/ypos/lastx/lasty/duration 全部丢弃，只取 `last.newstate/fh`。
- `Mob::update` 对非受控怪沿用**本地物理外推**（按本地 speed 常量 +
  `physics.move_object` 推进）：两包之间各端必然分叉，下一包到来再被
  `set_position` 硬拉回 → 位置不同步 + 回跳。

## 修复（Change Summary）

- 新增纯头文件 `src/client/Gameplay/MapleMap/MobMovementPlayback.h`：
  `mobplayback::State`（片段批次 + 游标）与自由函数 `begin/step`，无类、
  无副作用隐藏，状态由调用方持有。
  - 起点：片段 0 一律从**包内 start** 起播（服务器权威纠偏）；
  - 插值：每帧按 dt（Constants::TIMESTEP=8ms）消耗片段 duration，位置
    从段起点线性插值到片段 (xpos,ypos)；后续片段起点取上一片段终点
    （链式连续，避免把线上 lastx/lasty 误当位置的外来客户端造成扫描
    glitch）；无位置语义的片段（teleport/换装）不占时也不成为路径点；
  - 兜底：队列耗尽后**保持末片段终点与 stance/fh**（静止）。选择静止
    而非本地外推：外推必然分叉并回跳（即本 P0），静止在各端确定性一
    致，且命中/仇恨判定用的位置与服务器完全对齐。
- `Mob::send_movement`：`set_position(start)` 保留为重锚点，片段批次入
  回放状态；24-41 普攻 / 42-59 技能仍在**包到达即触发**（动作时序来自
  攻击/技能动画而非移动路径，且与旧行为一致，取更简单者）；不再立刻
  `set_stance(last.newstate)`。
- `Mob::update`：`!dying && !control` 分支改为 `update_remote`——按帧
  `step` 回放，清零 phobj 力/速度后直接写入插值位置（Linear 赋值保留
  帧间插值渲染），应用片段 fh（>0 时）与 stance（<24 的纯姿态字节），
  `update_fh` 维护图层/落地状态供绘制与战斗判定；动作动画结束照旧
  `finish_action`。**控制端（control==true）本地权威逻辑一行未动。**
- `Mob::set_control(false)`：清空陈旧回放批次（重锚到当前位置、无片
  段→冻结），避免失去控制权瞬间被旧片段拉回历史位置；从未收到包的
  远端怪保持出生位置不动，不再本地乱走。

## Impact Surface

- 客户端非受控怪移动表现与位置权威性（Mob / MobPresentation /
  MobMovementPlayback.h）；控制端、死亡/濒死、掉落、其他玩家路径未动。
- 验证：`tools/verify/mob-movement-playback.cpp`（两片段 + 固定 8ms 步
  进的推进序列、片段切换 stance/fh、耗尽保持、duration=1 即达、start
  重锚、无位置片段跳过、空批次）；`bash scripts/docker_build_wasm.sh`
  全量编译通过。
