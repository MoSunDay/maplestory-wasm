# 商城装备分类二级菜单

## Context

用户要求「商城物品再加菜单做个装备分类」。现金商城左侧分类栏只有一级分类
（全部/装备/消耗/设置/其他/现金/礼包），「装备」会命中全部 1xxxxxxx 物品
（实测 1977 件在售商品中装备占 1448 件），无法按部位浏览帽子、上衣、武器等
常见购买目标。

## Change Summary

- `cash_shop_view` 新增纯函数：`equip_sub_of()`（物品 ID → 装备槽位子分类，
  100 帽子 … 114 勋章、130+ 武器，15..29 空档归「全部装备」）、
  `equip_sub_name()`（17 项中文标签）与 `Filter::equip_sub` 过滤字段；
  `filter_catalog` 仅在装备分类下应用子分类过滤。
- `UICashShop` 新增装备子分类浮层：点击（或再次点击 toggle）分类栏「装备」
  展开，覆盖商品区第一列；表头「装备分类」+「×」收起钮，17 行子分类各带
  实时在售数量。`BT_EQSUB_*` 按钮ID排在卡片之前，保证浮层覆盖区域点击优
  先；展开期间被遮挡列上的购买按钮禁用，防止误触购买。切换其他分类时浮层
  收起并复位为「全部装备」。
- `CashShopLayout` 增加浮层几何（x100 y86、宽 96、表头 20px、行高 22px）；
  顶部摘要文本在选中子分类时显示「装备 · 武器 · N 件」。
- 新增只读探针 `mscashshop_filtered_count` / `mscashshop_equip_sub` /
  `mscashshop_equip_menu_open`（ImeBridge EMSCRIPTEN_KEEPALIVE，商城元素
  不存在时返回 -1），供浏览器专项验收读取真实客户端状态。

## Verification

- 纯函数单测（`tools/verify/cash-shop-view.cpp`）：槽位映射、标签越界回退、
  子分类过滤、非装备分类不受子分类影响，`PASS cash shop view policy`。
- Emscripten Release 构建（-Werror）通过，探针导出核对无误。
- 浏览器专项 `tools/verify/maplestory-webui/cash-shop-equip-flow.mjs`
  17 项全部 PASS：菜单展开/收起、武器(233)/帽子(336)/全部装备(1448)过滤
  数量与复原、「×」与分类栏 toggle、切回「全部」复位、Escape 退出回游戏；
  readPixels 断言选中行暖橙高亮（rgb≈184/111/37）、未选中行深色、收起后
  原行带无暖色（卡面偏蓝）。截图证据含 11-开单/12-武器/13-帽子/14-收起/
  15-复原/16-退出。

## 不在范围

- 子分类记忆（跨分类切换持久化）刻意不做，切走即复位「全部装备」。
- 键盘遍历子分类未加（鼠标/触控即可达）；如需可复用 `next_category` 模式。
