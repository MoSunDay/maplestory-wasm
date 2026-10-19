# 撤销频道窗未用格位客户端遮盖

## 背景

单频道由服务端配置驱动：`link_repos/MapleStory-Server/world.ini` 的
`channels0..4=1`，当前服务端实例每世界仅开放 1 个频道。频道数量本就是
服务端事实，客户端不应在 UI 层另行"画盖子"伪装。

## 改动

- 撤销 `UIWorldSelect` 的 `draw_channel_cover()` 遮盖改动（ebe5301 /
  0c15bb9 的 UI 部分）：`src/client/IO/UITypes/UIWorldSelect.{cpp,h}`
  恢复至 49e4dc6 版本，删除遮盖相关的面板配色常量与绘制分支。
- 频道窗口恢复 NX 素材原生外观：未用格位显示为素材自带的禁用态，
  而非客户端绘制的纯色矩形。
- 同步移除对应 changelog 文档
  （`channel-window-disabled-cells-cover.md`，及组合文档中的遮盖小节）。

## 保留

- `tools/verify/nxppm` 取证工具保留：另被 FM NPC（079MAX4 移植）工作
  引用，与本次撤销无关。

## 验证

- 重建 WASM 客户端通过；webui e2e exit 0，频道窗截图仅显示频道 1，
  素材原生禁用态格位可见。
