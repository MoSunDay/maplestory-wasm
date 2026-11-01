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
    工具本体与复用助手分文件维护；
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

### 服务端数据（`link_repos/MapleStory-Server/wz/Skill.wz/*.img.xml`）

服务端不读 NX，只读自己 checkout 里的 `Skill.wz` XML（容器 `ms-game-server`
以 `-Dwzpath=wz/`、WorkingDir `/srv` 加载），且 `wz/Skill.wz` 从未同步过
本次 NX 变更，因此新增独立补丁工具把同一份契约搬到 XML 上：

- `tools/nx/fa_variants_xml_common.py`：NX 侧读取/校验 + XML 侧拼接。
  graft 的 30 级 level 子树由 `1100002`/`1200003`/`1300002` 逐级渲染后
  **按数值序**（1,2,…,30）插回——服务端 `Skill.getEffect(level)` 取
  `effects.get(level-1)`，是位置索引，字节序（1,10,11,…）会让 2 级读到
  10 级数据；XML 转义（`&`/`<`/`>` 与属性引号）与 int 子节点计数同步重建。
- `tools/nx/fa_variants_xml_verify.py`：契约校验（10 个技能 30 级、
  每级 2 段 8 只、graft 与 NX 逐级一致且与原生兄弟相等、lv1 2%/105% →
  lv30 60%/350%、lv30 `lt`/`rb`、两张触发表）。
- `tools/nx/patch_final_attack_variants_xml.py`：CLI，`--dry-run` 预览、
  `--check` 只校验待办（exit 0 表示 XML 已是目标态）、默认落地；
  幂等（无编辑即 noop，不重写文件、不覆盖备份），一次备份
  `<name>.img.xml.pre-fa-variants-backup`。
- 实跑 8 个文件（字节，前 → 后）：`110.img.xml` 42674→49220、
  `120.img.xml` 43894→50439、`130.img.xml` 46875→53421、
  `310.img.xml` 61834→63844、`320.img.xml` 36696→38671、
  `1100.img.xml` 50441→50683、`1110.img.xml` 46526→48536、
  `1310.img.xml` 42270→44280；二次运行 9 项全 noop（size/inode/mtime
  不变），`--check` exit 0 且 `pending_edits=0`。

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
- 服务端 XML（独立验证器用 `ElementTree` 解析 + `difflib` 对比备份）：
  8 个文件均可解析；10 个 FA 技能各 30 级且每级 `attackCount=2`/
  `mobCount=8`；3 个 graft 与 NX dump 逐级 int 完全一致（lv1 `2`/`105`
  → lv30 `60`/`350`，lv30 `lt=(-300,-300)`/`rb=(300,300)`）；两个
  `finalAttack` 表均为 `11101002: {0:30, 1:40}`；相对备份**只有插入**，
  `non-insert opcodes=none`（未改动任何既有字节）。
- `patch_final_attack_variants_xml.py --check` exit 0 / `pending_edits=0`。
- 重启 `scripts/game_server_restart.sh` 后 `Skills: Loaded in 0.331 seconds`，
  5 个世界 + 8484 全部监听，日志无 XML/`Skill.wz` 解析异常。
- 运行时探针（直接调服务端 `SkillFactory`/`Skill.getEffect`，`-Dwzpath=wz/`）：
  10 个技能 `maxLevel=30`，lv1/lv2/lv15/lv29/lv30 的 `getAttackCount()=2`、
  `getMobCount()=8`，`getDamage()` 依次 105/110/175/245/350（`11101002`、
  `13101002` 同样 2 段 8 只，lv30 伤害沿用自身 250）——位置索引未错级。

## 后续修复：弓箭手 / 魂骑士 FA 仍然只打一段（`bulletCount`）

### 根因

客户端对**远程**攻击的段数取值路径与近战不同：

- `src/client/Gameplay/Combat/Skill.cpp` 的 `apply_stats` 里
  `case Attack::RANGED: attack.hitcount = std::min(stats.bulletcount, 15)`，
  近战分支才用 `stats.attackcount`；
- `src/client/Data/SkillData.cpp` 读 `bulletCount` 时缺省 `get_integer(1)`；
- `Player::prepare_attack(true)` 对弓 / 弩（带箭）返回 `Attack::RANGED`。

因此上面第一轮补的 `attackCount=2` 对弓箭手终极攻击完全无效：`3100001`
只有 30 级自带 `bulletCount=2`（1-29 级缺键），`3200001` / `13101002`
则 30 级全部没有该键，客户端一律按 1 段结算。`11101002` 是魂骑士
**剑**终极攻击（`Attack::CLOSE`），走 `attackCount`，本来就不需要
`bulletCount`，补了反而会污染近战语义。

### 变更

- `tools/nx/level_children.py`：`plan_counters(nx, targets, extras=None)`
  新增 `extras`（`{(img, sid): ((prop, value), ...)}`），每条目标的 level
  都要同时满足 `attackCount=2`/`mobCount=8` **加上**该技能的 extra 项；
  report 行新增 `props` 字段，`plan_level` / `splice_props` 改成按调用方
  传入的 prop 序列工作（`splice_props` 改为从待插入节点本身 intern 名称，
  不再假定只有 `attackCount`/`mobCount` 两个名字）。
  `patch_final_attack_counts_nx.py` 新增默认参数 `props=PROPS`
  （`plan_level`、`xml_edits`）与按需编译的 `prop_int`，原 CLI 行为不变。
- `tools/nx/patch_final_attack_counts_nx.py`（`splice_props`）顺带修了一个
  潜在碰撞：同一个 level 缺 `attackCount` 与 `bulletCount` 时两者算出**同一个
  插入点**（`bisect_left` 到块首），旧的「插入点必须唯一」断言会直接 fail。
  现在一个插入点可以带一组按名排序的新 int（`shift_base`/`bump_level_counts`
  改为按每个点的叶子数累计位移，单点单叶时与旧行为完全等价），
  `apply_counters` 返回拼接的 int 个数而非插入点数。
- `tools/nx/inject_final_attack_variants_nx.py`：新增
  `RANGED = 3100001/3200001/13101002` 与 `EXTRA_PROPS = {target: ((bulletCount, 2),)}`；
  `plan_counters(nx, ALL_FA, EXTRA_PROPS)` 让 noop 判定把缺的
  `bulletCount` 也算进待办，`check_counters` 对这三个 id 额外断言
  `bulletCount=2`，并反向断言其余七个 FA（含 `11101002`）**不得**出现
  `bulletCount`；报告 / SUMMARY 增加 `props`、`ranged` 与
  `ranged FA also at bulletCount=2` 一行。
- 实跑（NX）：`Skill.nx` 280566611 → **280568387** 字节（+1776），
  splice **89** 个 int 节点（`3100001` 29 个、`3200001` 30 个、
  `13101002` 30 个），0 个 level 整体替换，104 张未触及 image 摘要不变；
  sha256 `a53af13ce0b3d81e4053ff95ebb8bd658fb5347a9f28f925c947932008ec1152`。
  一次性备份 `assets/Skill.nx.pre-fa-variants-backup`
  （280446245 字节，sha256
  `0325497115b20c98d6b300035f83b966d19d188813fb849973d96721df81e9e4`）
  仍是最初 graft 前的原文，未被覆盖；二次运行 size/inode/mtime 全不变
  （严格 noop）。把这份 pristine 原文拷到临时目录重跑工具（`--assets-dir`）
  得到与 `assets/Skill.nx` **逐字节相同**的产物（sha256 一致，280568387
  字节），即工具是「pristine 输入 → 唯一输出」的确定性函数；同样的方法用
  `Skill.nx.pre-fa-counts-backup` 重跑 `patch_final_attack_counts_nx.py`
  也复现出 `Skill.nx.pre-fa-variants-backup` 的原文（sha256 一致）。
- `tools/nx/fa_variants_xml_common.py`：新增 `RANGED_COUNTERS`、
  `BULLET_LEVELS`、`COUNTER_PROPS`/`BULLET_PROPS`、`counter_props(sid)` 与
  `level_contract(sid)`；`plan()` 对这三个 id 走
  `attackCount/mobCount/bulletCount` 三项，report 增加 `props`。
- `tools/nx/fa_variants_xml_verify.py`：`check_levels` 按
  `level_contract(sid)` 断言（远程三个多一项 `bulletCount=2`），并反向
  断言非远程 FA 不得携带 `bulletCount`。
- 实跑（服务端 XML，前 → 后字节）：
  `310.img.xml` 63844 → **64859**、
  `320.img.xml` 38671 → **39721**、
  `1310.img.xml` 44280 → **45330**（其余 5 个文件无编辑、原地 noop），
  一次性备份 `<img>.xml.pre-fa-variants-backup` 全部保留未覆盖；
  `--check` exit 0 且 `pending_edits=0`，二次默认运行 size/inode/mtime 不变。
  sha256：`310.img.xml` `eb1cc550…e66117`、`320.img.xml`
  `3c388f70…c23274`、`1310.img.xml` `1f591bd2…a9deb8`
  （`1110.img.xml` `1fc333e9…3ee877`，无 `bulletCount`）。
- `tools/verify/final-attack-combo-nx.cpp`：新增
  `check_every_level_bullet`，断言 `3100001`/`3200001`/`13101002`
  30 级全部 `bulletCount=2`，且 `11101002` 一级都不带；原有
  「10 个 FA 每级 2 段 8 只」「全部技能 id → image 前缀」断言保留。

### 验证

- `final-attack-combo-nx assets/Skill.nx`：**93** 项全绿（exit 0）。
- `/tmp/fainv/lvdump assets/Skill.nx 3100001|3200001|13101002`：lv1/15/29/30
  均为 `attackCount=2 bulletCount=2 mobCount=8`；`11101002` 四级均无
  `bulletCount`。`fa_inventory --levels`：10 个 FA 技能 `missing=none`，
  `3100001`/`3200001`/`13101002` 每级 keys 含 `bulletCount`。
- 回归（同一归档）：`skill-2023-port-nx`（35）、`second-job-attack-nx`（228）、
  `attack-gap-skills-nx`（28）、`wide-attack-nx`（214）、`magic-damage-nx`（29）、
  `job-damage-settlement-nx`（586）全部 PASSED；`patch_final_attack_counts_nx.py
  --dry-run` 双侧 no-op。
- 服务端重启（`scripts/game_server_restart.sh`）：`Skills: Loaded in 0.31
  seconds`，6 个监听（5 世界 + 8484）全起，日志无 `Skill.wz` 解析异常。
- 运行时探针（容器内 `-Dwzpath=wz/`，直接调 `SkillFactory`/`Skill.getEffect`）：
  10 个 FA 技能 `maxLevel=30`，lv1/2/15/29/30 的 `getAttackCount()=2`、
  `getMobCount()=8` 未受影响；`3100001`/`3200001`/`13101002` 的原始
  `bulletCount=2` 覆盖 **30/30** 级，`11101002` 与三个近战 FA 为 0/30 级。

## 备注

- graft 的图标位图是 v83 原始子树，只带 `origin`/`z`，没有 079MAX4 给
  原生位图加的 `_hash` 子节点；客户端与 nlnx 都不读 `_hash`，不影响加载。
- graft 技能没有 `effect` 子节点（v83 亦无），客户端施放特效按可选处理。
- 服务端 XML 只在 `link_repos/MapleStory-Server/wz/Skill.wz` 内插入子树与
  int 节点，未改动图标等其他资源；8 份 `<img>.xml.pre-fa-variants-backup`
  保留改前原文，回滚即 `mv` 覆盖，重跑补丁工具可重新生成。
- 服务端 graft 的 30 级数据由 NX 侧原生兄弟技能渲染而来（而非从别处
  拷贝 XML），因此 XML 与客户端读到的数值天然同源；`--check` 可作为
  「服务端是否已同步 NX 契约」的一键断言，适合常驻 CI 或发版前自检。
