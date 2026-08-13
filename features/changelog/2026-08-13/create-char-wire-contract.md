Commit: 27a4a800c0ed9e8d0af20a3a0b5d062ebbdfebc7

# 角色创建请求对齐 Cosmic 合约

## Context

角色创建界面已经能从非法名称、服务端拒绝和超时中恢复，但客户端仍把基础发型与颜色偏移合并为一个整数。Cosmic 在请求入口分别读取这两个字段，旧请求因此少四个字节并使后续外观字段错位，合法名称也无法可靠完成创建。

## Change Summary

- `CREATE_CHAR` 按名称、九个整数和性别字节的顺序编码，`hair` 与 `hairColor` 独立传输。
- 建角 payload 提取为无共享状态的纯编码函数，并用精确字节测试固定字段顺序、宽度和 UTF-8 名称长度。
- 登录流程将聚焦弹窗最后绘制；弹窗出现时释放并阻止底层重新取得鼠标捕获、暂停父输入框的透明 IME 覆盖层，关闭后再恢复输入，非法名称提示可见且可交互。
- 浏览器验收基于 UI 元素真实 active 状态探测非法名称提示、外观定制和返回角色列表，避免把输入框失焦、名称占用弹窗或缓存的非活动界面误判为成功；可选择在创建成功返回角色列表后结束专项流程。

## Impact Surface

- 角色创建请求编码、非法名称提示验收和生产建角专项流程。
- 不改变服务端、数据库、名称规则或现有 `CreateCharPacket` 调用方式。

## Validation

- 建角状态与 payload 精确编码测试通过，编译启用 `-Wall -Wextra -Werror`。
- Docker WASM Release 构建无警告通过。
- 真实 Chromium 候选流程通过：非法名称提示显示和关闭、重新编辑、合法名称进入定制、取消返回角色列表均有状态断言，浏览器异常为零。
- 攻击特效、残影状态及 Skill/Character NX 审计回归通过。

## Related Docs

- [网络层](../../../agents/client/Net/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [网络协议](../../../docs/ms-network-protocol.md)
