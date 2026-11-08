# 美容/美发 NPC 样式池改为全集（不再按城市区分）

## 症状（Context）

- 各城市美容院/理发店（如射手村 Natalie 1012103、天空之城 Rinz 2012007、
  昭和村 912010x、新加坡 927003x 等）只能选到本城市硬编码的少量发型/脸型。
- 同一张普通券/EXP 券/VIP 券在不同城市可选池不同，旧素材（如 33000/34000
  系）与新素材被城市切割，无法互通。
- GM 全能造型师 9900000.js（NimaKIN）的 `hair` 数组甚至漏掉 30100/30430/30850。

## 根因（Change Root Cause）

- 服务端 NPC 脚本用硬编码数组 `mhair_r/_e/_v/_d/_q`、`fhair_*`、`mface_*`、
  `fface_*`（及 9900000.js 的 `hair/fhair/face/fface`）定义样式池，这就是
  "按城市过滤"的全部实现；数据真源（wz 素材 + String.wz 名称）从未回填。

## 修复（Change Summary）

- 新增父仓工具 `scripts/beauty-styles/`：
  - `extract_styles.py`：从 `wz/Character.wz/{Hair,Face}/*.img.xml`（素材存在）
    ∩ `wz/String.wz/Eqp.img.xml`（有 name，`getCosmeticItem` 才不返回 -1）
    提取全集，产出 `beauty_styles.json`：
    男发型 97（30000..30950 连续 + 30990/33000/33040/33100）、女发型 96
    （31000..31950 去缺号 + 34000/34010/34020/34030/34050/34110）、
    男脸型 32 款、女脸型 29 款（按款式去重，代表 ID 优先瞳色 0 变体）。
  - `patch_npc_scripts.py`：幂等替换 47 个已知美容脚本中的样式数组
    （`--check` 模式做结构/一致性校验），保持单行与原 `Array()`/`[]` 写法、
    原样保留 CRLF 行尾；skin/价格/对话/券逻辑不动。
- 41 个脚本共替换 94 个样式数组（6 个美容 NPC 是纯美瞳脚本，无样式数组）；
  9900000.js 的 `face/fface` 原本已是全集，幂等跳过。
- 脚本自行叠加发色（`hair % 10`）与瞳色（`face % 1000 - face % 100`），
  数组只放基础代表 ID，玩家现有配色不受影响。

补丁留档：`scripts/server-patches/0016-beauty-npc-full-style-pools.patch`
（重放：`cd link_repos/MapleStory-Server && patch -p1 < 本文件`；
已打过的树重复应用会被 patch 识别跳过，建议 `--forward`）。

## Impact Surface

- linked server `scripts/npc/*.js` 41 个美容脚本（纯 JS，运行期加载，无需重编
  dist；服务端运行中需 `docker restart ms-game-server` 或等脚本缓存失效）
- 不影响：skin 数组、价格、对话文本、券消耗逻辑、非美容 NPC

## Validation

- `python3 scripts/beauty-styles/patch_npc_scripts.py --check`：96 个样式数组
  全部非空、无重复 ID、与全集一致、单行、括号/逗号平衡
- 幂等性：重跑 patch 报告 0 替换；patch 文件对原始 checkout 重放后与
  当前工作树逐字节一致
- `node --check` 通过（除 9900000.js，其含 E4X `for each` 语法）
- 逐行 diff 断言：所有变更行均为样式数组声明行
