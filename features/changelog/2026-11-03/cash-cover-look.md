# 现金覆盖外观：重登/他人视角保持与现金帽整帽渲染修复

承接 2026-10-29「现金商城装备纯装饰化」：现金装备取出落 CASH 栏、双击
以覆盖方式穿戴（隐藏 EQUIP 槽 100+x）、不占真实装备槽不影响属性。本轮
收口该特性的两个遗留缺陷，并附带加固物品栏 slotmax 之外的格子。

## 背景（bug 现场）

- 库存链路本就正常：覆盖穿戴后 MOVE_ITEM dst=-(100+eqslot)、服务端外观
  封包把现金件放进第一段「可见列表」。但客户端 `CharLook` 构造曾把封包
  第二段 `maskedequips`（被现金件覆盖而隐藏的普通装备）**后应用**，把
  第一段里同一部位的现金外观覆盖掉——重登/他人视角看到的帽子从现金帽
  退回普通帽（如 1003052 → 1002047）。
- 现金帽（如爵士礼帽 1003052）全部正面姿势帧 `z=capOverHair`，归入
  `Clothing::CAP_OVER_HAIR` 层；而 vslot（CpH1H5）分类为
  HALFCOVER/FULLCOVER 的绘制分支原先只画 `Layer::CAP`——现金帽一像素
  都不画（look 数据对、画面没帽子）。

## 机制

### 修复一：maskedequips 跳过已有可见对应者（数据）

`CharLook` 构造遍历 `entry.maskedequips` 时：

- 某部位键已存在于第一段 `entry.equips`（即被现金件覆盖的普通装备，
  其槽位已在可见列表出现）→ **跳过应用**，避免后写把现金外观盖掉；
- 无可见对应者照常应用——现金武器由服务端以 `-111` 单独发送，属此类。

### 修复二：HALFCOVER/FULLCOVER 补画 CAP_OVER_HAIR（渲染）

现金帽的正面姿势帧挂在 capOverHair z 轴。现在 HALFCOVER/FULLCOVER
分类分支在画完 `Layer::CAP` 之后补画 `Clothing::CAP_OVER_HAIR`
（`CharLook.cpp` 两条 draw 路径的对应分支共 4 处），整帽得以完整
呈现；HEADBAND/HAIRPIN 等其余分类不受影响。

### 附带：物品栏 slotmax 之外的格子锁定

- **绘制**：`UIItemInventory` 对 `slot > slotmax` 的格子调用
  `draw_slot_lock`——32x32 半透明黑遮罩 + 14 块 4x4 白色方块拼成的
  X 叉（偏移表 `UIItemInventoryNodes.h lock_x_offsets`），不再被误认
  为可用空间。
- **滚动条边界**：上/下界改用 `slot_rows(slotmax)` /
  `last_visible_slot(slotmax)` 推导（六行以内容固定按窗口背景的 24 格）；
  扩容（商城 +4 格）后可滚到最后一行、不再多滚一行。
- **交互守卫**：锁定格不再是有效点击/拖放目标——`slot_by_position`
  统一命中判定加 `slot <= slotmax` 上限（双击穿戴/取物、拖拽起手一律
  经它），移动物品目标 `drop_on_items` 同守卫。

## 改动位置

- `src/client/Character/Look/CharLook.cpp`：maskedequips 跳过逻辑；
  HALFCOVER/FULLCOVER 分支补画 CAP_OVER_HAIR（两条 draw 路径共 4 处）。
- `src/client/IO/UITypes/UIItemInventory.h` / `.cpp`：
  `draw_slot_lock`、滚动条边界、`slot_by_position`/`drop_on_items` 守卫。
- `src/client/IO/UITypes/UIItemInventoryNodes.h`：`slot_rows` /
  `last_visible_slot` / `lock_x_offsets`。

## 实测记录

- Docker WASM 全量重建通过（75s、零告警、`[100%] Built target
  JourneyClient`），产物 `build/JourneyClient.{js,wasm}` 更新为
  2026-11-03；同批收口的物理特性新增的 `_msplayer_*` 4 个探针随构建
  导出。
- e2e 全 PASS（`tools/verify/maplestory-webui/cash-cover-look-flow.mjs`，
  角色 test1Warr，39 行断言）：
  1. 基线：look=1002047（普通帽）、cover=0，CASH 栏 slot1 现金帽
     1003052 未穿；
  2. 物品栏 CASH 标签双击现金帽 1003052 → 覆盖穿戴：cover=1003052
     且 look=1003052；
  3. 干净登出重登后 look 仍 1003052（修复前此处回归为 1002047，即
     bug 现场回归断言）、隐藏槽 cover 探针同步保持；
  4. 装备窗 CAP 双击卸下 → look 回 1002047、cover=0、现金帽回到
     CASH slot1。
  产物 `/tmp/maplestory-webui-LVPqsT/`。
- 遗留人工待办：他人同图视角的覆盖外观目检（机制同路径，未单独双端
  实测）；FULLCOVER 类整脸现金帽的遮挡层级目测。

## 相关文档

- 前作（覆盖穿戴契约与服务端 patch 0005）：
  [2026-10-29 现金商城装备纯装饰化 + 购买背包格子](../2026-10-29/cash-shop-decorative-equips-and-slotbuy.md)
- 验收入册：`tools/verify/maplestory-webui/index.md`
  「现金覆盖装备外观全流程」一节。
