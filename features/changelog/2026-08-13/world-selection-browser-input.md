Commit: 7e68ababb9b31bcb7f6237f78081f84cdd25ab93

# 世界选择浏览器输入兼容

## Context

部分浏览器在 Canvas 上没有把物理点击稳定送入 GLFW，表现为确认按钮单击和频道双击均无反应；协议与 Cosmic 角色列表响应本身正常。

## Change Summary

- 增加浏览器原生 `click`/`dblclick` 兼容路径，把 CSS 缩放坐标还原为 800×600 游戏坐标后识别确认按钮与频道 1。
- 浏览器兼容路径和 GLFW 路径共用幂等的角色列表请求入口，避免一次物理操作重复发包。
- 登录连接已经关闭时不再静默无响应，直接提示用户刷新页面。
- 浏览器验收覆盖物理单击确认、物理双击频道、纯 DOM 单击确认和纯 DOM 双击频道。

## Impact Surface

浏览器世界选择输入、WASM 导出接口和 WebUI 自动验收。

## Notes / Compatibility

不修改 Cosmic 服务端、登录协议或频道配置；正常 GLFW 鼠标输入仍是主路径。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [Web 基础设施](../../../agents/web/index.md)
