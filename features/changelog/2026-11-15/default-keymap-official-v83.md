# 默认键位对齐官方 v83：修正表情键整体右移与背包/属性等错绑

## 背景（Context）

- 9bfbf218 起，`Keyboard` 构造时预置 `DefaultKeymap.h` 的默认键位（此前
  该表只驱动键位设置窗口的“默认”按钮）。该表抄自 post-Chaos 客户端布局，
  与服务器（`link_repos/MapleStory-Server` 的 `GameConstants`
  DEFAULT_KEY/TYPE/ACTION，即角色创建时写入 DB 的官方 v83 键表）不一致：
  keymap 记录缺失/不全的角色（SQL 直建测试号、只跑过 skill-keymap-prep 的
  号）在运行时与“默认”按钮下都会得到错位的键位。
- 具体错位（客户端旧值 → 官方 v83）：
  - **F4 空绑导致表情整体右移一位**：F5/F6/F7/F8 → 表情4/5/6/7；官方是
    F1..F7 → 表情1..7。
  - **S 缺失、C 错绑**：C → CHARSTATS（官方 C → MESSAGE，CHARSTATS 在 S）。
  - **B 错绑**：PROFESSION（大乱改后动作，v83 无效）→ 官方 MONSTERBOOK。
  - **T/Y/V 错加**：BOSS/ITEMPOT/EVENT 均为大乱改后动作，官方 v83 无绑定。
  - **F/O/数字6 缺失**：官方为 FAMILY/PARTYSEARCH/CHATSPOUSE。

## 改动（Change Summary）

- `src/client/IO/DefaultKeymap.h`：`default_basic_mappings()` 重写为与
  服务器 DEFAULT_KEY/TYPE/ACTION **40 条逐条一致**的官方 v83 键表
  （已用脚本比对：除有意保留的右 Ctrl/右 Alt 便利绑定外零差异）。
  ESC 不再预绑 MAINMENU（官方无此行，ESC 由 `UI::send_key` 的 escape
  路径原生处理，行为不变）。
- 键位设置窗口“默认”按钮与运行时预置继续共用同一张表
  （`Keyboard.cpp` / `UIKeyConfig.cpp` 无需改动）。
- 右 Ctrl/右 Alt → 攻击/跳跃为客户端便利绑定，非服务器默认行，保留
  （9bfbf218 的“CTRL 攻击不依赖服务器 keymap”目标不受影响）。

## 影响与恢复（Impact）

- DB keymap 记录完整的角色不受影响（服务器包逐条覆盖预置值）。
- 此前若在键位设置窗口按过“默认→确定”把错表写进 DB 的角色，重新打开
  键位设置 → 默认 → 确定 即可写回官方键位。
- 增量重编 `Keyboard.cpp`、`UIKeyConfig.cpp` 并链接通过
  （`scripts/docker_build_wasm.sh -j16`）。
