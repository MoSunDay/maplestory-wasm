# 交付路线图：残余缺陷与观察窗（2026-11-02）

> 本页是交付后跟进的**单一承载页**：P1 残余缺陷状态、24 小时交付观察窗
> 判据与复核命令、明确范围外事项，以及原 known-issue
> `2026-10-31-game-server-sql-seeding-pitfalls.md` 中仍有复用价值的服务端
> 种子规范与反馈事项（该文件已删除，要点并入本文
> 「服务端种子与反馈事项」一节）。

## P1 残余缺陷（原编号保留）

| # | 缺陷 | 状态 | 说明 / 证据 |
|---|------|------|-------------|
| 1 | `open()` 回放路径 size 快照档（文件变大需重开才可见） | **已修复** | 8689c62 闭合回放路径残余 0/二次挂起缺口；根因背景见 `docs/known-issues/2026-10-31-asyncify-wedge-and-private-e2e-stack.md`「残余风险」 |
| 2 | socket 层 `ws_recv`/`connect` 自旋与 LazyFS 共用 Asyncify 单 rewind 缓冲（并发 park/wake 无跨 chunk 保障） | **阻塞于 /emsdk 上游回归验证** | 待编译根治方案已就位（`lazyfs_sync_cache.js` 同步缓存命中不挂起、`LazyFileBackend.cpp` 同步快路径、CMakeLists pre-js 接线）。**警示：其前身热补丁版曾致 boot 期 `null function` 回归（技能名变空 + 启动崩溃），重编后必须先过 boot 冒烟，勿在无回归保障时盲动**；runbook 见上述 known-issue |
| 3 | 断线看门狗 | **最小版已实现** | 链路三段：ws-proxy 下游 liveness（30s ping / 90s 静默即拆上游 TCP，`WS_PROXY_LIVENESS_TIMEOUT_SECS`）→ 服务端空闲收割确定性生效（补丁 0003，`features/changelog/2026-10-26/deterministic-idle-session-reaping.md`）→ 收割改走 `session.close(true)` 完整断链路、幽灵会话落库（补丁 0010，`features/changelog/2026-11-02/idle-reap-preserve-disconnect-save.md`）。**完整自动重连 / 会话恢复仍为后续**（客户端断线后无自动重进） |
| 4 | `ws_send` 的 timeout 形参被忽略 | **已修复** | 253b08e 删除从未实现的 timeout 形参，调用点同步收敛 |
| 5 | 公会 / 家族 / 备忘录等 5 个 opcode 无处理 | **决策：显式不支持** | 95696be NullHandler 显式声明不支持备忘录/举报/家族/公会协议，收包走限频日志（避免刷屏），不再静默丢弃 |
| 6 | 服务端 `CharSelectedHandler` 缓存未命中静默 `closeSession`（双击角色无声掉线、零日志） | **补丁 0011 已加 WARN 日志** | 41e1681：静默 close 前输出含 accountName/charId/world 的 `[WARN] character cache miss`；纯日志零行为变更。根治（缓存未命中时的补救路径）仍属服务端侧跟进 |

### 观察项（未定级，复现后再定级）

- **4031008 对话重放竞态**：会话被强杀后重放对话脚本时观测过一次
  `[HACK] Not enough items available of Item:4031008 (0/1)`（道具移除与
  上一会话残留状态竞态），非必现；发生在 NPC 对话 handler，与
  `CharSelectedHandler` 无关（补丁 0011 备注已排除该入口）。
- **高台 NPC 不可达（102020300 / fh189）**：原生 NPC 1072000 站 fh189
  高台，对步行玩家天然不可达（超出 120/80 对话射程）；以 plife 自定义
  低点刷点绕过（种子规范见下文「plife 三要素」），plime/plife 机制
  文档化以本文为初稿。

## 交付观察窗（24h）

三项判据全部满足即视为交付稳定；任一失败按对应 changelog 回溯。

### ① 排行 Worker 零语法异常且 rank 1-45 持续落库

修复背景：`features/changelog/2026-11-02/ranking-worker-rank-reserved-word.md`
（`RANK` 为 MySQL 8 保留字，补丁 0009 给总榜 UPDATE 加反引号）。

```bash
# 计数必须为 0（异常逐字含 MySQLSyntaxErrorException）
docker logs ms-game-server --since 24h | grep -c MySQLSyntaxErrorException
```

rank 落库复核：DB 端点用 `scripts/maplestory_db_endpoint.sh` 解析
（`endpoint` 输出 `host:3306`、`host` 只输出 IP；凭据运行时从
`link_repos/MapleStory-Server/configuration.ini` 解析，同
`tools/verify/maplestory-webui/pq-harness-db.mjs` 的 `readDbConfig`，
**不得写死任何凭据**）。判据：观察窗内至少跨过一个整点 tick 后，
`maplestory.characters` 的 `rank` 1-45 名非零且 `rankMove` 有值
（修复前总榜 UPDATE 从不落库）。

### ② 容器连续在网

```bash
docker ps --filter name=ms-game-server --format '{{.Status}}'
# 期望 Up 超过 24h；若中途重启，需核对重启时刻日志（bind 竞态见
# docs/known-issues/2026-11-01-game-server-restart-bind-race-and-world-cache.md）
```

### ③ SaveChar 落库无断档（连续采样）

服务端按会话写 `link_repos/MapleStory-Server/logs/<日期>/players/SaveChar.txt`。

```bash
grep 'Attempting to save' \
  link_repos/MapleStory-Server/logs/$(date +%F)/players/SaveChar.txt | tail -20
```

判据：任一在线会话的相邻落库行时间差为秒级；不应重现
2026-10-31 事故基线（浏览器静默死亡后 14 分钟零落库，见
`features/changelog/2026-11-02/idle-reap-preserve-disconnect-save.md`）。

## 明确范围外

- **并发会话在途工作**：aqua 道路游泳 / snow 地图滑行 / cash 装备覆盖
  外观（`tools/verify/maplestory-webui/aqua-swim-flow.mjs`、
  `snow-slide-flow.mjs`、`cash-cover-look-flow.mjs`，均 untracked）及
  `src/client/Character/Look/CharLook` 系列改动——归并发会话收口，
  本页不跟踪其状态。
- **新版本地图数据源 gate**：079 wz 无数据源，新版本地图复刻在获得
  数据源前不排期。
- **现金装备边界**：现金装备为「纯装饰覆盖、不影响属性」，属设计边界
  而非缺陷，不接受相关修复需求。

## 服务端种子与反馈事项

> 并入自已删除的 `docs/known-issues/2026-10-31-game-server-sql-seeding-pitfalls.md`
> （SQL 造数 + 服务端缓存排障记录，2026-10-31 二转 e2e 排障实证）。

### 原「其他待反馈服务端的问题（未修）」3 条去向

1. `CharSelectedHandler` 缓存未命中静默 close 建议至少打日志 →
   **已覆盖**：补丁 0011（见上文 P1#6 与
   `features/changelog/2026-11-02/charselected-cache-miss-logging.md`）；
2. 4031008 对话重放竞态 → **观察项**（见上文「观察项」第一条）；
3. 高台 NPC plife 机制文档化 → **观察项**（见上文第二条；种子规范见下）。

### SQL 造数陷阱速查（仍有复用价值）

- **改库后必须重启**：`Server.java isFirstAccountLogin` 只在启动后首次
  登录缓存账号→角色集合，SQL 直插角色不在缓存里即静默 close（现在至少
  有 P1#6 的 WARN）。任何 characters/queststatus/plife 改动后
  `docker restart ms-game-server`（地图对象另有进程内缓存，plife 同理）。
- **queststatus 大任务 id**：服务端 `rs.getShort("quest")` 对 id>32767
  抛 `MySQLDataException` 已改 `rs.getInt`（已修，留意同类 SMALLINT 读法）。
- **plife 三要素**（`MapleMapFactory.loadLifeFromDb` 自定义刷点）：
  1. `world` 列必须与角色所在 world 一致（本环境真实世界 world=4
     "Reboot"；端口规则 7575+world*100，客户端连 7975 即 world4）；
  2. `y` 必须高于目标 foothold 顶部（客户端 `get_y_below` 向下吸附；
     实测 102020300 地面 fh83 顶 2382，填 2385 会掉进 y=2805 深坑致
     NPC 永不可交互）；
  3. `fh` 填该图真实 fh id（多数图无 fh0，填 0 无效；wz foothold 解析
     可得 fh id）。参考行：
     `insert into plife(world,map,life,type,cy,f,fh,rx0,rx1,x,y,hide,mobtime,team) values (4,102020300,1072000,'n',2300,0,83,-900,-700,-800,2300,0,0,-1), (4,108000300,1072004,'n',2150,0,122,830,930,880,2150,0,0,-1);`
- **数据格式两处静默坑**：`characters.sp` 必须逗号格式 `'61,0,0,0'`
  （分号触发服务端 `NumberFormatException` → 无 WORLD_LIST → 客户端
  park）；`accounts.characterslots` 必须 ≥ 账号角色数（否则
  CharlistHandler 校验异常重抛 → park）。选角 UI PAGESIZE=4，账号只留
  1 张卡最稳（第 5 张卡在第 2 页易点空）。
- **重启后 world 端口绑定偶发失败**：docker restart 后 7975 可能因
  TIME_WAIT 绑定失败而登录服仍把客户端引导过去 → `Connection refused`
  → park；解法为再 restart 一次。复跑判据见
  `docs/known-issues/2026-10-29-game-server-restart-client-freeze.md` 与
  `docs/known-issues/2026-11-01-game-server-restart-bind-race-and-world-cache.md`。
- **infoNumber 任务种子规范（陷阱六，重点）**：`Check.img` 中带
  `infoNumber` 的任务（20000→20022、20010→20022、29005→27010、
  29007→27012）服务端 `addQuestInfo` 会内联追加
  `short infoNumber + mapleAsciiString(data)`；客户端 `parse_questlog`
  靠 `is_started(qid)+i--` 配对且要求父任务先出现。**只种父任务不种子
  任务 → 字节漂移 → `PacketError: Unsupported non-empty minigame info`
  → SET_FIELD 中断 → 双击角色卡住**。种子规范：queststatus 只种无
  `infoNumber` 的任务（验证集 20001-20008）；原生任务不受影响。复盘见
  `features/changelog/2026-10-31/quest-log-usability-fix.md`。

### DB 种子端点用法

`scripts/maplestory_db_endpoint.sh {host|endpoint|proxy|run-cosmic}`：
从运行中的 `mysqld`/`mariadbd` 进程（声明 `MYSQL_DATABASE=maplestory`）
定位实例并解析容器 IPv4——`host` 输出 IP、`endpoint` 输出 `IP:3306`、
`proxy` 起 systemd socket 代理、`run-cosmic` 注入环境后执行 linked
server `run.sh`。种子/复核 SQL 一律经该端点连接，不写死回环地址与凭据
（凭据从 `configuration.ini` 运行时解析）。
