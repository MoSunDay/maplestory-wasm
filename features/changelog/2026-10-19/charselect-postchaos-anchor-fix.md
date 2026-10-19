# 选角界面 post-Chaos 锚点修正：世界横幅与信息板归位

## Context

1. 上游 LibreMaple（c55bcf6）的 post-BB 选角锚点为
   `selworldpos (578,42)`、`charinfopos (662,355)`，但代码把 v83 的
   pre-BB 值 `(578,112)/(662,305)` 用在了 post-Chaos 分支，导致右上角
   整列下移约 50px：
   - `Common/selectWorld` 看板（190×89、origin (11,11)、不透明行
     12-76）在 (578,112) 下占 y 113-188，压进 NX 自带定位的
     `charSlot` 数量窗（pos (589,106)、162×38），两者重叠错位；
   - `charInfo` 信息板（194×271、origin (97,135)）压到翻页箭头与
     名牌区域，BtSelect 也坐不到板底。
2. `character/0` 圆盘底座（62×8、origin (29,7)）画在脚锚点时盘体
   覆盖 y..y+8，`NAMETAG_OFFSET{0,9}` 让 86×16 名牌（origin (36,8)）
   上半截埋进盘体。

## Change Summary

- `src/client/IO/UITypes/UICharSelect.cpp`（post-Chaos 分支）：
  - `selworldpos (578,112) → (578,42)`：看板内容止于 y≈107，恰好
    落在 charSlot 数量窗上缘，恢复 NX 权威布局；
  - `charinfopos (662,305) → (662,355)`：信息板不透明区回到
    (579,229)-(746,475)，BtSelect 落在板底按钮区；
  - pre-Chaos 回退分支 ((625,305)/(578,112)) 不变。
- `src/client/IO/UITypes/UICharSelectLayout.h`：
  `NAMETAG_OFFSET {0,9} → {0,19}`，名牌 y+11..y+27，与盘体
   （y..y+8）留 3px 间隙。
- `tools/verify/maplestory-webui/layout-pixels.mjs`：新增
  bannerBoard/gapStrip/countWindow 三个采样矩形与两条亮度断言
  （banner-gap ≥60、countWindow-gap ≥60），把"横幅必须止于数量窗
  之上"锁进 e2e——pre-fix 锚点下 bannerBoard 实测 32.2（横幅压进
  窗口带），post-fix 155.0/gap 52.2/窗 154.7。

## 排查备注

- 修复后数量窗与信息板之间（x≈652-716、y≈146-242）出现一处
  "文字状"图形，经 NCC 模板匹配（0.9）确认是 `Map.nx
  Back/login.img/back/11` 背景自带的路牌/植被装饰（位图
  (892,196) 附近，origin (600,373) 反算映射到该区域），此前被
  错位看板遮住，非 UI 元素，无需处理。
- 等级/名称表头验证：等级数字画于 (685,262)、`l` 徽标与 A18M
  名称 (662,270)，均落在信息板顶部区（板内位图 y 9-122），正确。

## 验证

- `scripts/docker_build_wasm.sh` 增量重建通过；
- e2e（`E2E_STOP_AT_CHAR_SELECT=1`）全绿，含新增断言：
  横幅 y55-115、数量窗 y125-145 互不重叠、板顶 ≈y240、
  nametag0 125.7 vs bgL 87.68。
