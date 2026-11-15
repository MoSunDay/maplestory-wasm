# 角色选择界面重做为 2023 扁平 UI：程序化面板绘制、NX 素材降级为点缀层

## 背景（Context）

- v83 客户端的角色选择沿用 pre-Chaos 位图窗框（旧 `UICharSelectLayout.h`
  提供的 NX 节点布局），与现金商城已落地的 2023 扁平（现代）风格割裂，
  且 chrome 硬依赖旧位图节点，素材缺失即整窗塌陷。
- 目标：与 CashShop 同风格的近黑半透明面板 + 橙色点缀，改为程序化绘制
  （procedural flat chrome），摆脱对 pre-Chaos 位图的硬依赖；同时保持
  e2e 锚点不变，协议零改动。

## 改动（Change Summary）

- `src/client/IO/UITypes/UICharSelect.cpp/.h` 全量重做（795 → 768 行）：
  删除全部 pre-Chaos 回退分支，chrome 改由扁平绘制原语生成。
- 新增 `src/client/IO/CharacterSelect/CharSelectModernLayout.h`（纯布局
  常量/函数）、`CharSelectModernPanels.h/.cpp`（扁平面板绘制）、
  `src/client/IO/Components/FlatDraw.h`（扁平绘制原语）。
- 删除 `src/client/IO/UITypes/UICharSelectLayout.h`。
- NX 素材仅作点缀层：世界横幅 (578,42)、数量窗 charSlot (589,106)、
  名牌、等级 charset、翻页箭头 (46,240)/(533,240)、BtSelect/BtNew/
  BtDelete 按钮位图、角色圆盘/预览；可选 NX 节点缺失时降级为 Console
  警告而非报错。
- e2e 兼容锚点原样保留：单行花名册落地点 (130+120i,270)、热区
  (100+120i,160,60x120)、翻页箭头、建角按钮点击 (250,515)。
- 协议零改动：SelectCharPackets / LoginHandlers / CharlistTransition
  均未触碰。

## 验证（Verification）

- `tools/verify/maplestory-webui/layout-pixels.mjs` 的角色选择断言按
  扁平 chrome 重写（暗色面板 vs 亮色背景边距的亮度对比，画布
  VIEWYOFFSET=10 偏移已注明），`E2E_STOP_AT_CHAR_SELECT=1 node
  tools/verify/maplestory-webui/run.mjs` 下通过。
- 新增原生契约测试 `tools/verify/charselect-2023-geometry.cpp`，校验
  布局不变量（落地点/热区/翻页箭头/按钮/世界横幅/数量窗锚点）。

## 影响面（Impact Surface）

- 仅客户端角色选择界面；登录、建角、协议、DB 均不变。
- 缺失可选 NX 节点的资产包不再阻断渲染，只降级为 Console 警告。
