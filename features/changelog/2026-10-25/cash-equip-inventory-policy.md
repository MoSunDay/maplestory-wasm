# 点卡装备：CASH 目录归类 + 穿戴仅改外观不影响属性

## 背景

用户反馈「点卡买的装备应该出现在 cash 目录，带上装备后应该只改外观
不影响属性状态」。v83 中点卡（NX）装备与普通装备共用 ID 空间（帽子
1002xxx、衣服 104xxxx、武器皮肤 1702xxx 等），只能靠 Character.nx 的
`info/cash` 标志区分，而客户端此前只按物品 ID 前缀归类，导致：

1. 点卡装备进 EQUIP 背包而非 CASH 目录；
2. SET_FIELD 的 CASH inventory 段若携带装备，会被按堆叠物品格式解析
   （封包错位）；
3. 穿上后进入 EQUIPPED 参与 recalc_stats 属性求和；
4. 角色外观封包里的 cash 覆盖列表（maskedequips，槽位
   -101..-112/-111）虽被解析却从未应用到 CharLook 渲染——穿了点卡
   装备外观不变。

本轮按 `info/cash` 重建点卡装备全链路：归类进 CASH 目录、封包按 equip
线格式解析、穿戴落入 EQUIP 隐藏槽不进 EQUIPPED、外观覆盖应用到渲染。

## 变更

### 目录归类与数据

- `src/client/Data/EquipData.h/.cpp`：暴露 `is_cash()`（nx `info/cash`，
  此前是读了没用的死数据）。
- `src/client/Character/Inventory/InventoryType.cpp`：`by_item_id` 对
  装备前缀查 `EquipData::is_cash()`，点卡装备归 CASH 目录。

### 封包解析与穿戴状态机

- `src/client/Net/Handlers/Helpers/ItemParser.cpp`：封包格式分派改为
  按每个条目开头的 wire type 字节（3=pet、1=equip、2=stackable），
  不再由外层 InventoryType/EquipData 推断；修复 CASH 目录内 equip 被
  按堆叠物品格式解析、最终触发 SET_FIELD `Stack underflow` 的问题。
  `add_equip` 中 slot<-100 的已穿 cash 装备落入 EQUIP 隐藏槽
  （100+）而非 EQUIPPED。
- `src/client/Character/Inventory/Inventory.cpp`：`modify` 支持 cash
  穿脱——穿上进隐藏 EQUIP 槽（目标槽 100+）、脱下回 CASH 目录；负槽位
  归一化区分 EQUIPPED 与隐藏槽；`remove` 的 CASH 分支补 equip 元数据
  清理。
- `src/client/Net/Packets/InventoryPackets.h`：`EquipItemPacket`/
  `UnequipItemPacket` 增加 cash 参数（源目录 CASH、槽位
  ±(100+eqslot)）。
- `src/client/Character/Player.cpp`：`change_equip` 重写——100+ 为 cash
  外观槽，cash 覆盖优先于同槽普通装备，脱下 cash 自动回落普通装备
  外观；穿上普通装备时若同槽有 cash 则外观不变。

### 外观渲染

- `src/client/Character/Look/CharLook.cpp`：构造时应用
  `LookEntry::maskedequips`（其他玩家/角色选择的 cash 外观覆盖生效；
  id≤0 跳过，并送入 equip_observer 供按需资源预取）。

### UI 与交互

- `src/client/Net/Handlers/InventoryHandlers.cpp`：MOVE_EQUIP/
  MOVE_UNEQUIP 联动——cash 槽（100+）刷新装备窗口对应遮盖槽位与
  CASH 页，避免把 100+ 槽位直接传给装备窗口导致枚举越界。
- `src/client/Net/Handlers/CashShopHandlers.cpp`：商城取出（0x68）改按
  原始负槽位判断穿戴，cash 装备正确走 change_equip。
- `src/client/IO/UITypes/UIItemInventory.cpp`：CASH 页双击点卡装备即可
  穿戴（宠物行为不变）。
- `src/client/IO/UITypes/UIEquipInventory.cpp`：装备窗口每个槽位优先
  显示 cash 覆盖装备；双击脱下时识别 cash 并送回 CASH 页。

### 语义约定

已穿 cash 装备统一存于 EQUIP 目录隐藏槽（100+装备槽位），与 SET_FIELD
的 cash-equipped 段序列化约定一致；EQUIPPED 只含普通装备，
`recalc_stats` 仅对 EQUIPPED 求和，从而保证「只改外观、属性状态不受
影响」。

## 验证

- Docker WASM 全量构建通过（`scripts/docker_build_wasm.sh`，
  emscripten 4.0.21，Release）。
- 既有回归：`tools/verify/character-data-parser.cpp`、
  `tools/verify/login-parser.cpp` 通过。
- 新增 `tools/verify/cash-equip-policy.cpp`（315 行）：真实
  Character.nx 采样（cash 帽 1000000 / 普通帽 1000025）驱动假封包与
  Inventory 状态机，断言：归类（含 2000000→USE 回归）、CASH 目录
  equip 封包逐字节解析、MOVE_EQUIP 进隐藏槽 101 且不在 EQUIPPED、
  STR+30 的 cash 装备不进面板而 STR+5 普通装备生效、脱下回 CASH 后
  普通装备属性保持、SET_FIELD -101 槽位解析。运行命令已登记在
  `tools/verify/index.md`。

## Related Docs

- [角色系统](../../../agents/client/Character/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
