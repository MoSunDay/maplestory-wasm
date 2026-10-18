Commit: 31fe7db09f74d8c7baa70a68f598de8084e90dac

# 角色列表挂载优先与渐进预览

## Context

远端账号 `amoshe` 的登录服链路已经证明 CHARLIST 回包成功，但旧角色选择构造器会在元素真正写入 `UIStateLogin` 前同步遍历全部 `CharLook`。包含 `1082003` 这类 158 个贴图部件装备的外观会触发大量 Character NX 元数据读取和 bitmap 入队，使前一界面已删除、后一界面尚未安装的窗口被放大为长期白屏。

## Change Summary

- CHARLIST 完整校验角色数、PIC、槽位和尾部字节，并记录解析角色、剩余字节和失败阶段。
- `UICharSelect` 构造器不再读取 NX；先安装 state 3 的轻量 UI 壳，再在更新帧加载静态 UI。
- 人物预览每帧最多构造一个，按已选角色、当前页、其余角色排序；外观纹理采用延迟模式，实际绘制时才请求当前可见贴图。
- 延迟建立的共享外观缓存被普通游戏角色复用时会补回后台预取，其他场景语义保持不变。
- WASM 导出 CHARLIST 阶段、角色、装备、剩余字节和预览进度；浏览器证据同时拦截 Character NX 元数据读取、预取、优先和前台请求，禁止任何角色素材在 state 3 安装前发生。

## Impact Surface

- 登录服 CHARLIST 解析与世界选择到角色选择的事务边界
- 角色选择静态素材和人物外观的加载时序
- Body/Hair/Face/Clothing 共享缓存的预取恢复语义
- WebUI 角色选择诊断与冷素材回归证据

## Verification

- Emscripten Release 全量编译通过，新增代码在 `-Werror` 下无警告。
- 角色预览纯策略验证覆盖选中角色、当前页、其余角色和越界页。
- Cosmic 角色列表解析验证覆盖 UTF-8 定长名、short 等级、Evan SP 表及精确消费位置。
- Character NX 审计覆盖 236 件手套、40208 个部件和 14 个 z 层，确认 `01082003.img` 有 158 个贴图部件。
- 真实浏览器已通过 WebGL2、WASM 初始化和素材失败/重试门禁；登录到角色选择的运行时转换被现有 Java Server 数据库通信故障阻断，未重启服务或改动数据库。

## Compatibility

不改变 CHARLIST 线上字段宽度、服务端代码、数据库、资源文件、环境变量或原生客户端默认同步图集行为。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [图形渲染](../../../agents/client/Graphics/index.md)
- [角色系统](../../../agents/client/Character/index.md)
