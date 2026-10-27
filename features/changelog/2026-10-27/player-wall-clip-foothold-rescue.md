# 人物沿地图底部穿墙：虚拟地板幽灵态与幻影地面

## 症状

用户复报：修宠物移动后多次看到人物移动穿墙。（核对结论：最近两笔宠物
提交未改人物路径——宠物使用独立 `PhysicsObject`，`Char::get_fhid()` 只读；
但人物与宠物共用 `Physics::move_object` / `Footholdtree` 内核，穿墙根因在
内核历史改动中，宠物侧对悬崖/坠落场景的高强度使用把它暴露了出来。）

## 根因（两个，均在人物/宠物共用的碰撞内核）

1. **虚拟地板幽灵态**（#88 引入）：对象坠过最低 foothold 后，
   `update_fh` 的"图底回收"在拿不到 last-known foothold 时把对象钉在
   `borders.second()` 虚拟地板上且 `fhid` 保持 0。`get_wall(0, …)` 沿
   默认构造的空 foothold 走链，只能返回 ±30000 地图边界——**内部墙体
   判定完全失效**，对象沿图底滑行穿过所有墙。坠落过程本身即会把
   `fhid` 清零（链末 + 下方无平台的列 `get_fhid_below` 返回 0），所以
   任何掉进无平台列的对象都会进入该状态。
2. **幻影地面**（基线遗留）：`limit_movement` 的落地判定用
   `ground_below` 对 x 跨度外做斜面外推，平台侧边的"延长线"也被当作
   地板，会把漂移到平台侧方的下落对象卡成"半空中冻结"
   （`limitity` 每帧拉回外推 y，`update_fh` 又判定悬空）。

## 变更

- `Footholdtree::update_fh`：图底回收优先锚定到**本列最低真实
  foothold**（新增 `get_lowest_fhid(fx)`，扫 `footholdsbyx` 列取最大
  ground_below；x 已在跨度内，无横向瞬移、不穿墙）；仅无平台列退回
  last-known（原 #88 行为），再退回虚拟地板。
- `Footholdtree::limit_movement`：竖直支撑判定加 x 跨度条件——
  `crnt_x` 或 `next_x` 至少一个在当前 foothold 跨度内才判定落地，
  否则只做 borders 边界钳制；消灭幻影地面与侧边冻结。

## 验证

- 新增 `tools/verify/player-wall-clip-nx.cpp`（真实 Map.nx 四图）：
  幽灵态重锚、缺口走落后回走重锚、获救后邻墙 1200 帧内报 WALL；
  修复前"幽灵重锚/贴地"两断言红，修复后全绿。
- 回归：`pet-physics-nx`（斜坡/墙体/终端边缘/warp 锚定/边缘纪律）、
  `foothold-grounding-nx`（落地容差带/下跳/上升穿越/下层链坠落）全绿。
