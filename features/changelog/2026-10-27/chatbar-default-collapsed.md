# 状态栏聊天栏默认收起（黑色日志盒穿透窗口底部的根因修正）

## 症状

仓库、任务日志等窗口的底部下方常驻一块"没有意义的黑色对话框"。此前
一版修复只对 pre-Chaos 状态栏生效，实际视觉无变化。

## 根因

shipped `assets/UI.nx` 含 `StatusBar2.img` → `statusbar::prechaos()` 为
false → 线上客户端运行的是 **post-Chaos** 状态栏，而 post-Chaos 聊天
面板默认 `chatopen = true`：`chatspace/chatbox`（502 宽 BLACK@0.6 +
聊天行 + chatenter/tapbartop/chattargets/chatcover）整块绘制在地图与
窗口之间（`UIChatBar.cpp` draw post-Chaos 分支）。pre-Chaos 分支的修复
在此部署上是死代码。

铁证：修复前后底部探针截图逐像素相同（对照区域亮度不变，面板区域
仍为暗带 + 亮文本行）。

## 修复（src/client/IO/UITypes/UIChatBar.cpp）

1. ctor 无条件 `chatopen = false`：两代状态栏都默认收起。pre-Chaos 无
   关闭按钮（一旦展开永远收不回），post-Chaos 保留 BT_OPENCHAT/
   BT_CLOSECHAT 手动开关。
2. Enter 发送（或空回车）后 `set_chat_open(false)` 扩展到两代：否则
   post-Chaos 下一次聊天就把黑盒重新带回窗口底部；下次 Enter 经
   `focus_chatfield()` 重新展开。

## 验证

- 底部像素探针（`/tmp/ui-bottom-probe.mjs`，test2 仓库/任务窗口）：
  面板区域均值 75→116、69→110（对齐地图参照亮度 97/105），修复前
  y391 的亮聊天文本行消失；右侧对照区逐像素不变。
- `cash-shop-take-reject-retry.mjs` 阶段 A：进图即收起（无聚焦输入框）、
  点击 chatOpen 按钮展开、发送后自动收起并释放 IME。
