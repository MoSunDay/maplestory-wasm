Commit: 12dd3160bc45b2fdf18da6c1f740a3e705002e0a

# 非安全来源跳过持久物品全量预载

## Context

通过公网 HTTP 地址访问时，浏览器不会提供持久存储 API。旧逻辑仍完整预载 `String.nx`、`Item.nx` 和 `Character.nx` 元数据，结束时仅标记 `degraded`；这既没有跨会话持久收益，也会让大批后台请求占用当前会话资源。

## Change Summary

- 持久存储不可用或未获授权时，将持久预载明确标记为 `unavailable`，不启动批量资源请求。
- 普通 LazyFS 按需读取保持不变；持久权限可用时仍执行完整预载并校验 IndexedDB 写入结果。
- 回归测试断言不可用状态不会调用任何全量范围预取。

## Impact Surface

- 公网 HTTP、受限嵌入环境及其他没有 `navigator.storage.persist` 的浏览器来源。
- 首次进入游戏后的后台物品资源任务。

## Notes / Compatibility

- 不把不可持久的执行结果报告为降级成功；实际读取失败仍进入明确的前台错误与重试状态。
- 未修改 assets-server 协议、缓存结构或数据库。

## Related Docs

- [LazyFS](../../../agents/client/LazyFS/index.md)
