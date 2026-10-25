# 自由市场：玩法向导 NPC（PQ 招募 / BOSS 入口）

自由市场 910000000 在仓库、妮娜、导游妮妮、佳佳与佩里、科皮之外，
本轮补上两个玩法向导 NPC，把玩家从枢纽直送到各地组队任务(PQ)的
招募点与 BOSS 入口：

| life | NPC | 坐标/地面 | 功能 |
|---|---|---|---|
| 6 | 9010000 玩法向导（PQ） | x=850, fh=181, cy=23 | 13 个组队任务招募点 |
| 7 | 9010005 玩法向导（BOSS） | x=1150, fh=181, cy=23 | 12 个 BOSS 入口 |

两个 NPC 都不做等级与队伍校验，只负责把人送到正确的招募地图/入口。

## PQ 向导：scripts/npc/9010000.js

新建 `link_repos/MapleStory-Server/scripts/npc/9010000.js`（83 行，
纯函数式单页菜单，无额外状态变量），用 `#L<idx>##m<mapid>##l` 同时
给出选项与地图名，13 个目的地全部直接 `cm.warp(mapid, 0)`：

| 选项 | 目的地 | 地图 |
|---|---|---|
| 废弃组队任务(21-30级) | KerningPQ | 103000000 |
| 玩具城组队任务(35-50级) | LudiPQ | 221024500 |
| 玩具城迷宫任务(35-50级) | LudiMazePQ | 220000000 |
| 天空组队任务(51-70级) | OrbisPQ | 200080101 |
| 海盗组队任务(50-70级) | PiratePQ | 251010404 |
| 月妙组队任务(10-200级) | HenesysPQ | 100000200 |
| 艾琳森林任务(45-55级) | EllinPQ | 300030100 |
| 玛加提亚组队(45-65级) A路 | Magatia A | 261000021 |
| 玛加提亚组队(45-65级) Z路 | Magatia Z | 261000011 |
| 结婚前任务(40级+) | AmoriaPQ | 670010100 |
| 圣殿远征(90级+) | CWKPQ | 610030020 |
| 宝物地下城 | TreasurePQ | 674030100 |
| 波塞冬挑战(连战) | BossRushPQ | 970030000 |

菜单末尾固定「我要离开」（selection 99），直接 `cm.dispose()`。

## BOSS 向导：scripts/npc/9010005.js

新建 `link_repos/MapleStory-Server/scripts/npc/9010005.js`（112 行，
纯函数式，对话状态由 `state`/`pendingMap` 两个变量承载），12 个
入口如下；其中帕普拉图斯与昭和首领战是 BOSS 本体所在的危险地图，
先 `sendYesNo` 二次确认（镜像 9110000 佩里的写法），其余单击直接
warp：

| 选项 | 地图 | 备注 |
|---|---|---|
| 扎昆入口(50级+) | 211042300 | |
| 暗黑龙王入口(80级+) | 240050000 | |
| 粉豆入口(120级+) | 270050000 | |
| 帕普拉图斯(80级+) | 220080000 | 危险，sendYesNo 二次确认 |
| 闹钟蝙蝠怪(40级+) | 105100100 | |
| 昭和首领战(80级+) | 801040004 | 危险，sendYesNo 二次确认 |
| 天蝎双王(80级+) | 551030100 | |
| 拉塔尼卡(75级+) | 541010060 | |
| 元素之杖(70级+) | 220050300 | |
| 戴利之战(120级+) | 925010200 | |
| 守护塔防战·其一(70级+) | 240070202 | TD_Battle1 |
| 玛哈失控战(170级+) | 140000000 | |

## life 注入与选号依据

- 与佩里/科皮同链路：客户端不读 `Map.nx` 的 life，NPC 全部由服务端
  SPAWN_NPC 生成，因此只需在 link_repos 侧
  `wz/Map.wz/Map/Map9/910000000.img.xml` 的 `<imgdir name="life">`
  追加 index 6（9010000）/ index 7（9010005）两条；`Map.nx`
  不需要重建。XML 改动需要重启服务端才生效（MapleMapFactory 缓存），
  NPC 脚本则支持热更新（保存即生效）。
- `sql/custom_fm_service_npcs.sql` 头部注释同步登记 index 6/7 两个
  NPC；它们只由 life 注入、不需要任何 SQL 行，商店段保持不变。
- 选号依据：`assets/Npc.nx` 中可正常渲染，且 `scripts/npc/`
  无既有脚本占用。

## 宏替换首字母坑

沿用 2026-10-22 fm-service-npcs 条目记录的约束：客户端 `UINpcTalk`
的 `replace_macros` 会把 `#b/#d/#e/#f/#g/#k/#n/#r` 当作格式 token，
选项文本若以这些字母（大小写不敏感）开头，首字母会被吞掉。两个
向导的全部选项标签（含「我要离开」）都以中文字符开头，不落坑。

## 验证

脚本语法与 life 注入检查：

```bash
node --check link_repos/MapleStory-Server/scripts/npc/9010000.js
node --check link_repos/MapleStory-Server/scripts/npc/9010005.js
grep -n '9010000\|9010005' \
  link_repos/MapleStory-Server/wz/Map.wz/Map/Map9/910000000.img.xml
```

新增 `tools/verify/maplestory-webui/fm-gameplay-guides.mjs`（243 行），
在真实 WASM 客户端中走位到 NPC 旁按 Space 打开对话、物理鼠标点击
选项，全部 PASS：

- PASS  出生地图 = 910000000（实测 sp 区 x∈[-180,20]）
- PASS  玩法向导: PQ 招募菜单出现（prompt 含「玩法向导」「组队任务」）
- PASS  玩法向导: Escape 关闭窗口
- PASS  BOSS 向导: 入口菜单出现（prompt 含「BOSS 向导」「入口」）
- PASS  BOSS 向导: 首选项扎昆入口(50级+) 非危险地图，单击即 warp
- PASS  BOSS 向导: 传送到 211042300

截图 fm-guides-pq-menu / fm-guides-boss-menu / fm-guides-zakum-door
与 browser-console.log 落在 artifacts 目录。注意脚本结束时角色被
传送到扎昆入口一带（客户端先落 211042300，会话结束后 DB 落点
211041900），重复运行前必须重新执行下面的 SQL。

运行前置（凭据不入代码仓库）：

```sql
update accounts set loggedin=0 where name='test2';
update characters set map=910000000, spawnpoint=0 where id=219;
-- keymap 需含 Q：16 → type 4 / action 8。注意 keymap.key 是客户端
-- Keytable 的 MS 扫描码下标，Q=16 而不是 GLFW 键码 81（81 对应的
-- 槽位是 PAGE_DOWN）；KeyType 枚举 MENU=4 / ACTION=5，打开窗口必须
-- 用 MENU 类型。
```

```bash
FM_GUIDES_ACCOUNT=test2 FM_GUIDES_PASSWORD=test2 FM_GUIDES_CARD=0 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-gameplay-guides.mjs
```

## 相关文档

- [2026-10-22 自由市场：佩里传送 NPC 与科皮稀有商店](../2026-10-22/fm-service-npcs.md)
  （life 4/5、宏替换首字母坑、custom_fm_service_npcs.sql）
- [2026-10-21 自由市场：地图名回退与传送 NPC](../2026-10-21/fm-name-teleporters-gohome.md)
- [任务日志窗口](quest-log-window.md)（本枢纽的另一个配套入口）
