# NPC 商店 post-Chaos 布局重标定

## 问题

post-Chaos（`UIWindow2.img`）商店窗口此前按目测估算落位，整窗错乱：

- 卖出页签悬在窗口顶端（y28..46），未骑在官方分隔线（y110）上；
- 买/卖列表行网格整体偏上 62px（首行 y53），行高 41 与 35px 格子不符；
- 买/卖/退出按钮挤在底部暗带（y276 一带），实际官方位置是分隔线
  两侧的药丸按钮（BtExit 比BtBuy 高 20px，BtSell 正对金币盒上方）;
- 金币标签画在窗口外（y3），图标/名称/价格与 35px 行格错位。

## 修复

`src/client/IO/UITypes/UIShopNodes.h` 按 UI.nx 美术真值整体重标。关键发现：
**全部页签/按钮共用窗口锚点 pos=(0,0)，art = pos − origin**（页签底边
pos.y+110 恰好骑分隔线；BtSell 右缘 454 与金币盒右缘精确重合）。

| 常量 | 旧值 | 新值 |
|------|------|------|
| `row_top` | 53 | 115 |
| `row_height` | 41 | 35 |
| `sell_row_x` | 246 | 241 |
| `recharge_pos` | {398, 70+42s} | {398, 125+42s} |
| `sell_tab_pos` | {5, 27} | {0, 0} |
| `buy_button_pos` / `sell_all_button_pos` / `exit_button_pos` | {−79,276} / {−114,276} / {190,276} | 均 {0, 0} |
| `selection_shift` | {33, −1} | {36, 0} |
| `meso_label_pos` | {451, 3} | {452, 65} |
| `icon_offset` | {0, 0} | {0, 32} |
| `name_offset` | {40, −1} | {40, 3} |
| `price_offset` | {53, 17} | {53, 20} |
| `cursor_over_sell_list` / `scroll_over_sell_list` | >245/<444、244..458 | >240/<443、241..443 |

行内偏移依据：Item 图标为底锚（origin (−3,30)，27×30），(0,32) 使图标落
y117..147；名带 y116..131、价带 y133..148（中线 y132）。滑块 {227,28}+h277
本已正确，未动。v83 分支全部保持原值。

## 验证

```bash
docker compose run --rm wasm-builder ./scripts/build_wasm.sh -j 16
mysql ... maplestory   # 前置（按名取 id，勿写死——DB 重建后自增漂移）：
                       # test1Hero 迁 260000000 + USE 栏 2000000×37@pos1
E2E_NPC_SHOP=1 E2E_CREATE_CHARACTER=0 CHROME_BIN=/usr/bin/chromium-browser \
  node tools/verify/maplestory-webui/run.mjs
```

45 项全绿（开店→USE 页签→行选中→双击卖 1 件→一键售卖清空→关店），
`shop geometry row0=(509,208) useTab=(473,177) sellAll=(590,129)` 与
窗口 (168,76)+新常量逐项吻合。像素核验（对 UI.nx 合成底图逐点比对）：
分隔线 y110、页签带 y91..109（x241+43k）、BtBuy/BtExit/BtSell 药丸中心
逐字节一致（BtSell 空分类 disabled (204,204,204)、USE 页 enabled
(255,187,0)）、行网格 115/157/199/242 与槽线/中线/底边、金币硬币与
右对齐金额（右缘 ~451）全部命中。
