Commit: d80cad67b11e35baf1b7b0ac78ce8ea8e8a94e63

# 角色选择重贴图白屏防回归

## Context

旧版 WASM 制品会在构造角色选择 UI 时同步读取和解压人物贴图。装备 `1082003` 等重贴图角色可让资源流程长时间挂起，而旧流程已先删除世界选择 UI，导致登录界面停留在没有活动元素的 state 0。

## Change Summary

- 保留 `Texture` 构造阶段只入非阻塞 bitmap 队列、主循环按帧准备贴图的现有行为。
- 角色列表回包先完整构造并挂载 `UICharSelect`，确认安装路径完成后才移除 `WORLDSELECT` 和残留建角 UI，使登录界面转换保持有已挂载页面。
- 浏览器验收逐帧记录世界选择到角色选择的 UI state，并记录 `Character.nx` 前台贴图请求；NX 审计明确覆盖包含 158 个手套贴图部件的 `01082003.img`。

## Impact Surface

- 世界/频道确认后的角色选择页面切换
- 冷缓存角色外观贴图加载时序
- WebUI 角色选择专项验收证据

## Notes / Compatibility

不改变登录协议、角色数据、数据库、环境变量或资源格式；原生客户端仍保持既有同步图集行为，WASM 继续使用非阻塞队列。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [图形渲染](../../../agents/client/Graphics/index.md)
- [角色系统](../../../agents/client/Character/index.md)
