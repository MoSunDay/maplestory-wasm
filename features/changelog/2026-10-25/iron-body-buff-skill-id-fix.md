# 圣甲术（Iron Body, 1001003）使用无效修复

## 现象

战士一转圣甲术施放后只有本地起手动作：MP 被扣除，但无 buff 图标、属性面板
WDEF 不变、服务端不结算减伤，表现为"使用无效"。

## 根因（服务端）

`link_repos/MapleStory-Server/src/constants/skills/Swordsman.java` 中技能常量写错：

```java
public static final int IRON_BODY = 1000003;   // 错：1000003 不是任何技能
```

真实圣甲术 ID 为 **1001003**（客户端 `src/client/Character/SkillId.h:42`、
服务端 `handbook/Skill.txt:14` 与 `wz/Skill.wz/100.img.xml` 的节点均一致；
1000003 在 handbook/wz 中不存在）。

失效链路：本仓 wz dump 为裁剪版，1001003 节点只有 `level`/`req`/icon，没有
`effect`/`hit`/`ball`/`skillType`，`SkillFactory.loadFromData` 的启发式判定
得到 `isBuff=false`；本应兜底的白名单 `case Swordsman.IRON_BODY`（
`src/client/SkillFactory.java:201`）因常量错写成 1000003 永不命中 →
`MapleStatEffect.overTime=false` → WDEF（wz 字段 `pdd`，2..40）永远进不了
`statups` → `applyTo` 跳过 `applyBuffEffect`，既不 `registerEffect` 也不回发
GIVE_BUFF(0x0020) 封包。

## 变更

- `Swordsman.IRON_BODY`: `1000003` → `1001003`，仅此一处（全仓该常量只在
  `SkillFactory` 白名单被引用；`1000003` 无其它技能语义引用，命中均为地图 ID）。
- 使用 `posix-compile.sh` 全量重编服务端，`dist/constants/skills/Swordsman.class`
  已更新为 `IRON_BODY = 1001003`（javap -constants 验证）。

## 修复后链路

客户端发 USE_SKILL(0x5B) → `SpecialMoveHandler.applyTo` →
`applyBuffEffect` 回发 `giveBuff(1001003, 75..300s, [WDEF])` 并注册定时 buff →
客户端 `BuffHandler`（双 8 字节 mask，WDEF = second mask `0x200000000`，与
服务端 `MapleBuffStat.WDEF` 一致）→ `Player::give_buff` → `recalc_stats`
WDEF 提升并显示 buff 图标；到期 CANCEL_BUFF 正常移除。

## 验证（协议级端到端）

新增 `scripts/test_accounts/iron_body_buff_proto.mjs`（复用 `maple_wire.mjs`
裸协议栈：登录 → 选角 → SPECIAL_MOVE 施放 1001003 → 断言 GIVE_BUFF 形状），
对 `test1Hero`（build 脚本已全职业线技能满级）实测：

- **修复前**（重启前旧 class 仍驻留）：`CAST 1001003 level=20` 后 3 秒内无
  GIVE_BUFF（收包以 0x1F enableActions 结束，MP 被扣）——复现"使用无效"。
- **修复后**（`docker restart ms-game-server` 重载 dist）：

  ```
  GIVE_BUFF first=0x0 second=0x200000000 value=40 source=1001003 duration=300000ms
  PASS: Iron Body settled — WDEF +40 for 300s
  ```

  second mask 命中 WDEF 位 `0x2_0000_0000`，数值 40（20 级 pdd）、时长
  300 秒（wz `time=300`），与客户端 `Buff.cpp` second_codes 契约一致。

**真实客户端验证**（`tools/verify/maplestory-webui/iron-body-client-flow.mjs`，
test1Hero 绑 I 键施放，buff 图标槽区域像素对比 + 施放前静态自检）：
`PASS: Iron Body buff icon appeared in the client buff strip`——覆盖真客户端
`BuffHandler` 解析（无未知 mask 丢弃）与 `UIBuffList` 图标渲染。

复跑方式：

```bash
# DB: UPDATE accounts SET loggedin=0 WHERE name='test1';
WIRE_USER=test1 PASS=test1 WIRE_WORLD=4 CHAR_SUFFIX=Hero \
  node scripts/test_accounts/iron_body_buff_proto.mjs   # exit 0=通过
```

## 追加处置（同类常量审计）

- `Rogue.NIMBLE_BODY`: `4001000` → `4000000`（handbook/wz 中 Nimble Body
  实为 4000000；原值不存在且常量无任何引用，零行为变更，拆除地雷），
  已随本次 `posix-compile.sh` 一并重编。
- `Bowmaster.ARMOR_CRASH = 3121010`: **保留不动**。v83 神射手技能槽止于
  3121009，3121010 在 handbook/wz 均不存在，其全部引用
  （MapleStatEffect 的 crash 家族、SkillFactory 白名单、MapleMonster）均为
  不可达死分支；改值反而可能给真实技能挂上 SEAL_SKILL debuff 行为。
- 服务端已通过 `docker restart ms-game-server`（unless-stopped）重启生效，
  登录 8484 与 5 个频道端口全部恢复监听。

## 持久化与回归接入

- `link_repos/MapleStory-Server` 为无版本控制 checkout，两处常量修复以补丁
  留档：`scripts/server-patches/0001-fix-iron-body-skill-id-constants.patch`
  （`patch -p1` 可重放，已做反向 dry-run 校验）；机制说明见
  `scripts/server-patches/README.md`。
- 协议探针挂入 `scripts/test_accounts/verify_all.sh` proto 阶段
  （`proto iron-body` 行），随技能回归常驻运行。
- 客户端流程驱动说明收录于 `tools/verify/maplestory-webui/index.md`
  「圣甲术客户端生效流程」。
- 测试残留：test1Hero 位于 910000000、keymap I→1001003（`skill-keymap-prep.py`
  重跑会重建 keymap，届时按 index.md 前置 SQL 重绑）。

## 追记（2026-10-25 22:25）：report 总判定折入

- `verify_all.sh` proto 阶段将 iron-body 探针 rc 持久化至 `/tmp/iron_body_proto.rc`，report 阶段读取并折入总 verdict：rc=0 计 1 settled、非 0 计 1 failed（stderr 出 `PROTO-FAIL` 行），rc 文件缺失时显示 `-` 不计入（与其余缺文件行同口径）。
- `phase_proto` 支持可选 suffix 过滤（`./verify_all.sh proto Hero`），与 visual 阶段对齐，便于压缩共享账号独占窗口。
- 管线实测（`proto Hero` + `report`）：`proto iron-body rc=0 : PASS: Iron Body settled — WDEF +40 for 300s`，汇总表出现 `iron-body 1/1`，TOTAL 112 settled。当时 verdict=FAIL 的 4 项均为历史遗留（Hero `Panic : Axe`/`Coma : Axe` 需配斧、Pala 10/24 旧 visual 数据），非本次回归。
