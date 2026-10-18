# Ariant NPC 商店端到端 + 一键售卖服务端补齐

## Context

- v1 `E2E_NPC_SHOP` 依赖训练营地附近商店 NPC，实况核查全服仅 5 个
  开店脚本，环境敏感：走位后商店不弹，e2e 形同抽奖。
- v2 重写后卡在“卖出数量框不弹”：像素比对证明 row 点击后只有光标
  精灵变化、无选中高亮——资产流高负载下 CDP 点击整击丢失，且
  `SellState::sell()` 在 `selection < 0` 时静默返回，无任何可观测
  状态，无法区分“没点中”与“点了没反应”。
- 客户端 bulk-sale 特性（commit a45d3a8）发送 NPC_SHOP_ACTION
  mode=4（整 Tab 售卖），但 linked server `NPCShopHandler` 只实现
  mode 0–3，包石沉大海。

## Change Summary

- 客户端探针（`src/client/IO/UITypes/UIShop.h`、
  `src/client/IO/UITypes/UIShop.cpp`、
  `src/client/Gameplay/Probes/GameplayProbes.cpp` +
  `src/client/CMakeLists.txt` 导出）：
  - `msshop_sell_item(row)`、`msshop_sell_x/_y(row)`（卖出行中心）；
  - `msshop_selected`（选中槽位）；
  - `msshop_button_x/_y(id)`：BUY_ITEM=0/SELL_ITEM=1/EXIT=2/
    SELL_ALL=23 按钮矩形中心，含 MapleButton origin 锚定换算。
  e2e 从“猜 origin”改为直读几何。
- `tools/verify/maplestory-webui/npc-shop-flow.mjs` v2（Ariant
  260000000 NPC 2100002）：全部状态变迁点击 `retryClick` + 真实
  状态谓词；卖 1 件走 UIEnterNumber 默认数量 1 回车确认（数字框
  未注册为 UI 焦点文本框、键盘无法改值，属上游移植限制）；
  SELL_ALL UIYesNo 回车确认；Escape 关店断言。
- 服务端（linked `/data/MapleStory-Server`，非 git 管理，
  `posix-compile.sh` 编译 + 容器重启生效）：
  - `NPCShopHandler.java` 增加 mode 4 分支；
  - `MapleShop.java` 新增 `sellAll(c, type)`：EQUIP/USE/SETUP/ETC
    快照遍历、复用 `canSell`/`getSellingQuantity`、逐件
    `removeFromSlot` + `gainMeso`，末尾单次 `shopTransaction`
    0x8/0x5。
- `tools/verify/maplestory-webui/run.mjs`：`E2E_NPC_SHOP=1` 时地图
  素材门放宽到 300s（Ariant 冷块远大于默认图）。

## Notes

- SellState 图标偏移终判（维持上游）：卖出行图标列与右侧文本/价格
  列的错位为上游 UIShop 布局行为（`SellState::SellItem::draw` 的
  `pos + (0,32)`/`(40,-1)` 等偏移），不做本地修正；证据截图
  `21-npc-shop-sell-tab.png`（`/tmp/maplestory-webui-*/` 产物目录，
  运行 `E2E_NPC_SHOP=1` 可复现）。

## Verification

- /tmp/npc12.log 全绿：37 药 → 卖 1 → 36 → sell-all → 0，行清空，
  Escape 关店。
- 服务端日志 `NPC_SHOP`×3（卖 1、sell-all、LEAVE）且各回
  `CONFIRM_SHOP_TRANSACTION`；角色 meso 0→950。
- `E2E_CREATE_CHARACTER` 0/1 回归全绿（/tmp/run7.log、
  /tmp/run9.log）。
- 预备 SQL（char5 迁 Ariant + 37 个 2000000 药水，注意
  `inventoryitems.type` NOT NULL 列）见 `npc-shop-flow.mjs` 头部
  注释。
