# 多玩家同图：远端玩家可见性与怪物动作/位置同步收口

## 症状（Context）

双账号同图（地图 40001）时：

- 一侧角色（尤其是带召唤宠物 accept1Hero 的角色）对另一侧**完全不可见**；
  服务端确实广播了 `SPAWN_PLAYER`，但客户端整包丢弃。
- 怪物：控制端与观察端的动作/位置对不上；重复 `SPAWN_MONSTER` 未恢复
  出生状态；击杀后残留"幽灵怪"；旧诊断还报告 x 方向漂移逐步增长到
  127px。
- `SOCIAL_MODE=actions` 下的表情/攻击同步本身已正常，说明动作层修复
  未回退，问题集中在生成解析与观察端回放。

## 根因

### 1. SPAWN_PLAYER 解析按宠物块错位，整包被丢弃

`MapObjectHandlers.cpp` 的 SPAWN_PLAYER 采用逐字段手写读取，遇到带宠物
的角色时对齐错位：

- linked server 的 `addPetInfo(..., false)` 先写 `1`；因为 `showpet=false`，
  它**不写**后续的 byte2，随后直接是 item id / name string / 8 字节 long
  uid / pos / stance / int fh。旧解析按"有 byte2"读取，多吃一个字节。
- 旧解析没有消费宠物列表末尾的 `0` 终止符。
- 旧解析假定 foreign buff 值总是存在：morph 写 short，combo/energy charge
  写 byte，其他高位 mask 只置位不写值，三种情况被当成同一种。
- ring / announce box / marriage ring / new-year card 块的跳过规则错误，
  进一步累积错位。

任一处越界或校验失败都会抛 `PacketError`；`PacketSwitch` 捕获后**整包
丢弃**，于是远端角色永远不生成，表现为"一侧隐身"。

### 2. MOVE_MONSTER 观察端缺少"上一锚点 + 已耗时"

控制端此前发送的 `startPos` 等于当前位置，整包只有一个终点也等于当前
位置的片段，duration 取默认 1ms。观察端收到后没有可插值的区间，只能
冻结或瞬移到终点。回放需要**上一个网络位置作为起点**，并带上该位置到
本次采样之间真实经过的时间。

服务端 `MoveLifeHandler` 广播时写 `startPos = (x, y - 2)`，客户端观察端
必须把 2 像素加回 y，否则两端纵坐标恒差 2px。

### 3. 重复 SPAWN_MONSTER 未恢复出生状态

已存在的 oid 命中重复 `SPAWN_MONSTER` 时只改控制权，没有恢复服务器的
position/stance/foothold，重生点与姿态停留在旧状态。

### 4. makeMonsterReal 固定块长度写错

`makeMonsterReal` 的固定状态是 **15 个零字节 + 0x88 + 6 个零字节**，不是
12 个尾随零；fixture 与解析器都按错误长度断言。

### 5. 击杀动画被 awaitdeath 门控

`kill(animation=1)` 只在 `awaitdeath` 为真时推进到死亡；观察端从不接收
控制端的本地击退/等待标记，于是停在 dying 前的状态，形成幽灵怪。

### 6. 旧漂移诊断的采样方式本身失真

旧诊断每个 CDP 往返只读一个字段，把两个客户端、不同游戏循环时刻的字
段拼在一起比较，耗时数秒，得到"持续增长到 127px"的假象；它测量的不是
同一瞬间的两端状态。

## 修复（Change Summary）

### SPAWN_PLAYER 全量解析迁出

- 新增 `src/client/Net/Protocol/SpawnPlayerPayload.{h,cpp}`，提供
  `spawn_player::parse_payload(InPacket&)`，完整解析 v83 spawn body：
  固定 header、guild、morph/buffs（morph short、combo/energy byte、其余
  mask 不读值）、各跳过块、job/look、cash/effect/chair、position/stance、
  最多 3 只宠物（8 字节 uid、无 byte2）、宠物列表尾 `0`、mount
  level/exp/tired、announce box、chalkboard、两个 ring look、marriage
  ring、new-year cards、末尾 2 字节 + team。
- `SpawnCharHandler` 改为委托 `parse_payload`，只保留组包与 spawn 调用。

### 怪物移动/动作

- `Mob` 增加 `respawn(position, stance, fh)`：重复 SPAWN_MONSTER 时恢复
  位置、姿态与 foothold，并清空陈旧回放批次。
- 新增网络锚点字段 `last_net_position`、`net_elapsed`、
  `last_net_elapsed`：控制端每 240ms、且位置确实变化时发一次移动；
  整包只带一个片段，起点是**上一个网络位置**，duration 是实际经过时间
  （钳制 1..1000ms），观察端据此线性插值，不再 1ms 瞬移。
- 观察端 `send_movement` 对 `startPos.y` 加回 2px（抵消服务端广播的
  `y - 2`）。
- 控制权交接（`set_control`）重置回放锚点与动作，避免新控制端继续回放
  别人的旧片段。
- `make_real` 固定块改为 15 零 + 0x88 + **6** 零。
- `kill(animation=1)` 总是推进到死亡，移除 `awaitdeath` 门控，消灭幽灵怪。

### 验证工具

- `tools/verify/maplestory-webui/social-flow.mjs` 新增原子 E2E 模式
  `SOCIAL_MODE=mobs`：每个客户端只在页面内做一次 `evaluate`，循环读取
  `msmob_count` 与 `msmob_state` 字段 0..9，中间**没有任何 await**；两端
  用 `Promise.all` 并发采样。断言 oid 集合一致、位移差 ≤32px、
  action/action_pos 相等，全场最差 ≤48px。
- `tools/verify/spawn-mob-payload.cpp` fixture 改为真实的 6 字节固定状态
  填充；解析器 `parse_fixed_state` 去掉未使用的 `make_real` 形参。

## 验证（Validation）

- `bash scripts/docker_build_wasm.sh` → `Build finished successfully`。
- `spawn-mob-payload`（`-Wall -Wextra -Werror -Wno-narrowing`）→ exit 0。
- `mob-movement-playback`、`char-movement-playback`、`mob-action` → passed。
- 双账号真实 WASM E2E，地图 40001，test1Warr(243, 无宠物) +
  accept1Hero(227, 已召唤 15 号宠物)：双向 `mschar_visible=1`，两侧
  `others=1`——带宠物角色不再整包丢弃。
- `SOCIAL_MODE=mobs`：26 次快照（3 idle、10 ArrowLeft、10 ArrowRight、
  3 settle），每次均为 27/27 怪，missing=0、action mismatch=0、
  位置差 >32px=0、控制端计数 27/0；最差 maxDelta=15px（上限 48px），
  无累积增长。
- `SOCIAL_MODE=actions`（同一带宠物账号）：本地 F1 → 远端 expression=2 →
  复位 0；远端攻击姿态渲染（stance=31，frame=0）。
- `SOCIAL_MODE=coop` 双账号真实 UI 协作验收全绿：双向交替行走的
  local/remote 位移区间为 122/117px 与 116/108px，最差同步 8/11px，
  单步最大 18px；同时行走时两端远端位移 125/120px、同步差 12px；
  页面 rAF 帧间隔 p95 33.3ms，远端运动 55–60 帧、最大步进 8/7px、
  p95 步进 8/6px；F1 表情中继与双向 Ctrl 攻击均通过。产物
  `coop-acceptance.json` 记录逐步断言与截图。

11-15px 的残余来自一个 240ms 插值批次内的采样相位差（两端采样并非同一
渲染帧），量级有界且不累积；它不是旧的"持续增长漂移"，旧 127px 结论是
跨时刻采样造成的假象。

## 已知边界（本轮验收范围外）

- 入场首包沿用服务端 `enteringField=true` 的 `y-42 + JUMP` 语义；客户端
  尚未模拟这段下落动画。若入场角色静止且不发送 `MOVE_PLAYER`，观察端
  可能持续看到高于地面约 42px 的悬空姿态。本轮协作验收覆盖持续行走、
  表情与攻击，不复现该静止入场边界。
- `SPAWN_PLAYER` 与同批 `MOVE_PLAYER` 在同一网络读取批次内到达时，移动
  片段会在角色实例化前被丢弃；后续片段仍正常回放。当前未做待生成角色
  的移动缓存。
- 远端玩家位置按逐 tick 的绝对片段回放；本机 60fps 下实测 p95 步进
  6–8px，低于 12px 门限。未实现子包插值，极低帧率或高速移动时仍可能
  出现阶梯。

## Impact Surface

- 客户端 SPAWN_PLAYER / SPAWN_MONSTER 解析、Mob 移动与动作回放、生成
  fixture、E2E 工具链；人物/怪物绘制与本地移动逻辑未改。
- 本轮不需要新增服务端补丁：`MoveLifeHandler` 的 `y - 2` 广播语义与
  `addPetInfo(..., false)` 的线上格式均沿用既有 linked server 行为。
