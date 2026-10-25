# 现金商城购买按钮点击丢失与右栏双层网格修复

## 背景

商城目录与右栏都使用真实商品图标，但图标首次出现时会把整屏拖进素材
加载态，购买按钮因此偶发点不动；确认框和服务器拒绝路径还会留下错误的
输入捕获或 `pending` 状态。右栏同时叠加了两套网格，空栏也被烘焙格线
穿透。排查后确认三个独立根因：

1. 商品图标在 `ItemData` 中以默认 `LoadMode::BACKGROUND` 构造，首次
   绘制走 `Texture::draw`（`BLOCKING_VISIBLE`）。位图未就绪时
   `GraphicsGL` 把它记入 `pending_blocking_bitmap_ids`，
   `hasblockingbitmaps()` 随即拉起全屏素材加载层；加载期间画布不接收
   输入，按在「购买」上的 mousedown 在进入 `request_purchase()` 前就
   被吞掉。
2. 购买确认由 mousedown 内的 `UIYesNo` 打开。
   `UIStateCashShop::send_cursor()` 在按钮处理返回后仍把
   `cursor_captured` 写成 `CASHSHOP`，覆盖了弹窗 `pre_add()` 刚清掉的
   捕获；若对应的 mouseup 丢失（加载层拿走事件），下一次点击确认时
   事件会先送到已捕获的商城，确认按钮点不动。服务器拒绝购买/转移时
   可能只重发 `showCash` 余额（不回 `0x57`/`0x68` 成功，也可能没有
   `0x5C` 错误提示），`pending` 会永久保持，后续操作全部被锁死。
3. 右栏先画 `CSInventory/backgrnd`（380×213 拉伸为 288×166，内含
   烘焙槽位网格），再叠 `CSInventory|CSLocker/backgrndCover`
   （270×114）。两套网格列距不同，逐列累计偏移 8-9px；空仓库/空
   现金栏还会同时显示 `noItem`、烘焙格和 cover。

## 变更

### 商品图标改为瞬态绘制

- `src/client/IO/CashShop/UICashShopPanels.cpp::draw_icon()` 从
  `Texture::draw` 改为 `Texture::draw_effect`（`TRANSIENT_EFFECT`）。
  图标仍会在首次绘制时入队并按需流式加载，但不再进入 blocking 集合，
  也不会触发全屏素材加载层；缺图的那一帧只跳过该图标，后续帧补齐。
- 目录卡、右栏槽位、详情和推荐面板共用该 helper，因此所有商品图标
  一致非阻塞。

### 购买按钮探针与真实点击路径

- `src/client/IO/CashShop/UICashShop.h::button_center()` 读取按钮真实
  命中矩形的中心，`src/client/IO/ImeBridge.cpp` 的
  `mscashshop_buy_x`/`mscashshop_buy_y` 暴露 `BT_BUY` 中心；商城未
  打开或按钮缺失时返回 `-32768`。
- `cash-shop-pet-flow.mjs` 的购买路径改为「选中目录卡 → 读取 `BT_BUY`
  真实中心 → 点击购买按钮 → 确认框确认」，不再复用卡片坐标二次点击；
  探针为 `-32768` 时直接报错，避免旧构建误判。

### 服务器拒绝与 pending 释放

- `src/client/IO/CashShop/UICashShopActions.cpp::set_balances()` 在收到
  余额同步时若 `pending` 仍为真，调用
  `show_error("商城操作未完成，请重试。")` 释放 pending；这覆盖服务端
  拒绝交易但只重发 `showCash`、不回 `0x57`/`0x68` 的路径。
- 同文件 `request_purchase()` 在原有页内校验之外补充 `pending` 守卫：
  翻页/切分类/搜索后已离开当前页的失效选中不会再发购买封包，进行中
  的请求也不会被重复触发。

### 弹窗捕获与资源阻塞

- `src/client/IO/CashShop/UIStateCashShop.cpp::send_cursor()` 进入分发
  前先检查：焦点在非商城弹窗上而 `cursor_captured` 仍停在 `CASHSHOP`
  时清空捕获；分发后只有焦点未变化才继续持有捕获。`BT_BUY` 在
  mousedown 内打开 `UIYesNo` 后，商城不会再把后续点击抢回自己。
- 新增 `src/client/IO/UI.cpp::set_cursor_idle()`；`Journey.cpp` 更新
  循环在 `asset_blocking` 且当前处于 `CASHSHOP` 时复位光标状态，丢掉
  因加载层吞掉 mouseup 而卡住的 `CLICKING`，加载结束后的第一下点击
  回到当前焦点/弹窗。

### 右栏扁平单层网格

- `UICashShop.cpp` 的右栏绘制抽到
  `src/client/IO/CashShop/UICashShopPanels.cpp::draw_right_pane()`，
  面板本身改为一块扁平单层的填充 + 边框 + 标题下分隔线，不再绘制
  `CSInventory/backgrnd` 的烘焙网格与原生标题带。
- 仅当当前页有物品时才画 `CSInventory|CSLocker/backgrndCover`：
  7 列 × 3 行、格 36px、间隙 3px，合计 270×114，起点
  `SLOT_GRID_POS{517,88}`，末格右下角落在 cover 右下角 `(787,202)`；
  面板 `508..796` 左右各留 9px 框住 cover。
- 空仓库/空现金栏不画 cover 与槽位，只在面板中央画 `Base/noItem`
  （348×148 原生，显示 272×116），覆盖整个槽位区域，不再在空提示下
  叠烘焙格或 cover。

## 验证

- 静态检查：`git diff --check`、相关 `.mjs` 的 `node --check` 通过。
- WASM 重建：`./scripts/docker_build_wasm.sh -j 8`，输出
  `Build finished successfully.`。
- 右栏几何纯函数回归：`tools/verify/cash-shop-scale.cpp` 通过；新增
  四条断言：cover 为 7×3 36px 格 + 3px 间隙 = 270×114、末格右下角
  对齐 cover 右下角 `(787,202)`、空提示四角覆盖 cover、面板左右各
  9px 边距。
- 真实购买链路：`cash-shop-pet-flow.mjs` 54 项通过；`BT_BUY` 探针
  `(146,214)`、确认框正常打开、`pending=0`、NX 扣 4900、商城仓库 +1，
  并完成取出、召唤、自动捡取与自动嗑药。
- 弹窗输入隔离：`cash-shop-modal-guards.mjs` 68 项通过；确认弹窗外
  双击/滚轮/翻页不穿透，守卫用例本身不结算交易。
- 宠物 UI 交互：`cash-shop-pet-ui-interactions.mjs` 81 项通过；确认
  图标加载不再吞点击。
- 两档分辨率截图：`cash-shop-resolution-shot.mjs` 以 `SHOP_RES=1`
  （1280×720）与 `SHOP_RES=none`（800×600）各跑一次，均
  `filtered=1984`，右栏单层网格缩放后仍对齐。

## 回退

右栏扁平化只改 `draw_right_pane()` 的绘制。若扁平单层丢失过多原生边框
质感，可保留扁平面板，仅在 `SLOT_GRID_POS`/`SLOT_COVER_SIZE` 区域补
一块不透明遮罩再画 cover，遮掉旧烘焙格线即可，无需恢复整张
`CSInventory/backgrnd`。`draw_effect` 可单独改回 `draw`，但会重新引入
blocking 加载与点击吞掉的风险。服务端购买/转移逻辑未改动，回退不影响
协议与结算。

## Related Docs

- [现金商城](../../cash-shop/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [图形渲染](../../../agents/client/Graphics/index.md)
- [MapleStory WebUI 全流程](../../../tools/verify/maplestory-webui/index.md)
- [验证脚本索引](../../../tools/verify/index.md)
