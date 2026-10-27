# NPC 对话框按钮几何只读探针

## 背景

FM PQ 单排验收（2026-10-26 `fm-pq-teleport-and-solo-verification.md`）曾用
视觉/网格坐标推断 `UINpcTalk` 选项按钮位置，但 v83 客户端与 post-Chaos
519 版 `UI.nx` 的按钮几何整体偏移约 88px，右下角 HUD 还会制造亮度
假阳性；harness 侧（`pq-harness-dialog.mjs`，随 56ffebc 提交）已改为
"客户端探针优先、视觉/网格仅兜底"，本次补齐其依赖的客户端导出。

## 变更

- `src/client/IO/Components/Button.h`：新增公有只读 `get_position()`
  （原 `position` 为 protected，探针需要相对父元素的真实坐标）。
- `src/client/IO/UITypes/UINpcTalk.h/.cpp`：新增 `probe_button(which, field)`
  与 `probe_element(field)` 两个 const 只读探针。
  - `which`：0=YES 1=NO 2=END 3=OK 4=NEXT 5=PREV，经
    `PROBE_BUTTON_IDS = {4,5,3,0,1,2}` 映射到类内私有 Buttons 枚举键。
  - 按钮 `field`：0=x 1=y 2=width 3=height 4=active。
  - 元素 `field`：0=pos.x 1=pos.y 2=dim.x 3=dim.y 4=dialogue_mode
    5=post_chaos 根（供 harness 区分 v83/post-Chaos 皮肤）。
  - 错误码：-9998 非法 which、-9997 按钮不存在、-9996 非法 field。
- `src/client/GameplayProbes.cpp`：导出
  `msnpctalk_button_field(which, field)` 与 `msnpctalk_element_field(field)`，
  元素未激活时统一返回 -9999。

## 验证

- FM 13 单排 PQ e2e 13/13（`fm-pq-solo-flow.mjs`，探针已编译进当时
  部署的 WASM，harness 以探针坐标点击对话按钮全部命中）。
- 部署冒烟：`run.mjs` 登录→世界选择→角色选择全 PASS、浏览器异常 0，
  探针导出随当前构建分发。
