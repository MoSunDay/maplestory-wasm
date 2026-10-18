Commit: e683a36c8ac98d8f13d5a15928b91366c126879d

# 人物信息卡片与宠物资料

## Context

游戏内只能通过右键远端玩家发起交易，无法像旧版客户端那样双击查看自己或他人的人物资料，也没有入口查看已召唤宠物的状态或进行人气评价。

## Change Summary

- 双击本人或地图内其他玩家打开人物信息卡片；远端单击队伍选择和右键交易菜单保持可用。
- 接入 linked server 既有 `CHAR_INFO_REQUEST` / `CHAR_INFO` 合约，完整解析最多三只宠物以及坐骑、愿望单、怪物书和勋章尾段。
- 人物卡按原版 271×190 坐标展示资料；宠物卡在正下方展开为 271×387，可切换三只宠物并查看名称、等级、亲密度、饱食度和装备。
- 他人卡片提供交易和人气增减；本人卡片只读，15 级门槛、频率限制和最终人气结果由服务端裁决。
- 资料或人气响应异常、超时和过期回包只影响当前卡片，不禁用全局游戏 UI；反馈写入聊天区，不覆盖卡片字段。

## Impact Surface

地图人物双击命中、游戏 UI 窗口栈、人物信息与人气收发包、linked server 合约验证以及单/双账号浏览器验收流程。

## Notes / Compatibility

复用 linked server 现有 v83 opcode 和 `UIWindow2.img/UserInfo` 素材；未新增服务端代码、数据库字段、环境变量或持久化测试操作。

## Related Docs

- [好友与组队](../../social/)
- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
