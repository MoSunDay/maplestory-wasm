# 自由市场(910000000) 大面积白屏修复：chewchew Obj/Back 包装层还原

## 现象
2026-10-19 完成的 079MAX4 1:1 复刻注入后，自由市场 hub（910000000）可行走中间带大面积白屏
（e2e 实测白像素 42.9%–58.2%，房间 910000001 正常 7.4%）。

## 根因
`tools/img079_parser.py` 的 `parse_img` 取"第一个能恰好消费到 EOF 的 LIST_PREAMBLE"作为根，
把 Obj/Back 库的包装层压平：

- `Obj/chewchewIsland.img` 丢了 `MainField` 包装（079 地图 obj 引用 `l0=MainField/l1=nature/l2=14`，
  326 个 obj 中 229 个查找失败）；
- `Back/chewchewIsland.img` 丢了 `back` 包装（客户端按 `Back/<bS>.img/back/<no>` 查找）。

未渲染区域露出 `glClearColor(1,1,1,1)` 的白底。真值来源：原始 .img 头 `u16@0x0C`
（Obj 根=1 子节点 MainField；Back 根=2 子节点 back+ani）与服务端 `wz/Map.wz/Obj/chewchewIsland.img.xml`。

## 修复
- `link_repos/.ref-079max4/extracted/map079/chewchewIsland.json`：14 个 section 整体移入 `MainField`；
  `Back.chewchewIsland.json`：`0..52` 移入 `back`，`ani` 留根。canvas `pixels/` 相对路径键原样保留。
- 从 `assets/Map.nx.pre-079fm-style-backup` 恢复后重跑 `tools/nx/inject_map_nx.py`
  （+2 包装节点，bitmaps 35,973 与修复前一致），并用 `tools/nx/patch_nx_string.py` 重打 22 个房间
  `/info/mapMark`（Henesys/ElNath/Ludibrium/Perion 分组不变）。
- `tools/nx/verify_map_nx.py` 新增 `MAPMARK_TOWNS` 豁免 + `check_mapmark_patches()` 正向校验，
  报错归零（ALL CHECKS PASSED）；并按客户端语义解析 910000000 引用的 105 obj + 27 back → 0 缺失。

## 验证
`tools/verify/maplestory-webui/fm-map-flow.mjs`（fmflow1）15/15 PASS；各层截图白像素：
入口 5.6% / 店门层 4.2% / 二层 6.1% / 三层 5.8% / 四层 5.7% / 房间<1> 3.4%
（修复前 hub 42.9%–58.2%）。画面呈 ChewChew 岛蓝绿风格（蓝绿桶 0.20–0.40）。

## 关联
- 前置注入：`features/changelog/2026-10-19/fm-map-079-max4-1to1.md`
- 同日另一修复：`npc-dialog-f-inline-images.md`
