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

## 内容扩充

用户复核现代版截图后认为「内容太少」，要求把 `CashShop.img` 中尚未使用的可用素材
和现有目录/仓库字段补进可见界面。本轮只扩充展示，不改变购买、仓库协议、分页选择
与探针 API。

- 顶栏余额前补 `CSIcon/0`、`CSIcon/1` 图标（预付点复用 NX 图标），图标与右对齐
  位图余额分开定位，长余额不会压住图标。
- 分类栏每行右侧显示该分类在当前性别/搜索条件下的商品数，选中分类用黑字、其余
  灰字；搜索栏上方增加「分类 · N 件」筛选摘要。
- 商品卡片补 `CSEffect` 徽章与期限标签。徽章优先级：多件→特惠、1-30 天→限时、
  礼包→活动、priority≥10→新品、priority≥8→热卖，其余无徽章。
- 左侧空余区域新增徽章图例：五枚 24×22 徽章配「特惠/限时/活动/新品/热卖」
  释义和底部说明。
- 右栏详情由单行扩展为名称、数量、SN、期限/性别/赠送来源/有期限四组字段；仅
  商城仓库读取 SN、赠送与到期数据，切到角色现金栏时不复用仓库字段。
- 右栏底部新增「本期推荐」面板：随目录页轮换 `Base/Preview/0..2` 暗化底图，
  展示当前筛选首个商品的图标、名称×数量、分类·期限和价格；点击面板选中当前页
  高亮项，不重置分页与搜索，空结果显示「当前筛选没有在售商品」。
- 左右库存槽位补 `CSInventory/backgrndCover`/`CSLocker/backgrndCover`
  7×3 半透明格底，保留原素材槽位层次。
- `CashShopSkin` 对 `Base/Preview/0..2`、`CSEffect/{new,hot,sale,event,limited}/0`、
  `CSIcon/0..1`、`CSLocker/backgrndCover` 增加尺寸硬契约；`CashShopView` 新增
  纯函数 `commodity_badge`/`badge_name`，便于单测优先级。

### 验证

- `docker compose run --rm wasm-builder`：WASM 构建通过。
- `cash-shop-view`：新增 SALE/LIMITED/EVENT/NEW/HOT/NONE 六种徽章与阈值断言，
  连同 12/21 分页 PASS。
- `cash-shop-ui-nx assets/UI.nx src/client/data/cash-shop-v83.csv`：预览帧、五枚
  徽章、两张货币图标和仓库格底尺寸契约 PASS。
- `E2E_CASH_SHOP=1 E2E_CREATE_CHARACTER=0`：搜索 SN、购买、入库、取出、退回与
  退出 PASS，浏览器异常 0；还原截图确认顶栏图标、分类件数、卡片徽章、图例、
  详情四行、推荐面板和槽位格底均可见。

## 人物穿戴预览

用户反馈「本期推荐」只显示商品图标，要求把高亮装备穿到当前角色身上预览。实现复用
`CharLook` 现有绘制栈，不改动商品模型、封包与探针 API。

- `CashShopView` 新增纯函数 `is_previewable_equip` 与 `highlighted_index`：前者只
  接受 CharLook 有可见层的装备槽位（帽、脸饰、眼饰、耳环、上衣、套服、下装、鞋、
  手套、披风、盾与 130-149 武器组），戒指、坠饰、腰带、勋章及非装备返回 false；
  后者优先返回当前页内的选中项，未选中或选中项不在本页时取本页首项，空页返回
  `nullopt`，供面板绘制与点击选择共用同一高亮口径。
- `UICashShop` 进入商城时把玩家 `CharLook` 复制为 `preview_look`，高亮项可试穿时
  调用 `add_equip(item_id, Texture::LoadMode::DEFERRED)`；重建筛选只置
  `preview_dirty`，真正的 `refresh_preview()` 延后到 `update()` 执行，避免 IME
  Enter 回调在 Asyncify 挂起栈内重入角色换装。
- 推荐面板内新增 92×98 头像框，位于 (504,452)，按 `STAND1`/`DEFAULT` 绘制并带
  y=548 地板线；标题在「本期推荐」「试穿预览」间切换，提示区分「点击卡片选中并
  试穿」「已穿戴 · 点击其他装备试穿」与「角色预览不可用」，不可试穿时保留商品
  图标。
- `BT_FEATURED` 点击时把当前页高亮项写入选中态，不重置分页、搜索与分类。

### 验证

- `docker compose run --rm wasm-builder`：WASM 构建 PASS。
- `cash-shop-view`：新增可见装备槽位/回退范围与 `highlighted_index` 跨页选择断言，
  连同既有徽章与分页断言 PASS。
- `E2E_CASH_SHOP=1 E2E_CREATE_CHARACTER=0`：搜索 SN `20000016`（在售帽子
  `1002000`）并点选卡片后输出截图 `10b-cash-shop-avatar-preview`，确认推荐面板
  绘制当前角色试穿；随后继续 SN `80000002` 购买、取出、退回与退出流程 PASS，
  浏览器异常 0。

## 余额请求恢复

- `Net/OutPacket.h`: 补回发送侧 `CHECK_CASH = 0xE4` 枚举(HEAD 链上缺失,仅存于 `origin/add_cashshop`)。
- `Net/Packets/CashShopPackets.h`: 新增 `CheckCashPacket`(无字段)。
- `IO/CashShop/UICashShop.cpp`: 进入商城后首次 `update()` 发送一次 CHECK_CASH,服务端回 `QUERY_CASH_RESULT(0x144)` 经 `QueryCashHandler` 写入余额,现代 UI 顶栏信用点立即显示。

## 运维注记

- e2e 进图 `std::length_error "vector"` 崩溃根因:`assets-server` 启动时 `fs::metadata` 缓存 NX 文件长度,而 `scripts/nx-l10n` 就地 append 改写了 `String.nx`(header string offset 已指向新偏移),客户端经 WebSocket 拉到「新 header + 旧长度」截断流导致字符串表越界。临时处置是重启 `ms-assets-server` 重新扫描;永久修复见同日 `assets-server-stale-nx-reload.md`(按需重载过期条目)。
- e2e 首次余额为 0 属测试账号数据问题,已为 `test1` 充值 NX(数据库 `accounts.nxCredit`)。
