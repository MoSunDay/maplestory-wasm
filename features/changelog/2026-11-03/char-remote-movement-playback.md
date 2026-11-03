# 多玩家同图：远端角色改为服务器移动片段回放（人物位置不同步 P0）

## 症状（Context）

双账号/多客户端同图时，**其他玩家**的位置与本人客户端不一致：远端角色
一帧瞬移/回跳、滑步（走路动画但不前进），换图或服务端重发
SPAWN_PLAYER 后角色卡死在旧图旧位置甚至"消失"。

## 根因（客户端，改动前）

- `OtherChar::send_movement`（OtherChar.cpp 旧:82）：
  `movements.push(newmoves.back())` 只取每包**末尾片段**，其余片段
  （起跳、中途位移、stance 变化）全部丢弃，包内轨迹不可复原。
- `OtherChar::update`（OtherChar.cpp 旧:42-56、84-88）：片段经固定
  `timer=50`（帧）滞后逐个弹出，随后（旧:65-67）
  `phobj.hspeed/vspeed = lastmove - 当前位置` + `phobj.move()` 把角色
  **一帧瞬移**到片段终点——两包之间没有任何插值与物理，各端必然回跳。
- `MapChars::instantiate_spawns`（MapChars.cpp 旧:22-27）：收到重复
  cid 的 SPAWN_PLAYER 命中**空 if**——既不刷新已有角色也不新建，
  服务端纠偏/换图重发被静默丢弃。

## 修复（Change Summary）

- 远端角色**复用怪物回放内核** `MobMovementPlayback.h`（mobplayback::
  State/begin/step）：`send_movement` 把整包片段入回放状态，按片段
  duration 以 8ms（Constants::TIMESTEP）步进从段起点线性插值到
  (xpos,ypos)；MOVE_PLAYER 不携带显式 start，批次锚定首片段线上
  lastx/lasty（_ABSOLUTE/JUMPDOWN），否则保持当前回放位置。
- `update`：phobj 力与速度（hforce/vforce/hspeed/vspeed）**先清零**，
  插值位置直写 `phobj.x/y`——除片段回放外无任何来源可移动远端角色，
  本地物理模型不再分叉；帧位移写入 hspeed/vspeed 充当动画速度输入
  （对齐本地玩家的逐帧速度语义）；`update_fh` 维护图层/落地状态。
- stance 随片段切换（`sample.state`），队列耗尽后**保持末片段终点与
  末态**（静止等待下一包，不瞬移、不本地乱走）。
- 重复 SPAWN_PLAYER：`MapChars::instantiate_spawns` 对已知 cid 改调
  `OtherChar::respawn(position, stance)`——重锚位置、清空陈旧片段
  批次、恢复 stance，幂等重锚不再丢弃；`CharSpawn` 相应补
  `get_position/get_stance` 访问器。

## Impact Surface

- 客户端远端角色移动表现与位置权威性（OtherChar / MapChars / Spawn）；
  本地玩家、NPC、怪物（走 MobMovementPlayback 的另一套调用点）未动。
- 验证：`tools/verify/char-movement-playback.cpp` PASS（批次锚定、
  8ms 步进插值序列与片段切换 stance、耗尽保持末态、重复 spawn 重锚
  清片段且幂等、空批次不动）；`bash scripts/docker_build_wasm.sh`
  全量编译通过。
