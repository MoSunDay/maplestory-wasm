Commit: ab436306c6b1126346d683645d170930594e1fc5

# Cosmic 角色列表线格式兼容

## Context

登录成功后点击 `Go To Selected Channel` 会收到 `CHARLIST`（opcode 11），旧解析器把单字节等级当作 short，并按 13 个字节读取服务器按 Java 字符长度补齐的 UTF-8 角色名。中文角色名会使后续字段整体错位，最终报 `Stack underflow`。

## Change Summary

- 角色名按 13 个 Java UTF-16 code unit 消费 UTF-8 字节，覆盖中文和四字节 Unicode 字符。
- 角色等级恢复为协议规定的单字节，并按 Cosmic 规则解析 Evan 多池 SP 表。
- `InPacket` 在读取整数前先检查完整字段长度，避免越界访问后才报告截断。
- 新增精确模拟 Cosmic `CharEntry` 的原生回归测试，验证中文名、补充平面字符、等级字段、普通 SP 与 Evan SP 表。

## Impact Surface

客户端角色列表、角色创建回执、完整角色状态解析及底层入站封包读取。

## Notes / Compatibility

仅修正客户端对 Cosmic 现有发包格式的解析，不修改服务端和 `assets/`。

## Related Docs

- [网络层](../../../agents/client/Net/index.md)
- [网络协议](../../../docs/ms-network-protocol.md)
