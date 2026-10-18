# 商城 UI 回归 2023 高版本素材

## Context

用户要求商城 UI 使用其提供的 2023 年度冒险岛 UI 完成渲染。此前商城复用老版
`UIWindow2.img/Shop` 465×328 对话框；UI.nx 已整体换为 v153+ post-Chaos 素材，
其中带有完整的现代 `CashShop.img` 节点集，商城是最后一块仍走老版素材的界面。
现代版首轮上线后，用户反馈「可用但样式和元素都错位」；把 e2e 截图按画布还原
后确认所有元素 y 坐标统一多出 `VIEWYOFFSET`（10px），同时分类标题、支付来源
标签和卡片价格因颜色/文本遗漏而不可见。

## Change Summary

- `UICashShop` 重写为全屏现代渲染：客户端画布固定 800×600，`Base/backgrnd`
  1024×768 拉伸铺满；`CashShopLayout` 用 `px()` 把 1024 原生坐标等比映射到
  800 宽画布。商品 4×3=12 张 93×144 卡片，右栏 `CSInventory` 面板 7×3=21
  槽位；余额与价格用 `Base/Number` 位图数字；翻页用
  `CSBargainSale/BtLeft/BtRight`；空列表用 `Base/noItem` 占位。
- 新增 `CashShopSkin` 素材加载与硬契约校验（缺失/尺寸不符即抛错），以及
  `CashShopLayout` 800×600 画布几何与分页/选中纯函数。模型、封包、handlers
  与 WASM 探针 API 全部不变。
- 绘制统一经 `draw_position()` 减去 `VIEWYOFFSET`，布局常量与 `AreaButton`
  命中框保持画布坐标，视觉与点击区域同时对齐；搜索框向 IME 报告画布坐标，
  避免候选框和 caret 再低 10px。
- 分类行、卡片、右侧页签、库存面板头部、槽位、详情与操作按钮改为扁平自绘，
  覆盖素材内嵌韩文；仅背景、卡片底、面板、页签底条、数字、空态图标、选中
  标记和翻页箭头继续使用位图。按钮四态仍由 `AreaButton` 驱动，选中/悬停使用
  橙色或灰色内框。
- 分类标题改白色；支付来源 chip 补上「信用点/地图点/预付点」文本；卡片价格
  用黑色位图数字（`Base/Number` 原为白色字形，直接画在白色卡面上会消失），
  顶栏余额仍为白色。`pill_label()` 按 A11M 的 10px 可见行高修正文字基线。
- 刻意不使用 `CSTab/Tab`、`CSTab/OnSale`、`CSTab/BtExit`、
  `CSStatus/BtCharge`、`CSLockerNew/LockerTransfer/BtTransfer` 等内嵌韩文
  按钮位图，标签由客户端文字渲染，避免混排外文。
- `CashShopView` 分页参数化：商品 12/页、右栏 21/页；分类列/支付来源改为
  多选一区域按钮，保留购买协议所需的三种支付来源选择。e2e 浏览器流程的商城
  点击坐标按新布局更新（搜索框、卡片、槽位网格）。

## Verification

- `docker compose run --rm wasm-builder`：WASM 构建成功。
- 纯视图策略测试 `cash-shop-view`：12/21 分页、page_count/clamp_page/
  page_range 全部 PASS。
- 素材契约测试 `cash-shop-ui-nx`：现代节点集（背景 1024×768、面板、四态
  按钮、数字字形）对 assets/UI.nx 与 linked-server 商品 CSV（SN 80000002）
  PASS。
- `E2E_CASH_SHOP=1` 浏览器全流程 PASS：搜索 SN、购买确认、入库、取出、
  退回仓库与退出商城均通过，浏览器异常 0；还原截图确认顶栏、搜索、币种、
  分类、卡片、右侧面板、details 与按钮的 y 坐标和布局常量一致。

## 余额请求恢复

- `Net/OutPacket.h`: 补回发送侧 `CHECK_CASH = 0xE4` 枚举(HEAD 链上缺失,仅存于 `origin/add_cashshop`)。
- `Net/Packets/CashShopPackets.h`: 新增 `CheckCashPacket`(无字段)。
- `IO/CashShop/UICashShop.cpp`: 进入商城后首次 `update()` 发送一次 CHECK_CASH,服务端回 `QUERY_CASH_RESULT(0x144)` 经 `QueryCashHandler` 写入余额,现代 UI 顶栏信用点立即显示。

## 运维注记

- e2e 进图 `std::length_error "vector"` 崩溃根因:`assets-server` 启动时 `fs::metadata` 缓存 NX 文件长度,而 `scripts/nx-l10n` 就地 append 改写了 `String.nx`(header string offset 已指向新偏移),客户端经 WebSocket 拉到「新 header + 旧长度」截断流导致字符串表越界。临时处置是重启 `ms-assets-server` 重新扫描;永久修复见同日 `assets-server-stale-nx-reload.md`(按需重载过期条目)。
- e2e 首次余额为 0 属测试账号数据问题,已为 `test1` 充值 NX(数据库 `accounts.nxCredit`)。
