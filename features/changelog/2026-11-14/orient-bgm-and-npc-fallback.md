# 东方神州/泰国 BGM 补齐与无脚本 NPC 对话兜底

## 背景

079MAX4 地图落地后还有两处端到端缺口：

- 客户端东方神州/泰国地图 NX 把 BGM 平铺在地图节点上（701000000 为
  `BgmCN/GoShanghai`、702100000 为 `BgmCN/shaolin_daxiongbaodian`、
  500000000 为 `BgmTH/ThaiTown`）；`MapInfo` 在缺少 `info` 时回退读
  平铺字段，并归一化为 `BgmCN.img/...`；而 `assets/Sound.nx` 只有 44
  个原版 wrapper，没有 `BgmCN.img`/`BgmTH.img`，进图解析失败而静音。
- 新地图上的 NPC（上海 9300015 大当家、9310002 Owner Jang、
  9330042 微微安，嵩山 9310034 大宝等）没有专属脚本，也没有
  `shops`/`shopitems` 记录。`NPCTalkHandler` 在
  `NPCScriptManager.start(..., null)` 失败且 `!npc.hasShop()` 时只写
  `NPC_UNCODED` 日志就返回，点击 NPC 无任何对话窗口。

## 变更

### 1. Sound.nx 注入 BgmCN/BgmTH

- 用 `tools/nx/inject_orient_sound_nx.py` 把两个 wrapper 追加进
  `assets/Sound.nx`（先写 `.pre-orient-sound-backup`，已有 wrapper 或
  备份时拒绝重跑）：
  - `BgmCN.img`：13 首，含 `GoShanghai`、`ShanghaiField`、
    `shaolin_*`、`summerboating01..03` 等；
  - `BgmTH.img`：5 首，含 `ThaiTown`、`ThaiField`、
    `goldTemple*TH`。
- 归档规模：root wrapper 44 → 46（+2），audio blob 3966 → 3984
  （+18），全部新音频索引落在追加区间且长度与 blob 一致；原有 44 个
  wrapper 名称、排序与 3966 条音频不动。
- 只改客户端资产，无 C++ 代码改动；服务端地图 XML 与
  `FieldMusicPath::normalize_bgm_path` 的既有路径约定不变。

### 2. 无脚本 NPC 通用对话兜底

服务端补丁：[`scripts/server-patches/0021-orient-npc-fallback.patch`](../../../scripts/server-patches/0021-orient-npc-fallback.patch)

- `NPCTalkHandler`：无专属脚本且无商店时，改用脚本名 `fallback_talk`
  再调一次 `NPCScriptManager.start`；仅当这次也失败才回落到原来的
  `NPC_UNCODED` 日志。有专属脚本或带商店的分支不变。
- `NPCConversationManager.getFallbackTalk()`：读取
  `String.wz/Npc.img/<id>`，按 `n0/d0/n1/d1/n2/d2` 顺序返回第一条非空
  文本；没有台词时退回 `name`（有 `func` 拼成 `name - func`）；节点
  缺失、空串或读取出错统一返回 `...`，保证一定可显示。`Npc.img` 经
  静态字段 `npcStringData` 懒缓存（`private static synchronized
  getNpcStringData()`，仅成功后缓存），不再每次点击重建
  `MapleDataProvider` 重复解析 2.4MB XML（单文件 WZ 下也避免泄漏
  文件句柄）；读取异常不再静默，经
  `FilePrinter.printError(FilePrinter.NPC + npc + "-fallback.txt", e)`
  记入 `game/npcs/<id>-fallback.txt` 后仍返回 `...`，wzpath 配错与
  单纯缺条目可区分。
- 新增 `scripts/npc/fallback_talk.js`（11 行）：`start()` 调
  `cm.sendOk(cm.getFallbackTalk())`，`action()` 调 `cm.dispose()`，
  只读、无传送/发物品副作用。
- 已有专属脚本但脚本本身报错（eval/start 抛异常）的 NPC 也会落到
  本兜底：`NPCScriptManager.start` 此时返回 false，真实错误仍记录在
  原有 NPC 异常日志中，兜底只保证玩家侧不静默。

## 验证

- `python3 tools/verify/verify_orient_sound_nx.py --report`：
  `46 root children, 3984 audios`，`BgmCN.img: 13 children`、
  `BgmTH.img: 5 children`；`BgmCN.img/GoShanghai`、
  `BgmCN.img/ShanghaiField`、`BgmTH.img/ThaiTown` 均 `resolved`，
  `ALL CHECKS PASSED`。
- 服务端补丁 `patch -p1` 重放后两个 Java 文件与当前 checkout
  byte-match，`-R --dry-run` 可干净反向；已 `bash posix-compile.sh`
  并 `docker restart ms-game-server`（2026-11-14；2026-11-15 懒缓存/
  异常日志硬化版同样重放 byte-match 并重编重启）。
- 浏览器 e2e（真实登录/进图/点击 NPC）：

| 地图 | NPC | 结果 |
|---|---|---|
| 701000000 | 大当家 9300015 | 对话非空 |
| 701000000 | Owner Jang 9310002 | 对话非空 |
| 702100000 | 大宝 9310034 | 文本以「练武之人也要保护好皮肤啊」开头 |
| 701000000 | 微微安 9330042 | 文本以「想要来试试看」开头 |

- 2026-11-15 硬化版复测：同一服务端进程内，连续对话 701000000 的
  9300015 与 9310002，两次均成功打开、返回非空文本并以 Escape 关闭；
  未生成任何 `*-fallback.txt` 异常日志。验证了第二次命中缓存
  Npc.img 的场景。

- 同一轮 e2e 中 701000000 的 BGM 成功解码播放，clip 长度
  1174465 字节（`GoShanghai` blob 1174547 字节减去 82 字节音频头），
  控制台无音频解析/播放错误。

## 已知限制

- 东方神州/泰国的商店型 NPC 仍没有 `shops`/`shopitems` 行，因此只会
  走本兜底给出通用台词，不会打开真实商店；补齐商品表后需另做商店
  验证。
- `assets/Sound.nx` 注入是一次性操作，脚本检测到 wrapper 或备份已存在
  会拒绝重跑；回滚用 `.pre-orient-sound-backup` 覆盖回原归档。

## 2026-11-15 追加：fallback 台词中文化

fallback 链路打通后，部分 orient/泰国 NPC 仍会读到 079 源的英文
（或个别韩文占位 `대사1`）台词。本轮把实际放置在 28 张上海/嵩山地图
与 9 张泰国地图上的非中文 NPC 全部中文化：

- 新增 `tools/npc_chinese_dialogue.py`（52 个 NPC 的中文字段表：上海/
  嵩山、泰国水上市场、以及混在这批地图里的通用 NPC）与
  `tools/patch_orient_npc_chinese.py`（幂等行级 patcher，`--check`
  干跑，原子写入，不重排 2.4MB XML 的其他字节）。
- 应用于 `link_repos/MapleStory-Server/wz/String.wz/Npc.img.xml`：
  52 个条目、199 个字段（name/func/n0-n2/d0-d1），改动前后行数一致、
  CRLF 保留、XML 可解析；原文件备份为
  `Npc.img.xml.pre-chinese-backup`（同目录）。
- 仅改 WZ 数据、不改 Java，无需重放补丁或重编；但 0021 的
  `getNpcStringData` 静态缓存会持有旧 DOM，改完必须
  `docker restart ms-game-server`。
- 顺手翻译了带专属脚本的 NPC（9000020 斯皮内尔、9000021 加加、
  9010000 枫叶管理员、9209000/9209001、2091006 公告牌）的 WZ 兜底
  文本：脚本正常时不生效，脚本异常回落 fallback 时不再是英文。
- `9310039 达摩画卷`（n0 本就是「... ...」）与无 Npc.img 条目的
  9000087/9209103/9310045 保持原样（fallback 显示 `...`）。

### 复测（重启后，专用账号 orient1，世界 Reboot）

| 地图 | NPC | 结果 |
|---|---|---|
| 701000000 | 摄影师小曹 9310012 | 「大家说——茄子~！」 |
| 701000100 | 果汁大婶 9330031 | 「来一杯现榨果汁吧，解暑又解渴！」（原韩文占位） |
| 500000000 | 蓬德 9250005 | 「水上市场充满了活力！我太喜欢了！」 |
| 500000000 | 水上市场枫叶电视 9270015 | 名字兜底「水上市场枫叶电视」 |

同一账号在 701000000 的 BGM 仍命中 1174465B clip，控制台无音频
错误；`patch_orient_npc_chinese.py --check` 复核为 clean（幂等）。

### 排障记录（与本次改动无关，留档备查）

- `docker restart` 后偶发 `Channel bind on port 7975 failed`
  （旧实例 socket 未释放），Reboot 频道缺位会让世界 4 的角色卡在
  选人界面、ws-proxy 报 connection refused；确认 7975 释放后再次
  restart 即恢复。重启后应检查 6 个端口（7575/7675/7775/7875/7975/
  8484）都在监听再跑 e2e。
- test1/test2/test3 账号被并行批次占用时同账号登录互踢，表现为
  PLAYER_LOGGEDIN 已到服务端但客户端被断开；本轮改用
  `scripts/test_accounts/create_accounts.py --prefix orient --count 1
  --chars-per-account 12` 生成独立账号 orient1 规避。
