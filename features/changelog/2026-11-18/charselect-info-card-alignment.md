# 选人信息卡数值对齐：079 位图 glyph 顶偏移重校与扁平右列锚点修正

## 背景（Context）

- 079 位图界面沿用旧字体 metric：`CharSelect079Layout.h` 把 A11M 的 glyph
  顶偏移记成 11px、A12M 记成 14px（隐含 bitmap top 3）；实际打包的 Arial
  字体是 14px 行高、bitmap top 8（偏移 6）与 17px 行高、bitmap top 9
  （偏移 8），名字与九行数值的 glyph 顶因此比烘焙 caption 色带高约 5px、
  脱离色带；按真实 metric 重算后整体下沉约 5px，重新落回色带。
- 2023 扁平界面信息卡的 INT/LUK 右列锚点写成
  `CARD_CELL_INNER_X + CARD_CELL_GAP + CARD_CELL_HALF - CARD_CELL_INNER_X`
  （约 x+100），与左列锚点（x+88）几乎重合，右列数值压到 caption 上。

## 改动（Change Summary）

- `src/client/IO/CharacterSelect/CharSelect079Layout.h`：新增
  `A11M_GLYPH_TOP_OFFSET = 6` / `A12M_GLYPH_TOP_OFFSET = 8`；`INFO_NAME_POS`
  与九个 `INFO_*_POS`（job/level/fame/str/int/dex/luk/rank/jobrank）全部改为
  `INFO_TOPLEFT + 局部 y - 偏移` 表达，数值本身不变、语义更清晰。
- `src/client/IO/CharacterSelect/CharSelectModernLayout.h`：`card_label_pos()`
  的 INT/LUK 分支改为 `CARD_CELL_HALF + CARD_CELL_GAP + CARD_CELL_HALF -
  CARD_CELL_INNER_X`（x+188，落在右列药丸内边距上）。
- 协议、素材、DB 零改动。

## 验证（Verification）

- `tools/verify/charselect-079-geometry.cpp` 改为直接引用布局头常量，并新增
  6/8 字面量钉子（`A11M_GLYPH_TOP_OFFSET == 6`、
  `A12M_GLYPH_TOP_OFFSET == 8`），布局头与契约不再各自漂移。
- `tools/verify/charselect-2023-geometry.cpp` 补右列数值锚点断言：str/dex
  落左列、int/luk 落右列，右列 == `stat_row.r() - CARD_CELL_INNER_X`。
- 原生契约测试：`PASS charselect 079 bitmap geometry (800x600)` /
  `PASS charselect 2023 chrome geometry (800x600)`。
- `./scripts/docker_build_wasm.sh --jobs 8` 构建通过。
- `E2E_STOP_AT_CHAR_SELECT=1 node tools/verify/maplestory-webui/run.mjs`
  全绿：079 bitmap 契约 PASS、0 browser exceptions。
- 原生 800x600 截帧复核：数值 glyph 顶落回 caption 色带。

## 影响面（Impact Surface）

- 仅角色选择界面的文案排版与扁平回退布局；无协议、存档、素材变更。
