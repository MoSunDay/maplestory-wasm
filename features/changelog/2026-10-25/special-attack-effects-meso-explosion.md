# 特殊攻击与动效修复：金钱炸弹爆桩、二段跳位移、远程攻击尾部字段

## 背景

审计全部特殊攻击路径后发现三类缺陷：

- **金钱炸弹（4211006）没有爆炸动效**：客户端只收集钱堆 OID 并发送
  `MesoExplosionPacket`；服务端逐堆（每 100ms）广播
  `REMOVE_ITEM_FROM_MAP` mode 4（explode），而客户端把 mode 4 当成
  无动画立即擦除（`IMMEDIATE_ERASE`），还错误地播放了**拾取金币**音效。
  官方行为是在每个被引爆的钱堆位置播放技能 `effect` 帧（金币喷泉，
  0..10 帧，origin 锚在地面）并配爆炸音。
- **施法者身上错挂了爆桩动画**：Skill 通用分支把 4211006 的 `effect`
  当作施法者施放特效（`SingleUseEffect`），一根 348px 高的金币柱从
  侠盗身上竖起；官方该节点只用于爆桩，施法者仅出攻击姿势。
- **二段跳（4111006）是空壳**：`Combat::apply_use_movement` 中
  `case FLASH_JUMP: break;`——按键只播特效并 USE_SKILL，人物不位移。
- **远程玩家攻击尾部字段未读**：服务端 `rangedAttack` 恒定追加
  `int 0`、`magicAttack` 对吐息/Big Bang 5 技能追加 `charge` int，
  客户端从不读取（虽是尾部不影响解析，但协议不完整）。

## 变更

### 金钱炸弹爆桩（`src/client/Gameplay/MapleMap/DropLifecycle/`）

- 新增纯函数模块 `MesoExplosion.h/.cpp`：惰性加载
  `skill/421.img/skill/4211006/effect` 动画与
  `Sound/Skill.img/4211006/Hit` 音效（各缓存一份，避免逐堆重复注册）。
- `RemovalPolicy.h`：mode 4 从 `IMMEDIATE_ERASE` 改映射到新增的
  `Effect::MESO_EXPLOSION`（仍读 character id，与
  `removeItemFromMap(oid, 4, cid)` 布局一致）。
- `MapDrops`：`remove()` 遇 `MESO_EXPLOSION` 先记录钱堆世界坐标，向
  `explosions`（`std::list<Sprite>`，世界坐标锚定）压入金币喷泉动画并
  播放爆炸音，再移除掉落；`update()` 按 TIMESTEP 推进并回收动画，
  新增 `draw_explosions()` 在所有图层之上绘制（伤害数字仍在其上）。
- `Drop::expire` 补 `MESO_EXPLOSION` case（与立即擦除同路径）。
- `RemoveLootHandler`：mode 4 不再播放 `Sound::PICKUP`（仅玩家/宠物
  拾取播放），爆桩音效由 MapDrops 按堆触发——服务端每 100ms 逐堆移除，
  天然形成官方的连爆节奏。
- 本地与远程侠盗共用该路径（服务端对施法者本人也广播 mode 4）。

### 施法者特效归位（`Combat/Skill.cpp`）

- 4211006 在 use-effect 分支前特判为 `NoUseEffect`：金币喷泉只属于
  钱堆，施法者仅保留攻击姿势、残影与命中特效。

### 二段跳位移（`Combat/Combat.{h,cpp}`）

- `apply_use_movement` 的 `FLASH_JUMP` 分支实现 `apply_flash_jump()`：
  面向方向 `hspeed = ±9.0 px/tick`（下落态保留 hspeed，仅反向键减速，
  单次冲量即可携带 ~390px 冲刺，对齐官方距离）；地面起跳附带
  `-jumpforce/1.5` 的低弧线起跳（与爬梯跳同系数），空中施放只冲刺。
- 仿照传送冷却加 `flashjump_spent` 闸门：每个滞空周期只允许一次
  （落地自动复位、换图 `clear()` 复位），`use_move` 在滞空且已用时
  直接拒绝，防止无限空中二段跳。
- 服务端 `MovePlayerHandler` 无速度校验，冲量位移与疾冲（Rush）
  同一运动管线，安全。

### 远程攻击尾部字段（`Net/Handlers/AttackHandlers.cpp`）

- RANGED 广播末尾 `skip(4)`（`rangedAttack` 恒定 `writeInt(0)`）。
- MAGIC 广播对 2121001/2221001/2321001/22121000/22151001 读取尾部
  `charge` int 存入 `AttackResult::charge`（与服务端
  `MagicDamageHandler` 的 `charge != -1` 条件逐一对应；近战路径无此
  字段，故不能复用客户端更宽的 9 技能 charge 白名单）。

## 已知边界（维持现状）

- Big Bang/手雷/木塞堵等的**按住蓄力**（charge 值随按键时长增长）仍
  未实现：攻击输入是边沿触发（`pressed_once`），无按键保持状态机，
  本轮只对齐协议布局（charge=0），不做蓄力玩法。
- 召唤兽（Puppet/鸟/龙）与 THROW_GRENADE 投掷弧线：客户端无对应
  opcode 与实体，属未实现功能而非回归。
- Snipe 服务端固定伤害、ME 客户端兜底 100% 伤害：维持 2026-10-20
  既定设计（白名单 + 上限校验不触发 autoban）。

## 验证

- `bash scripts/docker_build_wasm.sh` 通过（新文件经 GLOB_RECURSE
  入构建）。
- 新增 `tools/verify/meso-explosion-nx.cpp`（12 项检查 0 失败）并登记
  `tools/verify/index.md`：4211006 effect 0..10 连续位图帧、无
  effect0、Hit 音频节点、4111006 施放特效与 20 级无距离字段、mode 4
  策略映射及 0/1/2/5/未知模式回归。
- `drop-removal-payload` 常规 + UBSan 通过（mode 4 断言更新为
  `MESO_EXPLOSION`）。
