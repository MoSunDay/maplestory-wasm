Commit: 38e27f7292c566204925875efda52d4d81508d07

# 创建角色最终确认可恢复

## Context

角色名称首次检查通过后，玩家可能停留在外观定制界面。期间名称可能失效；Cosmic 对不可用名称的 `CREATE_CHAR` 不一定回包，客户端原先也没有最终提交状态，确认按钮会一直停留在等待外观，用户无法判断失败原因。

## Change Summary

- 名称输入在客户端识别协议长度、编码、常见非法字符和服务端保留片段，非法 ID 直接显示原生提示并保留输入。
- 最终确认前再次请求服务端检查名称，已占用或非法时返回名称输入阶段。
- 创建请求使用独立状态并禁止重复确认，但取消和外观控件保持可交互。
- 创建无回包时自动复查名称；名称被占用则明确提示，其他失败或超时恢复定制控件。
- Cosmic 复用 `DELETE_CHAR_RESPONSE(state=9)` 返回的创建数据库失败也会解除请求状态。

## Impact Surface

- 角色创建名称输入、外观定制和最终确认状态。
- `CHECK_CHAR_NAME`、`CREATE_CHAR`、`CHAR_NAME_RESPONSE` 与创建失败兼容处理。

## Validation

- 纯状态与名称策略验证程序通过，覆盖非法名称、最终预检、创建失败响应和超时复查。
- Docker WASM Release 构建成功且无编译警告。
- 浏览器 E2E 因当前 Chromium 不提供 WebGL2，未能进入客户端流程。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [网络协议](../../../docs/ms-network-protocol.md)
