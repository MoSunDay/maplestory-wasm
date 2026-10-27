# 落地容差带：机制性修复"不停往下掉"

## 症状

多张地图反复出现"进图/传送/走斜坡后人物（或宠物、怪物）一路坠到地图底部，
被恢复块弹回又再掉"的循环。历次修复（宠物 warp 锚定 `anchor_after_warp`、
OrbisPQ 改 wz 的 cy）都是点位级补丁，换图或传送后复发。

## 根因（机制层面）

`Footholdtree` 判定"脚下的地面"用零容差严格比较：

- `get_fhid_below`：`ycomp >= fy` —— 平台表面必须不低于对象 y；
- `update_fh`：`onground = (phobj.y == ground)` —— 精确相等才算落地；
- `limit_movement` 只对 `get_fh(phobj.fhid)` 这一个面做碰撞。

出生点/传送点 y 是整数，斜面表面是插值出的非整数（如 OrbisPQ cy=-41 vs
表面 -41.8）：只要 y 比表面低哪怕 0.1px，该平台就被永久跳过，`limit_movement`
对着空 fh（ground=0）没有任何东西能拦住对象，直坠 `borders.second()`。
服务端（MovePlayerHandler）完全信任客户端坐标，不会纠正。

## 变更

- `FootholdTree`：`get_fhid_below` 增加 `tolerance` 参数（默认 0，
  CHECKBELOW/`get_y_below` 语义不变）；`update_fh` 的两个落地查询带
  `GROUND_EPS = 5.0px` 容差；落地判定前对"不在上升（`vspeed >= 0`）且
  在表面下方 `(0, GROUND_EPS]` 内"的对象向下吸附 `phobj.y = ground`
  （只接下落物，跳起穿板不受影响）；下跳起点
  `groundbelow = ground + GROUND_EPS + 1.0`，保证 Alt+↓ 起点低于容差带、
  仍能正常穿板。
- `Player::respawn`：补 `fhid=0 / onground=false / fhlayer=0`——宠物
  `anchor_after_warp` 的玩家版，复活/传送后让物理引擎带容差重新找地面，
  而不是背着旧 fhid 判断。`Combat` 传送已置 `fhid=0`，无需改。
- 连带收益：Mob/NPC 直接吃服务器 fhid（`Mob.cpp`/`Npc.cpp`），容差带同样
  保护它们，"cy 比斜面高 0.8px 就掉下层"不再需要改 wz 数据。

## 验证

- 新增 `tools/verify/foothold-grounding-nx.cpp`（真实 `assets/Map.nx`、
  地图 102000002）：
  `c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function -Isrc/client -Isrc -Isrc/nlnx tools/verify/foothold-grounding-nx.cpp src/client/Gameplay/Physics/Physics.cpp src/client/Gameplay/Physics/FootholdTree.cpp src/client/Gameplay/Physics/Foothold.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/foothold-grounding-nx && /tmp/foothold-grounding-nx assets/Map.nx`
  PASS。改动前（git HEAD 的 FootholdTree）同用例先行复红：平坦/斜面下方
  0.8px 出生全部 FAIL（复现根因）。OrbisPQ 定向场景（200080101 招募 NPC
  缺陷值 cy=-41，fh14 斜面 x=198 处表面 -41.8）：修复前独立探针终态
  fhid=26、y=168.5（即 2026-10-26 记录的 215px 层差缺陷，当时靠改 wz cy
  绕过）；修复后落在 fh14、y≈-41.8，从机制上消除、无需 wz 补丁。
- 宠物物理回归 `tools/verify/pet-physics-nx.cpp` PASS。其中"保留旧 fhid
  的 warp"子用例语义更新：容差带现在把该场景直接接回主人平台
  （grounded、y=117），不再直坠下层 y=167——机制修复覆盖了原补丁场景。
- 全量构建：`scripts/docker_build_wasm.sh -j4` 通过。

## Related Docs

- [游戏世界](../../../agents/client/Gameplay/index.md)
- [验证工具索引](../../../tools/verify/index.md)
