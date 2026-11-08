# 同图幽灵怪/画面不同步/表情卡闭眼修复

## 症状（Context）

- 偶发**画面不同步**：同图怪物/玩家状态与服务器不一致（多出服务器不存在的
  怪、或击杀后怪不消失），重新进入地图后恢复正常。
- 队友偶发**一直闭眼**：远端玩家表情停留在 BLINK 闭眼帧不再推进，直到下一次
  表情/动作切换。
- 偶发**打不死、无法计算伤害的怪**：怪物被击杀后仍在场，攻击命中却始终不死、
  不掉血不结算（客户端幽灵怪 oid 服务端已注销）。

## 根因

1. 客户端 `src/client/Character/Look/CharLook.cpp` `update()` 三处动画计时为
   `uint16_t` 无符号直减（stance 循环 `delta = delay - stelapsed`、action 分支
   同式、表情循环 `expdelta = expdelay - expelapsed`）：表情/动作帧已播放余量
   （elapsed）超过新一帧延迟（换装、表情切换会重置帧延迟）时结果回绕成巨大
   `uint16_t`，`timestep >= delta` 永假，动画**永久冻结**——表情卡死在闭眼帧。
2. `src/client/Gameplay/MapleMap/MapMobs.cpp` `clear()` 只清已实例化 `mobs`，
   **不清待实例化 `spawns` 队列**：切图瞬间上一张地图延迟到达的 `SPAWN_MOB`
   在新图被实例化成服务器并不存在的怪（跨图幽灵怪）。
3. `MapMobs::remove()` 只查已实例化 map、不查 `spawns` 队列：同一网络批次
   `SPAWN`+`KILL` 一起到达时，KILL 在实例化之前被丢弃，随后队列又把怪生成
   出来——形成打不死的怪。
4. `src/client/Gameplay/MapleMap/MobPresentation.cpp` `Mob::respawn` 只重锚
   position/stance/foothold，**不复位 `dying/dead/fading`**：对已存在 oid 的
   重复 `SPAWN`（服务器复活）拿到的是仍处死亡/淡出状态的"复活体"，不可选中、
   不可攻击。
5. linked server `MapleMap.killMonster`（else 分支）在 `removeKilledMonsterObject`
   通过后先执行 `killBy` 经验/掉落结算与 `dispatchMonsterKilled`，最后才广播
   `KILL_MONSTER`：该结算窗口内任一异常被吞则击杀**永不广播且无补救**——
   客户端幽灵怪的 oid 在服务端已注销，后续攻击被服务端静默丢弃，表现为打不死、
   无法计算伤害。

## 修复（Change Summary）

- **A. CharLook 动画计时钳制**：三处改为饱和减法——stance 循环与 action 分支
  `uint16_t delta = stelapsed < delay ? delay - stelapsed : 0;`、表情循环
  `uint16_t expdelta = expelapsed < expdelay ? expdelay - expelapsed : 0;`，
  消除无符号下溢，动画不再永久冻结。
- **B. MapMobs spawns 队列生命周期**：`remove(oid, animation)` 开头先用
  `std::queue<MobSpawn> retained;` 过滤掉同 oid 的待实例化条目再
  `spawns = std::move(retained);`（同批次 SPAWN+KILL 不再丢 KILL）；
  `clear()` 在 `mobs.clear();` 之外补 `spawns = std::queue<MobSpawn>{};`
  （切图不再实例化旧图残留 SPAWN）。
- **C. Mob::respawn 复位死亡状态**：末尾补 `dying = false; dead = false;
  fading = false; hppercent = 0;`，重复 SPAWN 的复活体完全复活、可选中。
- **D. 服务端 killMonster 广播提前**：两分支的
  `broadcastMessage(MaplePacketCreator.killMonster(monster.getObjectId(), animation), monster.getPosition());`
  移到 `if(removeKilledMonsterObject(monster)) {` 之后立即执行——chr==null 分支
  位于 `dispatchMonsterKilled` 之前；else 分支删除原方法末尾那行、只在开头保留
  一处。此后结算（`killBy`/`dropFromMonster`/`dispatchMonsterKilled`）途中异常
  也已向全图广播死亡，客户端不再滞留服务端已注销的幽灵怪。

补丁留档：`scripts/server-patches/0017-killmonster-broadcast-before-settlement.patch`
（重放：`cd link_repos/MapleStory-Server && patch -p1 < 本文件`）。

## Impact Surface

- 客户端 `CharLook` 动画循环（动作帧/表情帧计时推进）
- 客户端 `MapMobs` 生命周期（`spawns` 待实例化队列的 remove/clear 维护）
- 客户端 `MobPresentation` 复活复位（`Mob::respawn` 死亡状态清零）
- linked server `killMonster` 击杀广播时序（广播先于经验/掉落结算）

## Validation

- `./scripts/build_wasm.sh` 构建通过。
- `bash posix-compile.sh` 后 `docker restart ms-game-server`，服务端正常在线。
- `node tools/verify/mob-ghost-sync-source-contract.mjs` PASS——CharLook 三处
  钳制、MapMobs 队列过滤/清空、respawn 四项复位、killMonster 每分支恰一次且
  先于结算的广播顺序契约成立（由父 agent 统一执行）。

## Related Docs

- [带宠物远端角色可见性与怪物位置/动作同步](../2026-11-06/remote-visibility-mob-sync.md)（重复 SPAWN 重锚与幽灵怪观察的前作）
- [同图远端表情与攻击动作同步](../2026-11-06/remote-action-sync.md)（表情链路背景）
- [同图怪物血条广播与死亡贡献者经验](../2026-11-03/mob-hp-broadcast-and-exp-share.md)
