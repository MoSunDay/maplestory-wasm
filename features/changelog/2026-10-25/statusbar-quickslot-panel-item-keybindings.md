# 底部快捷栏：抬升面板、物品绑定与键名常显

## 背景

post-Chaos 状态栏的快捷栏面板使用 `StatusBar2.img/mainBar/quickSlot/quickSlot`
（145×93，NX origin `(-143,143)`）。按原偏移 `{143,-143}` 绘制时，面板底边
落在 `viewH-60`，而状态栏 21px 快捷图标行顶边在 `viewH-68`，两者实际重叠。
同时快捷栏只支持技能：技能图标会盖住槽位内的键帽，物品栏图标拖入无响应，
用户无法确认每个槽位对应的按键、绑定的物品或冷却状态。

## 变更

### 面板几何

- `StatusBar/StatusBarPostChaos.h` 新增 `CHAOS_SHORTCUT_ICON_TOP = -58`
  （相对状态栏锚点的图标行顶边，取按钮 NORMAL 原点 y=58）和
  `CHAOS_QUICKSLOT_OFFSET_Y = CHAOS_SHORTCUT_ICON_TOP - CHAOS_LAYOUT.dimension.y() - 2`
  （`-58 - 93 - 2 = -153`），`CHAOS_QUICKSLOT_OFFSET` 由 `{143,-143}`
  改为 `{143,-153}`。
- 面板顶边 = `viewH-163`、底边 = `viewH-70`，图标行顶边 = `viewH-68`，
  两者留 2px 间隙；`static_assert` 保证面板底边恰好落在图标行顶边上方 2px
  （不重叠且间隙不会漂移）。
  v83 的 `{143,-143}` 不变。

### 面板贴图原点回归

- `QuickSlotBar.cpp::quickslot::draw` 之前直接 `panel.draw(origin)`，而
  `Texture::draw(DrawArgument(pos))` 以 `pos` 为中心、`Texture::get_rectangle`
  再减去 NX origin，于是 145×93 面板实际落在 `origin - (-143,143)`，
  即向右下偏移 `(143,-143)`、几乎整块移出屏幕：背景消失，而图标/键帽
  走 `draw_slot`（显式加上 `texture.get_origin()`）仍停在预期坐标，
  视觉上快捷栏格子就与快捷图标行 / SYSTEM+MENU 按钮叠在一起。
- `QuickSlotModel.h` 新增纯函数
  `texture_draw_position(top_left, texture_origin) = top_left + texture_origin`，
  `QuickSlotBar.cpp` 改为
  `panel.draw(quickslot::texture_draw_position(origin, panel.get_origin()))`，
  使面板贴图、命中框、图标与键帽共用同一个 top-left；键位/图标坐标与
  命中测试不变。

### 技能与物品共用槽位

- `QuickSlotModel.h::State` 新增 `items[COUNT]`，纯函数
  `item_at` / `assign_item`；`assign` 清物品位、`assign_item` 清技能位、
  `clear` 两者都清，保证一槽最多一个绑定。
- `UIStatusBar::send_icon` 接受 `Icon::get_item_id()`，物品图标可拖入快捷栏；
  物品栏 `ItemIcon` 暴露自身 `item_id`，`UIStateGame::drop_icon` 的路由条件
  改为 `skill_id > 0 || item_id > 0`。
- 左键物品槽调用 `Player::use_item`（USE 消耗品、SETUP 椅子生效，其他类型
  安全空操作），技能槽继续 `Combat::use_move`；悬停物品显示物品 tooltip，
  右键在技能或物品任一存在时清空槽位。
- `QuickSlotBar::draw` 对物品槽绘制 `ItemData::get(item_id).get_icon(false)`；
  技能冷却黑罩（0.55 不透明度）与剩余秒数保持原逻辑。

### 拖放输入落点

- 浏览器 `mousedown` / `mouseup` 可能早于 GLFW cursor callback；此前按键分发
  使用 UI 中上一帧的光标位置，拖拽物品时会从错误的物品格起拖（实测点击
  slot1 却拾到 slot7 的 2000000）。`Window.cpp::mousekey_callback` 现在在
  左键按下/释放与右键按下前调用 `glfwGetCursorPos()`，经
  `UI::set_cursor_position()` 同步后再分发，保证物品/技能拖放的起点与
  释放点都取自当前鼠标事件。

### 键名常显

- `QuickSlotModel.h::Layout` 新增 `key_label_y`（v83 `{0,0}`、post-Chaos
  `{8,86}`）和 `labels_outside_slots`（v83 `false`、post-Chaos `true`）。
- post-Chaos 面板在 16px 顶带/14px 底带每槽恒画
  `StatusBar.img/key/<i>` 原生键帽，绑定技能或物品后键名仍可见；
  v83 保持只在空槽画键帽。键帽为客户端原生默认键位标签
  （Shift/Ins/Hm/Rup/Ctrl/Del/End/Pin）。

### 验收探针

- `Gameplay/Probes/QuickslotProbes.cpp`（从 `GameplayProbes.cpp` 拆出）
  新增 `msquickslot_item`、`msquickslot_center_x/y`、
  `msquickslot_panel_left/top/right/bottom`；冷却与悬停探针一并迁入。
  `msquickslot_panel_*` 读取 `Texture::draw_bounds` 在
  `quickslot::draw` 里同一次面板绘制捕获到的 destination（上一帧实际
  绘制矩形），不再是另算的命中框，因此探针断言与渲染路径强绑定。
- `UIStatusbar::get_control_bounds(uint16_t id)` 暴露按钮真实矩形
  （找不到时返回零尺寸 `Rectangle<int16_t>()`）；
  `GameplayProbes.cpp` 新增 `msstatusbar_control_bounds(id, edge)`
  （edge：0=left、1=top、2=right、3=bottom，缺失返回 `MISSING_COORDINATE`）；
  `id` 超出 `0..0xFFFF`（如 65540）直接返回 `MISSING_COORDINATE`，
  不会按 `uint16_t` 回绕成真实按钮。
- `CMakeLists.txt` 的 `-sEXPORTED_FUNCTIONS` 同步导出全部新探针；
  `tools/verify/maplestory-webui/runtime-flow.mjs` 增
  `quickslotItem`、`quickslotCenterX/Y`、`quickslotPanel`、
  `statusbarControlBounds` 封装。

## 验证

- 纯模型单测：`tools/verify/quickslot-model.cpp` PASS，覆盖
  `assign_item`/`item_at`/技能物品互斥/`clear`、键帽带与
  `labels_outside_slots` 断言，以及
  `texture_draw_position(top_left, (-143,143)) - (-143,143) == top_left`
  （旧写法 `draw(top_left)` 会整体偏移）。
- 浏览器 E2E：最终构建在 `QS_PRESET=0`（800×600）与
  `QS_PRESET=1`（1280×720）两档均通过 `verifyQuickslotLayout`（布局段用
  `QS_SKIP_SKILLS=1 QS_SKIP_COOLDOWN=1 QS_SKIP_ITEMS=1` 独立运行）。
  `quickslot-flow.mjs` 还覆盖 slot0 技能点击绑定、slot7 拖拽绑定、
  Hero's Will 1121011 服务端冷却、从 USE 物品栏拖入默认 2000005 至
  slot1、点击消耗与右键清空；本轮构建的 800×600 技能段与冷却段分别
  通过（冷却是 60s、标签长度 2），物品段沿用上一提交已通过的同一流程
  （SwiftShader 在单进程连续打开多次技能书/物品栏时偶发挂起，分段运行
  可避开该环境问题）。`verifyQuickslotLayout` 断言：
  - 面板 bounds 是**实际绘制矩形**（`Texture::draw_bounds`，非命中框）：
    800 宽 `{655,437,800,530}`，1280 宽 `{1135,557,1280,650}`；
  - 8 个槽位中心分别落在 `panel + {23,56,89,122} × {31,64}`；
  - 经 `msstatusbar_control_bounds` 读取 **UIStatusbar::Buttons 全部 12 个**
    真实按钮矩形（BtChat/BtClaim/BtCashShop/BtMTS/BtMenu/BtSystem 与
    BtCharacter..BtSkill，不使用硬编码期望值）：面板不与任一方形重叠，
    快捷图标行也不与 BtSystem/BtMenu 重叠；
  - 越界控制 id 65540（未校验会回绕成 BtMenu）四个边界都必须返回
    `MISSING_COORDINATE`，覆盖探针范围校验；
  - 图标行顶边实测 `viewH-68` 与 `earliestShortcutTop - panel.bottom == 2`
    均为**硬断言**（偏差 1px 即失败，不再是 INFO），且 BtSystem/BtMenu
    顶边严格低于所有快捷图标底边（不共用底边）。
- 同一 E2E 在两档分辨率下还覆盖状态栏整体 chrome；冷却 60s 标签在
  800×600 的独立冷却段验证。
- WASM 重建成功（`scripts/build_wasm.sh`，产物
  `build/JourneyClient.wasm` / `JourneyClient.js`；`.js` 已确认导出新增探针）。

## 回退

面板位置只受 `CHAOS_QUICKSLOT_OFFSET_Y` 控制；如需恢复旧视觉可将该常量
改回 `-143` 并移除对应 `static_assert`。物品绑定与键帽常显分别可通过
`QuickSlotModel.h::State::items` 和 `Layout::labels_outside_slots` 独立回退，
不影响 v83 路径。
