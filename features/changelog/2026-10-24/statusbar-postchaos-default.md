# 底部状态栏回归 post-Chaos 新版（StatusBar2.img）

## 背景

README 资产配方为「v83 全套 WZ，唯独 UI.wz 取 v153+」。2026-10-18
已将 `assets/UI.nx` 换回高版本组合包（登录链、选角与四件套游戏内窗口
均呈现新版），唯独底部状态栏仍渲染 v83 旧版 `StatusBar.img`：组合包
内两代状态栏节点并存，而 `statusbar::prechaos()` 的探测标记
（`StatusBar.img/base/quickSlot` 存在即判旧版）在高版本包里恒为真，
且此后三笔提交（fe5d6a5 / 0ec4fbf / 772677a）持续在旧版路径上迭代
（量条修复、HUD 重锚定、快捷栏技能绑定），把状态栏进一步锁死在旧版。

## 变更

- **世代探测翻转**（`StatusBarPreChaos.h`）：`prechaos()` 改为
  「`StatusBar2.img/mainBar/backgrnd` 缺失才判旧版」——两代并存时
  优先 post-Chaos，纯 v83 UI.nx 仍回退旧版，与登录框探测
  （`UiAssetVersion.h`）口径一致。
- **新增 post-Chaos 几何词汇**（`StatusBarPostChaos.h`）：元素锚点
  `{viewwidth-288, viewheight-VIEWYOFFSET}`（288 = quickSlot 位图
  origin 偏移 143 + 面板宽 145；800 宽视图下等于原 `POSITION.x=512`，
  右侧控制簇随视图右缘停靠）；快捷栏面板偏移 `{143,-143}`、两代共用的
  `quickslot_layout()` / `quickslot_panel_pos()` / `quickslot_dock_range()`
  分发函数。
- **快捷栏模型参数化**（`QuickSlotModel.h`）：命中带/槽位中心/冷却遮罩
  尺寸收进 `Layout` 值——`V83_LAYOUT`（151×80）与 `CHAOS_LAYOUT`
  （145×93，按位图实测：16px 头带 + 两行 30×30 槽 + 14px 底带，列心
  x {23,56,89,122}、行心 y {31,64}）；`index_at`/新增 `key_center`
  均接收布局参数，纯函数模型不依赖具体世代。
- **UIStatusBar 解除世代门控**：绑定/悬停/点击使用/拖拽/右键清空/
  冷却倒计时/tooltip 全链路在两代通用；`initialize_postchaos` 加载
  `mainBar/quickSlot/quickSlot` 面板与保留的 v83 `StatusBar.img/key/0..7`
  键帽精灵；绘制拆出共享 `draw_quickslot_dock()`。
- **量条数字分隔符**：post-Chaos `gauge/number` 字符集的分隔字形是
  `\` 节点（无 `/`），HP/MP 标签组串改用 `\`，避免分隔符缺字。
- **MapButton `ani` 回退**：mainBar 图标行六键（BtCharacter/Stat/Quest/
  Inven/Equip/Skill）只有 `ani` 动画节点、无 normal/pressed 状态，
  `find_state` 增加 `ani` 首帧回退，按钮保持可见可命中。
- **E2E 同步**：`quickslot-flow.mjs` 坐标改为新面板几何（两档分辨率），
  `quickslot-model.cpp` 双布局断言。

## 验证

- `bash scripts/docker_build_wasm.sh -j 8` 通过（0 warning，-Werror）。
- 纯模型：`quickslot-model`（两代布局命中带/中心/assign/clear）PASS。
- 浏览器 E2E `quickslot-flow.mjs`：QS_PRESET=0（800×600）与
  QS_PRESET=1（1280×720）全链路 PASS（绑定/悬停/使用/拖拽/右键清空/
  Hero's Will 冷却倒计时）；截图像素统计证实 1366×85 新版条带与
  145×93 新版快捷栏面板实际渲染（条带区 1443 色、面板区 268 色）。
- 冒烟：聊天输入/发送/服务端回环（含 IME 中文 E2E `run.mjs
  E2E_IME_CHAT=1`）、BtMenu/BtSystem 弹窗打开与关闭、按钮探针
  （last_button 4/5）均通过。

## 迭代补记（2026-10-25）

首轮合入后用户反馈两处问题，本轮修复：

- **底条全宽覆盖**：原实现把 `mainBar` 各贴图当作元素 sprites 一次性画在
  右锚 `position` 上，backgrnd（1366×85）左缘落在 `viewW-800`，1280 宽
  视图下左侧漏出地图。现拆为独立 chrome 贴图：backgrnd 视宽 ≤1366 时
  贴左缘裁右、>1366 时以 `DrawArgument(left, {viewW, 85})` 拉伸；等级块/
  量条框架/名字职业标签等正 origin 左簇改锚定固定左锚
  `postchaos_left_anchor()`（`{512, viewH-VIEWYOFFSET}`，即 800 宽校准，
  两锚在 800 视图重合、零回归）；`dimension` 与 `is_in_range` 改为全宽
  条带 `[0, viewW] × [viewH-95, viewH-10]`；聊天行（chatbar + BtChat/
  BtClaim 按钮）随左簇布局。post-Chaos 绘制拆出
  `StatusBar/UIStatusBarPostChaosDraw.cpp`（UIStatusBar.cpp 保持 ≤800 行）。
- **HP/MP 分隔符**：`gauge/number` charset 的分隔字形节点名为 `\`，
  `Charset` 构造时把该节点重映射为 `/` 查找键（字形本身是正斜杠），
  组串应用 `/` 而非 `\\`——此前用 `\\` 查不到字形导致分隔符不渲染。
- **E2E**（`quickslot-flow.mjs`，两档 preset 均过）：新增
  `msstatusbar_stat_label` 探针断言 `[cur/max]` 格式；条带四点 + 上下
  边界 `msstatusbar_hit` 位断言全宽输入区；左段 vs 上方地图行亮度差
  ≥35 的像素佐证（`layout-pixels.mjs` 采样器支持传入实际视图尺寸，
  登录链默认 800×600 调用方不受影响）。

## 回退

如需临时回旧版：仅改 `StatusBarPreChaos.h::prechaos()` 的探测标记为
「`StatusBar2.img/mainBar/backgrnd` 存在才判新版」即可，两代路径均
完整保留。
