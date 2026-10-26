# 组队任务单人模式：NPC 队友补位协同

## 背景

本服 13 个组队任务（PQ）均要求 3-6 人组队才能进入，单人玩家无法体验
副本内容。目标：单人报名时由 5 名 NPC 队友（特工 M/C/E/S/O，
NPC ID 9000061-9000065）补位协同进行副本，包括跟随走位、平台站位、
计数达标与协同攻击。

NPC 队友选型依据：有客户端与服务端 wz 造型（可正常渲染/同步）、
无 `scripts/npc` 交互脚本（不会抢占玩家对话）、ID 互异
（destroyNPC 每图每 ID 限一个）。

## 服务端契约（link_repos/MapleStory-Server）

- `src/constants/ServerConstants.java`：`USE_PARTYQUEST_SOLO_MODE` 总开关。
- `src/server/partyquest/solo/`（新增三件套，纯函数式风格）：
  - `SoloPqRoster.java`：NPC 队友花名册（ID/名字/坐标展开规则）。
  - `SoloPqTeammates.java`：召唤/清除/查询（±120px 展开、warpEventTeam
    跟随 +40px 间隔、dispose/exitPlayer 清理）。
  - `SoloPqAssist.java`：5s 周期协同攻击，伤害 `max(1, maxHp/40)` 走
    `MapleMap.damageMonster` 管线防 NPE。
- `EventInstanceManager.java`：6 个契约方法——`isSoloPqFillActive`、
  `isSoloTeamLackingNow`（替换全部 `isEventTeamLackingNow`）、
  `setupSoloTeammates`（afterSetup 调用，读 setup 里 `minPlayers` 属性；
  CWKPQ 在 changedMap 惰性补挂）、`stationSoloTeammates(mapId,[areaIdx…])`、
  `countSoloTeammatesInArea`、`getSoloTeammateNames`。
- `EventManager.java`：`isSoloPqModeEnabled()`；`getLobbyRange` 修复
  standalone-Nashorn `ScriptObjectMirror` 强转 ClassCastException
  （改 instanceof List/Map 稳健转换，附带修复
  `convertToIntegerArray` 的 Integer→Double CCE 与 `getIntProperty`
  null→0）。

## 脚本改造（31 文件）

- 13 个 event 脚本：setup 写入 `minPlayers` 属性、afterSetup 调
  `setupSoloTeammates`、判空改 `isSoloTeamLackingNow`、`getEligibleParty`
  单人放宽（minOk=1）。
- 13 个招募 NPC：无队伍或人数不足时 `sendYesNo`「单人模式：NPC 队友将
  与你协同进行副本……」→ `em.startInstance(cm.getPlayer())`；Amoria 单人
  绕过已婚限制；CWKPQ 远征 size<=1 放行。
- 机制脚本：9020001（KPQ 平台站位+计数）、2013001（Orbis 封印间 3 平台）、
  9201044（Amoria 5 平台自适应站位）、9201110（CWKPQ glpq5 自动补
  4 圣物）。
- `9010000.js`（FM 玩法向导）：13 个标签改为真实等级段+单人模式说明，
  修正「玩具城A路」→「玛加提亚A路」笔误。

## 连带缺陷修复（e2e 发现）

1. 12 个招募 NPC 调用 `isRecvPartySearchInviteEnabled` /
   `toggleRecvPartySearchInvite`——MapleCharacter 无此死接口（本服无
   组队搜索），Nashorn 抛 TypeError 对话框弹不出：改为静态文案+
   「本服务器暂未开放组队搜索功能。」。
2. `EventManager.getLobbyRange` Nashorn 强转崩溃（见上）。
3. `wz/Map.wz/Map/Map2/251010404.img.xml`：Pirate 招募 NPC 2094000
   落点悬空 197px 不可达，吸附 fh=6（y=242/cy=242）。

## 验证

- `posix-compile` 通过；`node tools/verify/pq-solo-server-contract.mjs`
  PASS（结构契约，已登记 tools/verify/index.md）。
- 浏览器全链路见同日
  [fm-pq-teleport-and-solo-verification.md](fm-pq-teleport-and-solo-verification.md)。
