# 宠物坠崖与 leash warp 陈旧 foothold 死循环

## 症状

用户复报：宠物跟着主人跑着跑着就掉到平台下面，在下面一层横向爬行、
撞墙、原地发呆；主人走远后宠物被瞬移回身边，又立刻掉下去，无限循环。
历次修复（failed_loot_jumps / ABANDON_FRAMES / blocked_oid / in_reach
窗口）全部无效。

## 根因

两个结构性缺陷叠加：

- **触发端**：`PetLook` 在 STAND/MOVE 每帧主动 `clear_flag(TURNATEDGES)`
  （上游 Journey 客户端为 set），且 `pet_follow::decide()` 只按水平 dx
  导航。只要主人在有开放边缘的平台上移动，宠物就径直走出平台边缘
  自由落体；追拾取时 `FALL_HEIGHT_LIMIT=240` 更允许主动向下扑。
- **恢复端（要害）**：`PetLook::warp_to()` 重置了位置与速度，但保留了
  `phobj.fhid / onground / fhslope / fhlayer`。leash 把宠物放到主人脚下
  平台面上后，`Footholdtree::update_fh` 仍用旧的下层地板判定：
  `ground = 旧地板的 y` → `onground = (y == ground)` 为假 → 重力生效；
  下落一帧后宠物低于主人平台面，`get_fhid_below` 只接受 `ycomp >= fy`
  的地面，头顶的主人平台被跳过，锁定更下层地板 → 宠物从主人身边直坠
  下方；层高不足 300px 时 leash 不再触发，宠物在下面爬行直到主人走远
  → warp → 又掉，循环成立。

## 变更

- `PetFollowPolicy`：新增纯函数 `anchor_after_warp(phobj, owner_fhid)`
  （重锚 fhid=主人当前 foothold、`onground=true`、fhslope/fhlayer/
  enablejd 归零；owner_fhid==0 时 fhid=0 走 `get_fhid_below` 兜底）；
  `decide()` 在 300px leash 之后新增"主人头顶恢复"——非拾取状态且
  `owner_dy < -height_limit`（基准 -88px）直接返回 `WARP_TO_OWNER`，
  消灭"平台底下爬行等 leash"阶段。
- `PetLook`：`update()` 与 `warp_to()` 增加 `owner_fhid` 参数，三处
  warp 调用点（STAND/MOVE leash、decide 返回 WARP、FLY leash）改为
  warp 时重锚 foothold；STAND/MOVE 恢复上游边缘纪律——跟主人或 loot
  不在下方（dy ≤ HEIGHT_TOLERANCE）时 `set_flag(TURNATEDGES)` 在平台
  末端停住，仅 loot 明确在下方才清旗走落体俯冲（保住 Wing Boots 式
  拾取）。
- `Char`：新增 `get_fhid()` 访问器，`pet.update()` 传入主人 foothold。
- 顺带解阻（属在途 quickslot 移除工作流、不随本提交）：工作区删除
  `QuickslotProbes.cpp`（原定义 `msiteminventory_icon_item_id`）后未迁移
  该探针导致 wasm 链接失败，且 `GameplayProbes.cpp` 已达 800 行上限，
  工作区补独立小 TU `Gameplay/Probes/GameplayProbesItemInventory.cpp`
  恢复全量构建（待 quickslot 工作流一并提交）。

## 验证

- 策略单测（含新增：头顶 200px 立即 warp、±88px 内不 warp、拾取中不
  触发）：`c++ -std=c++17 -Wall -Wextra -Werror -Isrc tools/verify/pet-automation.cpp src/client/Gameplay/Pet/PetAutomation.cpp src/client/Gameplay/Pet/PetCapabilityPolicy.cpp src/client/Gameplay/Pet/PetFollowPolicy.cpp src/client/Gameplay/Pet/PetLootPolicy.cpp -o /tmp/pet-automation && /tmp/pet-automation`
  PASS（改动前新增用例先行复红：`FAIL owner 200px overhead warps back
  instead of crawling below`）。
- 宠物物理 NX 回归（新增双层平台 warp 探针与边缘纪律探针，地图
  102000002：平坦上层 fh29 y=117、下层链 fh15-18 y≈167-179）：
  `c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing -Wno-unused-function -Isrc/client -Isrc -Isrc/nlnx tools/verify/pet-physics-nx.cpp src/client/Gameplay/Pet/PetFollowPolicy.cpp src/client/Gameplay/Physics/Physics.cpp src/client/Gameplay/Physics/FootholdTree.cpp src/client/Gameplay/Physics/Foothold.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/pet-physics-nx && /tmp/pet-physics-nx assets/Map.nx`
  PASS。关键数据：锚定 warp（fhid 18→29）60 帧保持 grounded 且 y=117；
  保留旧 fhid 的同一 warp 一帧后主人平台在头顶被跳过，终态 y=167、
  fhid=16（复现根因）；TURNATEDGES 跟随在末端 x=-159 停住且 y 恒为
  117，无旗标（loot 俯冲）仍可离台下落。
- 全量构建：`scripts/docker_build_wasm.sh -j4` 于 wasm-builder 容器内
  100% 通过（PetLook/Char/PetFollowPolicy/新探针 TU 全部编入）。

## Related Docs

- [永久新手宠物与自动化](../../pet-automation/index.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)
