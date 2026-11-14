# 东方神州/泰国 079MAX4 素材落地：客户端 NX + 服务端 wz + 新 canvas 解码

## 背景

v83 客户端/服务的 Map 数据缺少 079MAX4 年度包的海外内容：

- 泰国 500xxx：9 张图（String.wz `Map.img/elin`）
- 东方神州 701/702：28 张图（String.wz `Map.img/event`，上海 16 + 嵩山 12）
- 地图引用的 `Tile/ShanghaiNJL`、`Tile/thai`、`Back/YuyuanCN` 三个
  library，以及 63 个 NPC img、13 个 Mob img（9600012..9600024）和
  对应名字条目。

079 数据目录中还混有 2023 年度版的**新 canvas 载荷格式**，旧解析器
直接按 zlib 解会失败；不先破解它就无法把 miniMap/部分 NPC/Mob 画布
写进 NX。

## 素材与注入结果

提取源：`link_repos/.ref-079max4/extracted/orient/`（40 个 img JSON
+ `pixels/*.bin` 88 个唯一像素名）。40 个 wrapper 的构成：

- `Map/Map7` +28（701/702）
- `Map/Map5` +9（500xxx）
- `Tile` +2（ShanghaiNJL、thai）
- `Back` +1（YuyuanCN）

执行 `python3 tools/nx/inject_orient_maps_nx.py`（一次性；刷新备份存在
即拒绝重跑），结果（`/tmp/orient_inject.log`）：

| 归档 | 字节 | 节点 | 字符串 | 位图 | wrapper |
|---|---|---|---|---|---|
| Map.nx | 1648550354 → 1654374268 (+5823914) | 7984465 → 8187104 | 15742 → 15947 | 35973 → 36061 | +40 |
| Npc.nx | 133123947 → 138241967 (+5118020) | 205674 → 211298 | 8290 → 8392 | 26180 → 27601 | +63 |
| Mob.nx | 1195049525 → 1197436867 (+2387342) | 334455 → 337369 | 2491 → 2504 | 51697 → 52163 | +13 |
| String.nx | 5881908 → 5950800 (+68892) | 137718 → 138023 | 81238 → 81466 | 0 | +82 名称条目 |

- String.nx：`Map.img/elin` +9（共 38）、`event` +28（共 59）、
  `Npc.img` +32、`Mob.img` +13；079 源缺 1082100/9000087/9310045
  三个 NPC 名字，跳过并记录（客户端缺名字只显示空名，不崩）。
- Npc.nx 的 63 个 wrapper 不含 9030000/9270052（此前已单独注入）。
- 注入后自动同步 `desktop/resources/msweb/assets/` 的 4 个 NX 与
  `nxFiles/String.nx`；三处逐字节一致。
- 回滚点：`assets/{Map,Npc,Mob,String}.nx.pre-orient-backup`。

### 新 canvas 格式（2023 年度包）

`tools/img079_parser.py::read_canvas` 增加分支（`NEW_CANVAS_MARK`）：

```
[u32 = 2][2B KMS-XOR zlib 头][u16 body_len][u16 全零][body_len 字节 body]
```

- 头和 body **各自从 XOR key 偏移 0** 重新起算；
- body 是去掉结束块的 zlib 流，`inflate` 恰产 `w*h*bpp` 像素即停
  （`decompressobj` 不要求流完整）；
- 命中样本：701000000/701000100/701010320/702000000/500000000 的
  miniMap，以及 Npc/9310060、Npc/9330078、Mob/9600013 的全部画布。
- key 取 `key_kms` 前 0x8000 字节，base64 内嵌在
  `tools/img079_key.py`（最长的流 18361B，最长的 UTF-16 字符串 819
  字符均覆盖；出处 `.cache/tools/NoLifeWzToNx/Keys.cpp`）。
- Npc/9310082 的 3x2 画布最小 LZ4 块 21B > 24B 窗口的严格余量；
  `map079_tree.window_ok` 对 ≤64B 的微型窗口允许 `clen <= win+2`
  （与出厂 v83 归档里 1x1 位图的 1 字节越界同规），
  `best_tiny_block` 选最小合法块。

### 注入器修复

- 新增 `tools/nx/orient_data.py`（268 行，纯函数）：37 图/63 NPC/13
  Mob id 表、`deref_uols`（把 WZ uol 子节点物化为兄弟目标的深拷贝，
  链式跟随到定点，共享目标才复制）、`image_canvas_desc`（带微型窗口
  slack）、`map_name_entries`（079 `Map.json` + 服务端 String.wz）。
- 重写 `tools/nx/inject_orient_maps_nx.py`（262 行）：复用
  `inject_map_nx.insert_wrapper` / `inject_npc_nx.splice` 等原语；
  修复旧实现遇到 `list`/`uol` 属性直接崩溃，以及排序插入时
  `head-of-block base` 偏移错误。
- 新增 `tools/patch_orient_server_wz.py`（348 行）：幂等、行级文本
  手术，只改目标条目；`--check` 为 dry-run（有待改内容时退 1）。

## 服务端落地

- 37 个 `Map.wz/Map/Map{5,7}/<id>.img.xml` 从 079 服务端逐字节
  拷贝（md5 全部一致）；hub 的 Map7 现有 31 个文件（28 新 + 3 旧），
  Map5 包含全部 9 张泰国图。
- `String.wz/Map.img.xml`：elin +9、event +28；`Npc.img.xml` +32、
  `Mob.img.xml` +13。`tools/patch_orient_server_wz.py --check` 退 0，
  仅打印三个无源条目的 NOTE。
- 自由市场 life 的导游妮妮/工作人员G 移除与佩里目的地见
  `fm-world-tour-npcs-and-perry-destinations.md`。

## 验证

- 独立自检 `/tmp/validate_orient.py`（按提取 JSON 逐节点 + 位图像素
  对比，不是只看节点数）：
  - 40/40 map wrapper、63/63 Npc、13/13 Mob fully identical；
  - 抽查既有 wrapper（910000000/0002000/0100100）位图正常；
  - 全局 `unsorted=0 bad_blobs=0`，`ERRORS 0`。
- `tools/nx/verify_map_nx.py` 的基线是旧版资产，对当前 assets 报 13
  个历史 079FM 漂移错误、对 pre-orient-backup 也报 5 个，故本次不
  作为判据，改用上述独立自检。
- 浏览器 e2e：`FM_SERVICE_DEST=thai|shanghai|shaolin|shaolin-hall`
  均成功落图 500000000/701000000/702000000/702100000，miniMap 正常
  渲染（新 canvas 格式的 5 张 miniMap 全部命中），详见前一篇 changelog。
- 注入的地图 JSON 没有 `info/bgm` 字段，客户端进图保持静音；不阻塞
  玩法，BGM 素材可后续单独补。

## 回滚

- 停止使用注入后的 NX，按 `assets/*.pre-orient-backup` 还原四个归档，
  并重新同步 desktop/nxFiles 副本。
- 服务端删除 37 个新增 Map XML 与 String.wz 追加条目即可；补丁脚本
  不删除非目标数据。
- 新 canvas 分支只对 `payload[:4] == 02 00 00 00` 生效，旧格式路径
  不变；无需回滚其他解析逻辑。
