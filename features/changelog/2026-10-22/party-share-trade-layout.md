# 组队收益、邀请入口与交易室布局

## Context

组队击杀仍按伤害贡献和队伍加成拆分经验，人物卡与右键菜单没有组队邀请入口；交易室的部分文本、物品和聊天输入框使用手填坐标，未对齐 `CashTradingRoom` 的 NX 底图。

## Change Summary

- linked server 对击杀者队伍改为同图成员各获得怪物完整基础经验，保留原有等级门槛和等级差限制，不再叠加伤害拆分与队伍加成。
- 金币拾取继续按同图队员均分，无同图队员时由拾取者获得全额。
- 人物卡的 `BtParty` 按钮发送 `InviteToPartyPacket`，仅对已加载的他人卡片启用。
- 地图右键菜单扩展为「交易」和「组队邀请」两行，按行分发对应邀请封包。
- 交易室的双方铭牌、角色预览、物品格栅、金币、页码、聊天记录和输入框改用集中布局常量，对齐 NX 底图的铭牌、白色金币行和右侧聊天面板。

## Impact Surface

- linked server 怪物经验分配
- 玩家金币拾取
- 人物信息卡与地图右键菜单
- 面对面交易窗口几何与聊天输入

## Validation

- `tools/verify/trade-ui-nx.cpp` 校验交易室底图尺寸、origin 与按钮资源。
- `tools/verify/trade-layout.cpp` 校验物品五行的 40px 步距、金币行、聊天区和输入框几何。
- `tools/verify/player-context-menu.cpp` 校验右键菜单交易/组队两行命中范围。
- `tools/verify/user-info-ui-nx.cpp` 校验人物卡 `BtParty` 四态资源与卡片内边界。
- `tools/verify/party-share-server-contract.mjs` 校验 linked server 全额经验与同图金币平分逻辑。
- `tools/verify/maplestory-webui/social-flow.mjs` 双账号真实浏览器验收全部断言通过：人物卡 `BtParty` 与右键菜单两条组队邀请入口（含拒绝后清理）、好友申请/接受/聊天/离线与上线同步、组队创建/邀请/拒绝/重新邀请/成员快照/队聊/HP/转让队长/踢出/解散、好友删除与被拒申请清理。
- 另一轮独立真实浏览器交易验收经右键菜单「交易」行进入交易室并留存截图，只覆盖入口与房间呈现，不构成完整交易事务；交易提交语义不在该轮范围内；客户端状态与封包解析由 `trade-state.cpp`、`trade-item-parser.cpp` 覆盖，最终以 linked server 校验为准。

## Related Docs

- [好友与组队](../../social/index.md)
- [玩家面对面交易](../../player-trade/index.md)
- [验证工具](../../../tools/verify/index.md)
