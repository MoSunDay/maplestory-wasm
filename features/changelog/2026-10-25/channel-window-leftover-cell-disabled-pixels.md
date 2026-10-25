# 频道窗剩余格位复用 layer:bg 烘焙的禁用格像素

## 背景

Post-Chaos 素材的 `Login.img/WorldSelect/BtChannel/layer:bg`（396x275）
把 5x6 频道网格的前两格直接烘焙成可用（亮）样式，其余格位为禁用
（灰）样式。服务端每世界仅开放 1 个频道，客户端只在第 1 格上绘制
频道按钮，于是第 2 格（“频道 2”）显示为可用的亮样式，而第 3 格
（“频道 3”）为禁用样式，观感不一致。

## 变更

- `src/client/Graphics/GraphicsGL.{h,cpp}`：新增按位图像素区域取样的
  `draw(const nl::bitmap&, const Rectangle<int16_t>& rect,
  const Rectangle<int16_t>& region, const Color&, float)` 重载，按
  `sub = Offset(rect.l + region.l, rect.t + region.t, region.w, region.h)`
  从整张位图截取区域绘制；要求位图已上传且 region 落在尺寸内。
- `src/client/IO/UITypes/UIWorldSelect.{h,cpp}`：post-Chaos 建窗时置
  `channelwindow = true`；`draw()` 顺序改为
  `draw_sprites → draw_disabled_cells → draw_buttons`；
  `draw_disabled_cells()` 对 `[channelcount, 2)` 的剩余格位，以
  `region = -button:2/normal/0.origin = (164,76,68,27)` 从 `layer:bg`
  拷贝烘焙好的禁用格像素，使未开放频道与频道 3 同源同色。
- 不叠加素材的 `button:N/disabled/0`：该节点与 `button:N/normal` 同为
  同一张 `_hash` 位图，板面带频道编号字形，叠上去会重影且样式仍是
  可用态外观；因此改为复用窗口位图里已经烘焙好的禁用格区域。
- 仍属“用素材像素呈现禁用态”，延续
  2026-10-19 撤销客户端遮盖（channel-window-ui-cover-revert.md）的口径。
- `tools/verify/world-select.cpp`：资源契约改为校验
  `window=Login.img/WorldSelect/BtChannel/layer:bg` 与
  `template_cell=button:2/normal/0`，并断言 `-origin`（= 烘焙禁用格在窗口
  位图中的位置）落在窗口位图范围内；越界时返回失败。
- 同一用例的请求门闩断言改回 `begin_request`/`recover_request` 纯函数
  写法：HEAD 仍引用已删除的 `RequestGate`，该 verifier 此前无法编译。

## 测试覆盖

| 层级 | 命令 | 结果 |
|------|------|------|
| 资源契约 | `c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/world-select.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/world-select && /tmp/world-select assets/UI.nx` | pass（exit 0）：`window_dimensions=396x275 disabled_cell_origin=-164,-76 dimensions=68x27 disabled_region=164,76 68x27 inside-window` |
| WASM 构建 | `bash scripts/docker_build_wasm.sh -j 16` | pass（Build finished successfully） |
| 浏览器端到端 | `E2E_STOP_AT_CHAR_SELECT=1 E2E_CREATE_CHARACTER=0 node tools/verify/maplestory-webui/run.mjs` | pass（exit 0，26 项 PASS，浏览器异常 0，产物 `/tmp/ws-final`；`02-world-select.png` 与 `03-channel-one-only.png` md5 相同） |
| 像素取证 | 截图画布取样（窗口左上 + 格子几何） | pass：频道 2/3/4/5 均值 (111.9,89.9,67.7)/(112.3,90.0,68.1)/(112.1,89.9,67.9)/(112.0,89.8,67.9)，逐通道差 ≤ 0.4；频道 1 (105.7,87.3,69.0) 保持可用态 |
| Rust 工作区 | `cargo test --workspace` / `cargo clippy --workspace --all-targets -- -D warnings` / `cargo build --workspace` | pass（未改动 Rust，仅确认工作树可用） |

- 模板匹配另确认运行时走的是 post-Chaos 分支：`layer:bg` 外框环
  NCC 0.58，pre-Chaos `chBackgrn` 0.15。

## Impact Surface

世界选择频道窗的绘制顺序与剩余格位外观；`GraphicsGL` 新增区域绘制
重载（纯新增，不改旧 `draw(bmp, rect, color, angle)` 行为）。不涉及
协议、服务端、数据库或 `.nx` 资源改动。

## Related Docs

- [验证索引](../../../tools/verify/index.md)
- [MapleStory WebUI 全流程](../../../tools/verify/maplestory-webui/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [2026-10-19 撤销频道窗未用格位客户端遮盖](../2026-10-19/channel-window-ui-cover-revert.md)
