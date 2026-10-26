# 未学习技能攻击包打崩 parseDamage（斧系 2 fail 真根因）修复

## 现象与误诊纠正

test1Hero 的 `1111004 Panic : Axe` / `1111006 Coma: Axe` 长期 not settled，此前 changelog 记为「需配斧」基线。字节级取证推翻该假设：服务端该路径不检查武器，真正死因是**角色没学这两个技能**。

## 根因链

1. 斧系技能 2026-10-20 才移植进服务端 wz（`features/changelog/2026-10-20/final-attack-combo-orbs-and-damage-effects.md`），test1Hero 创建早于该日，`skills` 表无行。
2. 探针发出携带该 skill id 的 CLOSE_ATTACK → `AbstractDealDamageHandler.parseDamage:578` `chr.getSkillLevel()` 返回 0 → `:661` `skill.getEffect(0)` → `Skill.java:47 effects.get(-1)` → `IndexOutOfBoundsException (Index -1, length 30=技能 max level)`。
3. 异常被 `MapleServerHandler.messageReceived` 吞掉（只写 dump 不回包不踢线）→ 客户端零响应。dump 文件 `logs/2026-10-26/error/game/packethandler/...CloseRangeDamageHandler.txt` 内 5 条堆栈的封包字节与探针 buildAttack 逐字节吻合（damage=2、speed=44、1 mob×6/8 hits）。
4. NL 静默丢包同症状不同因（等级门槛见 `proto-self-heal.md`），本修与彼修互补。

## 修复（两层）

| 层 | 手段 | 文件 |
|----|------|------|
| 服务端 | `parseDamage` 中 `skilllevel<=0` 时把 `ret.skill` 降级为 0（普攻路径），风格对齐同块 PQ 技能 fallback；`Skill.getEffect` 本身不动（返回 null 会把 IOOBE 换成 NPE，调用面 5 处未全审） | `scripts/server-patches/0004-parse-damage-unlearned-skill-guard.patch`，已编译（dist 23:13:44）并随容器重启生效 |
| 测试基建 | `grant_wide_attacks.py --sync-job-line`：按 wz job line 幂等补发缺失 skills 行（只插缺行、已有行不动，apply 后回读校验）；挂进 `verify_all.sh` phase_setup | `scripts/test_accounts/grant_wide_attacks.py`、`verify_all.sh` |

## 验证

- 补发 dry-run 精确命中：仅 test1Hero 缺 `1111004@30,1111006@30`（其余 11 角色全量）。
- 重启 + 补发后 Hero 探针：斧系双技能经**真实连击终结技路径**结算（`[combo-buff-sent,orb-hit settled] 0x1F,0xFA`），`9 fired, 1 skipped, 0 not settled`。
- dump 计数基线 5 → 验证后仍 5（零新增崩溃）。
- 全量 report：**TOTAL proto 114 settled / 0 failed**（历史首次 proto 侧全绿；visual 侧剩 Pala 10/24 陈旧数据 2 fail）。

## 已知边界与残留

- SPECIAL_MOVE（USE_SKILL）路径依赖「level 字节须等于真实等级」前置校验，未走 getEffect(0)，未改；`Skill.getEffect` 的越界暴露面其余 4 个调用点（Close/Magic/Ranged :165/:71/:215 等）经 parseDamage 守卫后不可达 crafted-skilllevel-0，但 level>max 的 crafted USE_SKILL 未审计——后续可在 Skill.getEffect 收口钳位。
- Pala visual 2 fail 待空闲窗口 `./verify_all.sh visual Pala` 刷新。
- 并发会话曾在分钟级窗口覆写本补丁的源文件两次（竞态写实录），依赖 patch 0004 作为持久化锚点。
