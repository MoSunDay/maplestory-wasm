# 自由市场（910000000）地图名恢复

## 背景

玩家在自由市场打开小地图/世界地图时，地图名显示为 079 私服品牌文案
（streetName「怀旧岛079MAX4年度版」、mapName「主城区域」），而非原版
v83 的「自由市场 / 自由市场入口」。

根因：`scripts/wz-converter` 的 079 注入器（inject_map_nx.py 管线）在
回灌 String 数据时，把品牌名覆盖进了 `String.nx` 的
`Map.img/etc/910000000`（自由市场被归入 etc 分类）。桌面端 nxFiles 副本
与服务端 wz 均为正确值，仅 web 端实际服务的 `assets/String.nx` 被污染，
且每次重跑注入器都会再次回归。

## 变更

- `tools/nx/patch_nx_string.py`：一次性修复 `assets/String.nx` 中
  `Map.img/etc/910000000` 的 streetName→自由市场、mapName→自由市场入口。
- `tools/nx/inject_map_nx.py`：新增
  `FM_NAME_OVERRIDES = {910000000: ('自由市场', '自由市场入口')}`，
  注入管线对自由市场强制使用原版名，防止后续重跑回归。
- `tools/nx/verify_map_nx.py`：跳过 `Skill.img`（wide-attack 管线已管理
  全部该节点，避免误报），其余全量校验。
- `scripts/wz-converter/README.md`：补充上述例外说明。

客户端读取路径无需改动：`UIMiniMap::update_text` 实时经
`NxHelper::Map::get_map_info_by_id` 读取 streetName/mapName 渲染。

## 验证

- `python3 tools/nx/verify_map_nx.py`：**ALL CHECKS PASSED**。
- 游戏内取证：test3 位于 910000000，按 M 展开 MAX 小地图，
  OCR（tesseract chi_sim）读出地图名「自由市场入口」
  （"自"被 OCR 形近误读为"白"，逐字置信度 70-96）；
  截图与运行日志在 `/tmp/fm-mapname-artifacts/`（02-fm-minimap.png）。
