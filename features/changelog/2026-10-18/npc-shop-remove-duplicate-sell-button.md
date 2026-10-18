# NPC 商店移除重复的卖出按钮

## 问题

post-Chaos 布局下，窗口按钮条出现两颗同贴图（`BtSell`）的"卖出"按钮：
底部的 `SELL_ITEM`（原生槽位）与上浮 20px 的 `SELL_ALL`（`{0,-20}` 偏移）。
用户视角即"卖出所有物品的按钮重复了"——点击底部那颗时，`SellState::sell()`
在未选中行的情况下静默返回（`selection < 0`），表现为"按了没用"。

## 修复

| 文件 | 变更 |
|------|------|
| `src/client/IO/UITypes/UIShop.cpp` | 删除 `buttons[SELL_ITEM]` 创建与 `case SELL_ITEM` 处理；按钮条只剩 `BUY / 卖出(SELL_ALL) / EXIT` |
| `src/client/IO/UITypes/UIShopNodes.h` | `sell_all_button_pos()` 落位 BtSell 原生槽位（post-Chaos origin / v83 原 sell 列 `{82,62}`）；删除死代码 `sell_button_pos()` |
| `src/client/IO/UITypes/UIShop.h` | `SELL_ITEM = 1` 保留枚举值（维持 `SELL_ALL = 23` 探针契约），标注 reserved |
| `src/client/Gameplay/Probes/GameplayProbes.cpp` | `msshop_button_x/y` id 注释更新 |
| `tools/verify/maplestory-webui/npc-shop-flow.mjs` | 单件卖出改走行双击（`SellState::select` 同槽二连击 = `sell()`，原有内建行为） |

单件售卖能力不丢失：卖侧行双击即弹数量框（上游行为）。

## 验证

```bash
bash scripts/docker_build_wasm.sh
CHROME_BIN=/usr/bin/chromium-browser E2E_NPC_SHOP=1 E2E_CREATE_CHARACTER=0 \
E2E_URL=http://127.0.0.1:8000/web/index.html \
node tools/verify/maplestory-webui/run.mjs
```

全绿：`sell quantity notice opened (row double-click)` → `sold 1 potion` →
`sell-all confirmation opened`（唯一卖出按钮）→ `sell-all confirmed (stack emptied)` →
`NPC shop window closed`；完整 WebUI 流程（登录/世界/角色选择/进图/中文聊天）同步通过。
