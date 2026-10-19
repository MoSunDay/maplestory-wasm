# FM(910000000) 079MAX4 年度版自由市场 1:1 复刻（ChewChew 岛风格整图注入）

## 背景

079MAX4-2023 年度版的自由市场入口是一张 ChewChew 岛风格自定义图：
326 个 obj 摆放（9 层：41/75/109/40/37/24/0/0/0）+ 31 背景 + 271 foothold
段 + 40 传送门 + 2 NPC，miniMap 246×143、场地 3937×2289。此前
（`fm-npc-079-max4-port.md`）只照搬了 NPC 而 foothold 做了 v83 适配，
地图样式与传送点均非 1:1。本次把整图、配套 Obj/Back 库与地图名按 079
原版在服务端 wz XML 与客户端 NX 两端同步落地。

## 变更

服务端（`link_repos/MapleStory-Server`，非 git 仓库，手工同步）：

- `wz/Map.wz/Map/Map9/910000000.img.xml`：整棵替换为 079 版（7332 行），
  备份 `.pre-079fm-style-backup`。life=2 NPC（9030000、9270052）。
- life foothold 吸附（依据：079 NPC 坐标来自其 SQL `wz_customlife`，
  fh=144/81、y=1023，在 079 新图上几何不成立，系陈旧数据——fh144 线段
  x∈[248,348] 不含 9030000 的 x=1590，fh81 线段 x∈[-751,-731] 不含
  9270052 的 x=34，y=1023 也不落在任何承载面上）：
  - 9030000（x=1590）→ seg232：x∈[1577,1667]，y=-1；
  - 9270052（x=34）→ seg123：x∈[-32,234]，y=23（cy=23 与线段一致）。
- `wz/String.wz/Map.img.xml`：`etc` 分类下 910000000 与 910000001..022
  共 23 条 streetName/mapName 改为 079 原文（910000000=怀旧岛079MAX4年度版/
  主城区域；001..022=自由市场/自由市场<N>，`<`/`>` 按 XML 转义为
  `&lt;`/`&gt;`）；其余字段与 CRLF 行尾保持原样。910000023/024 两端均无
  079 数据，未动。

客户端 `assets/Map.nx`（备份 `.pre-079fm-style-backup`），注入三个与
map079 树逐节点等价的子树：

| 子树 | 节点 | bitmap | 说明 |
|------|------|--------|------|
| `Obj/chewchewIsland.img` | 4888 | 1080 | 14 组物件库，有序插入 |
| `Back/chewchewIsland.img` | 810 | 185 | 54 组背景库，有序插入 |
| `Map/Map9/910000000.img` | 6578 | 1 | 原位替换；v83 旧子树留作不可达孤儿节点 |

整档前后：1,617,939,138 → 1,648,550,306 字节；节点 7,976,943 →
7,989,218；字符串 14,507 → 15,742；bitmap 34,707 → 35,973。

String 名称双语端同步：

- `assets/String.nx`：`Map.img/etc/910000000..022` streetName/mapName
  原位改写为同一批 079 原文（其余 etc 条目不动），5,933,568 → 5,933,632
  字节，节点数不变（137,562）。
- 服务端 `wz/String.wz/Map.img.xml` 同值（见上），两端地图名一致。

工具（新增，均纯函数、<400 行）：

- `tools/img079_key.py`（53 行）：079 img 名字 XOR 流密码 key。
- `tools/img079_parser.py`（255 行）：079 `data/*.img` 结构解析
  （canvas 为 ARGB4444+zlib flush）。
- `tools/dump_079_tree.py`（323 行）：抽取 map079 JSON 树 + `pixels/*.bin`。
- `tools/nx/map079_tree.py`（314 行）：map079 树读取与子树工具。
- `tools/nx/inject_map_nx.py`（222 行）：一次性注入（备份存在即拒绝重跑）。
- `tools/nx/verify_map_nx.py`（339 行）：注入前后基线对照校验。
- `scripts/wz-converter/README.md`：Post-convert 段补充 079 FM 注入的
  重跑守则（与 inject_npc_nx 并列）。

## 验证

- 服务端重启后 5 个世界 wz 全量加载无错（本次重启日志另见提交记录）。
- 服务端 wz XML 校验 64 项全过（良构 + 结构 + life 落点几何）。
- `python3 tools/nx/verify_map_nx.py`：三个子树逐节点（name/type/value）
  等价、全部 bitmap LZ4 解码后与 `pixels/*.bin` 逐像素全等且落在客户端
  读取窗口内；相邻未触碰子树（spot 抽查 signboard/910000001/grassySoil）
  结构摘要不变；`String.nx` 23/23 名称匹配、既有 `Npc.img/9270052`
  （妮娜）仍在 → ALL CHECKS PASSED。
- `nxdump2`（真实 `nl::file/node/bitmap` 客户端读取路径）冒烟：
  `Map/Map9/910000000.img` 树可遍历（VR/bgm/foothold/…），miniMap canvas
  为 246×143 且可解码；`String.nx` `etc/910000000` 读出双名正确。
- assets-server stale-reload：注入后无需重启，`get_size`/尾块与磁盘
  逐字节一致（复用 2026-10-18 AssetCache 修复）。

## 已知限制

- NPC fh 吸附（seg232/seg123）为按几何最近承载面的推断，待用户追认。
- 910000023/910000024 的地图名两端仍为 v83（079 客户端缺失，无从取证）。
- 浏览器端到端冒烟（进图渲染/传送门跳转/NPC 对话）尚未执行。

## 待办

- 浏览器 e2e 冒烟 910000000 整图。
- 用户追认两个 NPC 的落点（如需微调 x/fh）。
- 评估是否回填 SQL `wz_customlife` 的新 fh，避免后续再导出时复用陈旧值。
