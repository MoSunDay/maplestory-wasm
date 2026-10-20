# 攻击封包对齐（charge/远程附加字节/影子伴侣）+ 攻击白名单缺口（Snipe/金钱爆炸）+ 疾风步时长

## 背景

全面审计所有攻击场景后发现三类问题：

- **封包字段错位**：服务端 `AbstractDealDamageHandler.parseDamage`
  （link_repos 只读参照）对 GRENADE 5201002 / CORKSCREW_BLOW 5101004 /
  三系 Big Bang / Evan 吐息等无条件在 skillid 后读 4 字节 charge，对
  HURRICANE 3121004 / PIERCING_ARROW 3221001 / RAPID_FIRE 5221004 /
  WindArcher 13111002 在远程头 skip(7) 后多读 4 字节；客户端从不写 →
  后续全部字段错位 → OID 乱读 → 零伤害/封包被弃。另外服务端每怪尾部
  skip(4) 的条件是恒真式（`skill != A || skill != B` 链），客户端却对
  5221004 少写这 4 字节。
- **影子伴侣（4111002 SHADOWPARTNER）行丢失**：服务端按 numDamage 读全部
  伤害行、仅对后半行（j >= numDamage/2）把校验上限减半并据此显示暴击，
  实际伤害完全采用客户端值 → 协议要求客户端翻倍发送（真实行在前、影子行
  减半在后）；客户端未翻倍 → 影子行完全丢失。
- **白名单缺口**：Snipe 3221007（ATTACK|RANGED）、Meso Explosion 4211006
  （专用封包布局）未加白，加白后还有数据层坑（见下）。FP/IL Big Bang
  2121001/2221001 经双源核验在客户端 Skill.nx 与服务端 wz 均不存在
  （数据集死条目），不加白（与服务端 parseDamage 常量对应的其他数据集版本
  不冲突——charge 列表仍保留该 id，死数据无法触发）。

## 变更

### 封包对齐（src/client/Net/Packets/AttackAndSkillPackets.h）

- AttackPacket：numAttackedAndDamage 写入钳制半字节（mobcount/hitcount
  ≤15）；charge 改为 `charge > 0 || is_charge_skill(skill)`（列表
  2121001/2221001/2321001/5201002/5101004/15101003/14111006/22121000/22151001，
  与服务端常量集一致，含 Evan/骑士团 id）；远程块 skip(7) 后对
  is_ranged_extra_skill（3121004/3221001/5221004/13111002）补 skip(4)；
  每怪尾部 skip(4) 改为无条件（删除 5221004 例外）。
- 新增 MesoExplosionPacket（CLOSE_ATTACK）：严格按服务端 ME 分支布局——
  纯钱堆（damagelines 空）时 byte 0 + skip(10) + 钱数 + [oid int + skip(1)]*；
  有钱怪时 (mobs<<4)|hits + skip(6) + 每怪 [oid + skip(12) + 行数 +
  伤害 int* + skip(4)] + **尾部先 skip(4) 占位 oid**（服务端循环最后一轮先
  readInt 再读钱表）+ 钱表。

### 影子伴侣翻倍（src/client/Gameplay/Combat/Combat.cpp）

- 匿名命名空间自由函数 `apply_shadow_partner(player, result)`：has_buff
  (Buffstat::SHADOWPARTNER) 时对每怪伤害行追加减半副本（0 伤害保持 0，
  其余 max(d/2,1)，暴击标志沿用），hitcount 翻倍（上限 15 保护：
  hitcount*2>15 不翻倍）；MESO_EXPLOSION 例外。在 apply_move 与
  perform_final_attack 两处 send_attack 之后调用。

### Meso Explosion 攻击路径

- `MapDrops::collect_mesoids(rect)`（MapDrops.h/.cpp）：遍历 drops，
  is_meso() && is_lootable() && 矩形包含 → 收集 oid。
- `Combat` 构造函数增加 `MapDrops& drops`（Combat.h/.cpp、Stage.cpp 接线；
  Stage.h 成员 drops(183) 声明在 combat(187) 之前，初始化顺序安全）。
- apply_move 中 4211006 分支：用 world_range(attack)（复刻 MapMobs::
  send_attack 的翻转矩形）收集攻击范围内钱堆（上限 100 防溢出），有怪或
  有钱才发 MesoExplosionPacket；extract_effects/apply_use_movement 照常，
  跳过 apply_result_movement/AttackPacket/schedule_final_attack。
- `Skill::apply_stats`（Skill.cpp）：attackcount/mobcount/bulletcount 赋值处
  std::min(·, 15)——本数据集 ME 中级 attackCount 达 20，超出封包半字节。

### 白名单与数据兜底（src/client/Character/SkillId.h、src/client/Data/SkillData.cpp）

- SkillId 新增 `SNIPE = 3221007`（Marksman 段）；flags_of 新增
  `{SNIPE, ATTACK|RANGED}`、`{MESO_EXPLOSION, ATTACK}`（复用既有常量
  4211006）。
- SkillData 构造：无任何伤害源（damage/mad/fixdamage 全无）且 id ∈ {SNIPE,
  MESO_EXPLOSION} 时 damage 兜底 1.0（基础武器伤害）——两技能 NX 均无
  damage/mad/fixdamage/x；Snipe 服务端忽略客户端值改发固定 195000+rand(5000)
  （crit 显示走负数编码），ME 服务端按自己 XML damage=110..% 做上限校验、
  采用客户端数值，兜底后客户端随机 ≤ 服务端上限不触发 autoban。
- `reqweapon` 守卫：`if (nl::node wsrc = src["weapon"])` 才 by_value(100+值)，
  否则 Weapon::NONE——消除数据集无 weapon 节点时的控制台警告刷屏（远程三
  技能确有 weapon 节点 145/146/149，行为不变）。

### 疾风步（0001002）时长

- `link_repos/MapleStory-Server/wz/Skill.wz/000.img.xml`：三级 `time`
  4/8/12 → 7200（秒）。服务端 MapleStatEffect 对技能 time ×1000 转毫秒，
  GIVE_BUFF 下发 2 小时时长；客户端不读该节点（仅落入从未用于 buff 时长的
  stats.recoverytime），cooltime 3600 不变（1 小时后可刷新）。已重启服务端
  生效。

## 验证

- `bash scripts/docker_build_wasm.sh` 通过；
  `bash scripts/game_server_restart.sh` 6/6 端口。
- 新增 `tools/verify/attack-gap-skills-nx.cpp`（28 项检查 0 失败）并登记
  `tools/verify/index.md`：Snipe 30 级且各级无 damage/mad/fixdamage/x、
  ME mobCount 上限 10 / attackCount 上限 20、主教 Big Bang 2321001 level30
  mad>0、FP/IL Big Bang 双源缺失回归、charge/远程附加字节技能 level 数据
  在位、疾风步 3 级 mpCon 4/7/10、weapon 节点存在性两态。
- 回归：final-attack-combo-nx PASSED、second-job-attack-nx PASSED、
  foreign-buff-mask exit 0。
