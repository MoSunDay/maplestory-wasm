# 多玩家同图：远端表情与攻击动作同步

## 症状（Context）

双账号同图时，远端玩家位置已由移动片段回放保持连续，但动作层仍不同步：

- 本地按 FACE1..FACE7（默认 F1/F2/F3/F5/F6/F7/F8）播放表情后，
  另一客户端看不到；服务端也没有收到表情请求。
- 远端玩家攻击时，人物挥砍/刺击动作晚于伤害数字和命中特效出现，
  视觉上像"先掉血、后出手"。
- 表情没有超时复位路径，远端脸可能长期停在最后一个表情。

## 根因

### 表情

- 客户端出站 opcode 表缺少 v83 `FACE_EXPRESSION(0x33)`，`StageInput`
  的 `KeyType::FACE` 分支只修改本地 `CharLook`，没有发送服务端封包。
- 入站 opcode 表缺少 `FACIAL_EXPRESSION(193)`，即使服务端广播，
  客户端也没有 handler 更新其他角色。
- 本地按键传入的是 `KeyAction`（`FACE1=100..FACE7=106`），而线协议
  emote 是 `1..7`；两种语义没有集中的转换点，远端 handler 无法直接复用。

### 攻击

- `Combat::push_attack` 对服务端广播的 `AttackResult` 额外排队 400ms。
  广播到达时攻击者客户端已经起手，服务端也已结算命中；再次延迟会让
  远端挥砍晚于该攻击产生的伤害数字/特效。

## 修复（Change Summary）

- 新增 `Character/Look/ExpressionMapping.h`，集中两套纯映射：
  - `faceemote::from_action(100..106) -> 1..7`，非 FACE 键返回 0；
  - `faceemote::from_wire(0/1..7/8..17) -> Expression::Id`，
    覆盖基础 HIT..STUNNED 与 5160000..5160009 现金表情，未知值回退默认。
- 出站 `OutPacket::FACE_EXPRESSION = 0x33` +
  `FaceExpressionPacket(int32_t)`；按下 FACE 键时写 wire emote 1..7。
- 入站 `FACIAL_EXPRESSION(193)` + `FacialExpressionHandler`：
  读 `cid, emote`，经 `Char::set_expression(emote)` 更新远端表情。
- 服务端 5 秒回退广播 `emote=0` 时，客户端映射回 `Expression::DEFAULT`，
  表情自动复位。
- 本地表情与远端 `set_expression` 统一走 `from_wire`，避免把
  `KeyAction` 数值误当 wire emote。
- `Combat::push_attack` 排队延迟 400ms → 0ms；命中伤害仍由
  `DamageEffect` 队列按 `get_attackdelay` 单独排程，不影响伤害节奏。

## Impact Surface

- 客户端表情收发、远端攻击起手时机；位置回放、本地攻击结算、
  伤害数字时序未改。
- 服务端既有 `FaceExpressionHandler` / `FaceExpressionScheduler`
  与 `facialExpression` 广播直接复用，无需新补丁。
- 验证：
  - `tools/verify/face-expression-sync.cpp` PASS（动作↔wire↔表情全表、
    非法 action、基础/现金/未知 wire emote 回退）。
  - `tools/verify/char-movement-playback.cpp` PASS。
  - 双账号真实 WASM E2E `SOCIAL_MODE=actions` PASS：副账号真实按 `F1`
    → 本地/远端 `HIT`，5 秒后远端复位默认；再真实按 Ctrl，主账号侧
    远端 `mschar_stance` 进入攻击姿态（SWINGO2），并保存表情/攻击
    前后 UI 截图与角色区域对比图。
