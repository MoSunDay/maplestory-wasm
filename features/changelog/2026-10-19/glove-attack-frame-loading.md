# 腕甲攻击动画帧加载修复

## 现象

佩戴腕甲（手套）后，攻击动作期间手部贴图错位或消失；普通攻击（类型 1 选 swingO1、
类型 5 选 swingO2/swingT2 等）尤其明显。

## 根因

- `Clothing.cpp` 用 `stancenode[frame]` 按整数下标探测帧。`nl::node` 的整数下标是
  按名字（字符串化后）查子节点，不是按序号取第 N 个孩子。
- v83 手套数据的 `swingO1`、`swingT2` 帧名从 `"1"` 开始（`"1"`、`"2"`，没有 `"0"`），
  首次探测 `stancenode[0]` 落空，整个循环一帧都不加载，手套在这些攻击姿势下消失。
- 锚点分支：`GLOVES` 原先恒用 `get_body_position`，而手套部件由 `map` 父节点名声明
  锚点（如 alert 的 `lGlove` 用 `handMove`），导致帧贴图相对身体锚点错位。

## 修复

- `Clothing.cpp`：帧遍历改为 `for (nl::node framenode : stancenode)` 迭代真实子节点，
  用 `std::strtol` 解析子节点名，非 [0,255] 纯整数名直接跳过，解析值作为 `uint8_t frame`
  进入原循环体（循环体其余逻辑不变）。
- `Clothing.cpp` `GLOVES` 锚点对齐 WEAPON/SHIELD 分支：`parent == "handMove"` 时用
  `get_hand_position`，否则用 `get_body_position`，最后减去 parentpos（手套数据无裸
  `hand` 锚点，navel 等其余锚点走身体位置）。
- `BodyDrawInfo.cpp`：姿势计时/动作重定向同型加固，同样按子节点数值名遍历，
  `body_actions[ststr][frame]` 以解析出的帧号 keyed，attackdelay 按子节点顺序累加，
  语义与原先一致。Face/Body/Hair 数据恒有帧 0，保持不动。

## 审计工具

`tools/verify/glove-layering-nx.cpp` 在 z-map 审计后新增姿势帧审计：

- 每个手套条目下所有姿势的帧级 imgdir 子节点名必须为纯整数；
- `swingO1`/`swingT2` 允许从 1 起（实测部分条目为 0，均不得晚于 1）；依据全量数据
  校准，`swingTF`/`stabTF`/`swingP2`/`swingO3`/`heal`/`rope`/`ladder` 也允许从非 0
  帧起或（爬梯姿势）为空，其余姿势必须含帧 `"0"`；
- 首帧超过 2 视为超出 v83 已知约定；`01082003.img` 的 swingO1 必须从 1 起（回归样本）。

## 验证

- `/tmp/glove-layering-nx Character.nx Base.nx`：236 items、41092 parts、20509 frames 通过。
- `bash scripts/build_wasm.sh` 通过；WebUI e2e 回归通过。
