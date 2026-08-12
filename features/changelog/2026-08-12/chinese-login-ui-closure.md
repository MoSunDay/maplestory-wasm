Commit: 25a0dc21919c39a58ab22eaa8d35ab91653c842f

# 中文登录链路与选择界面闭环

## Context

浏览器输入和字体已经支持 UTF-8，但角色名仍会在 HeavenMS 的 US-ASCII 封包及 latin1 数据库列中丢失；新版 `UI.nx` 还暴露了频道和角色选择布局缺失。

## Change Summary

- 登录及聊天协议字符串统一按严格 UTF-8 字节长度处理，固定角色名字段保持 13 字节并安全补零。
- 好友申请保留 v83 的 11 字节兼容副本；12 字节名字仅在该副本中按完整 UTF-8 码点截取，前置变长字段仍传输完整名字。
- 角色名允许 3–12 UTF-8 字节的 Unicode 字母或数字，并保留屏蔽词与唯一性检查。
- 频道按钮严格按服务端返回数量绘制，频道面板锚定顶部。
- 角色选择恢复角色槽计数、人物平台和属性面板对应字段。
- WebUI 验收增加真实 UI 状态探针、可达端点预检和浏览器控制台/网络错误证据，避免固定延时点击误报成功。

## Impact Surface

客户端登录 UI、HeavenMS 封包读写、角色名校验和 `characters.name` 数据库字符集。

## Notes / Compatibility

v83 的 13 字节固定字段不变；NX/WZ 仍是只读运行资源；自动注册默认策略不变。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [功能索引](../../index.md)
