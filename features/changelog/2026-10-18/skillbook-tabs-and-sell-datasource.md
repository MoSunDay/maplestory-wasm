# 技能书页签横排 + 售物行统一 shop_nodes 数据源

## Context

收尾两项装饰性遗留：
1. 技能书（UISkillBook）职业页签沿用 v83 竖排堆叠，在两代共用的
   175×289 紧凑面板（`UIWindow.img/Skill`）上溢出/叠压。
2. Shop 售物行（SellState）绘制仍硬编码 post-Chaos 字面量
   `(243, 116+42k)` + 选中偏移 `(35,-1)`，与 SELL0..4 命中框
   （本就用 `shop_nodes::sell_item_button_pos`）数据源不一致。

## Change Summary

- `UISkillBook.cpp`：页签循环改为 `TwoSpriteButton(tabd, tabe,
  Point(tab_x, 27))`；tab_x=10 起、TAB_GAP=4，步进按
  `Texture(tabe[tabid]).get_dimensions().x()` 数据驱动，不依赖世代
  宽度表。页签行落在标题分隔线（y19-22）与深线（y42-44）之间的
  空白横带 y24..41（两代资产一致，页签坐标历来客户端硬编码，
  v83 XML 与高版 UI.nx 的 Tab origin 均为 (0,0)）。
- `SellState.cpp`：draw 改用 `shop_nodes::sell_item_button_pos(
  visible_slot)` + `shop_nodes::selection_shift()`；`UIShop.cpp` 去掉
  `sell_parent_shift()`（UIShopNodes.h 同步删除该函数）。post 绘制
  x 243→242（与命中框对齐，1px），pre 分支恢复 237/124+40k 与
  (37,0)，双世代彻底同源。

## Verification

- 全量构建通过；默认 e2e 套件全绿（E2E_CREATE_CHARACTER=0，
  "MapleStory WebUI flow completed"，0 浏览器异常）。
- 技能书：probe-windows 模板匹配窗口于 game(49.5,160.6)，tab0 图标
  像素确认渲染在窗口局部 (10,27)。
- 商店实拍（GM `!gmshop`，shop 1337，免走位）：backgrnd3 451×254
  origin(-7,-64) → 窗口 game(175,140)；边缘检测实测售物槽线
  game y 191/233/275/317/359/401（window-local 115+42k，42 步进）
  与 post 几何吻合；USE 页签命中框 game(452..494,167..186) 点击
  生效（EQUIP/USE 状态互换）；2 件测试药品精确渲染于 row0/row1，
  row2+ 为空；点击 row0 后选中箭头落于 itempos+selection_shift
  = game(445,191) 且行高亮带 y201..217 居中于槽盒。
- npc-shop e2e（E2E_NPC_SHOP=1）失败为环境性：走位后 NPC 对话框
  可开但商店未弹出，与本次纯绘制改动无关；GM 商店通道
  （`characters.gm>=2` + `!gmshop`）已验证可作替代目检捷径。

## Notes

- 测试数据已清理：wt1210（char 9）gm 还原 0、USE 药品删除。
- GM 商店目检通道已沉淀为可复用脚本
  `tools/verify/maplestory-webui/gm-shop-flow.mjs`（含几何断言与用法
  文档），替代一次性 /tmp 探针；npc-shop e2e 的环境性失败在
  `tools/verify/maplestory-webui/index.md` 已标注，几何验证走该脚本。
- 提权字段澄清：`characters` 表的 `gm` 列（非 `accounts` 表）；
  账号 test1/test1 实测进入账号 test1test1(id 26)，角色卡
  card0=e2efix1/card1=wt1210/card2=验证员一，选卡前先核对归属。
- 像素采样陷阱：游戏内光标精灵绘制在点击点右下方（约 +50/+34px），
  点击后须先把光标移出采样区再 readPixels，否则行区/页签采样被
  光标污染（本脚本 clickAway 即为此设）。
- SellItem::draw 内部 +32/+20/+17 图标文本纵向偏移为上游 v83
  布局惯例，内容底部略侵入槽间 8px 间隙，属既有行为，未改动。
