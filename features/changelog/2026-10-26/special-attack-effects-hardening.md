# 特殊攻击动效评审加固：二段跳方向、协议注释引用、金钱炸弹动画上限

## 背景

对 2026-10-25 特殊攻击与动效修复的代码评审提出三项**非阻塞**加固，
均不改变正常玩法行为：

- **二段跳方向仅跟随朝向**：`apply_flash_jump()` 直接按 `getflip()`
  取冲刺方向，空中转身后按键会冲向背后；同文件的传送实现
  （`find_teleport_target_horizontal()`）已是"按住方向键优先、翻转兜底"。
- **charge 白名单注释缺服务端锚点**：`has_broadcast_charge()` 只笼统
  提到 MagicDamageHandler.java，未落到具体常量名，wire format 变更时
  难以对齐。
- **金钱炸弹动画无上限**：`MapDrops::remove()` 对每条 mode 4（explode）
  广播无条件 `explosions.emplace_back(...)`，异常服务端批量广播时
  动画列表可无限膨胀。

## 变更

### 二段跳方向（`src/client/Gameplay/Combat/Combat.cpp`）

- `apply_flash_jump()` 冲刺方向改为按住的方向键优先（LEFT→-1、
  RIGHT→+1），无按键时回落到 `getflip()` 朝向，与
  `find_teleport_target_horizontal()` 的既有判向逐字对齐；
  `hspeed`（double）经 `int16_t direction * FLASH_JUMP_HSPEED` 隐式
  提升为 double 赋值，速度与冲量管线不变。

### 协议注释（`src/client/Net/Handlers/AttackHandlers.cpp`）

- `has_broadcast_charge()` 注释补齐服务端锚点：
  `MagicDamageHandler.java` 以 `FPArchMage.BIG_BANG` /
  `ILArchMage.BIG_BANG` / `Bishop.BIG_BANG` / `Evan.ICE_BREATH` /
  `Evan.FIRE_BREATH`（`src/constants/skills/`）判定透传
  `attack.charge`，`MaplePacketCreator.magicAttack()` 仅在
  `charge != -1` 时追加 `writeInt(charge)`；各 case 行内注释同步
  改用常量名（顺带纠正 22121000/22151001 的吐息名称与服务端
  ICE/FIRE 常量相反的旧笔误）。仅注释，无代码变更。

### 金钱炸弹动画上限（`src/client/Gameplay/MapleMap/MapDrops.cpp`）

- 文件顶部匿名命名空间新增
  `constexpr size_t MAX_EXPLOSION_SPRITES = 200`：动画 ~0.9s 自回收，
  200 对真实 ME 钱堆绰绰有余，同时挡住异常服务端批量 mode 4 广播。
- `remove()` 的 `MESO_EXPLOSION` 分支仅在未达上限时压入金币喷泉
  精灵；爆炸音效无论是否达限照常播放，掉落本体照常移除。

## 已知边界（维持现状）

- 达到上限后多余的爆炸仅无动画（有声音、掉落消失），客户端无从
  区分"服务端异常"与"超大 ME 堆"，不做告警或断连。
- 二段跳/传送的按键优先语义只依赖本地按住状态，与服务端位移
  校验（本就无速度校验）无交互。

## 验证

- `bash scripts/docker_build_wasm.sh` 通过。
