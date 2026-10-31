# SQL 造数 + 服务端缓存：二转 e2e 排障记录（五个静默陷阱）

> 状态：2026-10-31 观测于 second-job-instructor-flow 排障，全部为服务端
> （link_repos/MapleStory-Server，非本仓 git 管辖）或数据侧问题，客户端
> 无需改动。若已过时请直接删除本文件。
> 目击者：二转教官 e2e 会话（commit 8ef1264 排障过程）。

## 共性症状

浏览器 e2e 卡在登录/选角/进图某一层：CDP evaluate 超时或探针返回 -1，
服务端日志**无异常无回包**。多层陷阱可叠加，需逐层剥离。

## 陷阱一：SQL 建角后必须重启 ms-game-server

`Server.java isFirstAccountLogin` 只在启动后**首次登录**时缓存账号→角色
集合。SQL 直插的角色不在缓存里，`CharSelectedHandler.haveCharacterEntry`
判定失败后**静默 closeSession**（无回包、无异常、连接直接关）。

判据：日志停在 `CHAR_SELECT → SERVER_IP` 之后无 channel `IoSession opened`。
解法：任何 characters/queststatus/plife 改动后 `docker restart ms-game-server`
（地图对象另有进程内缓存，plife 同理）。

## 陷阱二：queststatus 大任务 id 使角色无法载入（已修）

`MapleCharacter.java:1354` 曾用 `rs.getShort("quest")`，任务 id>32767
（如 100003）抛 `MySQLDataException '100003.0' outside SMALLINT`，角色
进图即断。已改 `rs.getInt` 并重编译生效。

## 陷阱三：plife 自定义 NPC 刷点三要素

`MapleMapFactory.loadLifeFromDb`（plife 表）是官方自定义刷点机制，但：

1. `world` 列必须与角色所在 world 一致（本环境真实世界是 world=4
   "Reboot"；端口规则 7575+world*100，客户端连 7975 即 world4）；
2. `y` 必须高于目标 foothold 顶部——客户端 `NpcSpawn::instantiate` 用
   `physics.get_y_below` 向下吸附。实测 102020300 地面 fh83 顶为 2382，
   填 y=2385 会掉进 y=2805 深坑，NPC 超出 120/80 对话射程永远不可交互；
3. `fh` 填该图真实 fh id（多数图无 fh0，填 0 无效）。

参考行（wz foothold 解析可得 fh id）：

```sql
insert into plife(world,map,life,type,cy,f,fh,rx0,rx1,x,y,hide,mobtime,team)
values (4,102020300,1072000,'n',2300,0,83,-900,-700,-800,2300,0,0,-1),
       (4,108000300,1072004,'n',2150,0,122,830,930,880,2150,0,0,-1);
```

## 陷阱四：数据格式两处静默坑

- `characters.sp` 必须逗号格式 `'61,0,0,0'`（分号触发服务端
  `NumberFormatException` → 无 WORLD_LIST → 客户端 park）；
- `accounts.characterslots` 必须 ≥ 账号角色数（否则客户端
  CharlistHandler 校验异常重抛 → park）。另选角 UI PAGESIZE=4，账号
  只留 1 张卡最稳（第 5 张卡在第 2 页易点空）。

## 陷阱五：重启后 world 端口绑定偶发失败

docker restart 后 7975 可能因 TIME_WAIT 绑定失败（日志
`Error while binding ... Address already in use`），登录服**仍会**把
客户端引导到 7975 → ws-proxy `Connection refused` → 页面 park。
解法：再 restart 一次。复跑判据见
[2026-10-29-game-server-restart-client-freeze.md](2026-10-29-game-server-restart-client-freeze.md)。

## 陷阱六：人工种入带 infoNumber 的任务使角色卡在选角（2026-10-31 实证）

`Check.img` 中带 `infoNumber` 的任务（20000→20022、20010→20022、
29005→27010、29007→27012）服务端 `addQuestInfo` 会内联追加
`short infoNumber + mapleAsciiString(data)`；客户端 `parse_questlog`
靠 `is_started(qid)+i--` 配对且要求父任务先出现。只种父任务不种子任务
→ 字节漂移 → `PacketError: Unsupported non-empty minigame info` →
SET_FIELD 中断 → 双击角色卡住（e2e 症状 "game UI did not activate"）。

**种子规范**：queststatus 只种无 `infoNumber` 的任务（验证集
20001-20008）；原生任务不受影响。复盘见
[features/changelog/2026-10-31/quest-log-usability-fix.md](../../features/changelog/2026-10-31/quest-log-usability-fix.md)。

## 其他待反馈服务端的问题（未修）

- `CharSelectedHandler` 对缓存未命中走静默 closeSession，建议至少打日志；
- 会话被强杀后重放对话脚本时观测过一次
  `[HACK] Not enough items available of Item:4031008 (0/1)`（道具移除
  与上一会话残留状态竞态），非必现；
- 高台 NPC（如 102020300 原生 1072000 站 fh189 高台）对步行玩家天然
  不可达，建议 plife 机制文档化（本文件即初稿）。
