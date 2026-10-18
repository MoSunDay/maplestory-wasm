# 商城 UI 回归 2023 高版本素材

## Context

用户要求商城 UI 使用其提供的 2023 年度冒险岛 UI 完成渲染。此前商城复用老版
`UIWindow2.img/Shop` 465×328 对话框；UI.nx 已整体换为 v153+ post-Chaos 素材，
其中带有完整的现代 `CashShop.img` 节点集，商城是最后一块仍走老版素材的界面。

## Change Summary

- `UICashShop` 重写为全屏现代渲染：`Base/backgrnd` 1024×768 拉伸铺满视口；
  商品改为 `CSList/Base` 119×184 卡片 3×2 网格，选中卡片定位 `BtBuy` 四态按钮；
  搜索栏复用 `CSSortBar`；右栏改为 `CSInventory` 面板内 8×3=24 槽位网格，模式
  切换用 `CSInventory/Tab` 芯片；余额与价格用 `Base/Number` 位图数字；翻页用
  `CSBargainSale/BtLeft/BtRight`；转移用 `CSLockerNew/LockerTransfer/BtTransfer`；
  退出用 `CSTab/BtExit`；空列表用 `Base/noItem` 占位。
- 新增 `CashShopSkin`（素材加载+硬契约校验，缺失/尺寸不符即抛错）与
  `CashShopLayout`（800×600 设计空间几何与 to_screen/to_design 纯函数），
  桌面窗口任意尺寸等比缩放居中，浏览器 800×600 视口 1:1 命中。
- `CashShopView` 分页参数化：商品 6/页、右栏 24/页；分类列/支付来源改为
  多选一区域按钮，保留购买协议所需的三种支付来源选择。
- 模型、封包、handlers 与 WASM 探针 API 全部不变；e2e 浏览器流程的
  商城点击坐标按新布局更新（搜索框、卡片、槽位网格）。
- 刻意不使用 `CSTab/Tab` 与 `CSTab/OnSale`（内嵌韩文标签），分类标签由
  客户端文字渲染，避免混排外文。

## Verification

- 纯视图策略测试：6/页与 24/页的 page_count/clamp_page/page_range 全部 PASS。
- 素材契约测试改为现代节点集（背景 1024×768、面板、全部四态按钮、数字
  字形），对 assets/UI.nx 与 linked-server 商品 CSV（SN 80000002）PASS。
- `clang++ -fsyntax-only` 整个 CashShop 目录 0 error（仅预存 deprecated 警告）。
- e2e `run.mjs` 现金商城分支坐标同步并通过 `node --check`。

## 余额请求恢复

- `Net/OutPacket.h`: 补回发送侧 `CHECK_CASH = 0xE4` 枚举(HEAD 链上缺失,仅存于 `origin/add_cashshop`)。
- `Net/Packets/CashShopPackets.h`: 新增 `CheckCashPacket`(无字段)。
- `IO/CashShop/UICashShop.cpp`: 进入商城后首次 `update()` 发送一次 CHECK_CASH,服务端回 `QUERY_CASH_RESULT(0x144)` 经 `QueryCashHandler` 写入余额,现代 UI 顶栏信用点立即显示。

## 运维注记

- e2e 进图 `std::length_error "vector"` 崩溃根因:`assets-server` 启动时 `fs::metadata` 缓存 NX 文件长度,而 `scripts/nx-l10n` 就地 append 改写了 `String.nx`(header string offset 已指向新偏移),客户端经 WebSocket 拉到「新 header + 旧长度」截断流导致字符串表越界。临时处置是重启 `ms-assets-server` 重新扫描;永久修复见同日 `assets-server-stale-nx-reload.md`(按需重载过期条目)。
- e2e 首次余额为 0 属测试账号数据问题,已为 `test1` 充值 NX(数据库 `accounts.nxCredit`)。
