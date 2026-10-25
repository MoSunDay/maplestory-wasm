# 自由市场：地图名回退、妮娜对话清理、传送 NPC 与 /gohome

## 地图名

上一轮 079 复刻把 910000000 的显示名改成了服务端品牌名
`怀旧岛079MAX4年度版 / 主城区域`。本次恢复 079 原值：

- `link_repos/MapleStory-Server/wz/String.wz/Map.img.xml`
  - `streetName` = `自由市场`
  - `mapName` = `自由市场入口`
- `assets/String.nx`、`desktop/resources/msweb/assets/String.nx`
  - 用 `tools/nx/patch_nx_string.py` 把
    `Map.img/etc/910000000/streetName` 重指向已有的 `自由市场`，
    `mapName` 重指向已有的 `自由市场入口`；仅改 8 字节字符串索引，
    未重建 NX/字符串表。
- `nxFiles/String.nx` 原本已是这两个值，未改动。
- `910000001..022` 的 `自由市场<N>` 未改。

## 妮娜对话框

`scripts/npc/9270052.js` 的 case 0 原先拼接 21 个
`#fUI/GuildMark/Mark/Pattern/00004014/11#` 作为分隔行。该图标高度
超过普通行高，和正文行发生重叠。本次：

- 删除该分隔行；
- 删除 `scripts/npc/lib/9270052_data.js` 中不再使用的 `四方印` 变量；
- 保留 3 个菜单项前的爱心图标 `ttt1`。

`tools/verify/maplestory-webui/npc-embed-icons.mjs` 随之改为验证
3 个菜单爱心，而不再要求 17 连排 GuildMark 色带；实测 prompt 无路径
泄漏，爱心列 3/3 染色，关闭对话后亮度回落。

## 自由市场传送 NPC

原 910000000 的 life 只有仓库 NPC 弗兰德里（9030000）和妮娜
（9270052）。本次在 `wz/Map.wz/Map/Map9/910000000.img.xml` 的 life
末尾追加两个现成 NPC（客户端 `Npc.nx` 均已有图，脚本也已存在）：

| life | NPC | 坐标/地面 | 功能 |
|---|---|---|---|
| 2 | 9000020 导游妮妮 | x=700, fh=163, cy=23 | 世界旅行（地图传送） |
| 3 | 9000021 佳佳 | x=1000, fh=181, cy=23 | BossRushPQ 招募（Boss 传送） |

两者都在自由市场入口层地面，避免与左侧妮娜、右侧弗兰德里重叠。
端到端实测：走到 x≈700 与 x≈1000 按 Space 均能打开对话，文本分别
出现「枫叶旅行社」和「旅行者/竞赛」。

## /gohome

- 新增 `src/client/command/commands/gm0/GoHomeCommand.java`（rank 0）：
  存活时传送到 910000000 的 `in01` 传送门（x=1332）；找不到该门时
  退回 `getRandomPlayerSpawnpoint()`。非 GM 在 event instance 或
  mini dungeon 中仍禁止使用；**不再**受 `FieldLimit.CANNOTMIGRATE`
  限制，因此可以在自由市场房间（如 910000001）里直接回家。
- `CommandsExecutor.isCommand` 仅放行 `/gohome` 这一个 slash 命令
  （大小写不敏感，允许尾随空格参数）；其余 `/` 开头消息保持原行为。
  这样客户端无需改动，`/gohome` 会作为聊天消息上行后被服务端识别。
- `@gohome`（普通玩家）/ `!gohome`（GM）同样可用。
- 新增 `tools/verify/maplestory-webui/gohome-flow.mjs`：从
  910000001（fieldLimit 含 CANNOTMIGRATE）发送 `/gohome`，断言地图
  切回 910000000，落点 x=1332 y=-1。

## 验证

- `link_repos/MapleStory-Server/posix-compile.sh` 通过（仅既有
  deprecation warning）。
- `npc-embed-icons.mjs` PASS：`3/3 heart columns colored in x=296..308`，
  关闭对话亮度 186 → 39.59/53.49（两次复跑）。
- `gohome-flow.mjs` 3/3 PASS：起始 910000001 → `/gohome` →
  910000000，落点 `x=1332 y=-1`。
- 临时 FM NPC 对话探针 PASS：9000020/9000021 均可见且可对话。
- 服务端已重启（`scripts/game_server_restart.sh`）加载新 XML 与命令。
