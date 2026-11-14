# 自由市场移除导游妮妮/工作人员G，佩里补全世界旅行目的地

## 背景

自由市场 910000000 的 life 里长期驻着两个 079 世界旅行相关 NPC：

| 原 life | NPC | 客户端名 | 脚本/作用 |
|---|---|---|---|
| 2 | 9000020 | 导游妮妮（Spinel） | `9000020.js` 世界旅行（收费传送） |
| 3 | 9000021 | 工作人员G / 佳佳（Gaga） | `9000021.js` BossRush 招募 |

079 服务端 `9000020.js` 的目的地表为
`500000000 / 702000000 / 600000000 / 540000000 / 800000000 /
701000000 / 702100000 / 550000000`。其中 540/550/600/800 已在佩里
（9110000）的「东方都市与海外」分类里，500（泰国水上市场）、
701（上海外滩）、702（嵩山镇）、702100（大雄宝殿）缺失；本次随
079MAX4 素材落地一并补齐，使妮妮退休后玩家仍能到达其全部目的地。

## 变更

### 服务端 life（`wz/Map.wz/Map/Map9/910000000.img.xml`）

删除两条 life 条目 `9000020`/`9000021`，其余条目重新连续编号为
`0..6`（`tools/patch_orient_server_wz.py` 的幂等文本手术）：

| 新 index | NPC | x | fh |
|---|---|---|---|
| 0 | 9030000 | 1590 | 232 |
| 1 | 9270052 | 34 | 123 |
| 2 | 9110000 佩里 | 300 | 143 |
| 3 | 9201116 科皮 | 600 | 163 |
| 4 | 9010000 玩法向导(PQ) | 850 | 181 |
| 5 | 9010005 玩法向导(BOSS) | 1150 | 181 |
| 6 | 1100000 仓库 | 450 | 143 |

脚本 `scripts/npc/9000020.js`、`9000021.js` 保留：这两个 NPC 仍作为
普通 life 出现在其他 11/4 张城镇图中，本次只从自由市场移除，不删
全局数据。客户端 `assets/Map.nx` 的 `910000000.img/life` 本来就是
0 个子节点，且场景 NPC 完全由服务端 `SPAWN_NPC` 包驱动，因此本次
**没有** Map.nx 改动。

`sql/custom_fm_service_npcs.sql` 顶部清单同步为移除后的 life index
（佩里 2 / 科皮 3 / 向导 4、5），并注明两个 NPC 的移除日期与原因。

### 佩里传送列表（`scripts/npc/9110000.js`）

`CATEGORY_MAPS[2]`「东方都市与海外」按地图 id 升序插入四个目的地，
从 9 项增至 13 项：

```
250000000, 260000000, 261000000, 270000000,
500000000, 540000000, 550000000, 600000000,
701000000, 702000000, 702100000, 800000000, 801000000
```

其余分类不变（12/7/8 项），`DANGEROUS_MAPS`、状态机、`#L<idx>##m<id>##l`
选项格式和 `#m` 宏渲染均未改。新增项不在危险名单，直接 `cm.warp`。

### e2e 脚本（`tools/verify/maplestory-webui/fm-service-npcs.mjs`）

- `WARP_DESTS` 的海外 `optionIndex` 校正为所在子菜单的行号：
  thai=4、shanghai=8、shaolin=9、shaolin-hall=10（最后一项固定是
  「返回主菜单」`#L99`，故地图行号就是 `CATEGORY_MAPS` 下标）。
- `confirmByButtons` 点 OK 后不再立刻读 prompt，而是等待
  「prompt 变化或对话关闭」：OK 只发包，新对话/传送由服务端回包晚
  一拍到达；此前直接读会误判为仍停在主菜单。
- 进入第三分类后若 prompt 不含「目的地」直接中止，避免后续断言级联
  误报；同时增加独立账号/调试端口说明，便于与其他 e2e 并发。

## 验证

- `node --check scripts/npc/9110000.js` 通过。
- `python3 tools/patch_orient_server_wz.py --check` 退 0：
  `removed: 0 []; life 7 -> 7 entries`，`check complete: nothing pending`；
  其余 String.wz 补丁均显示 already present（幂等）。
- 服务端重启 `ms-game-server` 后在线（NPC 脚本热更新，但 life XML 需
  重启；本次已执行 `docker restart ms-game-server`）。
- 浏览器 e2e（真实登录/走位/鼠标点选/服务端回包）全部 PASS：

| `FM_SERVICE_DEST` | 结果落图 | 关键日志 |
|---|---|---|
| 默认 henesys | 100000000 射手村 | 首行点击直接确认 |
| ant-park | 105070001 蚂蚁广场 | NEXT×6 + OK |
| thai | 500000000 水上市场 | NEXT×4 + OK |
| shanghai | 701000000 上海外滩 | NEXT×8 + OK |
| shaolin | 702000000 嵩山镇 | NEXT×9 + OK |
| shaolin-hall | 702100000 大雄宝殿 | NEXT×10 + OK |

  另含每轮开头的出生点/主菜单/维多利亚子菜单/科皮商店稀有物品与购买
  入库断言，均 PASS。实测使用 `fmflow1/fmflow1` card0（character
  id=129，world=4）、`FM_SERVICE_DEBUG_PORT=9266`；每轮前重置
  `accounts.loggedin=0` 与角色 `map=910000000, spawnpoint=0`。
  截图产物：`/tmp/maplestory-webui-{vrhA5Y,wug5J4,MHI3Iw,zT9UE7,
  xj5KXm,rHL1Vh}/`（`fm-service-*.png`）。

## 回滚

- life：把当前 index 2..6 的五个条目后移到 4..8，在 index 2/3 插回
  两条 NPC（字段与相邻条目同构：`type=n`、`y=18`、`mobTime=1000`、
  `f=0`、`hide=0`、`cy=23`、`rx0=rx1=x`）；x/fh 取 2026-10-21 记录：
  9000020 x=700 fh=163；9000021 x=1000 fh=181。最终 life 为 9 条、
  编号 `0..8`。
- 佩里：从 `CATEGORY_MAPS[2]` 删除 500000000/701000000/702000000/
  702100000 四项即回到原 9 项。
- 本次无客户端 NX、SQL 数据或协议改动需要回滚。
