Commit: f516d26de042f9666f55e27b9edfb310a3b17d2b

# 快捷键键帽图标对齐

## Context

快捷键窗口已切换到 `UIWindow2.img/KeyConfig`，但映射图标和鼠标命中仍沿用旧键盘面板的硬编码坐标，导致图标偏离白色键帽，拖放目标也与画面不一致。

## Change Summary

- 按当前 `backgrnd3` 键盘素材重新定义功能键、主键区、导航键和修饰键矩形。
- 图标绘制、点击拾取、拖入绑定和拖出解绑统一使用同一份布局。
- 布局提取为纯函数模块，避免素材坐标与交互坐标分别维护。

## Impact Surface

游戏内 System → Key Settings 窗口的键位映射显示和拖放交互。

## Notes / Compatibility

不修改 `UI.nx`、默认键位、保存封包或服务端逻辑；窗口整体位置和底部未绑定动作区保持不变。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
