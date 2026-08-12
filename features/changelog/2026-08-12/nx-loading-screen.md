Commit: fc979c0fe21730064ed2054302453773440d270d

# NX 素材加载提示

## Context

WASM 客户端初始化和按需读取 NX 素材期间，游戏画布尚未产生首帧，页面只显示黑屏，用户无法区分正常加载和卡死。

## Change Summary

- 页面打开后立即显示带动画的加载层，并在 NX 读取阶段明确提示正在加载游戏素材。
- NX 完成后提示正在初始化界面；客户端完成首帧绘制后再淡出加载层，避免提前露出黑色画布。
- 初始化失败时保留加载层并直接显示错误，避免动画无限运行。
- WebUI 验收覆盖加载层初始可见和首帧后关闭两个状态。

## Impact Surface

浏览器入口页面、WASM 客户端初始化生命周期和 WebUI 验收流程。

## Notes / Compatibility

加载层不修改 NX 素材和 LazyFS 协议，也不伪造无法准确计算的下载百分比。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
- [LazyFS](../../../agents/client/LazyFS/index.md)
