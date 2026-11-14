# 自由市场仓库 legacy 列表按钮与物品图标对齐

## 症状（Context）

- 自由市场 910000000 的仓库 NPC 打开 `UIWindow.img/Trunk` 的 463x318 legacy 列表窗时，底部 BtSort（整理）按钮向**左上偏移**：可见矩形落在 `(-7,263)`，而不是底部操作条预期的 `(28,272)`，与相邻按钮形成 35x9 的错位。
- 左右两列物品行中的 NX 图标**上浮 32px**：图标以 NX 底左原点直接画在行左上锚点上，和选中高亮、数量文字脱节；同一仓库的 post-Chaos 网格分支却已做原点折算，两个代际的渲染契约分叉。

## 根因

1. `MapleButton::bounds()` / `draw()` 的锚点语义是 `position - textures[NORMAL].get_origin()`（`src/client/IO/Components/MapleButton.cpp:137,145`）。legacy 按钮中只有 71x18 的 `BtSort` 带中心锚点 `origin=(35,9)`；`sort_pos()` 仍按其他 origin-less 按钮的“可见左上角”语义写成 `{28,272}`，实际可见矩形因此变成 `(28-35, 272-9)=(-7,263)`。
2. `ItemData::get_icon(false)` 返回的贴图保留 NX 左底原点（通常 `(0,32)`，`src/client/Data/ItemData.cpp:184-187`）。旧 legacy 分支把该贴图直接 draw 到 `list_icon_pos()`，没有像 grid 分支那样复制后 `shift(0,32)`，导致视觉位置整体上浮 32px。
3. `Texture::shift()` 实际只改 origin（`src/client/Graphics/Texture.cpp:204-207`）；对 `get_icon()` 返回的共享缓存贴图原地 shift 会污染后续绘制，因此必须复制。

## 修复（Change Summary）

- `src/client/IO/UITypes/UIStorageNodes.h:348-351`：legacy `sort_pos()` 从 `{28,272}` 改为 `{63,281}`（= 目标可见左上 `(28,272)` + `BtSort origin (35,9)`），并补注释说明它是 legacy 中唯一带 origin 的按钮；post-Chaos 仍返回 `{}`。
- `src/client/IO/UITypes/UIStorage.cpp:113-118`：删除 grid/legacy 两套图标分支，统一为 `Texture icon = ...get_icon(false); icon.shift({0,32}); icon.draw(iconpos);`。复制后再 shift 避免污染图标缓存；数量文字仍画在 `iconpos + (0,20)`（`:120-123`）。
- `tools/verify/storage-trunk-nodes.cpp:244-252`：新增 `sort_pos() - BtSort normal origin == (28,272)` 的 x/y 断言，未来美术 origin 变化会直接让静态验证失败。

## Impact Surface

- 仅影响 `UIStorage` 窗口：legacy 列表窗的 BtSort 可见矩形/命中框，以及左右两列物品图标；不改协议、DB 或窗口尺寸。
- post-Chaos 网格的按钮位置和图标视觉不变，但物品图标现在也走“复制 + `shift(0,32)`”的安全路径，避免共享贴图被缓存污染。
- 页签、金币、数量文字、选中高亮位置未改。

## 复现/验证建议

- 静态验证（asset 真值，期望 `ALL CHECKS PASSED`）：
  ```bash
  c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes tools/verify/storage-trunk-nodes.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp src/nlnx/nx.cpp src/client/includes/lz4.c -o /tmp/storage-trunk-nodes && /tmp/storage-trunk-nodes assets/UI.nx
  ```
- WASM 重建后在 910000000 打开仓库：模板匹配 BtSort 可见左上为 `(28,272)`，与预期 delta 为 `(0,0)`；物品图标落位 `(10,86)`、`(241,86)`、`(241,126)`、`(245,166)`、`(245,208)`。
- 存入/取出回归：`2000005` 数量 `200 → 199 → 200`，仓库与背包两侧图标均落位；操作后测试库恢复到角色 281 meso=1000000000、account 128 storage type=2 为空。
