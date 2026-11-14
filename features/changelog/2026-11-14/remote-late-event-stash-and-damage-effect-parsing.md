# 迟到角色事件暂存重放 + DAMAGE_PLAYER 坠落垫层 + SHOW_FOREIGN_EFFECT 分派修复

## 症状（Context）

- 对方**进图瞬间**同步发出的移动/宠物移动/换装/表情/坐椅子包，在其
  `SPAWN_PLAYER` 尚未实例化（同批到达竞态）时被客户端**静默丢弃**：
  出生首帧位置滞后、换装/表情/椅子状态缺失（remote-pets-and-chars-queue-desync
  中记录的已知边界，本次闭环）。
- **坠落伤害显示为 0**：`DAMAGE_PLAYER`（0x00C0）`skill==-3`（坠落）时 damage 前
  有 4 字节垫层 int 0（docs/ms-network-protocol.md §0x00C0），客户端不跳过垫层，
  把 0 读成 damage、把真伤害读进未消费区，`damage<=0` 提前返回 → 坠落数字永不显示。
- **SHOW_FOREIGN_EFFECT 裸包错读**：handler 以 `length()>=4` 探测优先分派 skill
  分支，effect 11（buyback，尾随 int）、15/16（maker，尾随 int）等带尾载荷的
  裸包被当作 skill 变体解析（skillid 读成 0/1），效果错乱。

## 根因

1. `MapChars::send_movement/update_look` 与 `PetMovedHandler/ShowChairHandler/
   FacialExpressionHandler` 在 `get_char(cid)` 落空时直接 return——只有 SPAWN_PET
   有 stash 通路，其余五类事件没有。
2. `DamagePlayerHandler` 按固定 `cid+skill+damage` 读取，未处理 skill==-3 的
   4 字节垫层（skill==-4 DoT 无 monsterId 尾块，本端本就不读尾块，无需处理）。
3. `ShowForeignEffectHandler` 的 `else if (recv.length() >= 4) // skill` 位于
   `effect==0/8` 判定之前，靠长度猜测布局而非按 effect 值精确分派。

## 修复（Change Summary）

- **A. 迟到事件 stash（MapChars，沿用 pending_pets 模式）**：新增嵌套
  `struct CharEvent{ Kind kind; uint8_t pet_index; int32_t value;
  std::vector<Movement> movements; LookEntry look; }`（MOVE/PET_MOVE/LOOK/
  EXPRESSION/CHAIR 五类）与 `pending_events` 按 cid 缓存；`stash_event` 仅对
  存在于 pending spawns 的 cid 生效（未知 cid 忽略防泄漏）、同 cid 同类事件经
  `same_event_target`（PET_MOVE 按 petindex 区分）覆盖仅留最新、每 cid 上限
  `MAX_STASHED_EVENTS_PER_CID=4` 超出丢最旧；`instantiate_spawns` respawn/新增
  两分支 `replay_pending_events` 按到达顺序重放；`remove/clear` 清对应缓存。
  `LookEntry` 按值拷贝（`CharSpawn` 已同型持有）。
- **B. 五个 handler 接线**：`send_movement/update_look` 落空转 stash；
  `MOVE_PET/SHOW_CHAIR/FACIAL_EXPRESSION` 改经 `MapChars::move_pet/set_chair/
  set_expression` 路由（三包均为排除发送者的广播，不涉本地玩家）。
- **C. DamagePlayerHandler**：`skill==-3` 时校验剩余 ≥8 后 `recv.skip(4)` 再读
  damage；`damage<=0` 提前返回保持在垫层跳过之后；skill==-4 本端预测注释补充
  确认（无尾块读取，无需额外 skip）。
- **D. ShowForeignEffectHandler**：重构为按 effect 值 `switch` 精确分派——
  0 升级（LEVELUP 音效）、8 转职、9 任务完成（无现成音效资源，安全忽略不误读）、
  10 恢复（读 1B amount）、11/15/16（读尾随 int）、13 怪物书（MONSTER_CARD 效果
  + 读 4B skillid，均有 length() 守卫）、1/2/5 skill 变体抽为
  `show_foreign_skill_effect`（先验 `length()>=5` 再读 skillid+direction，
  WK 充能视觉源与 show_buff 逻辑不变）；其余（6/7/14/17/19/21/26）按表无载荷
  安全忽略。任何 effect 不再因 length() 探测而错读字节。

## Impact Surface

- `MapChars`（stash/重放/五个事件入口）、`MapObjectHandlers` 五个 handler；
  不涉本地玩家路径；未实例化 cid 的出生首帧状态（位置/外观/表情/椅子/宠物位置）
  与坠落伤害数字、升级/转职/恢复/怪物书/maker/buyback 外显正确性。

## 复现/验证建议

- `c++ -std=c++17 -Isrc/client -Isrc -fsyntax-only src/client/Net/Handlers/
  MapObjectHandlers.cpp src/client/Gameplay/MapleMap/MapChars.cpp`（本机 g++
  对 Cache<T> 有既有 incomplete-type 噪声，emsdk 容器 em++ -Werror 全绿）。
- `node tools/verify/remote-pet-desync-source-contract.mjs`（已扩展断言：迟到事件
  stash 生命周期/封顶/覆盖、DAMAGE_PLAYER 垫层顺序、foreign effect switch 分派）。
- 双人联机：B 进图瞬间 A 移动/换装/表情/坐椅/宠物移动 → B 侧首帧即同步；
  A 从高处坠落 → 伤害数字正常显示；A 升级/使用 maker/buyback → 无错误 skill 特效。
