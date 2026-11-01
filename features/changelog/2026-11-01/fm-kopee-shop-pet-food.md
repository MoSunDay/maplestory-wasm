# 自由市场科皮商店：换货——下架射手村回城券，上架宠物饲料、棒棒冰与红豆刨冰

自由市场 910000000 的科皮（9201116 Kopee Relicseeker，shopid 1339）
稀有商店此前出售「射手村回城券」（2030004，500 金）。回城券在本地
linked checkout 的服务端数据里并非核心消耗品，而养宠玩家常驻市场却
买不到宠物饲料，两个常用回复消耗品（棒棒冰、红豆刨冰）也缺货。
本轮共下架 1 项、上架 3 项，货架 32 → 34 项，不动商店结构：

| itemid | 物品 | 变化 | 行为 |
|---|---|---|---|
| 2030004 | 射手村回城券（Nearest Town Return Scroll） | 500 金 → 移除 | 下架 |
| 2120000 | 宠物饲料（宠物食品 / Pet Food） | 新增 200 金 | 上架，复用 position 228 |
| 2001001 | 棒棒冰 | 新增 2300 金（HP+2000） | 上架，position 232 |
| 2001002 | 红豆刨冰 | 新增 4000 金（MP+2000） | 上架，position 236 |

换货后 shopid 1339 的 `shopitems` 共 34 项，position 仍按 4 步进
（…220、224、228、232、236），不产生空洞。

## 定价依据

- 2120000：参考 079MAX4（CongMS）服务端普通商店的售价：`shopitems`
  中 shop 100 与 shop 901 均为 200 金，故本店同样采用 200 金
  （slotMax 200，一组 4 万金可买满 200 个，与原版普通商店对齐）。
- 2001001 棒棒冰 / 2001002 红豆刨冰：079MAX4（CongMS）`shopitems`
  多数商店一致价分别为 2300 金 / 4000 金，本店照搬。服务端
  `wz/Item.wz/Consume/0200.img.xml` 中 `02001001` spec `hp=2000`/
  `price 1150`、`02001002` spec `mp=2000`/`price 2000`，客户端
  `Item.nx` `Consume/0200.img` 两项 icon/spec 齐全，`String.nx`
  名称为「棒棒冰」「红豆刨冰」。

## 数据链路确认

- 客户端 `assets/String.nx` `Consume.img/2120000`：名称「宠物食品」，
  desc「宠物喜欢的饲料.增加饱满感30.人不能吃.」。
- 客户端 `assets/Item.nx` `Consume/0212.img/02120000`：有
  icon/iconRaw/slotMax/spec（宠物饱食度 `inc` +30，spec 覆盖
  5000000.. 等宠物 itemid），可直接在商店格与背包中渲染。
- 服务端 `link_repos/MapleStory-Server/wz/Item.wz/Consume/0212.img.xml`
  存在 `02120000`（`slotMax` 200、`price` 15），WZ 侧物品定义齐全。
- 宠物喂食协议（USE_ITEM 触发宠物饲料 212 组）见
  [docs/ms-network-protocol.md](../../../docs/ms-network-protocol.md)
  「pet food」一节。
- 本改动为纯 DB 数据（shopitems 行），客户端无需任何改动；购买后
  走既有 USE_ITEM → 宠物喂食链路。

## 改动位置

仅两处：

- `link_repos/MapleStory-Server/sql/custom_fm_service_npcs.sql`：
  `shopitems`（shopid 1339）最后一行
  `(1339, 2030004, 500, 0, 228)` → `(1339, 2120000, 200, 0, 228)`，
  本次再追加两行 `(1339, 2001001, 2300, 0, 232),` 与
  `(1339, 2001002, 4000, 0, 236);`（position 232/236）。
- `tools/verify/maplestory-webui/fm-service-npcs.mjs`：`SHOP_ITEMS`
  集合与购买断言同步改为 2120000（购买 `2120000 || 2030000` 之一），
  本次 `SHOP_ITEMS` 同步 +2（追加 2001001、2001002），购买断言不变。

## 生效方式

该 SQL 可重复执行（幂等）：先 `DELETE FROM shopitems WHERE shopid =
1339` 再整体重插 34 行，重复执行结果一致。注意服务端 `MapleShopFactory`
在 JVM 内缓存商店对象，改 DB 后必须重启游戏服务端
（`docker restart ms-game-server`）才会生效，只重跑 SQL 不重启的话
玩家看到的仍是旧货架。

## 实测记录

分两轮验证（同一天、同一文件的两批换货）：

**第一轮（下架 2030004、上架 2120000，32 项）**：SQL 重放后
`docker restart ms-game-server`，15 项 PASS。科皮商店可见行
`[2120000, 2030000, 2020015, 2020014, 2000005]`，2030004 不在货架；
真实点击购买 2120000：确认框弹出、库存 0→1；佩里传送链路回归正常
（维多利亚→射手村 warp 100000000）。产物
`/tmp/maplestory-webui-Xj1eGv/`（perry-main、perry-victoria、
kopee-shop、henesys 四图）。

**第二轮（追加 2001001/2001002，34 项）**：SQL 重放（DB 抽查：
1339 共 34 项，末三位 2120000@228/2001001@232/2001002@236，
2030004 为 0 项）并重启服务端后，两轮实测（产物
`/tmp/maplestory-webui-DyRVsk/`、`/tmp/maplestory-webui-LWiS9B/`）
商店相关断言全部 PASS：

- 科皮商店可见行实测 `[2001002, 2001001, 2120000, 2030000, 2020015]`，
  棒棒冰/红豆刨冰在货架前两行，射手村回城券不在货架；
- 真实点击购买 2120000：确认框弹出、库存 1→2、2→3（DB 侧
  inventoryitems 中角色 245 的 2120000 = 3，三轮购买均有落库）；
- 佩里对话前半程（主菜单→维多利亚子菜单→Escape→商店→再次主菜单）
  全 PASS；两轮都在尾部「选射手村→warp 100000000」处被共享机
  CDP `Runtime.evaluate` 超时中断（见下方环境坑）。该尾段断言与
  本次改动无关（9110000 脚本未动），第一轮已在同环境全绿通过。

复跑备忘：

```sql
update accounts set loggedin=0 where name='fmverify01';
update characters set map=910000000, spawnpoint=0 where id=245;
```

```bash
E2E_CDP_TIMEOUT_MS=180000 FM_SERVICE_ACCOUNT=fmverify01 \
FM_SERVICE_PASSWORD=test2 FM_SERVICE_CARD=0 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-service-npcs.mjs
```

环境坑（本次踩到）：

- `docker restart ms-game-server` 后频道端口并非同时就绪——
  8484/7575 先起，7675/7775/7875/7975 晚约两分钟；客户端恰被分到
  未就绪频道（如 7975）会陷入 `Reconnect to channel server failed`
  循环，表现成 CDP `Runtime.evaluate` 超时。复跑前先确认
  7575/7675/7775/7875/7975/8484 全部可连再启动验证。
- 共享机上其它验证进程会周期性重启 `ms-ws-proxy`（RestartCount=0
  但 StartedAt 被外部刷新），代理重启瞬间客户端频道 WebSocket 断开，
  同样以 CDP 超时的形式暴露；跑长链路验证尽量避开或失败后重跑。

## 相关文档

- [2026-10-22 自由市场：佩里传送 NPC 与科皮稀有商店](../2026-10-22/fm-service-npcs.md)
  （shopid 1339 商店的初始数据、32 项商品与注入链路）
- [2026-10-25 自由市场科皮商店：药水价格调整](../2026-10-25/fm-kopee-potion-prices.md)
  （同商店上一轮调价与重启生效说明）
