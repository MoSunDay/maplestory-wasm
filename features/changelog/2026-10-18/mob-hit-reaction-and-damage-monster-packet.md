# 怪物受击动效修复 + 补齐 DAMAGE_MONSTER(0xF6) 封包

## Context

1. 怪物被攻击时无受击动效：hit1 动画已正确加载、`Mob::apply_damage`
   也设置了 HIT 姿态，但 `Mob::update` 中仇恨 AI `update_aggro_action()`
   在下一帧就用 MOVE/STAND/JUMP 覆盖 HIT 姿态；且 aggro 分支提前
   return 使 `case HIT` 的退出逻辑（counter > 200 → next_move）不可达，
   hit1 实际只显示不到 1 帧。
2. 客户端未注册 `DAMAGE_MONSTER(0xF6=246)`：服务端在毒 DoT、
   PowerGuard 反伤、Heal 打不死系、mob 互殴、反应堆伤害时广播该封包，
   客户端静默丢弃（Console 报 Unhandled packet, Opcode: 246），
   这些伤害既无数字也无受击表现。

## Change Summary

- `src/client/Gameplay/MapleMap/Mob.cpp`（2 行）：
  - `update_aggro_action()` 调用条件加 `stance != HIT`，受击期间
    姿态不被 AI 覆盖；
  - aggro 提前返回分支加 `stance != HIT`，HIT 姿态按 counter > 200
    正常退出（约 30 帧）后 `next_move()` 恢复 AI。控制端的 HIT 状态
    经 MoveMobPacket 中继，其他客户端同步显示。
- `src/client/Net/PacketSwitch.cpp`：枚举加 `DAMAGE_MONSTER = 246`
  并注册 handler。
- `src/client/Net/Handlers/MapObjectHandlers.{h,cpp}`：新增
  `DamageMonsterHandler`，解析 oid + type(byte) + damage(int) +
  curhp(int) + maxhp(int)（截断防护 17 字节）；伤害为正时转发
  Combat 显示，curhp/maxhp 有效时同步更新 mob HP 条。
- `src/client/Gameplay/Combat/Combat.{h,cpp}`：新增公开方法
  `show_damage(oid, damage)`——在 mob 身位显示白色伤害数字并复用
  `MapMobs::apply_damage`（受击音效/击退，方向按本地玩家位置估算）。
- `docs/ms-network-protocol.md`：修正 0x00F6 字段表（原漏
  curHp/maxHp、误标尾部 byte）。

## Verification

- docker wasm-builder 完整重编译通过（Mob.cpp、PacketSwitch.cpp、
  MapObjectHandlers.cpp、Combat.cpp 及链接）。
- 待游戏内回归：普通攻击 hit1 完整播放约 30 帧；毒/反伤等触发
  0xF6 时 mob 头顶出现伤害数字，Console 不再报 Opcode: 246。
