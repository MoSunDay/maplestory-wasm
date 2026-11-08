# 远端宠物可见性 + 角色待实例化队列偶发不同步修复

## 症状（Context）

- 多人联机**偶发两边不同步**：一个客户端能看到的角色/状态另一个看不到，或出现
  服务端并不存在的"幽灵角色"，直到再次换图才恢复。
- **看不到宠物**：对方召唤的宠物在自己画面上从不出现（自己看自己的宠物正常）。

## 根因

1. `src/client/Gameplay/MapleMap/MapChars.cpp` 与此前修复的 `MapMobs` 是同型缺陷
   但漏改：
   - `clear()` 只清实例表、不清 `spawns` 待实例化队列——换图瞬间上一张地图
     延迟到达的 `SPAWN_PLAYER` 会在新图实例化成服务端不存在的角色（跨图幽灵）。
   - `remove(cid)` 不查队列——同批 `SPAWN_PLAYER`+`REMOVE_CHAR` 到达时 REMOVE
     被丢弃，随后队列又把角色生成出来，形成**永久幽灵**。两个客户端 TCP 分帧
     不同，一端同批（幽灵）、一端跨批（正确），天然偶发双边不一致。
2. `SPAWN_PLAYER` 包中的宠物槽位（v83：每只已召唤宠物写 itemid/name/8 字节
   uid/pos/stance/fh，尾 0 终止）被客户端**解析后整块丢弃**：
   `SpawnPlayerPayload` 无宠物字段 → `CharSpawn` 无宠物 → 实例化的 `OtherChar`
   `pets[3]` 全空。服务端不会为他人进图时已召唤的宠物补发 `SPAWN_PET`，
   后续 `MOVE_PET` 也因 `get_pet()==nullptr` 静默 no-op——远端宠物"逻辑上不存在"。
3. `SpawnPetHandler` 在 owner 尚在 `spawns` 队列（同批 `SPAWN_PLAYER`+
   `SPAWN_PET`，典型于对方进图瞬间召唤）时整包丢弃，宠物从此不可见。
4. `OtherChar::respawn` 不复位 `attacking`：服务端重发 SPAWN_PLAYER 纠正时，
   曾看到攻击的一端姿态被攻击门控压住，与另一端短暂分歧。

## 修复（Change Summary）

- **A. MapChars 队列生命周期**：`remove()` 仿 `MapMobs` 用 `retained` 队列过滤同
  cid 待实例化条目并 `pending_pets.erase(cid)`；`clear()` 补
  `spawns = std::queue<CharSpawn>{};` 与 `pending_pets.clear();`。
- **B. SPAWN_PLAYER 宠物数据通路**：新增 POD `PetSpawnEntry`（
  `src/client/Character/Look/PetSpawnEntry.h`）；`Payload` 增加 `pets[3]`/
  `pet_count` 并存储解析值（uid 取 8 字节低 32 位，与 SPAWN_PET 读法一致）；
  `CharSpawn::set_pets` 携带；`MapChars::instantiate_spawns` 在 respawn 与新增
  两分支 `apply_pets`（先清 3 槽再按包内容全量同步）。
- **C. SPAWN_PET 暂存重放**：`SpawnPetHandler` 改为"先解析、后投递"，owner 未
  实例化且在队列中时经 `MapChars::stash_pet` 按 cid 暂存，实例化时
  `replay_pending_pets` 重放；cid 不在队列则丢弃（防泄漏）。
- **D. `OtherChar::respawn` 复位 `attacking`**，服务端纠正包不再被旧攻击动画门控。

验收：`node tools/verify/remote-pet-desync-source-contract.mjs`（源码契约，注册于
tools/verify/index.md）；5 个改动 cpp 经 clang++ -std=gnu++17 -fsyntax-only 零错误。

## Impact Surface

- `MapChars` 队列生命周期/宠物同步、`SPAWN_PLAYER` 解析、`SpawnPetHandler`、
  `OtherChar::respawn`；不影响本地玩家宠物链路（服务端对本人重发 showPet 通路
  原本完好）。
- 已知边界（沿用 remote-visibility-mob-sync 记录）：`MOVE_PLAYER`/`MOVE_PET` 在
  owner 尚未实例化时仍被丢弃，仅造成出生瞬间一帧位置滞后，无累积效应。

## 复现/验证建议

双人联机：A 召唤宠物后 B 进图 → B 应立即看到 A 的宠物；A 在 B 换图瞬间快速进出
图/召唤收回宠物 → 两端无幽灵角色、宠物状态一致。
