Commit: 18144286a501ba0db7bdd12eb80b3842db39d0ac

# 角色选择来源界面恢复

## Context

单个角色预览已经可以用 `FAILED` 结束 pending，但 CHARLIST 解析、角色选择静态 UI 或顶层更新若发生异常，旧流程仍可能留下禁用界面；世界选择的请求中标记也无法复位，返回后不能再次请求角色列表。

## Change Summary

- 世界选择或角色创建界面保留到全部角色预览收敛后再释放，作为失败恢复入口。
- CHARLIST 解析、UI 安装和角色选择顶层加载失败时恢复来源界面与全局输入；世界选择同时复位请求门闩，允许再次进入。
- 单个预览的构造和更新异常统一结束该角色 pending；绘制异常只记录一次警告并跳过该角色画面，不影响选择角色或进入游戏。
- 恢复来源选择、请求门闩和真实异常捕获均有独立回归边界；正常浏览器链路验证三名角色全部收敛、UI 解锁并成功进入地图。

## Impact Surface

世界选择到角色选择的 CHARLIST 事务、角色预览构造/更新/绘制失败路径与登录 UI 输入状态。登录协议、linked server、数据库、资源数据和环境变量不变。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [背面手套图层与角色预览失败收敛](./character-preview-failure-and-back-glove.md)
