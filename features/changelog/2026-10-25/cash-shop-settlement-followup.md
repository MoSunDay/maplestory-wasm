# 现金商城结算存活、操作码关联与超时

## 背景

上一轮商城修复已经解决确认弹窗释放和帧计数超时，但结算状态仍有三类脆弱点：

1. **余额门禁依赖一次性的入场推送**。`showCash` 只推一次余额，旧的
   `balance_expected`/`balance_received` 计数和 `CheckCash` 兜底让
   “少收一份推送”与“交易被拒绝后重推余额”共用同一个释放条件；迟到
   的 `0x144` 既可能错误解除 `pending`，也可能被当成交易失败。
2. **成功回包没有操作码关联**。旧的 `0x68`/`0x6A` 处理直接使用当时的
   `pending_cash_id`，任何迟到或错序的仓库回包都能清掉当前操作；而
   `take_succeeded()` 在清除 `pending_cash_id` 之后才用它选中物品栏行。
3. **超时绑定更新帧数，退出没有期限**。`PENDING_TIMEOUT_FRAMES=1800`
   按 8ms 更新步长实际约 14.4 秒，且不区分窗口卡顿；退出商城既不计
   时，也不确认 `SetField` 是否真的回到当前角色。

## 变更

### 纯结算契约

- 新增 `src/client/IO/CashShop/CashShopSettlement.h`（81 行），集中定义
  `PendingKind{NONE,BUY,TAKE,PUT,EXIT}`、`steady_clock` 期限常量与纯函数：
  `is_operation`、`can_mutate`、`can_exit`、`timeout_for`、
  `response_kind`、`success_matches`、`deadline_reached`。
- 余额期限 `BALANCE_TIMEOUT=10s`、操作期限 `OPERATION_TIMEOUT=10s`、
  退出期限 `EXIT_TIMEOUT=8s`，另加 `SETTLE_GRACE=500ms` 吸收一次弹窗
  切换或掉帧；三条路径的实际墙钟预算分别为 10.5s、10.5s、8.5s。
- 成功回包映射固定为 `0x57`/`0x89`→BUY、`0x68`→TAKE、`0x6A`→PUT，
  其余为 `NONE`；`success_matches` 只接受操作类 `PendingKind`。

### 余额存活与期限

- `UICashShop` 删除 `pending`、`balance_requested`、`pending_watch`、
  `pending_frames`、`balance_expected`/`balance_received`，改为
  `pending_kind_`、`balances_loaded_`、`balance_failed_`、
  `require_reenter_` 和两个 `optional<Clock::time_point>` 期限。
- `update()` 首次绘制时武装余额期限；超时只设置 `balance_failed_` 并排
  队“商城余额加载超时，请重试。”，不锁定浏览和退出，也不再发
  `CheckCashPacket`。
- `set_balances()` 变为纯数据写入：更新余额、标记已加载、清除余额期限，
  只清除自己拥有的余额超时文本；无论迟到还是重复的 `0x144` 都不会释放
  或失败当前操作。
- 操作期限超时时清空待结算状态、恢复输入、排队“商城操作超时，请重试。”
  并设置 `require_reenter_`，要求退出后重新进入商城再交易。退出期限超时
  走独立文案并直接 `UI::enable()`，避免禁用态无法离开。
- `show_error()` 统一 `clear_pending()` 后再延迟弹窗；客户端余额不足、
  `0x5C`、未知响应和回服失败都能释放待结算状态。

### 操作码关联与唯一 ID

- `CashShopHandlers.cpp` 引入 `settlement` 命名空间和
  `warn_unexpected(what)`：`0x57`/`0x89` 仅在 `expects(BUY)` 时结算，
  `0x68` 仅在 `expects(TAKE)` 时结算，`0x6A` 仅在 `expects(PUT)` 时结算；
  不匹配的回包只记录诊断，不触碰当前操作。
- `0x68` 在解析并应用物品栏增量前捕获 `transfer_cash_id()`，因为回包
  本身不带商城唯一 ID，且成功路径会清空 pending；`take_succeeded(cash_id)`
  用它移除仓库行并选中新物品栏行。
- `0x6A` 同样先捕获存入前的现金唯一 ID，再解析 locker 行；
  `put_succeeded(item, cash_id)` 用捕获值从角色物品栏删除，避免先清
  pending 后按 0 删除。
- `0x68` 对装备落点（负数槽位）仍走原装备栏与属性刷新；非装备才向
  `UIItemInventory` 发增量。`0x5C` 和未知响应保持错误诊断语义。

### 退出与回服确认

- 新增 `leave_succeeded()`/`leave_failed()`；`SetfieldHandler::set_field`
  在 `UI::CASHSHOP` 状态下提前取得 `UICashShop*`。
- 回服 `cid` 匹配时先 `leave_succeeded()` 清空 EXIT pending，再解析完整
  角色快照和切图；`cid` 不匹配时 `leave_failed()` 保留商城、恢复输入、
  设置重新进入要求并显示“返回游戏角色不匹配，请重试。”。
- `BT_EXIT`/Escape 通过 `can_exit()`（仅无 pending 时允许）；购买、
  取回、存回按钮与 Enter 通过 `can_mutate()`（余额就绪且未失败、无
  pending、无重新进入要求）。翻页、分类、搜索和退出仍可浏览。

### 探针与回归资产

- `ImeBridge.cpp` 新增只读探针：`mscashshop_balances_ready`、
  `mscashshop_balance_failed`、`mscashshop_can_mutate`、
  `mscashshop_require_reenter`、`mscashshop_pending_op`、
  `mscashshop_selected_right_cash_id`；无商城时返回 -1。
- 新增 `Gameplay/Probes/InventoryCashProbe.cpp`：`msinventory_cash_slot`
  按现金唯一 ID 在整个 CASH 物品表中查找实际槽位，找不到返回 0；从
  `GameplayProbes.cpp` 拆出后后者保持 799 行。
- `CMakeLists.txt` 导出上述 7 个符号；`runtime-flow.mjs` 增加
  `inventoryCashSlot(cashId)` 包装。
- `cash-shop-pet-flow.mjs` 在取出前读取选中的右栏现金 ID，取出后断言
  `msinventory_cash_slot` 落在 1..24，再按真实槽位召唤；多只 5000001
  同类宠物并存时不再依赖 `inventorySlot(5000001)` 的首个匹配。
- `cash-shop-pet-ui-interactions.mjs` 新增背靠背翻页连点：不等待素材、
  连续两次分页后轮询 `pending=0`/`notice=0`、`assetBlocking=0`、
  `balance_failed=0`、`balances_ready=1`，截图 `02b-rapid-clicks`；页码
  不作断言。
- `CashShopSkin` 删除未再绘制的 `inventory_panel` 与
  `CSInventory/backgrnd` 载入，保留单层右栏实际使用的 `inventory_cover`。
- 三个商城浏览器流程在登录框输入前增加 `login assets ready` 等待，避免
  初始素材流式加载期间 CDP 点击/键盘被吞；分辨率取证在进图后等待
  `msasset_blocking=0` 且加载遮罩隐藏后再点状态栏商城按钮。

## 验证

- `git diff --check`、4 个改动 `.mjs` 的 `node --check`：通过。
- WASM Release 重建：`./scripts/docker_build_wasm.sh -j 8`，输出
  `Build finished successfully.`。
- 纯状态机：`tools/verify/cash-shop-settlement.cpp` 83 项 OK、0 FAIL，
  最终输出 `PASS cash shop settlement state machine`。
- 离线几何/素材：`cash-shop-scale.cpp` 通过；`cash-shop-ui-nx.cpp`
  输出 modern CashShop 素材契约 PASS。
- 确认弹窗守卫：`cash-shop-modal-guards.mjs` 完整 A-D 69 项检查通过；
  `MODAL_GUARDS_BALANCE_ONLY=1` 的 D 分支 20 项检查通过。
- 宠物 UI：`cash-shop-pet-ui-interactions.mjs` 91 条 PASS（含阶段汇总与
  清理）通过，覆盖新增连点/加载层/余额存活段。
- 宠物全链路：`cash-shop-pet-flow.mjs` 51 条 PASS 通过；本次购买宠物
  现金 ID 19，取出后 `msinventory_cash_slot` 返回 15，召唤能力掩码 31，
  自动捡取 3 次、自动 HP/MP 药水恢复到 30000/12004。
- 分辨率取证：`SHOP_RES=1` 与 `SHOP_RES=none` 均 `filtered=1984`，
  画布 rect 与截图写入各自临时产物目录。
- 测试后恢复 test1：`loggedin=0`、`nxCredit=100000`、地图 40001、
  meso/HP/MP 复位，删除本轮新增 petid 19 及其 CASH 行，keymap 91/92
  清理，CASH 物品恢复为原 14 行。
- 行数：`UICashShop.cpp` 743、`UICashShopActions.cpp` 743、
  `UICashShop.h` 240、`CashShopSettlement.h` 81，均在 800 行限制内。

## 边界与后续

- linked server 的部分拒绝路径只重发 `0x144`（`enableCSActions()` 实际
  是 `showCash`），不附带 `0x5C`。本轮按契约把 `0x144` 视为纯余额数据，
  因此这类拒绝不会立即弹错，而会在 10.5 秒操作期限后收敛为超时提示并
  要求重新进入商城。若要即时错误，需要服务端补发 `0x5C` 或在协议中
  提供可关联的请求 ID。
- `mscashshop_selected_right_cash_id` 经 JS number 传递时截断为 32 位；
  对当前 v83 宠物/现金 ID 足够，若未来服务端分配超过 32 位的唯一 ID，
  需要扩展桥接为字符串或高低 32 位双探针。

## Related Docs

- [现金商城](../../cash-shop/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [MapleStory WebUI 全流程](../../../tools/verify/maplestory-webui/index.md)
- [上一轮余额门禁与延迟错误](cash-shop-error-deferral-and-balance-gates.md)
