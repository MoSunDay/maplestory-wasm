# 现金商城「取出被拒」后超时不锁死、余额推送兜底报错

## 症状

用户在现金商城「取出物品」再次失效：一次取出被服务端拒绝后，按钮永久
灰死，只能重进商城；后台表现为操作挂起 10 秒超时后商城进入
`require_reenter` 退出流程，或静默无任何提示。

## 根因（src/client/IO/CashShop/）

服务端包序事实（`link_repos/MapleStory-Server`）：

- BUY 成功先回 `0x57/0x89` 后推 `0x144`（showCash 重推余额）；
- TAKE 成功只回 `0x68`（无 0x144）；TAKE `item == null` 拒绝只回 `0x144`；
- 背包满时 TAKE 的 `addItem` 返回 -1，**完全不回包**（静默拒绝）；
- PUT 拒绝回 serverNotice + `0x144`。

客户端从不发送 CheckCashPacket（相关代码是死路径），因此 pending 期间
收到的 `0x144` 必为服务端推送。两处缺陷：

1. `UICashShop::update()` 的操作超时分支无条件 `require_reenter_ = true`
   并 `UI::get().enable()`（丢弃商城 UI）——把一次可重试的操作失败当成
   会话级故障，退出锁死。
2. `set_balances()` 应用余额字段后不校验当前是否处于操作 pending：静默
   拒绝场景的迟到 `0x144` 被无声吞掉，用户看不到任何失败提示。

## 修复

- `UICashShop.cpp` update()：超时分支的 `require_reenter_ = true` 与
  `UI::get().enable()` 仅在 `exiting` 时执行；操作类超时保持商城在场、
  可重试。
- `UICashShopActions.cpp` set_balances()：应用余额后若
  `cash_shop_settlement::is_operation(pending_kind_)`，弹
  「商城操作未完成，请重试。」并 return（EXIT 不受影响）。

## 验证

- `tools/verify/maplestory-webui/cash-shop-take-equip-flow.mjs` 28/28
  PASS（含「BUY 成功 + 0x144 后到」不误报，验证修复 2 的安全性）。
- 新增 `cash-shop-take-reject-retry.mjs`：背包满静默拒绝 → 超时后
  `require_reenter=0`、`can_mutate=1`、超时弹窗可关闭、仓库物品原位
  保留 → 清背包重进商城重试取出成功。
- 2026-10-30 定稿（final34，正式端点 :8000）：**42 PASS / 0 FAIL 全绿**——
  A 聊天收起、B 预填、C 第 2 件取出被拒后 pending 归零/商城不锁/按钮可点/
  弹窗 Enter 可关/物品原位保留、D `!clearslot cash` 清仓后重试取出成功并
  正常退出商城；DB 终态双通道印证（locker 4→2，两件 cap 均落 CASH 栏）。
  e2e 稳定性要点（双击 500ms 隔离、LOCKER 页签回切、行号 -1、clearslot
  需 gm=2、retry poll 弹窗容错）见 tools/verify/maplestory-webui/index.md。
