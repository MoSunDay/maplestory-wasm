# 现金商城装备纯装饰化 + 右栏详情面板改「购买背包格子」

## 背景

现金商城三项联动改动：

1. 商城装备（如现金帽 1002000）取出后落入 EQUIP 物品栏并占用真实装备槽，
   穿戴还会参与属性计算——纯装饰性现金外观件不应影响战斗数值，也不应
   与正常装备抢槽位。
2. 商城右栏 y294..388 的「物品详情」面板（分类/期限/剩余等静态字段）
   信息价值低，占用黄金版面。
3. 希望在商城内直接购买背包扩格（+4 格），服务端 CASHSHOP_OPERATION
   `0x06` 与 `0x60` 回包 HeavenMS 已实现，但客户端无入口。

## 方案

### 服务端（patch 0005，link_repos/MapleStory-Server）

- `CashOperationHandler 0x0D`：Equip 取出目标栏 EQUIP→CASH（宠物/道具券不变）。
- `MapleInventoryManipulator`：`equip(dst <= -100)` → `equipCashCover`
  （跳过职业/性别/等级校验；旧覆盖件挤回 CASH，CASH 满则整单拒绝）；
  `unequip(src <= -100)` → `unequipCashCover`（目标栏 CASH，满/占用拒绝）。
- `ModifyInventory` 新增 `(mode, item, oldPos, invType)` 覆盖构造器：
  CASH 栏装备穿上的公告 type 必须显式写 CASH(5)——按 itemid 推导会错成
  EQUIP(1)，客户端 swap 的源栏就错了。
- `MapleCharacter.recalcEquipStats` 跳过 `position <= -100` 的覆盖件；
  `ItemFactory` 让 CASH 栏中的 Equip 走装备持久化路径（重登不丢）。
- `MaplePacketCreator`：addInventoryInfo 的 USE/SETUP/ETC/CASH 四段位置
  改按 byte 写（客户端在这四段按 byte 读，装备写 short 会整包错位、
  SET_FIELD 解析 underflow）；equippedCash 位置原样发 101+（客户端
  `-pos >= 100` 路由到隐藏 EQUIP 覆盖槽 100+x）。

### 客户端（src/client）

- `ItemParser::parse_item_auto` 加 `equip_to_cash` 参数：0x68 取出与
  SET_CASH_SHOP 把装备解析进 CASH 栏；`UIItemInventory` 双击按标签分派
  （CASH 标签装备走覆盖穿戴、EQUIP 标签按 `EquipData::is_cash()`），
  update_slot 为 CASH 标签装备画角标。
- DETAILS 面板（布局常量、绘制、命中）全删；原位新增 SLOTBUY 面板
  (508,294) 288×94：标题行 + 右侧说明 + 2×2 pill（装备/消耗/设置/其他，
  标签实时显示当前格数，达上限或不可购买时置灰）。
- `UICashShopSlots.cpp`：点击 pill → 校验（上限/余额）→ UIYesNo 确认 →
  `BuyCashInventorySlotsPacket`（0x06）→ `0x60` 回包 `set_slotmax` + 结算，
  余额随 `0x144` 推送扣减；`CashShopSettlement` 增加 SLOTS 待结算种类。
- 探针：`msinventory_cover_equipped`（EQUIP 100+slot）、`mscashshop_slotmax`、
  `mscashshop_slotbuy_x/_y`。
- `Cryptography::check_length` 的 `static_cast<int16_t>` 改 `uint16_t`：
  大包（SET_CASH_SHOP ≈39KB）长度低字节 ≥0x80 被符号扩展误判为非法长度。

## 踩坑记录（协议语义）

- `modifyInventory` 每包只有一个 movement 字节、整包所有 mod 共享；
  「旧覆盖 MOVE_UNEQUIP 让位」与「新覆盖 MOVE_EQUIP 穿上」若混在一包，
  后写的 movement 覆盖先写的，客户端按错误 movement 应用双向 swap，
  EQUIPPED/EQUIP/CASH 三栏连环错乱（表象：真实装备被换进 CASH、覆盖件
  残留等）。修复：两包分开公告，先让位后穿上。
- 客户端 `Inventory::swap` 是双向交换（源/目的内容互换），服务端公告
  顺序必须与客户端预期槽位状态严格对齐。

## 踩坑记录（排查杂项）

- 「双击角色卡后卡在选人界面」排查结论：不是客户端死循环。`UICharSelect::doubleclick` 在 nx 驱动的 `ui_initialized` 完成前静默丢弃双击，PLAYER_LOGIN 根本没有发出（服务端日志停在 CHARLIST 即证）；页面保持存活、无新请求。单发双击的诊断脚本必现，正式 e2e 的 4×重试循环天然免疫；此前「CASH 栏存在装备触发卡死」的二分结论系双击时序巧合，与 CASH 装备无关（已验证 type=1 的 CASH 栏覆盖件登录进场正常，`msinv_slot_item(5,1)=1002000`，退出存档往返不丢）。
- DB 造数：`inventoryitems.type` 是 ItemFactory 工厂列（1=INVENTORY 角色背包、3/4/5=商城仓储、7=全账号仓储），角色级行必须 type=1 才会被登录装载与存档删除；type=3 的角色行服务端完全不可见（不加载、不发送、不删除）。characterid/accountid 归属同样决定装载路径，插错只会静默空跑——诊断时先 `SELECT type, characterid, accountid` 三列核对再跑浏览器。

## 验收

- `tools/verify/maplestory-webui/cash-shop-take-equip-flow.mjs` 全 PASS：
  取出落 CASH 栏、EQUIP 标签无新副本、覆盖穿戴（外观=覆盖件、真实装备
  标记与 15 项属性总值不变）、卸下回 CASH、退出商城正常。
- `tools/verify/maplestory-webui/cash-shop-slots-flow.mjs` 全 PASS：四 pill
  几何、+4 格结算（slotmax 24→28）、-4000 NX、其余栏不受影响、回图后
  持久化（characters.equipslots=28）。
- 服务端补丁重放校验：逆放 0001..0005 → 正放 0001..0005 → `diff -r`
  与 checkout 完全一致。
