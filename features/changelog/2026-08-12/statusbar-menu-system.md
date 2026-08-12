Commit: 3ac87b184f131b8c3c5e496384cbb6fa827435d8

# 状态栏 Menu / System 恢复可用

## Context

底部状态栏的 Menu 和 System 按钮只有按下效果，没有打开菜单或触发功能；按键设置窗口还引用了当前 UI.nx 中不存在的旧节点。

## Change Summary

- 使用 UI.nx 原生素材恢复 Menu 和 System 弹层，并保证弹层处于视口内。
- Menu 接通属性、装备、背包和技能窗口；System 接通按键设置和带中文确认的退出流程。
- 客户端尚未实现的入口使用禁用样式，单频道环境下频道切换同样禁用。
- 两个弹层互斥，支持重复点击、Escape 和外部点击关闭。
- 按键设置改为加载 `UIWindow2.img/KeyConfig` 的完整面板。

## Impact Surface

- 游戏内底部状态栏菜单交互。
- 按键设置窗口的素材来源。
- 客户端退出确认入口。

## Notes / Compatibility

- 未新增网络协议，未修改服务端或 NX 素材。
- 未实现的任务、社区、游戏选项和系统选项没有引入不完整替代窗口。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [功能索引](../../index.md)
