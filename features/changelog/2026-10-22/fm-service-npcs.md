# 自由市场：佩里传送 NPC 与科皮稀有商店

自由市场 910000000 此前只有仓库、妮娜、导游妮妮、佳佳四个 NPC。
本轮补上 079MAX4 风格的两个服务 NPC，并把客户端/服务端链路打通：

| life | NPC | 坐标/地面 | 功能 |
|---|---|---|---|
| 4 | 9110000 佩里（次元空间的帮手） | x=300, fh=143, cy=23 | 按大区分组传送到世界各地/BOSS 图 |
| 5 | 9201116 科皮 | x=600, fh=163, cy=23 | 出售飞镖、子弹、魔法石、超级药水、召回石等 |

## 佩里：传送脚本

新建 `link_repos/MapleStory-Server/scripts/npc/9110000.js`（纯函数式，
`state`/`page`/`pendingMap` 三个变量承载对话状态），主菜单分四类：

- 维多利亚岛及周边（10 图）
- 天空之城与冰雪世界（7 图）
- 东方都市与海外（9 图）
- 首领 BOSS 挑战地点（8 图）

子菜单用 `#L<idx>##m<mapid>##l` 同时给出选项和地图名；危险地图
280030000/220080000/270050100 先 `sendYesNo` 二次确认，其余直接
`cm.warp(mapid, 0)`。地图 105000000 在 Map.wz/String.wz 中都不存在，
改用确实存在且有中文名的 105040300（Dungeon/Sleepywood）。

脚本头部记录了客户端宏替换的坑：`UINpcTalk` 的 `replace_macros`
会把 `#b/#d/#e/#f/#g/#k/#n/#r` 当作格式 token。选项文本若以这些字母
（大小写不敏感）开头，首字母会被吞掉；实测分类名 `BOSS 挑战地点`
渲染成 `3OSS 挑战地点`，因此改为 `首领 BOSS 挑战地点`。后续新增
对话选项也应避免以这 8 个字母开头。

## 科皮：商店数据

`link_repos/MapleStory-Server/sql/custom_fm_service_npcs.sql` 幂等写入
shopid 1339（npcid 9201116）和 32 个商品：

- 飞镖：2070000..2070013、2070016、2070018（不含初学者专用 2070014/2070015）
- 子弹/胶囊：2330000..2330005、2331000、2332000
- 魔法石/召唤石：4006000、4006001
- 超级药水：2000004 万能药、2000005 超级药水
- 高级 MP 药：2020014 晨露、2020015 晚露
- 召回石：2030000 最近城镇、2030004 射手村

`src/server/MapleShop.java` 的 `rechargeableItems` 移除 2070015，
避免这个 0 价/初学者专用飞镖作为补全项出现在商店里。可充值物品在
DB 中没列出的项仍会以 price=0 自动追加；本店已显式列全需要的项，
购买时按 DB 价格收取整组费用。

客户端不读 `Map.nx` 的 life，NPC 全部由服务端 SPAWN_NPC 生成，所以
只需在 `wz/Map.wz/Map/Map9/910000000.img.xml` 的 life index 4/5
追加两条；`Map.nx` 不需要重建。

## 验证

已执行：

```bash
node --check link_repos/MapleStory-Server/scripts/npc/9110000.js
javac -d dist -cp "dist:cores/*" src/server/MapleShop.java
mysql ... < sql/custom_fm_service_npcs.sql
```

DB 抽查：shop 1339 共 32 项，position 104..228，2070015 为 0 项。

新增 `tools/verify/maplestory-webui/fm-service-npcs.mjs`，在真实 WASM
客户端中通过物理鼠标点击对话选项，断言：

- 出生在 910000000 sp 区（x≈-128）；
- 佩里主菜单含「佩里」「世界各地」，首分类进入维多利亚子菜单；
- 科皮商店弹出，可见行含 2030004/2030000/2020015/2020014/2000005
  等稀有物品，购买 2030004 后库存 1→2；
- Escape 可关闭对话/商店；再次打开佩里并选射手村后 warp 到 100000000。

测试全绿，console 无 `unresolved dialogue token`、无浏览器错误；
产物：`/tmp/maplestory-webui-qjQeLg/`（含 perry-main、victoria、
kopee-shop、henesys 四张截图和 browser-console.log）。

运行前置（每次重跑都要把测试角色放回自由市场）：

```sql
update accounts set loggedin=0 where name='test2';
update characters set map=910000000, spawnpoint=0 where id=219;
```

```bash
FM_SERVICE_ACCOUNT=test2 FM_SERVICE_PASSWORD=test2 FM_SERVICE_CARD=0 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-service-npcs.mjs
```

环境变量还支持 `FM_SERVICE_URL`、`FM_SERVICE_DEBUG_PORT`（默认 9255）。
