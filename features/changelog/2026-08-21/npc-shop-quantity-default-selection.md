Commit: 1dc1c57bd59dc76444cae0cacedd95e490237a8e

# NPC 商店购买数量默认值直接替换

## Context

购买可堆叠消耗品时，数量弹窗预填 `1`，但光标停在末尾。用户首次输入会得到 `15` 之类的拼接结果，必须先手动删除默认值，增加了高频购买操作的负担。

## Change Summary

- 数字输入弹窗打开后全选预填值；首次字符输入、退格或粘贴会替换整段默认值，直接确认则仍提交原默认值。
- `Textfield` 在本地键盘路径维护一次性全选状态，左右移动可分别折叠到开头或末尾。
- WASM IME 桥同步隐藏 textarea 的真实选择范围，并阻止全选触发的 `selectionchange` 立即折叠选择。
- 选择范围计算保持为可独立验证的纯函数。

## Impact Surface

所有复用 `UIEnterNumber` 的数字输入弹窗，包括 NPC 商店购买/出售与仓库存取；协议、数据库、环境变量和 NX 素材不变。

## Validation

- `node tools/verify/ime-input.cjs` 验证默认值全选、普通光标与空值边界。
- NPC 商店数量和售卖策略回归通过。
- Emscripten Release WASM 全量构建通过。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [NPC 商店批量售卖](npc-shop-bulk-sale.md)
