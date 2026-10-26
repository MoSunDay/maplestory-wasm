# 现金商城错误弹窗延迟化与余额就绪门禁

> 后续演进：本文的余额推送计数与帧数看门狗已由 [现金商城结算存活、操作码关联与超时](cash-shop-settlement-followup.md) 中的 `steady_clock` 期限、操作码关联和重新进入状态取代；以下保留为当轮实现记录。

## 背景

购买/转移路径上有三个相互独立的缺陷：

1. **错误弹窗释放了正在执行的回调宿主**。`UICashShop::show_error()` 在
   调用栈内直接 `UI::get().emplace<UIOk>()`。`UIOk` 与 `UIYesNo` 共用
   `UIElement::NOTICE` 槽，`UIStateCashShop::pre_add()` 会先
   `remove(NOTICE)` 把元素 `reset()` 掉；而余额不足这条错误恰好由
   `UIYesNo` 的 yes 回调触发，回到 `UIYesNo::button_pressed()` 后仍要写
   `active = false`，`UIElement::send_cursor()` 还要读它的按钮表——
   对已释放对象写入，典型的 use-after-free。
2. **余额回包可能晚于确认框**。`update()` 只发一次 `CheckCashPacket`，
   但没有记录「应该收到几份余额」。入场的 `showCash` 与 `CheckCash`
   的回包如果乱序或延迟，确认框里的余额检查会读到旧值；价格 0 的商品
   更是能直接穿过检查下单。
3. **`pending` 没有超时**。服务端既不发 `0x57`/`0x68` 也不重发
   `showCash` 时，`pending` 会永久保持，商城所有按钮持续禁用。

## 变更

### 错误弹窗改为下一帧绘制

- `src/client/IO/CashShop/UICashShop.h`：新增私有
  `std::string deferred_error`。
- `src/client/IO/CashShop/UICashShopActions.cpp::show_error()`：只做
  `set_pending(false)` + 记录 `deferred_error`，不再 emplace。
- `src/client/IO/CashShop/UICashShop.cpp::update()`：开头取出并清空
  `deferred_error` 后再 `UI::get().emplace<UIOk>(message, [](){})`。
  `update()` 不在模态回调栈上，此时 `NOTICE` 里的 `UIYesNo` 已自行
  `active = false`，替换它是安全的。错误文案与全部调用方保持不变；
  `BT_CHARGE` 的同步 `UIOk` 未改动。

### 余额推送计数与购买门禁

- 同头文件新增 `size_t balance_expected`（初值 1，入场的 `showCash` 即
  第一份预期推送）、`size_t balance_received`（初值 0）与
  `bool balances_ready() const`。
- `UICashShopActions.cpp::set_balances()`：先 `model->set_balances()`，
  再把 `balance_received` 递增到不超过 `balance_expected`（重复推送不会
  抬高欠账），最后按 `pending_watch` 释放或 `refresh()`。
- `UICashShop.cpp::update()`：只有 `!balance_requested && !balances_ready()`
  时才发 `CheckCashPacket`，发的同时把 `balance_requested` 置真、
  `++balance_expected`（查询回包算第二份预期推送）。入场推送已到达时
  不再多发查询。
- `UICashShopActions.cpp::refresh_rows()`：`BT_BUY` 的 active 与
  `buy_label` 的颜色改用 `buy_ready = left_selection_visible() &&
  balances_ready()`，原有条件（`!pending`、装备分类掩码）保留。
- 同文件 `request_purchase()`：守卫追加 `!balances_ready()`，即使有
  其他调用路径（Enter、目录卡二次点击）在余额未到前也发不出确认框，
  价格 0 的商品因此同样被拦住。

### 操作超时看门狗

- 同头文件新增 `bool pending_watch`（初值 false）与 `int pending_frames`
  （初值 0）；`set_pending()` 在设值的同时 arm/disarm 看门狗并清零计数。
- `UICashShop.cpp::update()`：`pending_watch` 时逐帧自增，达到
  `PENDING_TIMEOUT_FRAMES = 1800`（按引擎 8ms 更新步长约 14.4 秒；原文误写
  为 60fps 下约 30 秒）调用 `show_error("商城操作超时，请重试。")`，由既有
  的延迟弹窗在下一帧显示，不递归、不引入定时器或线程。
- `UICashShop.cpp::button_pressed(BT_EXIT)`：直接置 `pending = true` 的
  分支同时 `pending_watch = false`、`pending_frames = 0`，退出不是交易，
  不参与超时，也不会因为退出时的 `showCash` 弹错误。
- `set_balances()` 的错误分支由 `pending` 改为 `pending_watch`，退出
  期间到达的余额推送不再被误判为交易失败。

### 构造顺序

- `UICashShop::UICashShop()` 的初始化列表按声明顺序补齐
  `pending_watch(false), pending_frames(0), balance_expected(1),
  balance_received(0)`，避免 `-Wreorder`。

## 验证

- `git diff --check`、相关 `.mjs` 的 `node --check`：通过。
- 行数：`UICashShop.h` 215 行、`UICashShop.cpp` 727 行、
  `UICashShopActions.cpp` 659 行，均在 800 行限制内。
- WASM 重建：`./scripts/docker_build_wasm.sh -j 8`，输出
  `Build finished successfully.`。
- 几何回归：`tools/verify/cash-shop-scale.cpp` 全部断言通过。
- 弹窗隔离：`cash-shop-modal-guards.mjs` 在 test2 上完整 A-D 回归
  68 项通过；`MODAL_GUARDS_BALANCE_ONLY=1` 单独回归余额不足路径
  19 项通过。D 用例确认余额不足回车后 `msui_notice_active=1`、
  `mscashshop_pending=0`，延迟错误弹窗不再释放正在执行的 `UIYesNo`。
- 真实购买/取出/召唤：`cash-shop-pet-flow.mjs` 全部通过；购买走
  `BT_BUY` 探针，确认 `pending=0`、NX 扣 4900、仓库 +1。
- 宠物分类 UI：`cash-shop-pet-ui-interactions.mjs` 81 项通过。
- 分辨率：`cash-shop-resolution-shot.mjs` 在 1280×720 与 800×600
  两档均 `filtered=1984`，右栏单层网格与按钮几何正常。
- 浏览器用例改为等待物品栏真正打开、CASH 页选中后再按窗口实际位置
  双击槽位，避免素材流式加载期间丢点击造成假失败。

## Related Docs

- [现金商城](../../cash-shop/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
