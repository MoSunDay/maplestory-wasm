# 地图机关伤害：地刺/陷阱踏入即受击

## 现象

经过地刺、陷阱（跳跳任务地刺条、忍苦树林尖刺、椰子赛陷阱洞等）不掉血。

## 根因

机关伤害整条链路缺失：

1. **数据侧**：本数据源（079 系 CN WZ 重导出）的地图没有 v83 原生 `ob`
   伤害节点（5292 张图 0 个）；机关伤害定义挂在 **Obj 定义**上——
   `Map.nx Obj/<oS>.img/<l0>/<l1>/<l2>` 携带 `obstacle`（≥1 即机关）与
   `damage`（固定伤害值），全库共 224 个（`trap.img/trap/nature/*` 跳跳图
   地刺、`event.img/6thMaple/trap/*`、`trapGL.img/ckPQ/menhir/*` 50000 即
   秒杀石柱等）。
2. **客户端**：`Stage::load_map` 只加载图块/对象/背景/平台/传送门，
   `Stage::update` 只有怪物动作伤害与接触伤害两条路径，没有任何机关碰撞
   检测；`TakeDamagePacket::From` 也没有地图机关来源。
3. **服务端**：`TakeDamageHandler` 对 `damagefrom != -3/-4` 一律要求能按
   `oid` 解析出攻击怪物，`attacker == null` 时提前 `return`——无怪物来源
   的 `-2` 机关伤害包即使发过去也会被静默丢弃。

## 修复

### 客户端

| 文件 | 行数 | 说明 |
|---|---|---|
| `Gameplay/MapleMap/MapHazards.{h,cpp}` | 80/93 | 新纯函数单元：`collect_map_hazards` 遍历地图 8 层 obj，解析 Obj 定义上的 `obstacle`/`damage`，用首帧位图的 `origin`+尺寸计算足迹矩形（水平翻转围绕锚点镜像，与 `Texture`/`Obj` 摆放同源）；`hazard_bounds`/`find_map_hazard_hit` 为头文件内联纯函数 |
| `Gameplay/Stage.{h,cpp}` | — | `load_map` 收集机关、`clear` 清空；`update` 在无敌门与怪物动作伤害之后、接触伤害之前，用玩家矩形（x±10、脚部上 40px）与机关足迹求交，命中即 `show_damage`（本地伤害数字 + 2 秒无敌帧防刷屏）并按 `damagefrom=-2` 发 `TAKE_DAMAGE(0x30)` |
| `Net/Packets/AttackAndSkillPackets.h` | — | `TakeDamagePacket::From` 增加 `OBSTACLE = -2` 与固定伤害构造（element=0、mobid/oid=0） |

### 服务端（link_repos/MapleStory-Server，已重编译重启）

`TakeDamageHandler.java`：`attacker == null` 的早退改为
`damagefrom != -2` 才退出——`-2` 机关伤害按无怪物来源继续走既有扣血/
广播路径（魔法守护/冒险家守护等后续逻辑本就以 `attacker != null` 守卫，
`-2` 分支此前就预留过判断）。

## 验证

纯函数单测（命令已登记 [tools/verify/index.md](../../../tools/verify/index.md)）：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client -Isrc \
  tools/verify/map-hazards.cpp -o /tmp/map-hazards && /tmp/map-hazards
```

```
ALL CHECKS PASSED
```

真实数据离线核对（复用 collector 链接 NX）：地铁三号线 B1（103000900）
31 个地刺条、忍苦树林一层（101000100）6 处尖刺、椰子比赛（109080000）
23 个 20 伤害陷阱洞均正确识别，足迹矩形与摆位一致；普通地图（如
102020300 西部岩山Ⅳ）0 机关不误伤。

WASM 重编通过（`scripts/docker_build_wasm.sh`）；服务端单文件 `javac`
重编译并经 `scripts/game_server_restart.sh` 重启，6 监听口全部就绪。

## 相关文档

- [网络协议 0x0030 TAKE_DAMAGE](../../../docs/ms-network-protocol.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)
- [网络层](../../../agents/client/Net/index.md)

## 2026-11-01 追补：迷宫喷气蒸汽陷阱（无 damage 字段）

艾琳迷宫系列（105060000/105070000/105080000/105090000 及 105090100-
105090400、105090900、211041100-211042200、280020000/1、801000110/210、
921100000、990000611-641 水路之迷宫等）的喷气/蒸汽机关定义是
`Obj/trap.img/steam/0/{0,1,2}`：只有 `obstacle=1`、**没有 `damage`
字段**（解析为 0），被 `collect_map_hazards` 的 `obstacle>=1 且
damage>=1` 双门槛整体跳过，喷气全程不落伤。

修复（`MapHazards.{h,cpp}`）：

- 收集门槛改为只看 `obstacle >= 1`；定义缺 `damage`（<1）时按新增
  `DEFAULT_OBSTACLE_DAMAGE = 100` 兜底（steam/1-4 兄弟设计族伤害为
  100/130/160、100/150、100、150，取代表值 100）。
- 足迹优先取机关首帧自带的 `lt`/`rb` 碰撞向量（steam/0/0 帧 0：
  lt=(-18,-68)、rb=(16,0)，即整根喷气柱），无 lt/rb 的机关回退原
  origin+位图尺寸路径；新增内联纯函数 `hazard_bounds_from_hitbox`
  承接翻转镜像。

验证：`tools/verify/map-hazards.cpp` 补喷气断言（lt/rb 碰撞盒纯函数
两侧 + 带 Map.nx 实测 105060000 ≥38 个机关、全部 damage≥1、(717,522)
喷气柱与玩家盒相交；990000620 ≥3），命令已更新登记于
[tools/verify/index.md](../../../tools/verify/index.md)；地铁三号线 B1
31 个地刺条等既有机关回归不变，普通地图仍 0 误伤。
