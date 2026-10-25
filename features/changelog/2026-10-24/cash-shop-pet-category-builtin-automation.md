# 现金商城宠物：专属分类 + 全宠物内建自动化能力

## 背景

现金商城此前没有宠物入口：客户端分类列（7 类）把宠物物品和宠物能力装备
散落在「全部/其它」里，商品名解析查 `Cash.img` 为空只能显示「物品 5000xxx」；
想获得宠物只能靠新角色礼包。同时自动 HP/MP 药水袋必须装备 `1812004/1812005`
才生效，商店买来的宠物开箱即无自动化能力，体验割裂。

本轮在商城新增专属「宠物」分类、放开宠物购买，并让**所有召唤宠物**内建
金币磁铁、物品袋、飞行靴与自动 HP/MP 药水袋能力（魔法秤仍需实际装备）。

## 变更

### 商城客户端（目录与命名）

- `src/client/IO/CashShop/CashShopView.h/.cpp`：`CATEGORY_COUNT` 7→8，
  新增 `PET_CATEGORY=7`；`category_for_item` 迁移为纯函数——宠物
  （物品 ID `5000000–5000999`）与宠物能力装备（`1812000` 段）统一归入
  「宠物」分类，`category_name` 增加对应文案。
- `src/client/IO/CashShop/CashShopModel.cpp`：商品名解析修复——宠物名
  取 `String.nx` 的 `Pet.img`、宠物装备名取 `Eqp.img` 的 `Eqp/PetEquip`
  （index 81），不再显示「物品 5000xxx」兜底名。
- `src/client/IO/CashShop/CashShopLayout.h`：图例整体下移 34px 容纳第 8
  行分类，新增 `LEGEND_NOTE_SEPARATOR_POS{10,412}` 常量，
  `UICashShopPanels.cpp` 改用常量替代硬编码坐标。

### 宠物能力（客户端 + linked server）

- `src/client/Gameplay/Pet/PetCapabilityPolicy.h/.cpp`：能力判定改为
  `is_pet_item`（`itemId/1000==5000`），任意召唤宠物内建
  磁铁(1)|物品袋(2)|自动 HP(4)|自动 MP(8)|飞行靴(16)（掩码 31）；
  魔法秤(32) 仍需装备 `1812006`。
- `link_repos/MapleStory-Server/src/net/server/channel/handlers/
  PetLootHandler.java`：新增 `petHasBuiltinLootAbility`
  （`ItemConstants.isPet`），meso/item 拾取分支对内建能力放行，与服务端
  装备复核口径一致（`posix-compile.sh` 全量编译 + 容器重启生效）。

### 数据与验收工具

- `src/client/data/cash-shop-v83.csv`：确认与 linked server
  `wz/Etc.wz/Commodity.img.xml` 一致的在售目录（宠物 SN `6000xxxx`、
  宠物能力装备 SN `6010xxxx`，双侧 canBuy/onSale 口径一致）。
- 新增 `tools/verify/cash-shop-pet-category.cpp`（宠物分类商品
  契约单测）；`tools/verify/pet-automation.cpp` 断言更新为全宠物内建
  掩码 31；`tools/verify/maplestory-webui/cash-shop-pet-flow.mjs`
  全链路浏览器 e2e。

## 验证

- 单测：`cash-shop-view`、`cash-shop-pet-category`、`pet-automation`、
  `cash-shop-ui-nx` 全部 PASSED。
- `scripts/docker_build_wasm.sh` 客户端重建成功；游戏服经
  `scripts/game_server_restart.sh`（docker stop/start 方式）重启生效。
- e2e（三次完整通过，产物 `/tmp/cash-shop-pet-flow`：8 截图 + 2 状态
  JSON）：

```bash
PET_FLOW_ACCOUNT=test1 PET_FLOW_PASSWORD=test1 PET_FLOW_CARD=0 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/cash-shop-pet-flow.mjs
```

  覆盖：宠物分类过滤（当时 42 件；目录后续并入宠物穿戴装备 1802xxx 后为
  87 件，UI 验收脚本改为从内嵌 CSV 动态推算期望值）→ SN 60000001 搜索高亮
  5000001 → 购买
  （NX -4900）→ 双击仓库取出 → 召唤（能力掩码 31）→ 地图 40001 打怪
  宠物自动捡取（meso +5、confirmed OID 1000000037 移除）→ keymap 91/92
  绑 2000005 后压低 HP/MP 到 30% 重登，自动嗑药恢复 ≥99%（药水 198→197）。
- 运行环境注意：长跑 e2e 建议用
  `systemd-run --unit=cashshop-pet-e2e ...` 包裹，避免会话回收中断。

## Related Docs

- [现金商城](../../cash-shop/index.md)
- [永久新手宠物与自动化](../../pet-automation/index.md)
- [Web UI 端到端验收](../../../tools/verify/maplestory-webui/index.md)
