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

## 双客户端 E2E 验收（2026-11-08 补充）

- 新增 `tools/verify/maplestory-webui/remote-pet-desync-flow.mjs`（双浏览器、双测试
  账号 test1/test2，地图 40001）：四场景全绿——
  1. **基础可见性**：A 先召唤宠物，B 后进图 → B 侧 `mschar_pet_count(A)==1`
     且坐标跟随；
  2. **召唤竞态**：B 进图瞬间 A 召唤（SPAWN_PLAYER 同批 SPAWN_PET 竞态）→ 3/3
     次宠物立即可见（暂存重放生效）；
  3. **幽灵竞态**：A 在 B 换图瞬间快速进出图+召唤/收回 → 3/3 次 18 采样两端
     `visible==expected && pets==-1`，无跨图/永久幽灵；
  4. **协作回归**：双人同图跑图+技能动作采样，帧预算阈值放宽
     （`COOP_MAX_P95_FRAME_MS=60`，双浏览器负载），同步与步态断言不变。
- 配套 CDP 探针：`mschar_pet_itemid/x/y(cid,idx)`、`mschar_pet_count(cid)`（角色
  不在图返回 -1），经 `-sEXPORTED_FUNCTIONS` 导出。
- 注册于 `tools/verify/index.md`、`tools/verify/maplestory-webui/index.md`，并纳入
  `scripts/test_accounts/verify_all.sh` `cooppet` 阶段（`--skip-cooppet` 可跳过）。

## 回归期间的故障归因记录（2026-11-09 凌晨，非本修复回归）

`verify_all.sh all` 全量回归（setup→proto→visual×4→equip→cooppet）在本轮收尾时出现
大面积 visual/equip/cooppet 失败，逐项归因如下（均有复跑证据）：

1. **game-server 会话/地图注册表过载僵死（主因）**：连续约 120 次登录/换图后
   （server 容器 `ms-game-server` 已运行 2 天），同图双端互相收不到 SPAWN_PLAYER
   （"互见超时"），怪物伤害数字不广播（visual 大面积 `Δluma=0`），偶发登录握手
   卡死（"WASM did not reach calledRun"，客户端 console 停在 LazyFS 缓存行）。
   `docker restart ms-game-server` 后：cooppet **4/4 全绿**、test1DK visual
   0/5 → **4/5**。运营结论：连续大批量 e2e 后若互见/伤害断言异常，先重启
   game-server 再判定回归失败。
2. **equip 断言依赖遗留手工 fixture**：`equip-stat-flow.mjs` 的"数据库事实"
   （已穿 8 件含 1062000、背包 4 件 1302000/1002067/1302000/1052155）来自
   setup 工具化之前的旧角色（现快照于 `test1hold` 账号）。本轮首次重跑
   `create_accounts.py` 重建 test1-3 后，新角色蓝图（1052155 上身、无背包装备）
   与断言不符 → FAIL。已按旧 fixture 修复 DB（换装 1062000@-6、补背包 4 件）
   后 equip 两断言全 PASS。待办：把背包 4 件与 -6 裤子并入 create_accounts.py
   蓝图，否则每次 setup 后 equip 必红。
3. **test1hold 同名角色计数陷阱**：test1Hero/Pala/DK/FPA 各存在两份（旧
   `test1hold` 账号 + 新 `test1` 账号），按名字 GROUP BY 会得出"装备行翻倍"
   的假象；排查 DB 时需带 accountid 过滤。
4. **宠物物品行累积（服务端行为）**：服务端在召唤/存档回写时会为宠物新增
   `pets`+`inventoryitems` 行而非复用（历史 char 215/239 各累积 14 行
   Brown Puppy）。本次 e2e 高频召唤也触发同型累积，已在 injectPet 的幂等
   清理之外手动清账（summoned 全归零、幻影行删除）。不阻塞断言，但长跑
   e2e 前建议清一次 CASH 栏 5000001 幻影行。
5. **Rush 类位移技伤害探针局限**：1221007/1321003（Rush 冲锋）在重启后仍
   `Δluma=0`——位移技不保证在探针区域产生伤害数字，属 skill-anim-flow 已知
   边界（2026-10-26 基线 test1Pala 2/4 fail 同源）；test1Pala 为慢性失败项，
   与本次改动无关。

## 观察窗口（未定位，非本修复回归）

- 快速连续重登（登出→立即重登→立刻进图）偶发客户端**半进图冻结**：
  `msui_state==5` 但 `msmap_id` 仍为旧图/0，画面停在加载后首帧不再推进。等待
  8 秒重登冷却 + `resetLogin` 后复现率显著下降；本轮证据（登录握手卡死、console
  停在 LazyFS 行）提示部分案例可能是 server 过载侧症状，重启 server 后未再复现。
  与本次远端宠物/队列修复无因果（单客户端即可复现）。
