# 终极攻击缺失变体补齐 + 全量 2 段 8 只 + 魂骑士 1 转触发表

## 背景

`assets/Skill.nx`（079MAX4 数据集）里存在三处终极攻击（Final Attack，
FA）数据缺口，客户端与服务端都会受影响：

1. **缺 3 个 FA 变体**：`110.img/1100003`（终极斧）、`120.img/1200002`
   （终极剑·钝器向）、`130.img/1300003`（终极矛）整棵子树不存在，但
   `1001005`（群体攻击）的 `finalAttack` 触发表仍点名它们，
   技能窗口/追加攻击查表会落到空节点；
2. **弓箭手 / 魂骑士缺攻击次数**：`3100001`、`3200001`、`11101002`、
   `13101002` 有技能节点，但 30 级里完全没有 `attackCount`/`mobCount`，
   客户端 `SkillData` 与服务端 `MapleStatEffect.getInt` 都默认 1，只打 1 段 1 只；
3. **魂骑士 1 转丢触发表**：`1100.img/11001002`（强力攻击）、
   `11001003`（群体攻击）没有 `finalAttack` 子节点，魂骑士剑客的
   终极剑（`11101002`）永不触发。

`assets/Skill.nx.v83-backup` 仍保有这三块原始数据。

## 变更

### 数据（`assets/Skill.nx`）

- 新增 `tools/nx/inject_final_attack_variants_nx.py`：
  - **graft**：从 v83 归档逐字节克隆 `1100003`/`1200002`/`1300003`
    子树（`icon`/`iconDisabled`/`iconMouseOver` 三张位图 + `hit` +
    `req` + `skillType` + 30 级 level），按字节序插入
    `<img>/skill` 的正确位置（nlnx 用字节序二分查子节点）；
  - **level 继承**：graft 完成后把每一级 `level/<n>` 的子节点整体换成
    同图原生兄弟技能的同一级子节点（`1100003`←`1100002`、
    `1200002`←`1200003`、`1300003`←`1300002`），于是拿到 079MAX4 的
    平衡（30 级 `prop=60`/`damage=350`）与 30 级的 `lt`/`rb` 判定向量；
    缺的键按名排序插入、多出的键删除，只动 `level/*`，graft 自己的
    `icon`/`iconDisabled`/`iconMouseOver`/`hit`/`req`/`skillType` 不变；
  - level 子块拼接与计数补丁抽到 `tools/nx/level_children.py`（纯函数，
    供后续数据补丁复用；image 摘要助手并入 `tools/nx/map079_tree.py`），
    工具本体保持在 400 行以内；
  - **trigger**：给 `11001002`/`11001003` 重建 `finalAttack` 子节点，
    恰好含一项 `11101002`（`0=30`、`1=40`），其余子节点原样保留；
  - **counter**：对 10 个 FA 技能（3 个 graft + `1100002`/`1200003`/
    `1300002` + 上面 4 个弓箭/魂骑士技能）逐级写
    `attackCount=2`/`mobCount=8`；缺失的 int 节点按名排序插进 level
    子块并修正父节点计数，越界值就地覆盖；
  - 重建走读取端 tail padding，保留一次性备份
    `assets/Skill.nx.pre-fa-variants-backup`；已打补丁的归档直接
    noop（不重写、不覆盖备份），打印 JSON 摘要，断言失败 exit 非零。
- 实跑：280446245 → 280566611 字节（+120366），splice 239 个 int 节点、
  9 个 level 子节点整体替换（3 个 30 级），104 张未触及 image 的结构
  摘要逐字节不变。

### 验证工具

- `tools/verify/final-attack-combo-nx.cpp`：把原来「三个变体必定不存在」
  的回归断言换成：
  - graft 断言（节点存在、7 个子节点、30 级、三张图标位图、`skillType`、
    lv1 2%/105%、lv30 60%/350%、lv30 `lt`/`rb` 判定向量、每级 2 段 8 只）；
  - `11001002`/`11001003` 的 `finalAttack` 恰好点名 `11101002` 且
    `0=30`/`1=40`；
  - `3100001`/`3200001`/`11101002`/`13101002` 全 30 级 2 段 8 只；
  - `find_skill` 对 8 位技能 id 使用 4 位 `.img` 目录（`1110.img` 等）。

## 验证

- `final-attack-combo-nx assets/Skill.nx` 全绿（exit 0）。
- `fa_inventory assets/Skill.nx assets/String.nx --levels`：10 个 FA 技能
  `levels=30`、三张图标齐全；`1100003`/`1200002`/`1300003`
  lv1 `2%/105%` → lv30 `60%/350%`（30 级 keys 与原生兄弟一致，
  含 `lt`/`rb`），每级 `attackCount=2`/`mobCount=8`；
  `TRIGGER 11001002`、`11001003` → `11101002 vals=0:30,1:40`。
- 幂等：二次运行 status=noop，归档 size/inode/mtime 不变。
- 回归（同一归档）：`skill-2023-port-nx`（35）、`second-job-attack-nx`（228）、
  `attack-gap-skills-nx`（28）、`wide-attack-nx`（214）、`magic-damage-nx`（29）、
  `job-damage-settlement-nx`（586）、`attack-effect-nx` 全部 PASSED。

## 备注

- graft 的图标位图是 v83 原始子树，只带 `origin`/`z`，没有 079MAX4 给
  原生位图加的 `_hash` 子节点；客户端与 nlnx 都不读 `_hash`，不影响加载。
- graft 技能没有 `effect` 子节点（v83 亦无），客户端施放特效按可选处理。
