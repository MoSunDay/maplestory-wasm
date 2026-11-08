# 世界地图区域链接命中修复：数值序迭代 + 逐像素 alpha 命中

## 症状（Context）

- 世界地图（W 键）能正常打开并显示当前区域，但**点击区域链接无法切换到
  正确的区域大地图**：点玩具城岛屿进了 ElNath；部分区域（070/080/090，
  即武陵/滨海/尼哈沙）完全不可点；海域点击行为不可预期。
- 从子区域返回根图时偶发落在错误层级（resolved hash 与预期不符）。

## 根因（三个独立缺陷叠加）

1. **NX 子节点按名称字符串序迭代**：`MapLink` 子节点名 "0".."12" 按字符串
   排序得到 `0,1,10,11,12,2,3,...,9`，逐个绑到 `BT_LINK0..9` 后按钮与
   视觉链接整体错位；且 `BT_LINK0..9` 十个按钮的上限把名字排在末尾的
   "7"/"8"/"9"（→ WorldMap070/080/090）挤出——这三个区域永远不可点。
2. **linkImg 矩形大面积重叠 + 基类「首个命中矩形获胜」分发**：区域高亮图
   的外接矩形互相覆盖（ElNath 278×217 的矩形盖住整个玩具城岛屿），
   `UIElement::send_cursor` 按第一个命中矩形分发，导致点玩具城命中
   ElNath 的按钮。
3. **`update_world(parent_map)` 别名 bug**：形参 `const std::string& map`
   引用成员 `parent_map`，函数内 `parent_map` 先被重赋值、之后
   `resolved_map = map` 拿到的是新值——ESC 返回根图时 resolved 记录错误。

## 修复（Change Summary）

全部位于 `src/client/IO/UITypes/UIWorldMap.{h,cpp}`：

- **数值序迭代**：匿名命名空间新增纯函数 `link_node_order`——纯数字
  节点名按数值排序，其余按名称稳定排序；`update_world` 收集 MapLink
  后先排序再绑定。按钮枚举扩展 `BT_LINK10..BT_LINK15`（根图 13 链接）。
- **逐像素命中**：`link_alpha_mask`（static 纯函数）把 `nl::bitmap::data()`
  返回的 RGBA 原始像素（LZ4 解压、共享 buffer）压成 alpha>0 的字节掩码，
  data() 为空时回退整矩形；`link_hit`（static 纯函数）矩形粗筛 → 掩码
  过滤 → 最小面积决胜。linkImg 是 42-81% 不透明的透明高亮图，alpha 命中
  即官方视觉形状（实测玩具城中心仅玩具城不透明）。
- **`send_cursor` 精确分发**：命中形状时镜像基类语义（press-id、音效、
  MOUSEOVER 状态）；未命中任何形状时临时 `set_active(false)` 全部链接
  按钮再委托 `UIDragElement`，完毕恢复——防止高亮矩形吞掉关闭/搜索
  按钮的点击。
- **按值传参**：`update_world(std::string map)` 改按值，消除与成员
  `parent_map` 的引用别名。

探针 `msworldmap_*`（active/geometry/link_count/link_bounds/link_state/
link_target_hash/link_hit/resolved_hash/user_hash/parent_hash/name_hash/
search）新增于 `src/client/Gameplay/Probes/UIWorldMapProbes.cpp` 并经
`src/client/CMakeLists.txt` 导出，供 e2e 断言。

## 验证

- e2e `tools/verify/maplestory-webui/worldmap-region-links.mjs`（21 项断言
  全绿）：W 落在用户区域 WorldMap021；ESC 链 021→020→根图（首个 ESC 先关
  post-Chaos 搜索面板是正确行为——UI.nx 为 post-Chaos 代，Map.nx 的
  WorldMap 数据为 v83 BaseImg 640×470）；根图 13 链接全部可点且 resolved
  命中各自目标（000/010/020/030/040/050/060/070/080/090/100/140/142）；
  海域点击不导航；Victoria 区域图内子链接递归可点；关闭按钮生效、重开正常。

## 备注

- 已知边界：mapid 前缀 22/23/24（玩具城/水下世界/神木村）的 user_map 无
  对应 WorldMap 节点时仍回退世界根图，后续可按 MapList 反查最深层区域。
