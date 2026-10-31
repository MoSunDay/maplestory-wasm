# 移除 NPC 辛迪（Cody/9200000）在全部城镇地图的驻场

## 背景

NPC 9200000（英文 Cody，Wizet Wizard；汉化名"辛迪"）是 v83 原版彩蛋
NPC，作为出厂 life 条目驻场 11 张主要城镇地图（射手村/魔法密林/勇士
部落/废弃都市/南港/天空之城/雪域/玩具城/神木村/Happyville/GM 活动图）。
本次按需求将其从游戏世界中移除。该 NPC 无对话脚本
（`scripts/npc/9200000.js` 不存在），无商店/任务依赖。

## 变更

服务端（`link_repos/MapleStory-Server`，非 git 仓库，手工同步）：

- 11 个 `wz/Map.wz/.../<mapid>.img.xml` 的 `life` 节点各删除 1 条
  `type="n" id="9200000"` 条目，其余条目重新连续编号（与 2026-10-19
  FM life 删改同风格）：
  - Map1：100000000、101000000、102000000、103000000、104000000
  - Map2：200000000、211000000、220000000、240000000
  - Map6：680000000
  - Map7：777777777（life 归零，仅剩空 `life` imgdir）

客户端：

- 场景内 NPC 完全由服务端 `SPAWN_NPC` 封包驱动（`MapNpcs.cpp` 不读
  Map.nx life），服务端侧删除即全局生效；`Npc.nx`/`String.nx` 中
  9200000 外观与铭牌数据保留（数据资产，无 spawn 即不出现）。
- 地图 tooltip 的 NPC 名单读客户端 `Map.nx` life 节点
  （`Misc.cpp get_life_on_map` → `MapTooltip.cpp`）。为彻底清除残留，
  本次已用新工具按可达性压缩删除节点（见下"客户端 NX 清理"）。

## 验证

- 11 个 XML 均通过 ElementTree 良构校验；life 子节点编号 0..n-1 连续。
- 全 `wz/Map.wz` 复查 `id="9200000"` life 条目残留为 0
  （`192000000/192000001` 的命中为地图 ID 子串，非 life 条目）。
- 删除条目的 life 索引与客户端 `Map.nx`（`assets/` 与 desktop 副本）
  扫描结果逐一吻合（双数据源交叉验证）：11/15/13/13/15/11/8/17/3/15/0。
- CRLF 换行风格与文件其余部分保持一致。

## 生效与运行时验证（2026-10-28 执行）

- 服务端以 Docker 容器运行（`/data/MapleStory-Server` bind mount 至
  容器 `/srv`，host 网络），已 `docker restart` 生效；login(8484)/
  channel(7575) 端口 ~15s 恢复，8484 v83 握手（0x0e）正常。
- 数据库 `plife` 表为空——无 GM `!pnpc` 残留，无需清理。
- 运行时取证：容器内经服务端自身类路径（`MapleMapFactory` + wz XML
  provider，与线上进程同一 bind mount 与解析代码）加载全部 11 张图，
  枚举 `MapleMapObjects` 中 `MapleNPC`：9200000 全部缺席，各图 NPC 数
  29/27/26/28/35/21/19/26/22/18/0 与上文 XML life 条目数逐一吻合，
  777777777 为空图。⇒ 线上进图 spawn 包不再含辛迪。

## 客户端 NX 清理（2026-10-28 执行）

新增工具（纯函数式，均 <400 行，用法见文件头）：

- `tools/nx/remove_map_life_nx.py`：按可达性压缩删除 Map.nx 中
  `mapid/life/n<i>` 目标节点。节点表是 DAG 不可按块删，工具从根 BFS
  标记可达节点，跳过孤儿后重写节点表并回写 header `node_count`；
  `num=0` 空节点的 child 字段是哨兵值（=node_count），索引前已 clamp。
- `tools/nx/verify_map_life_nx.py`：结构 + 内容双重复核（header、
  目标缺席、非目标 life 保全、兄弟顺序、树深度、文件大小不变）。

执行结果（备份 `assets/Map.nx.pre-remove-cindy-backup`，备份存在即拒
绝执行，仓库惯例）：

- 删除 11 张图的 `life/n<i>` 辛迪节点共 127 个目标节点；顺带剔除
  4628 个既有孤儿（不可达）节点。
- `node_count` 7989220 → 7984465；文件大小 1648550354 字节不变
  （节点表定长 20B，仅头部计数与表内容更新）。
- verify 64/64 断言 ALL PASS；独立全树扫描脚本二次确认 9200000
  life 命中为 0。
- `desktop/resources/msweb/assets/Map.nx` 同步（md5 一致）。

回滚数据（版本化）：`features/changelog/2026-10-28/
cindy-9200000-life-entries.json`——11 条 life 条目完整字段（id/type/
x/y/mobtime/f/hide/fh/cy/rx0/rx1 及原索引），配合备份 NX 可完整还原；
服务端 XML 侧回滚同字段直接回插。

## 浏览器冒烟脚本（2026-10-28 就绪）

`tools/verify/maplestory-webui/henesys-cindy-gone-flow.mjs`：登录 →
进射手村 → 正向对照 NPC 1012000（x≈149）Space 弹对话 → 负向断言辛迪
原位 x=424 ±120px 无对话 → 截图留档。前置 SQL 已把 test1Hero(215)
迁回 100000000 sp=0。已登记 `tools/verify/maplestory-webui/index.md`。

调试记录（留给后续复用）：选角 doubleClick 在 WASM 忙时会静默失效
（Window.cpp 双击窗口 350ms/4px，服务端日志停在 CHARLIST 即此症状）；
本仓 UI.nx 为 post-Chaos 资产，BtSelect 按钮锚点在 (586,427) 而非
pre-Chaos 的 (440,495)，hit 区 (586,427)-(726,479)。共享宿主机高载
（load 100+）时 CDP 输入会被 busy renderer 丢弃，需 retryClick 并在
低载窗口执行。

## 待办

- 浏览器冒烟执行与截图留档：脚本就绪，当前共享环境过载（load 100+，
chromium 冷启动 10 分钟以上且输入丢弃），待低载窗口跑一次即可收尾。
