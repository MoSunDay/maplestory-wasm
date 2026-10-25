# 现金商城确认弹窗输入隔离与失效选中守卫

## 背景

商城用 `UIYesNo`（元素类型 `NOTICE`）承接购买确认，但确认框打开后鼠标
事件仍会穿透到底层商城窗口：双击右栏储物格会发出转移请求，滚轮会翻动
左栏目录，右键也会被派发到底层元素（商城本身未接手右键，属防御性收敛），
与「弹窗期间只有弹窗接收输入」的模态语义不符。

`send_cursor()` 早就优先把光标派发给 `focused` 元素，因此点击确认框按钮
一直是正常的；缺口只出现在 `doubleclick()`、`rightclick()` 与
`send_scroll()` 这三条不看焦点、直接按坐标找前台元素的路径上。

排查同一现象时还发现两处相邻缺口：

1. `UIStateCashShop::get_front(Point)` 直接倒序返回命中的元素，而
   `draw()` 先画商城、再画覆盖层；`UIYesNo` 打开时
   `msui_front_at(400, 240)` 仍返回 `CASHSHOP`，命中判定与真实绘制层
   不一致，探针无法区分「弹窗挡住商城」和「商城在最上层」。
2. 购买确认只看 `selected_left` 是否有值。翻页、切分类或搜索后原选中项
   可能已不在当前页，此时按 Enter 仍会弹出确认框，确认的是一个看不见的
   商品；`BT_BUY` 也可能停在页外商品的卡片坐标上。

## 变更

### 弹窗期间的指针输入隔离

- `src/client/IO/CashShop/UIStateCashShop.h/.cpp`：新增私有
  `modal_focused()`——焦点类型既不是 `NONE` 也不是 `CASHSHOP`、且该元素
  仍 active 时返回 true。`doubleclick()`、`rightclick()` 与
  `send_scroll()` 在分发前先查守卫，命中即直接返回，事件不再落到下层
  商城；`send_key()` 与 `send_cursor()` 仍按焦点元素分发，确认框自己的
  Enter/Escape 与点击不受影响。

### 命中的前台层判定

- 同文件 `get_front(Point)` 改为两段式：先按元素顺序倒序命中
  `CASHSHOP` 以外的覆盖层（`NOTICE`/`UIYesNo` 等），都不命中时才以
  `CASHSHOP` 作为最底层回退返回，与 `draw()` 的绘制顺序一致。
- 保留 `CASHSHOP` 回退而不是整体排除，商城自身的卡片、翻页和右栏槽位
  仍能正常命中；覆盖层被关闭后同一坐标自然回落到商城。
- 按类型过滤的 `get_front(types)` 重载未改动；`msui_front_at` 探针因此
  能反映真实层级，可直接断言「该点是否被弹窗覆盖」。

### 失效选中不再触发购买

- `src/client/IO/CashShop/UICashShop.h/.cpp`：新增
  `left_selection_visible()`，要求 `selected_left` 有值、索引小于
  `filtered.size()`，且 `*selected_left / CATALOG_PAGE_SIZE` 等于
  `left_page`。
- `src/client/IO/CashShop/UICashShopActions.cpp`：`refresh()` 里 `BT_BUY`
  的可见性改用该判定，`BT_BUY` 点击分流同样改用它；`request_purchase()`
  自身也补上同页校验，直接调用也拒绝页外索引。
- `src/client/IO/CashShop/UICashShop.cpp`：`position_buy_button()` 与
  `send_key()` 的 `RETURN` 分支改用同一函数。翻页/切分类/搜索把选中项
  挤出当前页后，购买按钮隐藏、Enter 不再弹确认；Escape 取消 `UIYesNo`
  本来就不发封包，余额、商城仓库与角色物品栏保持不变。

## 验证

- Docker WASM Release 构建通过（`scripts/docker_build_wasm.sh`）。
- 新增 `tools/verify/maplestory-webui/cash-shop-modal-guards.mjs`
  （399 行），对 `test1` 角色卡 0 跑完 56 项检查，输出
  `PASS cash shop modal guards: 56 checks`：
  - A 弹窗外双击储物格：`msui_notice_active` 保持 1，
    `mscashshop_pending` 与仓库/物品栏/NX 余额快照全部不变；
  - B 弹窗外滚轮：覆盖层与快照同样不变，左栏页基前后一致；
  - C 翻页使原选中项离开当前页后按 Enter：不弹确认且快照不变；正对照
    （可见选中按 Enter）仍弹确认，Escape 取消后快照不变。
  - 层级断言贯穿 A/B：`msui_front_at` 在储物格与滚轮点返回 `CASHSHOP`、
    在弹窗中心返回 `NOTICE`，即本文前台层判定的回归点。
- 守卫用例本身不结算交易；仅当商城仓库为空时，脚本用 SN `80000002`
  （1 NX）买入一件备货，并在开跑前通过 DB 把账号 `loggedin` 归零。
  右键路径只有代码守卫，本轮未纳入 E2E。
- 产物默认写入 `/tmp/cash-shop-modal-guards`：截图
  `01-slot-doubleclick-guard` 至 `05-visible-enter-cancelled`、
  `browser-console.log`、`browser-errors.log`，失败时另有
  `zz-failure.png`。

## Related Docs

- [现金商城](../../cash-shop/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [MapleStory WebUI 全流程](../../../tools/verify/maplestory-webui/index.md)
