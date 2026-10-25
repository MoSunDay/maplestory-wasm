# 宠物跟随对齐人物移动参数与可达性 + 成簇掉落批量扫拾

## 背景

宠物跟随此前用一组与人物无关的固定常量：地面水平力固定 `0.16`、跳
跃冲量固定 `4.5`、空中每帧把水平速度重设为 ±1.25 的“空中操控”；拾
取高度判定用固定 150px 对称阈值，而基础跳跃在共享重力 0.14 下物理可
达高度只有 72px，宠物会反复扑向人物自己都跳不上去的掉落。拾取请求
一次只锁定一个目标，成簇掉落要逐个走过去捡；宠物放弃目标后 Stage
同帧又可能重新选中同一个不可达掉落。

## 变更

1. **移动参数与主人完全同参**：新增 `pet_follow::MovementProfile`
   （`src/client/Gameplay/Pet/PetFollowPolicy.h`，walk_force/
   jump_force 镜像 `Player::get_walkforce()/get_jumpforce()`，即
   SPEED/JUMP 属性）；`Player::get_pet_movement_profile()`
   （`src/client/Character/Player.cpp`）把主人当前画像下发给宠物，
   远端角色用基础画像 `BASE_MOVEMENT`。`PetLook`
   （`src/client/Character/Look/PetLook.cpp`）地面水平力改用主人走
   力；起跳改为与人物一致的起跳——只给一次垂直冲量（-主人
   jump_force），空中水平速度保留起跳前走动惯性，删除原先空中每帧
   固定 ±1.25 的水平速度重设（不再空中操控）。
2. **可达性等于主人可达性**：重力常量统一为共享 `Physics::GRAVITY`
   （0.14，`src/client/Gameplay/Physics/Physics.h`）；新增纯函数
   `jump_reach_height(jump_force, gravity)`（v²/2g，基础 4.5→72px）
   与 `pursuit_height_limit(profile)`（单跳可达高度+16px 掉落中心容
   差，基础 88px，随 JUMP 属性提高）。拾取目标高度判定由固定 150px
   改为非对称：高于主人单跳可达高度即放弃；低于目标由对称 150px 放
   宽为 `FALL_HEIGHT_LIMIT = 240` 下落兜底。
   `MapDrops::find_pet_loot`（`src/client/Gameplay/MapleMap/
   MapDrops.cpp`）选目标时同样跳过高于该动态上限的掉落。
3. **成簇掉落批量扫拾**：新增 `MapDrops::find_pet_loot_batch`，返回
   宠物 50px `BATCH_RANGE` 内、有权限、高度可达的全部掉落并按距离
   最近优先排序；宠物到达拾取范围后每轮扫拾最多
   `pet_loot::BATCH_LIMIT = 4` 件（一次簇清，
   `src/client/Gameplay/Pet/PetLootPolicy.h`、
   `src/client/Gameplay/Stage.cpp`）。重试节奏沿用：服务端 400ms 新
   掉落锁后首发、每 25 帧一轮、最多 3 轮后 45 帧冷却；宠物自行放弃
   目标时 Stage 也进入 45 帧冷却，避免同帧重复选中同一不可达掉落。

## 验证

- `tools/verify/pet-automation.cpp`、`tools/verify/pet-physics-nx.cpp`
  扩展断言：主人画像常量（walk 0.16 / jump 4.5）、跳跃可达高度
  72/88、非对称高度放弃边界（-88/-89/240/241）、批量扫拾上限与 50px
  范围；命令行见 `tools/verify/index.md`。docker WASM 全量构建通过。

## 浏览器级验收（accept1 账号）

- `E2E_PET_OPTIONS=pickup`（accept1/accept1Hero，地图 40001）全绿：服务端确认宠物拾取 3 件、精确 OID（1000000029）1ms 内从地图移除、目标清空（finalTargetOid=0），跟随恢复达标（petP95 4 ≤ playerP95 3×1.2+0.5、距离 40、0 瞬移、0 越权跳跃、2 次折返）。
- `E2E_PET_OPTIONS=follow` 全绿：petP95=playerP95=4、距离 33（max 82.3）、3 次折返 ≤4、warpSamples=0、unauthorizedJumps=[]、recovered=true。
- 召唤链路修复：宠物召唤双击改为探针驱动（窗口原点 + 页槽位 + 命中测试），不再依赖硬编码坐标；证据目录 /tmp/accept-pet-pickup-final、/tmp/accept-pet-follow-final（pet-automation-evidence.json、pet-pickup-trace.json）。
- 已知环境干扰：并行会话/外部进程偶发杀掉 Chrome 导致 `unsettled top-level await`（exit 13），重跑即可恢复。
- 证据 JSON 已归档至 features/changelog/2026-10-25/pet-acceptance-evidence/（含 SHA256 清单）。
- 提速背书已固化为可重复回归：E2E_PET_SHOE_STATS="speed,jump"（绝对装备值写入 -7 鞋位，测试后按业务键还原并断言命中 1 行，见 pet-stat-boost.mjs；还原在浏览器断开并等待 5s 服务器落盘保存之后执行——登出保存会用内存中的增益值重写装备行主键）。
- 提速态 follow 复验全绿（SPEED 140/JUMP 127，playerP95=petP95=5、距离 34、2 折返、0 越权/0 瞬移、recovered）：动态 movement profile 随 buff 生效。
- 物品栏 tab 精确探针：`UIItemInventory::tab_button_center` + `msiteminventory_tab_x/y`（按 item id 定位 tab 按钮真实中心，两代布局通用）；召唤流程改为「窗口 active 硬门槛 + 探针坐标优先（origin+138 兜底）+ tab 切换三次重试」——状态栏菜单弹窗有时序竞态且首击可被吞（实测首击落空、第二次点击即命中），重试后召唤链路稳定。

## Related Docs

- [宠物自动化](../../pet-automation/)
- [角色系统](../../../agents/client/Character/index.md)
- [游戏世界](../../../agents/client/Gameplay/index.md)
