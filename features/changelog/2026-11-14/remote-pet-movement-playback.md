# 远端宠物移动片段插值回放 + MOVE_PET 整包透传

## 背景（Context）

- 远端玩家的宠物此前只对 `MOVE_PET` 的**最后一个片段**做落点瞬移：多片段批次
  （宠物跳跃/折返路径）在观察端一帧闪到终点，与主人移动回放的平滑插值观感割裂。
- `Char::move_pet` 上行时对片段列表有本地再加工，多宠物槽位/字段存在被改写的
  风险。

## 根因

1. `PetLook::apply_movement` 只接受单片段并直接 `set_position`——批量接口
   `apply_movements` 缺失，`MOVE_PET` 的片段序列没有回放通道。
2. 宠物本地跟随模拟（`PetFollowPolicy` 的 decide/leash/warp）与远端权威片段
   互相抢 `phobj`：同一帧既跑本地物理又吃远端落点，产生抖动。
3. `Char::move_pet` 未整包透传。

## 改动（Change Summary）

- **A. 复用 mobplayback 做远端回放**（`src/client/Character/Look/PetLook.cpp/.h`）：
  - 新增 `apply_movements(const std::vector<Movement>&)`：`pet_playback_anchor()`
    找首个定位片段，**锚定片段直接落地**（单片段批次行为与旧瞬移完全一致——
    服务器节奏下每包一个片段），其余片段交给
    `mobplayback::begin(position, rest)` 回放；
  - 新增 `update_playback()`：回放期间片段独占轨迹——`hforce/vforce/hspeed/
    vspeed` 清零、位置/fh/stance 直写 sample，动画按帧推进；本地跟随模拟
    （decide、leash warp、WARP 落地判定）暂停；
  - `sample.done` 即耗尽：以末片段端点复位本地状态（`reset_motion`、跳跳
    计数/方向/空中标记清零、碰撞缓存清零）并清空回放通道，**回落本地跟随**
    从端点确定性续跑（`is_playing_back()==false` 是回切信号）。
- **B. 回放期间的主人姿态联动门控**（`src/client/Character/Char.cpp`）：宠物
  stance 跟随主人 state 的强制改写（梯绳 HANG/游泳 FLY/回落 STAND）在
  `pet.is_playing_back()` 期间跳过——片段 newstate 拥有 stance，避免每帧把
  宠物动画重置。
- **C. `Char::move_pet` 整包透传**：不再改写字段，直接构造上行封包。

## 验证

- 新增 `tools/verify/pet-playback-interpolation.cpp`（已注册 index.md）：80ms
  片段 5 帧（40ms）到达空间中点、耗尽停在末点且 done、通道清空后空批次不动
  （本地回退信号）、单片段批次直接落地、空批次不动。
- `node tools/verify/remote-pet-desync-source-contract.mjs`（含 MOVE_PET stash
  断言）PASS。
- `tools/verify/pet-automation.cpp`、`pet-vitality.cpp`、`pet-physics-nx.cpp`
  回归 PASS（本地跟随策略未被破坏）。

## 已知边界

- 回放期间本地跟随完全冻结：若主人瞬移远离而片段批次仍在播放，宠物会在旧轨迹
  上播完才追——由后续 `MOVE_PET` 新批次重锚定收敛，不做中途打断。
- 服务器不会为宠物移动发"权威纠正包"以外的信息，耗尽后到下一包之间的空窗由
  本地跟随外推，与远端玩家角色的保持末位策略不同（宠物有自主跟随语义）。
