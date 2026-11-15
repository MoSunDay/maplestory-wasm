# v83 仓库动作按钮上移羊皮纸头部带 + 装备窗口默认位下移 40px

## 症状（Context）

- 自由市场 910000000 等 v83 Trunk 列表窗（`UIWindow.img/Trunk`，463x318）
  打开后，取出/存入/整理/关闭四个动作按钮紧贴**底部羊皮纸条** y246..289，
  看起来像压在纸张下沿的浮层；而页签条 y59 之上的羊皮纸头部带 y9..55
  整段空置，窗口上半部显得头重脚轻。
- 装备窗口默认出生位 `PosEQINV (250,150)` 与仓库窗口落位（canvas (168,81)
  起）在头部重叠：开仓时装备窗盖住仓库的头部/页签条，必须先拖开才点得到页
  签；现金商城取出装备后同样要先收起物品栏窗口，才能点到 CAP 插槽。

## 根因

1. `storage_nodes::*_pos()` 的 legacy 分支沿用了旧布局，把按钮锚点写死在
   底部条带上：`get_pos {28,247}`、`put_pos {110,247}`、
   `sort_pos {63,281}`（可见矩形 (28,272)）、`exit_pos {110,271}`；y9..55
   的头部带没有任何家具使用，纯属浪费。
2. `MapleButton::bounds()/draw()` 的落位语义是 `position - origin`
   （`src/client/IO/Components/MapleButton.cpp:137,145`）。legacy 四个按钮
   中只有 71x18 的 `BtSort` 带中心锚点 `origin=(35,9)`，其余 70x18 贴图
   origin-less，因此 `sort_pos()` 存的是“可见矩形 + origin”的锚点。
3. `Configuration.h` 的 `PosEQINV` 默认 `(250,150)` 与仓库窗头部带
   （y ≈ 81+9..81+75）处在同一屏幕高度，开仓即互相遮挡。

## 修复（Change Summary）

- `src/client/IO/UITypes/UIStorageNodes.h:342-360`：legacy 分支四个锚点改为
  `get_pos {28,13}`、`put_pos {110,13}`、`sort_pos {63,44}`、`exit_pos
  {110,35}`，即可见矩形 `(28,13)/(110,13)/(28,35)/(110,35)` 的 2x2 块
  （BtSort 锚点已含 origin (35,9)，可见仍是 (28,35)）；post-Chaos 分支继续
  返回 `{}`，网格窗按钮由 NX origin 自带落位。
- `src/client/IO/UITypes/UIStorageNodes.h:29-32,331-341`：文件头与按钮注释
  改为「空置的羊皮纸头部带 y9..55 承载 2x2 动作块（70x18），底部羊皮纸
  y246..289 留空、meso 行 y292..311 不变」。
- `src/client/Configuration.h:257`：`PosEQINV` 默认 `"(250,150)"` →
  `"(250,190)"`（下移 40px）；物品栏 `PosINV (300,150)` 不变。
- `tools/verify/storage-trunk-nodes.cpp:193-217,266-307`：新增
  `button_rect()`（可见矩形 = 锚点 - NX `origin`，宽高读 `normal/0` 位图），
  legacy 段对四个按钮断言 `y >= 9`、`y + h <= 58`、`x >= 4`、
  `x + w <= 248`（左于 x≈249 的中央分隔）、底边 `<= 59` 不压页签条、四者
  互不重叠；BtSort 可见左上断言由 `(28,272)` 改为 `(28,35)`。
- e2e 关窗点击点：`storage-flow.mjs:194-195` 改为 `(WINDOW_X+145, WINDOW_Y+44)`
  （BtExit 可见 (110,35)-(180,53) 的中心 (145,44)）；`storage-deposit-flow.mjs`
  与 `fm-storage-flow.mjs:209` 由底部条带的 `(306,364)` 改为 `(313,125)`。
- 装备默认位依赖：`cash-shop-take-equip-flow.mjs:54-56,306-307` 与
  `cash-cover-look-flow.mjs:93-95` 的 CAP 插槽候选 `[[309,191],[304,201]]`
  → `[[309,231],[304,241]]`（+40 y）；`tools/verify/pet-equip-layout.cpp`
  的 `window(250,150)` → `(250,190)`，注释标明镜像 `PosEQINV` 默认值。

## Impact Surface

- 仅影响 legacy（v83）仓库窗动作按钮落位与装备窗默认出生位；协议、DB、窗
  口尺寸、页签、列表行、金币、数量文字、选中高亮全部不变，post-Chaos 网格
  窗（`UIWindow2.img/Trunk`）的按钮与页签完全不变。
- 已写入本地 configuration 的 `PosEQINV` 不会被覆盖，只有新配置取新默认
  值；旧配置跑装备相关 e2e 需先重置该项。

## 复现/验证建议

- 静态验证（asset 真值，期望 `ALL CHECKS PASSED`）：
  ```bash
  c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes tools/verify/storage-trunk-nodes.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp src/nlnx/nx.cpp src/client/includes/lz4.c -o /tmp/storage-trunk-nodes && /tmp/storage-trunk-nodes assets/UI.nx
  ```
  实测四条「visible rect inside the header band」为 `28,13 70x18` /
  `110,13 70x18` / `28,35 71x18` / `110,35 70x18`，互不重叠且底边 53 < 59。
- `node --check` 覆盖五个改动的 e2e 脚本；宠物装备布局 verifier 按
  `tools/verify/index.md` 的全量命令跑（`OK pet equip layout`）。
- WASM 重建 `./scripts/build_wasm.sh --jobs 16`；浏览器验证（待执行）：910000000
  开仓后模板匹配 BtGet 可见 (28,13)、BtExit (110,35)，点 (145,44) 关窗；装备窗落 (250,190)。
