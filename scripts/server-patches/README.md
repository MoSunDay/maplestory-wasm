# 服务端源码补丁（server-patches）

`link_repos/MapleStory-Server` 是本地 linked checkout（目录内无 `.git`，
父仓也不跟踪其内容）。凡是对服务端 **Java 源码** 的修改都无法随父仓
提交持久化——重新 checkout / 同步该目录后修改即丢失。

因此服务端源码改动必须：

1. 在本目录以编号补丁形式留档（`NNNN-<slug>.patch`，统一 diff，可在
   checkout 根目录 `patch -p1` 重放）；
2. 补丁头部注释写明背景、应用与验证命令；
3. 对应的父仓 changelog 引用补丁文件路径；
4. 若改动影响运行产物（`dist/`），重放后需 `bash posix-compile.sh`
   并重启容器 `ms-game-server`（`docker restart ms-game-server`，
   unless-stopped 策略，登录口 8484）。

## 编号规则与消歧

历史上本目录曾出现**两个 0006**：`0006-cash-shop-exit-save.patch`
与 `0006-storage-30-slots-and-type-buckets.patch`。为不破坏既有
changelog / 复盘文档对旧文件名的重放引用，**有意不重编号**，两个
0006 均原样保留。此后规则：

- 新补丁编号 = 当前最大编号 + 1 顺延（当前最大 0016）；
- slug 必须唯一，禁止复用；
- 同一编号出现不同 slug，一律视为历史遗留，不表示覆盖或替代关系。

## 已知差异（相对上游 HeavenMS checkout）

| 补丁 | 内容 | 状态 |
|------|------|------|
| [0001-fix-iron-body-skill-id-constants.patch](0001-fix-iron-body-skill-id-constants.patch) | 圣甲术 `Swordsman.IRON_BODY` 1000003→1001003；`Rogue.NIMBLE_BODY` 4001000→4000000（未引用常量对齐） | 已应用于当前 checkout 并重编入 dist（2026-10-25） |
| [0002-storage-slots-byte-width-mismatch.patch](0002-storage-slots-byte-width-mismatch.patch) | STORAGE 系列封包（0x16/0x13/0x0D/0x09/0x0F）`writeShort(slots)`→`write(slots)`，修复与客户端 byte 解析的 1 字节错位 | 已应用于当前 checkout 并重编入 dist（2026-10-26） |
| [0003-deterministic-idle-session-reaping.patch](0003-deterministic-idle-session-reaping.patch) | `manageIdleSessions` 移除无条件 `idleSessions.clear()`，改为按 15s 阈值 testPing 后移除/断连移除/未达阈值保留，空闲会话 ~45-75s 确定性收割 | 已应用于当前 checkout 并重编入 dist（2026-10-26） |
| [0004-parse-damage-unlearned-skill-guard.patch](0004-parse-damage-unlearned-skill-guard.patch) | `AttackInfo.parseDamage` 未学习技能（skilllevel<=0）降级为普攻，避免 `effects.get(-1)` 越界吞异常 | 已应用于当前 checkout 并重编入 dist（2026-10-26） |
| [0005-cash-cover-equips.patch](0005-cash-cover-equips.patch) | 现金商城装备纯装饰化：取出落 CASH 栏，穿戴为 EQUIPPED -100-x 外观覆盖（`equipCashCover`/`unequipCashCover`，跳过 canWearEquipment；让位/穿上两条公告拆包发送，避免共享 movement 字节互相覆盖），ModifyInventory invType 覆盖构造器（CASH 栏装备公告 type=5），`recalcEquipStats` 跳过 <=-100 槽位，ItemFactory 4 处判定让 CASH 栏 Equip 走装备持久化，addInventoryInfo 四段 byte 位置 + equippedCash 原样发 101+ | 已应用于当前 checkout 并重编入 dist（2026-10-29） |
| [0006-storage-30-slots-and-type-buckets.patch](0006-storage-30-slots-and-type-buckets.patch) | 仓库 30 格上限（新建默认 30、`gainSlots` 上限 48→30）；`sendStorage`/`arrangeItems` 的 `typeItems` 改为按类型过滤桶，修复多类型混存时“页签内索引→全量下标”映射错位导致的取错/取不出 | 已应用于当前 checkout 并重编入 dist（2026-11-01） |
| [0006-cash-shop-exit-save.patch](0006-cash-shop-exit-save.patch) | `ChangeMapHandler` CS 退出分支 `open(false)` 后补 `chr.saveCharToDB(false)`：换线重连前把商城内 locker take/put/buy、CASH 余额、wishlist 落盘，修复退出商城丢档（进入侧 `EnterCashShopHandler` 本就有 save，退出侧原本缺失） | 已应用于当前 checkout 并重编入 dist（2026-10-30） |
| [0007-cash-equips-permanent.patch](0007-cash-equips-permanent.patch) | 现金装备改永久：`CashItem.toItem()` 提取 `isEquip` 局部变量，`!isEquip && EXPIRING_ITEMS` 才盖到期戳（EQUIP 类购买保持 expiration -1，WZ Commodity Period=90 不再生效）；`expirationTask()` 对 CASH 栏遗留 Equip 带 !=-1 到期戳的迁移为 -1 落库而非到期删除 | 已应用于当前 checkout 并重编入 dist（2026-11-01） |
| [0008-login-channel-bind-retry-and-world-cache-refresh.patch](0008-login-channel-bind-retry-and-world-cache-refresh.patch) | 登录口 8484 与频道口 bind 失败改退避重试（1/2/4/8/15s×6）后快速失败（exit(1)/RuntimeException），杜绝容器重启竞态下半活进程（频道全在、登录或单 world 死端口）；`Server.getCharacterWorldRefreshed` 按 DB 刷新 worldChars 缓存并在 CharSelectedHandler 使用，修复运行中直改 characters.world 后 SERVER_IP 仍发旧 world 端口 | 已应用于当前 checkout 并重编入 dist（2026-11-01） |
| [0009-ranking-worker-rank-reserved-word.patch](0009-ranking-worker-rank-reserved-word.patch) | `RankingLoginWorker.updateRanking` 总榜分支（job==-1）UPDATE 列名加反引号（rank/rankMove；jobRank 分支、SELECT c.rank、resetMoveRank 均不动），修复 MySQL 8.0+ 保留字 RANK 导致每小时整点 MySQLSyntaxErrorException 事务回滚、rank/rankMove 从不落库 | 已应用于当前 checkout 并重编入 dist（2026-11-02） |
| [0010-idle-reap-preserve-disconnect-save.patch](0010-idle-reap-preserve-disconnect-save.patch) | `MapleClient.testPing` 空闲收割主体改为单一 `session.close(true)`：删除收割时提前 `closeSession`/`updateLoginState`/`removeAttribute(CLIENT_KEY)` 的手工清理（异步 sessionClosed 因此拿到 null client 跳过 disconnect → 幽灵会话：断线角色不落库、残留地图），改由既有 `sessionClosed → closeMapleSession → disconnect → disconnectInternal` 链路接管（游戏内玩家 saveCharToDB/removePlayer，登录态会话落入等价清理分支）；不动锁序 | 已应用于当前 checkout 并重编入 dist（2026-11-02） |
| [0011-charselected-cache-miss-logging.patch](0011-charselected-cache-miss-logging.patch) | `CharSelectedHandler` 缓存未命中（`haveCharacterEntry` 查 accountChars 无该角色）静默 `closeSession` 前补一条 WARN（`System.err.println("[WARN] ...")`，同层 Server.java 既有日志格式），含 accountName/charId/world 与原因 "character cache miss"，纯日志零行为变更，消除"双击角色无声掉线零线索" | 已应用于当前 checkout 并重编入 dist（2026-11-02） |
| [0012-mob-hp-broadcast-and-dead-damager-exp.patch](0012-mob-hp-broadcast-and-dead-damager-exp.patch) | `MapleMonster.broadcastMobHpBar` 非 Boss 分支 SHOW_MONSTER_HP 改为遍历 `map.getCharacters()` 全图广播（保留 `packet.clone()` 防 crypto，Boss 分支 `broadcastBossHpMessage` 本就全图），修复非组队同图玩家怪物血条不同步；`giveExpToCharacter` 去掉 `if (attacker.isAlive())` 门槛方法体平铺，打怪途中死亡的伤害贡献者照常结算伤害经验（distributeExperience 伤害 80%+击杀 20% 加成、击杀者队伍全额经验定制与防 leech 等级门槛均不动） | 已应用于当前 checkout 并重编入 dist（2026-11-03） |
| [0013-mob-controller-vacuum-and-activity-range.patch](0013-mob-controller-vacuum-and-activity-range.patch) | `MapleMonster.aggroMoveLifeUpdate` 增加控制权真空接管分支：怪物无有效控制器（控制器离图/掉线到 `releaseControlledMonsters` 完成前）时不再返回 null 丢弃上报者 MoveLife，改为让活着且在同图的上报者经 `aggroSwitchController` 立即接管并返回正常 aggro 结果（交接沿用 `aggroUpdateLock` 锁风格、保留当前仇恨状态 不与 `aggroAutoAggroUpdate` 重复触发），修复同图怪物冻结画面不同步；`MoveLifeHandler.inRangeInclusive` 恒真表达式 `!(pVal<pMin)\|\|(pVal>pMax)` 修为 `!(pVal<pMin\|\|pVal>pMax)`，rawActivity≥42 不再 isAttack/isSkill 同时为 true（私有方法仅本 handler :78-79 两处调用） | 已应用于当前 checkout 并重编入 dist（2026-11-03） |

| [0015-idle-reaper-pong-baseline.patch](0015-idle-reaper-pong-baseline.patch) | `manageIdleSessions` 空闲收割基准修正：`testPing(timeThen)`（检查 tick 时刻-15s）改为 `testPing(mc.getValue())`（PING 下发/登记时刻），语义变为"本次 ping 后始终无 PONG 才关闭"；原基准在 10s tick 落后于登记 15~25s 时会把 ping 后数百毫秒内正常应答的 PONG 判为过期，健康登录连接 ~45-55s 空闲（如角色创建素材等待）即被错杀，并连带客户端阻塞 alert 楔死 UI 验收；≥15000ms 等待窗与不响应连接的收割节奏（0003 设计意图）不变 | 已应用于当前 checkout 并重编入 dist（2026-11-06） |

| [0016-beauty-npc-full-style-pools.patch](0016-beauty-npc-full-style-pools.patch) | `scripts/npc` 41 个美容/美发脚本（47 个已知美容 NPC 中含样式数组者，含 GM 9900000.js）的 `mhair*/fhair*/mface*/fface*` 及 `hair/fhair/face/fface` 样式数组由按城市硬编码小池改为 wz 全集：男发型 97 / 女发型 96（基础款×10 尾数 0）、男脸型 32 / 女脸型 29 款代表 ID（脚本自行叠加发色 hair%10 与瞳色）；真源 = `wz/Character.wz/{Hair,Face}` 素材存在 ∩ `wz/String.wz/Eqp.img.xml` 有 name；skin/价格/对话/券逻辑不动；生成与校验工具在父仓 `scripts/beauty-styles/`；纯 JS 改动无需重编 dist，重启 `ms-game-server` 生效 | 已应用于当前 checkout（2026-11-08） |

注意：`Bowmaster.ARMOR_CRASH = 3121010` 经审计为不可达死代码
（v83 无此技能槽位），**有意保留**，不做补丁。
