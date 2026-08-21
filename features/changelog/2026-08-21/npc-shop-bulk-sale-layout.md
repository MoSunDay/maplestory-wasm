Commit: adfc88a2bc1370452adc1a03e5faefc03cc979ad

# NPC 商店批量售卖按钮避让金币栏

## Context

批量售卖入口原先从 `SELL ITEM` 的默认位置向下偏移，覆盖了金币图标与余额，并额外绘制“全部”文案，破坏了商店右上区域的原生布局。

## Change Summary

- 批量售卖按钮改为位于单件 `SELL ITEM` 按钮正上方。
- 删除附加的“全部”文本，只保留原生按钮素材。
- 同步浏览器专项验收的批量售卖按钮命中坐标，功能和二次确认协议保持不变。

## Impact Surface

- NPC 商店右上按钮区和金币余额显示。
- 不改变批量售卖 mode 4 封包、服务端事务、现金物品过滤或单件数量上限逻辑。

## Validation

- Emscripten 4.0.21 从提交 `adfc88a` 的独立干净 worktree 完成 Release 全量构建。
- 使用 `amoshe` 角色在真实 WASM 页面通过只读客户端预览打开商店界面，确认两个 `SELL ITEM` 按钮上下排列、金币图标与余额完整可见且没有“全部”文本；上方按钮可打开并取消批量售卖确认。
- 售卖纯策略验证与 WebUI 脚本语法检查通过；制品 `adfc88a-2d6e38cf` 经公开 HTTP 外层及解压后双层 SHA-256 校验通过。

## Notes / Compatibility

- UI 验收取消了确认，没有出售角色物品。
- 不新增素材、数据库结构、环境变量或服务端改动。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [NPC 商店批量售卖](npc-shop-bulk-sale.md)
- [版本化部署制品](versioned-deployment-artifacts.md)
