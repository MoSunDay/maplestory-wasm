# 现金装备永久化：不再提示 90 天到期

## 背景

用户反馈现金商城里的装备显示「90天」到期。排查链路：

- v83 WZ `Etc.wz/Commodity.img` 给多数现金装备标 `Period=90`（构造器还把
  `Period=0` 强转 90），linked server `CashShop.CashItem.toItem()` 在
  `ItemConstants.EXPIRING_ITEMS`（恒 true）下给**所有**售出物品盖
  `now + 24h*period` 的到期戳；
- `MapleCharacter.expirationTask()` 每 60s 扫描，到期物品 `itemExpired`
  公告并删除——90 天后已购现金装备真的会消失；
- 客户端目录 `src/client/data/cash-shop-v83.csv` 原样内嵌 WZ Period，
  卡片期限标签与推荐面板「分类 · 期限」直接显示 90天。

现金装备在本服是纯装饰外观件（0005 补丁：落 CASH 栏、EQUIPPED -100-x
覆盖穿戴），不应有租期。

## 变更

### linked server（`scripts/server-patches/0007-cash-equips-permanent.patch`）

- `CashShop.java toItem()`：装备判定提为局部 `isEquip`，改为
  `!isEquip && EXPIRING_ITEMS` 才盖到期戳——EQUIP 类现金购买保持
  `Item.expiration = -1`（封包写入器把 -1 映射为永久哨兵值）。
  `period==1` 优惠券小时级特例与非装备分支不变；宠物、优惠券等非装备
  仍按 WZ Period 计期。
- `MapleCharacter.java expirationTask()`：循环体开头加遗留数据迁移守卫，
  CASH 栏中带盖章到期时间的 `Equip` 就地迁移为 -1 并 `forceUpdateItem`
  落库，而不是按到期删除——历史已购现金装备同样转为永久。

### 客户端

- `CashShopView.h` 新增纯函数 `effective_period(item_id, period)`：装备段
  物品 ID（`1xxxxxx`）期限归零，其余原样保留 WZ Period。
- `CashShopModel.cpp load_catalog()` 载入时统一归一化，卡片期限、推荐面板
  期限与「限时」徽章（period 1-30）共用同一口径：现金装备一律显示「永久」
  且不再挂限时徽章；宠物/优惠券等照旧显示租期。CSV 本身不动，保持 WZ
  原始数据，策略收敛在代码里，后续从 WZ 重新生成目录不会回退该行为。

## 验证

- `cash-shop-view`：新增期限归一化断言（装备/现金武器/宠物穿戴装备 90/14
  → 0，宠物 90→90、双倍券 1→1、喇叭 14→14）与「period=0 装备不得挂
  LIMITED 徽章」断言，PASS。
- `cash-shop-pet-category`：徽标优先级回归 PASS。
- linked server `posix-compile.sh` 全量编译通过，`docker restart
  ms-game-server` 后登录口 8484 就绪。
- WASM 构建：`docker compose run --rm wasm-builder` 通过。
- 浏览器 e2e（`tools/verify/maplestory-webui/cash-shop-equip-permanent-flow.mjs`，
  23 项断言全 PASS）：test1 登录进商城搜 SN `20000016` → 目录卡与「本期推荐」
  面板截图经 tesseract(chi_sim) OCR 确认显示「装备 · 永久」、无「90天」与
  「限时」徽标 → 真实购买（NX 7150→3450）→ 仓库取出落 CASH 栏槽位
  （`msinv_slot_item(5,2)=1002000`）→ 退出商城落盘后 SQL 断言
  `inventoryitems` 新行 `inventorytype=5`、`expiration=-1`（永久，无到期戳）。
- 遗留数据迁移实测：修复前购买的 test1Pala 现金帽（行 45269，
  `expiration=1801142852501`≈2027-01-28）在角色在线 >60s 触发
  `expirationTask` 后经商城进出落盘，DB 行翻转为 `expiration=-1`、物品完好
  未删除——0007 补丁两段行为（新购不盖章 + 存量迁移）均验证通过。

## Related Docs

- [现金商城](../../cash-shop/index.md)
- [服务端补丁 0007](../../../scripts/server-patches/0007-cash-equips-permanent.patch)
