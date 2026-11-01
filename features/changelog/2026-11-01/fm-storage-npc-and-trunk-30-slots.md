# 自由市场仓库 NPC 与 30 格 Trunk 窗口修复

## 背景

- 自由市场（910000000）此前没有仓库 NPC，玩家无法在 FM 内存取物品；
- 既有 Trunk/STORAGE 链路存在三处断裂：
  1. **协议错位**：服务端 `getStorage/storeStorage` 以 short 写 slots，客户端
     按 byte 解析，恒错位 1 字节，0x16/0x0D 物品列表 count 读 0 被静默丢弃
     ——窗口能开但左右列恒空、取出/存入均不可用；
  2. **装备块解析**：0x16 里装备条目需 skip(30) 属性块，客户端少 skip 2 字节，
     仓库内混存装备时后续条目全部串位；
  3. **ESC 白名单**：UIStorage 不在 escape_types 里，玩家按 ESC 关不掉窗口，
     只能点 EXIT。

## 变更

### linked server（`scripts/server-patches/0006-storage-30-slots-and-type-buckets.patch`）

- `MapleStorage.java`：MAX_SLOTS 16→30；`getStorage/storeStorage` slots 按
  byte 写出（与客户端解析对齐，修复 1 字节错位）；
- `StorageProcessor.java`：store/take 按页签类型分桶校验与寻位
  （filterItems per InventoryType），装备/消耗/ETC 各自独立 30 格上限。

### 客户端

- `NpcInteractionHandlers.cpp` StorageHandler：0x16 装备条目 skip(30) 对齐
  服务端 30 字节属性块；0x09/0x0D（set_slots/set_items_for_tab）只更新数据
  不动窗口，尾部 UI::get().enable() 保持输入恢复；
- `UIStorage.cpp`：网格按页签偏移删除（tab 内索引映射），修复混存时取错
  物品（装备排在页签前导致 ETC 取出走装备索引）；
- `UI.cpp`：escape_types 加入 STORAGE，ESC 可关窗（发 mode 8 leave）。

### 数据

- DB：storages.slots=30；FM 910000000 补 plife 行（NPC 1100000 x=450），
  地图 NPC 17→18。

## 验证

- `tools/verify/maplestory-webui/fm-storage-flow.mjs`（:8000 新构建 +
  pqLogin 健壮登录 + fm-map-retry 地图重试）：开仓、ETC 混存解析、页签内
  索引取出、30 格满仓拒存（0x11）、空位存入、ESC 关窗全链路 PASS，DB 断言
  （取出命中 ETC 而非装备、仓库行数=MAX_SLOTS-2、存入落库）全 PASS。
