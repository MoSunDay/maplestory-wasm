# 登录链界面贴合 2023 版素材：Nova 顶部槽位 + 选角单行布局

## Context

1. 世界选择界面（post-Chaos）Nova 世界按钮锚点硬编码 `(650, 20)`，
   而 `BtWorld/release/button:15` 位图自带 origin `(-14,-311)`（烘焙的
   靠下槽位），实际落在右侧栏中部偏下。只展示一个世界时应放到顶部槽位。
2. 选角界面沿用 v83 的 4×2 双行网格与旧姿势：新版素材实为单行 4 个
   小圆盘底座（`character/0` 62×8、origin (29,7)）+ `buyCharacter`
   购位卡；WALK1 选中姿势身体前倾、人物脚底陷进圆盘 7px、名牌与底座
   重叠、空槽无底座、翻页箭头不对称、点击热区仍按双行布局。
3. 四个登录链界面 `back/11`（1280×720、origin (600,373)）沿用 v83
   锚点 (370,300)，背景整体偏移约 (10,13)px。

## Change Summary

- `src/client/IO/UITypes/UIWorldSelect.cpp`：
  - 新增 `POSTCH_SIDEBAR_POS{650,45}`（`layer:bg` 左上角）与
    `POSTCH_SIDEBAR_TOP_SLOT{14,14}`（与 `button:10` origin (-14,-14)
    一致的顶部槽位）；Nova 按钮锚点改为
    `POSTCH_SIDEBAR_POS + POSTCH_SIDEBAR_TOP_SLOT + nx_origin`，
    位图左上角落在 (664,59)，不再使用魔法数 (650,20)。
  - `back/11` 锚点 (370,300) → (360,313)，1280×720 背景在 800×600
    窗口内精确居中（左上 (-240,-60)）。
- `src/client/IO/UITypes/UICharSelectLayout.h`（新增，纯函数）：
  选角布局常量与几何计算集中于此——`PAGESIZE=4`（单行）、槽位
  `char_slot_pos(i)={130+120i,270}`、点击热区、翻页箭头位置
  (46,240)/(533,240)（避开购位卡右缘 ~531 与信息板不透明左缘 ~573）、
  `PLATFORM_OFFSET{0,7}`（盘面贴脚）、`NAMETAG_OFFSET{0,9}`（名牌
    落在盘下方）、信息板标签 `label_pos`。
- `src/client/IO/UITypes/UICharSelect.cpp`（818 → 781 行，回到 800 行
  限制内）：
  - `PAGESIZE` 8 → 4，`get_char_pos`/`get_label_pos` 迁入布局头文件；
  - 占用槽：底座画在 `charpos+(0,7)`、人物踩盘、名牌 `charpos+(0,9)`；
  - 空槽：post-Chaos 补画底座 + `buyCharacter` 购位卡；
  - `AreaButton` 热区改单行 60×120；
  - `update_counts` 中创建按钮使能条件改为账户级
    `charcount_absolute < slots_absolute`：原按页比较在 PAGESIZE=4
    时，单页摆满（如 4/12）会错误禁用创建入口；
  - `back/11` 锚点同步 (360,313)。
- `src/client/IO/UITypes/UICharSelect.h`：`PAGESIZE` 指向布局头文件，
  移除已迁移成员声明。
- `src/client/IO/CharacterSelect/UICharSelectPreview.cpp`：选中角色
  姿势 WALK1 → STAND1，消除前倾错位。
- `src/client/IO/UITypes/UILogin.cpp`：`back/11` 锚点同步 (360,313)。

## Verification

- `scripts/docker_build_wasm.sh`（emscripten -Werror）通过。
- `tools/verify/maplestory-webui/run.mjs`（E2E_STOP_AT_CHAR_SELECT=1）
  截图像素核对：Nova 位于右侧栏顶部槽位（位图带 (664,59)-(760,85)）；
  人物单行踩盘、名牌在盘下、空槽为底座+购位卡、金色右翻页箭头
  (533,572) 与购位卡和信息板均无重叠。
- 全流程 e2e（登录 → 选世界 → 选角 → 建角 → 双击进图 → IME 聊天
  → 帧预算）全部 PASS。
- 本地探针：character-preview-policy、login-parser、
  character-creation-flow 均通过。

## 后续修正

- 前 Chaos 登录背景 `back/0`（20x600，origin (10,300)）锚点 (370,300) →
  (400,300)：位图左上角 = 锚点 - origin = (390,0) = ((800-20)/2, (600-600)/2)，
  即 800x600 虚拟屏幕的精确居中；原 (370,300) 对应左上角 (360,0)，偏左 30px。
- 同步应用于登录链三个界面：UILogin、UIWorldSelect、UICharSelect；
  `back/11` 锚点 (360,313) 保持不变。
- 随 2023 版 Map.nx（已含 back/11）该回退分支不可达，e2e 无法渲染，
  以上述算术推导作为验证依据。
