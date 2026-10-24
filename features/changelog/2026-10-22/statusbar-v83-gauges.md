# 状态栏 v83 HP/MP/EXP 量条修复

## 背景

v83 `StatusBar.img/gauge` 用“满量条 + 黑色遮盖”表达进度：`gauge/bar`
是一张自带 HP/MP/EXP 标签与 100% 渐变填充的 340×31 位图，未填充的
部分用黑色半透明 cover 压暗成空槽观感，`gauge/graduation` 再叠加
边框、刻度与暗色底。修复前三处叠加错误让量条全部显示为满：

1. **cover 高度未设**：`gauge_covers` 初始为 1×1 黑盒，每帧只调
   `setwidth(empty)`；几何盒不刷新高度就保持 1px 高，遮盖实际不覆盖
   任何填充区，三条量条视觉上永远是满的。
2. **旧内框坐标不准**：`gauge_fill` 沿用从 `gauge/graduation` 边框
   推出的 HP `{3,103,15,29}`、MP `{110,208,15,29}`、EXP
   `{214,338,15,29}`——HP/MP 宽度偏窄，EXP 把槽间分隔区算进填充区，
   且三条内框顶部都比真实填充区高 1px，导致填充比例与位图不对齐。
3. **数字字符集为空**：`statset`/`levelset` 取
   `StatusBar.img/gauge/number`，但该节点在 v83 资源里**不存在**，
   charset 构造后没有任何字形，HP/MP/EXP 数值完全不渲染。

## 变更

- `src/client/IO/UITypes/StatusBar/StatusBarPreChaos.h`：`gauge_fill`
  改为按 `gauge/bar` 位图像素实测的可填充内区，新内框为
  HP `{3,106,16,29}`、MP `{111,214,16,29}`、EXP `{224,337,16,29}`。
- `src/client/IO/UITypes/UIStatusBar.cpp`（`draw_prechaos`）：
  `draw_cover` 在 `setwidth(empty)` 之外补上
  `setheight(fill.height())`，黑 cover 恢复为全高遮盖，未填充段重新
  显示为空槽观感；cover 高度取自新内框，与渐变填充区逐像素对齐。
- 数字字符集改用根节点 `StatusBar.img/number`（v83 把
  数字/括号/斜杠/百分号字形放在根上，`gauge/number` 不存在）：
  `statset = { statusbar["number"], Charset::RIGHT }`、
  `levelset = { statusbar["number"], Charset::LEFT }`。
- 动态数值组串按 v83 字形的**节点名映射**（`Charset` 取子节点名的
  首字符作字形键）：HP/MP 为 `L<当前>s<上限>R`，EXP 为
  `<当前>L<百分比>pR`——`L`/`R` 是左右括号字形，`s` 是斜杠，`p` 是
  百分号；post-Chaos 分支沿用 `[..%]` 的 ASCII 组串不变。

本轮快捷栏技能绑定/冷却与状态栏外观回退都不触碰上述量条逻辑：背景
恢复为单张 `StatusBar.img/base/backgrnd` 后，量条仍使用按 `gauge/bar`
像素实测的内框、全高黑 cover 与根节点数字字形；快捷栏的冷却遮罩只画
在技能图标槽位内，不会覆盖 HP/MP/EXP 量条。

## 验证

- `./scripts/build_wasm.sh`（Release）编译通过，量条修复随 WASM
  客户端一起构建。
- 浏览器 E2E `tools/verify/maplestory-webui/quickslot-flow.mjs`
  （`QS_PRESET=0|1`，800x600 与 1280x720 两档）：登录 test1Hero
  （id 215，level 200）后 HUD 正常呈现，快捷栏绑定/使用全流程不受
  施法与 LazyFS 阻塞影响。
- 1280x720 截图像素观察：HP 29681/30000（约 99%）红色渐变铺满整条
  内宽；MP 5289/12000（约 44%）蓝色填充在约 45/103 内宽处截止、
  右侧为压暗空槽；EXP 在 level 200 为 0%，整条空槽；绿色括号与
  白色数字字形像素可见。800x600 截图同样通过。
- 开发期临时驱动只用于像素定位，不作为长期验证入口；回归一律以
  上述 Release 构建、两档 `quickslot-flow.mjs` 与截图像素观察为准。
