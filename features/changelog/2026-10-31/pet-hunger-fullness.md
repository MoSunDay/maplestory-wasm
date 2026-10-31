# 宠物温饱度与饥饿罢工系统（温饱闭环全绿）

## 背景

补齐 v83 宠物温饱度玩法：宠物随时间消耗温饱度，低于阈值进入饥饿态并
"罢工"（不拾取、不自动补给），喂食 212 段宠物食品后复工。功能代码随
8b5cde5 一并入库，本条目补齐变更追溯与验收证据。

## 功能变更

客户端（`src/client/`）：

- `Gameplay/Pet/PetVitalityPolicy.{h,cpp}`：饥饿策略纯函数库
  （namespace + 自由函数，无类），`HUNGRY_THRESHOLD=40`、状态分类、
  212 段食品判定与状态文案；
- `Gameplay/Stage.cpp`：`is_pet_hungry()` 罢工闸门（按已召唤宠物实际
  温饱度判定），饿宠保持跟随但拾取/自动药水停摆；
- `IO/UITypes/UIItemInventory.cpp`：USE 栏双击 212 段食品发送
  `PetFoodPacket`；CASH 栏饥饿宠物图标红 ! 角标；
- `IO/UITypes/Pet/UIPetOptions.cpp`：补给面板顶部温饱度状态行与橙色
  HUNGRY! 提示。

服务端（link_repos/MapleStory-Server）：

- `PetFoodHandler`（PET_FOOD=0x4C：timestamp + slot + itemId），喂给
  当前最饿的已召唤宠物；
- `PetFullnessWorker` 定时衰减 `pets.fullness`；温饱度 ≤5 强制收回并
  重置 15（仍处饥饿态，引导喂食）。

## 验收

`E2E_PET_OPTIONS=hunger`（`tools/verify/maplestory-webui/pet-hunger-flow.mjs`
+ `pet-hunger-fixture.mjs`，DB 夹具注入温饱度与食品）：

- 饥饿判定：`pet hungry state probe fullness=18 level=2`；
- 罢工保持：掉落 2 件、拾取计数 0（`hungry pet strike held`）；
- 喂食：双击 USE 栏食品 → `pet fed fullness=48 hungry=0`；
- 复工：`fed pet resumed looting (pickups 0 -> 2)`；
- 截图 20a（现金栏红 ! 角标）/20b（宠物面板 HUNGRY! 状态行）。

b106 全绿：47 PASS / 0 FAIL / exit 0（含 runtime 健康门禁）。

## 关联记录

- Asyncify 楔死根因、私有 e2e 栈与复跑环境坑：
  `docs/known-issues/2026-10-31-asyncify-wedge-and-private-e2e-stack.md`；
- opcode 198（SHOW_FOREIGN_EFFECT）短包解析容错：855371f；
- 功能详解：`features/pet-automation/index.md` 温饱度章节；
- e2e 配置说明：`tools/verify/maplestory-webui/index.md` 宠物段落。

## 遗留

- `lazyfs_sync_cache.js` 同步快路径（楔死根治）已入库待 /emsdk 重编，
  需先过 boot 冒烟再复跑 e2e；
- b106 通过基于守卫版 glue（含埋点），非埋点版独立复跑确认仍在遗留项。
