# MapleStory WebUI 全流程

`run.mjs` 通过 Chromium DevTools Protocol 驱动真实 WASM 客户端，断言每次 UI 状态转换，并依次保存登录、世界选择、人物选择、中文建角和中文聊天证据。未设置 `E2E_URL` 时会探测 loopback 上的 8000/8001 Web 服务；自定义绑定必须显式传入地址。

```bash
E2E_URL=http://127.0.0.1:8001/web/index.html node tools/verify/maplestory-webui/run.mjs
```

浏览器必须支持 WebGL2。系统浏览器版本过旧时，通过 `CHROME_BIN` 指向兼容 Chromium；无图形会话时可用 `xvfb-run -a env E2E_HEADED=1 ...` 运行。
驱动默认关闭 Chromium 的遮挡窗口和后台计时降速，保证双浏览器验收中两个游戏会话都持续消费服务端封包。
脚本会在加载客户端前验证 WebGL2，并且资产专项模式也必须等到 GLFW 窗口初始化成功且没有 fatal log 才能通过。
完整流程默认要求 P95 帧间隔不超过 34ms（覆盖正常的 30Hz 调度，但不接受 50ms 档位），可用 `E2E_MAX_P95_FRAME_MS` 收紧。

默认首次运行创建角色 `测试一`；已有该角色时设置 `E2E_CREATE_CHARACTER=0`。
设置 `E2E_REGISTER=1` 会先验证游戏内注册表单可填写和关闭，再刷新页面并执行独立的真实登录流程。

世界选择默认通过物理鼠标事件单击确认按钮；`E2E_WORLD_SELECT_ACTION` 可设为 `channel`、`dom-go` 或 `dom-channel`，分别验证物理双击频道、纯 DOM 确认和纯 DOM 双击兼容路径。设置 `E2E_STOP_AT_CHAR_SELECT=1` 可在确认进入角色选择后结束专项验收。
角色选择转换会逐帧记录 UI 状态、输入锁、CHARLIST 解析/挂载/预览阶段、角色和装备进度，并拦截 `Character.nx` 元数据读取、预取、优先请求和前台请求写入 `char-select-transition.json`。验收断言状态从世界选择直接进入角色选择且不经过 0、角色选择壳已经挂载、挂载前没有角色素材访问，预览完成前输入保持禁用，最终阶段为 `READY`、完成数等于角色总数且只在完成边沿恢复输入；进入游戏流程还会断言选角请求等待期间输入不会被预览更新重新启用。`E2E_CHAR_SELECT_LATENCY_MS` 可在该转换期间注入网络延迟以放大冷素材时序。
世界选择与角色选择阶段还执行页面内 WebGL readPixels 像素级布局断言（`layout-pixels.mjs`，纯函数，不依赖截图或外部工具）：世界选择锁定 49e4dc6 的 Nova 顶部槽位（`novaTop` 均值须高于旧中部偏下槽位与栏背景各 ≥ 20，实测差距约 36/46）；角色选择锁定单行现代布局——左右翻页箭头显著亮于背景参照（≥ 20，实测约 40/86）、占用槽位下方的白色名牌文本显著亮于左侧背景（≥ 25，实测约 45）。断言内置于 `run.mjs` 的世界选择与角色选择阶段，默认流程与 `E2E_STOP_AT_CHAR_SELECT=1` 专项模式均覆盖。箭头参照已改为与角色卡无关（左箭头上方天空条 / 卡顶与右箭头之间），空角色账号亦成立；名牌检查前置条件为页 0 槽 1 有角色，无角色时自动跳过并输出 NOTE（由 `run.mjs` 以 `previewTotal>0` 传入）。

设置 `E2E_STOP_AT_GAME=1` 会选择已有角色，完成登录服到频道服重连、完整角色数据解析和首次地图前台素材加载后结束；该模式不会创建角色或发送聊天消息，适合上线首尾稳定样本。

设置 `E2E_REQUIRE_MOBS=1` 会要求当前地图至少实例化一只服务端怪物，用于覆盖怪物构造和进图稳定性。测试角色仍在新手起始地图 `10000` 时，可同时设置 `E2E_ROUTE_TUTORIAL_MOB_MAP=1`，脚本会通过正常传送门依次走到首个有怪地图 `40000`；该路线会由游戏服务正常保存角色所在地图，不直接操作数据库。

设置 `E2E_CASH_SHOP=1` 会在进入地图后打开现代全屏商城（12 卡/21 槽分页），搜索 linked server 固定商品 SN `80000002`，完成真实购买、商城仓库取出、角色现金栏存回和退出商城，并通过 WASM 只读探针逐步核对 NX Credit、仓库和角色现金栏数量。该流程会产生真实测试账号数据；运行前应确保账号离线且至少有 1 NX Credit。

设置 `E2E_NPC_SHOP=1` 会走 Ariant(260000000) 武器店 NPC 2100002 的真实开店链路：右行至 NPC、双击开店、切换 USE 页签、选中卖出行、卖出 1 件（UIEnterNumber 默认数量回车确认）、一键售卖当前分类全部非现金物品（UIYesNo 回车确认，服务端 NPC_SHOP_ACTION mode=4）并验证库存清零与行消失，最后 Escape 关店。所有状态变迁点击使用带真实状态谓词的 `retryClick`，坐标来自 `msshop_*` 只读探针直读窗口几何；Ariant 冷素材较大，该模式下地图素材门放宽到 300 秒。前置 SQL（把 card0 角色迁到 Ariant 并放入 37 个 2000000 药水，注意 `inventoryitems.type` 为 NOT NULL 列）见 `npc-shop-flow.mjs` 头部注释。纯几何目检（不动角色）可用 gm-shop-flow.mjs（需 characters.gm≥2）。

CDP 输入在资产流高负载下会整击丢失，因此关键交互使用 `cdp.mjs` 的加固原语：文本框用 `clickField`（点击后轮询 `msime_field_focused` 聚焦探针并重试），状态变迁点击用 `retryClick`（以游戏内真实状态为谓词循环重发），双击固定 80ms 间隔（低于客户端 0.35s 双击窗口）。世界选择门接受 worldSelect/charSelect 双态，兼容 web 构建下 DOM GO 按钮桥接导致的快速跃迁。

设置 `E2E_CAPTURE_ATTACK_EFFECTS=1` 会在进入地图后使用普通攻击，通过 CDP screencast 连续保存真实合成帧，并断言攻击期间没有素材遮罩或全局资源阻塞；默认采集 20 次，可用 `E2E_ATTACK_SAMPLES` 调整。只读 WASM 探针会逐帧记录刺击/挥砍姿态、身体帧以及非零透明残影是否实际提交渲染，并要求两类攻击均覆盖。它可以与 `E2E_STOP_AT_GAME=1` 组合，用于冷缓存和热缓存的武器残影验收；合成帧仍需抽样确认剑光形状与人物图层观感。

设置 `E2E_PET_OPTIONS=1` 会打开宠物自动补给设置，再让角色移动并通过只读探针验证首只宠物的正常速度、跳跃样本及 300px 唯一瞬移门槛。生产等价验收分为两个阶段：`E2E_PET_OPTIONS=configure` 在 Perion 药店通过正常 NPC 商店购买可用药品、选择 HP/MP 药品并保存 98% 阈值；`E2E_PET_OPTIONS=runtime` 验证重连加载的药品设置、真实资源恢复、真实怪物掉落目标转换和服务端确认的本角色宠物拾取。`E2E_PET_OPTIONS=pickup` 可让新测试角色只验证 Brown Puppy 的内建能力、真实拾取和 300px 跟随恢复，不依赖已有药品设置。`E2E_PET_OPTIONS=follow` 不修改补给配置或拾取物品，只逐帧验证宠物斜坡贴合、真实受阻跳跃因果与 300px 安全绳。`E2E_PET_OPTIONS=hunger` 配合 `E2E_PET_HUNGER="fullness,foodQty"`（默认 `"10,10"`）与 `E2E_REQUIRE_MOBS=1 E2E_ROUTE_TUTORIAL_MOB_MAP=1` 验证温饱度闭环：夹具把首角色现金栏新手宠温饱度写入 DB 并向 USE 栏注入 2120000 宠物食品；客户端召唤后按 `mspet_hungry/fullness/level` 探针断言饥饿态，截图宠物面板橙色 HUNGRY! 状态行与现金栏红色 ! 角标，打怪掉落后 10 秒采样断言饿宠罢工（拾取计数不涨、`mspet_loot_target_oid` 恒 0、宠物不消失），双击 USE 栏食品喂食（PET_FOOD 0x4C）后断言 +30 温饱度、饥饿解除且掉落被捡走（复工）；结束按业务键还原 pets.fullness 并清理注入食品行。配置与运行阶段可通过 `E2E_PROFILE_DIR` 复用浏览器本地阈值设置，`E2E_ARTIFACT_DIR` 固定各阶段证据目录。召唤序列不再使用固定坐标：现金页标签与幼犬图标中心改由 `msiteminventory_x/y` 窗口原点、可见页槽位范围与 `msiteminventory_slot_at` 命中测试动态解析（现金页以 `itemInventoryTab()===5` 断言），窗口被拖动或布局代差（35/34px 行距）均可命中。

设置 `E2E_CHARACTER_INFO=1` 会双击本人角色，验证人物信息响应、卡片关闭，以及存在已召唤宠物时的宠物面板展开。`social-flow.mjs` 另用双账号验证远端人物信息卡片，以及卡片 `BtParty` 与右键菜单「组队邀请」行两条组队入口（右键菜单「交易」行的进房取证见下文交易段）；自动化只读取人气状态，不发送会持久化的人气增减请求。

设置 `E2E_RETRY_CHARACTER=<未使用名称>` 会先提交非法名称，断言原生提示实际出现并验证关闭提示后的输入恢复，再提交指定名称进入外观定制，随后取消返回角色选择；该模式默认不会创建测试角色或进入游戏。与 `E2E_STOP_AFTER_CHARACTER_CREATE=1` 组合时会执行最终创建，等待新角色响应返回角色选择后结束，适合隔离建角验收。

完整游戏流程使用独立浏览器缓存，并在进入地图前监听真实 `Texture::draw()` 驱动的前台 NX 范围请求。验收要求至少一个请求在发起前尚未驻留、素材遮罩实际出现，并将证据写入产物目录的 `natural-asset-loading.json`。`E2E_ASSET_ONLY=1` 仅运行独立 LazyFS 网络、失败和重试检查，不代替完整游戏触发验收。

## 选角进场双击竞态（所有进游戏脚本的公共坑）

`UICharSelect` 在 CHARLIST 到达后仍需若干帧完成 nx 驱动的 `initialize_ui()`；`ui_initialized` 置位前的 `doubleclick` 会被静默丢弃且客户端不会重发——单发双击的脚本会永远停在选人界面：服务端日志停在 CHARLIST（PLAYER_LOGIN 从未发出）、页面存活但无新请求、state 恒为 3。另一已知成因是 Window.cpp 双击窗口 350ms/4px 在 WASM 忙时超窗（见下文辛迪冒烟段）。进游戏一律用重试循环：最多 4 轮「双击卡片热区 + 轮询 `msui_state==5` 各 1s×5」（cash-shop-take-equip/slots 即此写法），或 state 3 后先稳等数秒再双击。

## 好友与组队双账号流程

`social-flow.mjs` 要求两个独立测试账号，驱动两个真实 WASM 客户端完成好友请求/接受/拒绝/删除、好友聊天、离线/上线同步，以及组队创建、邀请拒绝、重新邀请加入、成员快照、队聊、HP、队长转让、踢出和解散。远端人物卡 `BtParty` 与右键菜单「组队邀请」行分别发起邀请并验证拒绝后清理；流程通过正常 UI 清理其好友和队伍状态，不直接改数据库。最近一次双账号完整运行全部断言通过，覆盖上述两条邀请入口、好友全链路、组队全生命周期、好友删除与被拒申请清理。

```bash
PRIMARY_ACCOUNT=... PRIMARY_PASSWORD=... PRIMARY_CHARACTER=... \
SOCIAL_ACCOUNT=... SOCIAL_PASSWORD=... SOCIAL_CHARACTER=... \
E2E_URL=http://127.0.0.1:8001/web/index.html \
node tools/verify/maplestory-webui/social-flow.mjs
```

## 交易入口与交易室取证

交易入口另做过一轮独立双浏览器验收：经地图右键菜单「交易」行发起并进入交易室，截图留档双方形象、铭牌、物品格栅、金币行与聊天面板的呈现。该轮由一次性临时驱动完成，未沉淀为可复用脚本，且只覆盖入口与房间呈现，不构成完整交易事务；交易状态机与客户端封包解析由 `tools/verify/trade-state.cpp`、`trade-item-parser.cpp` 覆盖，交易室几何由 `trade-ui-nx.cpp`/`trade-layout.cpp` 校验；完整交易提交语义不在该浏览器轮次内，最终以 linked server 校验为准。

## GM 商店几何目检流程

`gm-shop-flow.mjs` 不走位、不找 NPC，登录后直接用 GM 聊天命令 `!gmshop`（服务端 HeavenMS GmShopCommand，gm2 级，打开 shop 1337）稳定打开商店窗口，再用页面内 WebGL readPixels 做几何断言：窗口左边缘竖线（game x≈174 列的相邻列亮度差分 > 40 且显著高于两侧参照列）、USE 页签切换（双页签区均值差均 > 3 直接通过；实测 EQUIP 页签选中/未选中位图均值差仅 ~1.5，此时退回可逆切换验证：USE 页签名均值先显著变化、点回 EQUIP 后复原），以及售物行区域（game x410..610 y196..232）的暗像素目检（实测空槽框最暗 ~119、物品描边近 0，阈值取 100）。开店检测刻意不用暗色标题条启发式（会误报）。注意其采样器 bbox 尺寸为排他宽度（width=x1-x0）而子区域循环为闭区间，当前所有子区域严格内嵌于 sampleRect 故未触发越界；若未来把采样区域扩到 bbox 边缘，需先改为闭区间尺寸（参照 layout-pixels.mjs 的 +1 修正）。

```bash
GM_SHOP_ACCOUNT=test1 GM_SHOP_PASSWORD=test1 GM_SHOP_CARD=1 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/gm-shop-flow.mjs
```

前置条件：所选角色 `characters.gm` ≥ 2（是 characters 表的 gm 列而非 accounts；`update characters set gm=2 where id=<该角色id>;` 验后还原 0）。账号 test1/test1 实测登录进的是账号 test1test1(id 26)，角色列表 card0=e2efix1 card1=wt1210 card2=验证员一，与直觉的账号 25 不同；`GM_SHOP_CARD`（角色卡索引 0..3，默认 1）选卡前先核对该账号名下角色。环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`GM_SHOP_DEBUG_PORT`（默认 9248）、`GM_SHOP_ACCOUNT`/`GM_SHOP_PASSWORD`（默认 test1/test1）、`GM_SHOP_CARD`（默认 1）、`GM_SHOP_URL`（默认 http://127.0.0.1:8000/web/index.html）。

## 现金商城装备分类菜单流程

`cash-shop-equip-flow.mjs` 登录进图后点击状态栏商城按钮进入现金商城，专项目检「装备」分类二级菜单：点击分类栏「装备」后左栏原位切换为子菜单（`mscashshop_equip_menu_open`/`mscashshop_equip_sub`/`mscashshop_filtered_count` 只读探针驱动状态断言），依次选择「武器」「帽子」「全部装备」核对过滤数量（实测全部 1984 件中装备 1402、武器 180、帽子 338，切回全部装备数量复原）。展开期间分类按钮 inactive，子分类选中后原位菜单自动收起；「← 返回」收起且过滤保持，再点「装备」重新展开；切回「全部」分类时子分类复位且目录数量恢复初始值，最后 Escape 退出商城回游戏。视觉层用页面内 WebGL readPixels 断言选中行为暖橙高亮（R>G>B 且 R-B>40，实测 rgb≈193/117/42）而未选中行为浅色（≈229/229/231），菜单收起后原「武器」行带不再出现暖色高亮（卡面 artwork 偏蓝 rgb≈234/230/234）。

```bash
CASH_EQUIP_ACCOUNT=test1 CASH_EQUIP_PASSWORD=test1 CASH_EQUIP_CARD=1 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/cash-shop-equip-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`CASH_EQUIP_DEBUG_PORT`（默认 9249）、`CASH_EQUIP_ACCOUNT`/`CASH_EQUIP_PASSWORD`（默认 test1/test1）、`CASH_EQUIP_CARD`（角色卡索引 0..3，默认 1，选卡前先核对该账号名下角色）、`CASH_EQUIP_URL`（默认 http://127.0.0.1:8000/web/index.html）。无需 GM 等级；登录偶发 reason=7（账号残留 loggedin 状态）时重跑即可。

## 现金商城分辨率取证

`cash-shop-resolution-shot.mjs` 分别以 `SHOP_RES=1`（1280×720）和 `SHOP_RES=none`（800×600，清除持久化设置）登录进图、打开商城并截图，输出画布 `getBoundingClientRect` JSON 与 `mscashshop_filtered_count`/`mscashshop_selected_left`/`mscashshop_locker_count`/`mscashshop_inventory_count` 探针。两种分辨率均应得到 `filtered=1984`，用于确认 800×600 设计画布在宽屏下等比缩放且左右栏不重叠。进图后脚本先等 `msasset_blocking=0` 且加载遮罩 `is-hidden`，避免素材流式加载期间状态栏点击被吞。

```bash
SHOP_RES=1 node tools/verify/maplestory-webui/cash-shop-resolution-shot.mjs
SHOP_RES=none node tools/verify/maplestory-webui/cash-shop-resolution-shot.mjs
```

## 现金商城试穿流程

`cash-shop-tryon-flow.mjs` 登录进图后进入现金商城，专项目检累计试穿（「试穿不生效」修复的验收）：选中「装备→帽子」子分类后点击目录卡入试穿集（`mscashshop_tryon_count`/`mscashshop_highlighted_item`/`mscashshop_preview_cap` 只读探针驱动断言），点击同槽第二顶帽子验证覆盖语义（计数仍为 1、预览帽切换为第二顶且不同于首顶和玩家基线帽），换「上衣」子分类再点一卡验证跨槽累计（计数 2、帽子预览保持），最后点击推荐面板「重置」按钮清空试穿集并还原玩家原始外观。视觉层用页面内 WebGL readPixels 采样人像带：换帽时均值亮度变化 ≥3、加上衣时降采样网格差异率 ≥2%（实测 4%）、重置后亮度回到基线 ±1.5。退出商城回游戏收尾。

```bash
TRYON_FLOW_ACCOUNT=test1 TRYON_FLOW_PASSWORD=test1 TRYON_FLOW_CARD=1 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-tryon-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`TRYON_FLOW_DEBUG_PORT`（默认 9250）、`TRYON_FLOW_ACCOUNT`/`TRYON_FLOW_PASSWORD`（默认 test1/test1）、`TRYON_FLOW_CARD`（角色卡索引 0..3，默认 1）、`TRYON_FLOW_URL`（默认 http://127.0.0.1:8000/web/index.html）。测试动态读取当前角色穿戴帽作为重置基线（实测 1002000）；帽子子分类至少需要两件同槽在售帽，若换测试账号或商品表需同步调整。

## 现金商城取出装备并穿戴流程

`cash-shop-take-equip-flow.mjs` 验收现金商城装备「纯装饰化」契约（服务端 patch 0005）：搜索并购买现金帽 SN `20000016`（物品 1002000，3700 NX）后双击取出，断言装备落入 CASH 物品栏（`msinv_slot_item(5,slot)`）而非 EQUIP 标签，真实装备槽（`msinventory_equipped_marker`/`_cover_equipped`）与 15 项属性总值全程不变。Escape 回图后在物品栏切到 CASH 标签（`msiteminventory_tab_x/_y` 按物品定位 tab 按钮）双击穿上：覆盖件移入隐藏 EQUIP 槽 100+slot，外观显示为覆盖件、真实装备标记与属性仍不变；再经装备窗口 CAP 插槽双击卸下，覆盖件回到 CASH 栏、外观与属性还原。测试会真实消耗 NX 并留下仓库/CASH 栏数据，运行前需角色离线且 CASH 栏至少有一个空位。

```bash
TAKE_EQUIP_ACCOUNT=test1 TAKE_EQUIP_PASSWORD=test1 TAKE_EQUIP_CARD=1 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-take-equip-flow.mjs
```

注意：取出物合法落位可能在 CASH 栏**任意空位**（本测试床常为 128 号位，
第 6 页），若断言只扫可见页 1..24 会误报 FAIL——取出的 dispatch 判定请以
`mscashshop_pending`/`msinv_count(5)` 变化或全栏定位为准（已在本仓二进制
上复验：take 流程本身通过，仅 slot 断言范围不足）。

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`TAKE_EQUIP_DEBUG_PORT`（默认 9251）、`TAKE_EQUIP_ACCOUNT`/`TAKE_EQUIP_PASSWORD`（默认 test1/test1）、`TAKE_EQUIP_CARD`（角色卡索引 0..3，默认 1）、`TAKE_EQUIP_ITEM_ID`（默认 1002000）、`TAKE_EQUIP_SN`（默认 20000016）、`TAKE_EQUIP_PRICE`（默认 3700）、`TAKE_EQUIP_CAP_SLOT`（默认 1）、`TAKE_EQUIP_URL`（默认 http://127.0.0.1:8000/web/index.html）。

## 现金商城购买背包格子流程

`cash-shop-slots-flow.mjs` 验收商城右栏「物品详情」面板删除后原位新增的「购买背包格子」面板：断言四个扩充 pill（`mscashshop_slotbuy_x/_y`，2x2 布局）中心均落在面板区域 (508,294)+288x94 内；点击目标 pill（默认 0 → EQUIP 栏）弹出 UIYesNo 确认框，确认后发送 CASHSHOP_OPERATION 0x06，服务端回 0x60 结算——断言 `mscashshop_slotmax` 恰好 +4（上限 128 前有空间）、`mscashshop_nx_credit` 恰好 -4000、`mscashshop_pending` 归零、其余三栏 slotmax 不受影响；Escape 回图后 slotmax 仍保持新值（服务端 gainSlots 已 saveCharToDB 持久化）。测试会真实消耗 4000 NX 并永久 +4 格，运行前需角色离线。

```bash
SLOTS_ACCOUNT=test1 SLOTS_PASSWORD=test1 SLOTS_CARD=1 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-slots-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`SLOTS_DEBUG_PORT`（默认 9252）、`SLOTS_ACCOUNT`/`SLOTS_PASSWORD`（默认 test1/test1）、`SLOTS_CARD`（角色卡索引 0..3，默认 1）、`SLOTS_PILL`（pill 0..3 → EQUIP/USE/SETUP/ETC，默认 0）、`SLOTS_PRICE`（默认 4000）、`SLOTS_URL`（默认 http://127.0.0.1:8000/web/index.html）。

## 现金商城宠物购买与自动化全链路流程

`cash-shop-pet-flow.mjs` 验收「商城宠物」功能闭环：登录（角色需在地图 40001、USE 栏备 `2000005` 超级药水）进图后进商城，断言第 8 分类「宠物」过滤生效（在售宠物 5000000–5000999 与宠物能力装备 1812000/1802 段，当前目录 87 件），SN `60000001` 搜索唯一高亮 5000001 并购买（NX 扣 4900、入库商城仓库）；双击仓库槽前先读 `mscashshop_selected_right_cash_id` 记录新宠物的商城唯一 ID，取出后断言 `msinventory_cash_slot(cashId)` 落在 1..24，再按该实际现金栏槽位通过 `ensurePetSummoned` 序列召唤，避免同 item id 多只宠物取错槽，随后断言已召唤宠物能力掩码为 31（磁铁+物品袋+飞行靴+自动 HP/MP 药水袋内建）。回图打蜗牛掉落金币/物品，验证宠物自动捡取（meso 到账且服务端 confirmed OID 从地图对象表移除）；随后 SQL 绑定 keymap 91/92 → 2000005，把 HP/MP 压到 30% 以下断线重登，断言登录同步后 HP/MP 均恢复 ≥99% 且药水消耗 1 瓶。产物（截图 + 状态 JSON）写 `/tmp/cash-shop-pet-flow`。测试会真实消耗 NX 并修改 keymap/HP/MP，运行前需角色离线。

```bash
PET_FLOW_ACCOUNT=test1 PET_FLOW_PASSWORD=test1 PET_FLOW_CARD=0 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-pet-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`PET_FLOW_DEBUG_PORT`（默认 9262）、`PET_FLOW_ACCOUNT`/`PET_FLOW_PASSWORD`（默认 test1/test1）、`PET_FLOW_CARD`（角色卡索引 0..3，默认 0）、`PET_FLOW_ARTIFACT_DIR`（默认 /tmp/cash-shop-pet-flow）、`MS_SERVER_CONFIG`（DB 凭证，默认 link_repos/MapleStory-Server/configuration.ini）、`PET_FLOW_URL`（默认 http://127.0.0.1:8000/web/index.html）。所需账号/角色卡由脚本按账号名核对，登录偶发 reason=7（账号残留 loggedin 状态）时重跑即可。

## 现金商城宠物 UI 交互缺陷猎手流程

`cash-shop-pet-ui-interactions.mjs` 专项目检宠物功能的 UI 交互边界（不做购买正向流，购买/取出/召唤/自动化由 `cash-shop-pet-flow.mjs` 覆盖）：宠物分类过滤计数**从内嵌 `cash-shop-v83.csv` 按客户端同口径动态推算**（在售 ∧ 性别 ∧ 宠物 5000xxx+能力装备 1812000-07+宠物穿戴 1802xxx，目录数据被并行任务扩充时自动跟随，实测 87 件）；翻页连点越界只钳制不崩溃（filtered/pending/notice 不漂移）；背靠背翻页连点专项不等待素材，随后轮询 `pending=0`/`notice=0`/`assetBlocking=0`/`balance_failed=0`/`balances_ready=1` 并截图 `02b-rapid-clicks`，只验证加载层与余额存活、不强求页码；「装备」子菜单(武器)选中后切「宠物」子菜单状态复位不泄漏；「全部」分类含宠物商品；点宠物卡不进试穿集、预览面板回退图标；SN 搜索 → 二次点击弹购买确认 → **Escape 取消**路径不扣款不动仓库；搜索框「×」按钮清空恢复列表；宠物自动补给面板 CANCEL 丢弃滑条编辑、OK 保存 30/40 阈值（±2 容差）、`location.reload()` 重登后 localStorage 持久化、药水槽左右箭头切换候选（含 OFF）并落盘、收尾复位 50/50；召唤态断言能力掩码 31（未召唤则跳过）。注意：商品图标已改用 `draw_effect` 非阻塞加载，不再进入 blocking 队列吞点击；`assetsIdle` 仍用于截图/静态素材稳定。产物写 `/tmp/cash-shop-pet-ui`。

```bash
CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-pet-ui-interactions.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`PET_UI_DEBUG_PORT`（默认 9263）、`PET_UI_ARTIFACT_DIR`（默认 /tmp/cash-shop-pet-ui）、`PET_UI_URL`（默认 http://127.0.0.1:8000/web/index.html）、`MS_SERVER_CONFIG`（DB 凭证路径）。固定 test1/test1 卡 0；会重置账号 loggedin 并改写本地阈值/药水配置（收尾已复位 50/50 与 2000005）。

## 现金商城确认弹窗输入隔离

`cash-shop-modal-guards.mjs` 专项目检购买确认弹窗（`UIYesNo`）的模态输入隔离与失效选中守卫：A 打开确认框后双击右栏储物格，断言 `msui_notice_active` 仍为 1 且 `mscashshop_pending` 与仓库/物品栏/NX 余额快照不变；B 弹窗外左栏滚轮同样不改变快照，关闭确认后左栏页基与操作前一致（滚轮符号先用无弹窗状态标定）；C 在第 2 页选中一张卡后翻页使高亮项离开当前页，按 Enter 不弹确认、快照不变，再用可见选中做正对照——Enter 仍弹确认，Escape 取消后快照不变。A/B 都用 `msui_front_at` 断言层级：储物格与滚轮点返回 `CASHSHOP`、弹窗中心 (400,240) 返回 `NOTICE`，这是 `get_front` 前台层判定修复的回归点（修复前该点为 `CASHSHOP`，鼠标事件会穿透到底层商城）。守卫用例本身不结算交易；商城仓库为空时脚本先用 SN `80000002`（1 NX）买入一件备货，开跑前通过 DB 重置账号 loggedin，要求账号离线且 NX Credit ≥ 1。产物写 `MODAL_GUARDS_ARTIFACT_DIR`（默认 `/tmp/cash-shop-modal-guards`）：截图 `01-slot-doubleclick-guard` 至 `05-visible-enter-cancelled`、`browser-console.log`、`browser-errors.log`，失败时另有 `zz-failure.png`。固定 test1/card0。D 在 Maple Points 余额 0 时验证确认后客户端的余额不足提示、`pending=0` 与快照不变；`MODAL_GUARDS_BALANCE_ONLY=1` 只跑 D。

```bash
CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-modal-guards.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`MODAL_GUARDS_DEBUG_PORT`（默认 9269）、`MODAL_GUARDS_ACCOUNT`/`MODAL_GUARDS_PASSWORD`（默认 test1/test1）、`MODAL_GUARDS_CARD`（角色卡索引 0..3，默认 0）、`MODAL_GUARDS_ARTIFACT_DIR`（默认 /tmp/cash-shop-modal-guards）、`MODAL_GUARDS_URL`（默认 http://127.0.0.1:8000/web/index.html）、`MODAL_GUARDS_BALANCE_ONLY`（=1 只跑余额不足 D 分支）、`MS_SERVER_CONFIG`（DB 凭证，默认 link_repos/MapleStory-Server/configuration.ini）、`MS_UI_ELEMENT_HEADER`（默认 src/client/IO/UIElement.h，用于解析 NOTICE/CASHSHOP 元素序号）。

## 现金商城结算状态探针

`cash-shop-settlement.cpp` 是不依赖浏览器的纯状态机回归，覆盖 `can_mutate` 的 16 种余额/失败/待结算/重进组合、`can_exit`、操作与退出期限、`0x57`/`0x89`→BUY 与 `0x68`/`0x6A` 的操作码关联，以及期限边界；运行命令见 [验证工具索引](../../index.md)。

浏览器专项通过以下只读探针观察商城结算：

- `mscashshop_balances_ready`：余额已到且未失败为 1，否则 0；商城不在前台为 -1。
- `mscashshop_balance_failed`、`mscashshop_can_mutate`、`mscashshop_require_reenter`：分别暴露 10.5 秒余额期限结果、购买/转移总门禁与操作超时/回服失败后的重新进入要求。
- `mscashshop_pending_op`：0=none、1=buy、2=take、3=put、4=exit，用于确认响应关联没有错误释放或残留。
- `mscashshop_selected_right_cash_id`：当前右栏选中项的商城唯一 ID（JS 侧截断为 32 位），用于把取出响应关联到具体槽位。
- `msinventory_cash_slot(cash_id)`：在角色 CASH 物品中按唯一 ID 查找实际槽位，找不到返回 0；宠物等同类物品多只并存时仍能定位本次取出的那一只。

## 自由市场 910000000 079 复刻流程

`fm-map-flow.mjs` 验收 079MAX4 自由市场 1:1 复刻：出生点、妮娜（9270052）
Space 对话、跳上店门层（L4 平台 y=-1）、弗兰德里（9030000）邻近取证、
up00→up01→up02→up03 上行链（地面→-284→-568→-851）、dn00..dn03 下行链、
in01 进房间 910000001、房间 out00 返回并断言落回 in01。每站用页面内
WebGL readPixels 输出 16×10 亮度 ASCII + 色相桶（mean/蓝绿/暖/绿/暗）取证。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
FM_FLOW_ACCOUNT=fmflow1 FM_FLOW_PASSWORD=fmflow1 FM_FLOW_CARD=0 \
node tools/verify/maplestory-webui/fm-map-flow.mjs
```

前置条件（每个新会话重置一次）：

```sql
update accounts set loggedin=0 where name='fmflow1';
update characters set map=910000000, spawnpoint=0 where accountid=42;
```

- 角色登录位置由 `characters.spawnpoint` 恢复，不重置会从上次断点开始。
- 服务端 KEYMAP 包会按 DB `keymap` 表下发整张键位表（空表 = 全空），
  必须带 Space=5/54、Alt=5/53 等行，否则 Space/Alt 全部失效；
  `scripts/test_accounts/create_accounts.py` 已默认写入。
- 本客户端所有门（含 pt=2）都只在 Up 按下瞬间触发，touch 门不会自动传送；
  WARPCD≈0.8s，刚落地后首次 Up 可能被冷却吞掉，脚本内置重试。
- 地面 y=23 与店门层 y=-1 之间有 24px 台阶（x=1179 竖墙），需跳上台；
  up00(1227,-3)/in01(1332,-4)/dn03(2281,-5) 都在该平台上。
- NPC 对话 = Space（±120px 最近 NPC）；弗兰德里在本服是仓库 NPC，
  且全局 Escape 名单不含 STORAGE，故只做邻近截图、不按 Space。

环境变量：`CHROME_BIN`、`FM_FLOW_DEBUG_PORT`（默认 9251）、
`FM_FLOW_ACCOUNT`/`FM_FLOW_PASSWORD`（默认 test1/test1）、`FM_FLOW_CARD`
（角色卡索引 0..3，默认 1）、`FM_FLOW_URL`（默认 http://127.0.0.1:8000/web/index.html）。
产物在 `/tmp/maplestory-webui-*/`（`fm00-entry..fm05-room1.png` 六张截图 +
browser-console/errors.log）。

## NPC 对话内嵌图片（#f 路径宏）流程

`npc-embed-icons.mjs` 验证妮娜（9270052，自由市场 910000000，出生点向左走到
x≈34）对话里菜单爱心 `#fEffect/CharacterEff/1062114/1/0#` 的端到端渲染。
妮娜 case 0 原有的 21 个 GuildMark 四方印分隔行已删除，因此测试不再要求
整行 17 连排色带：

- 文本层：`msnpc_prompt` 不得泄漏 `UI/`、`Effect/` 等资源路径子串；
  旧的 105 空格分隔行不得再现（实测最长空格跑为 3）；
- 像素层：页面内 readPixels 在菜单爱心图标列（game x296..308、
  y258..350）按 5px 横带扫描，自动锁定最彩带为爱心行，要求其 chroma
  显著高于灰阶文本参照，且 3 个窄列中至少 1 列染色（实测 3/3）；
- 差分层：Escape 关闭对话后同带亮度须大幅回落（白色面板消失；实测
  186 → 39.59~53.49），色度差作为备选判据；
- 卫生层：console 无 `unresolved inline image` 告警、无浏览器错误。

图标纹理经 LazyFS 异步入图集，脚本对色带扫描带 20s 重试。每次运行前需
`update accounts set loggedin=0 where name='fmflow1';`。
环境变量：`NPC_EMBED_ACCOUNT/PASSWORD/CARD/X/URL`、`CHROME_BIN`，
产物在 `/tmp/maplestory-webui-*/`（open/closed 截图 + npc-embed-icons.json）。

## 射手村辛迪移除冒烟（正向对照 + 负向缺席）

`henesys-cindy-gone-flow.mjs` 验证 NPC 9200000（辛迪/Cody）从 Map.nx 移除后
射手村 100000000 的 NPC 系统仍正常：正向对照 1012000（x≈149）Space 可弹出
对话（`msnpc_active=1`），负向断言辛迪原位（x=424 ±120px）Space 不产生对话。
选角不用 doubleClick——Window.cpp 双击窗口 350ms/4px，WASM 忙时两次
glfwPollEvents 间隔易超窗（服务端日志停在 CHARLIST 即此症状）；改为单击
卡片热区 + 单击 BtSelect。注意本仓 UI.nx 为 post-Chaos 资产
（Login.img/WorldSelect/BtChannel marker），BtSelect 锚点 =
charinfopos(662,355)+(-76,72)，sprite 140x52 origin(0,0)，hit 区
(586,427)-(726,479)，点击 (656,453)。busy renderer 会丢 CDP 输入，务必用
retryClick。环境变量 `CINDY_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT`；前置 SQL
把 card0 角色迁回 100000000 sp=0 并清 loggedin。回滚数据见
features/changelog/2026-10-28/cindy-9200000-life-entries.json。

## /gohome 快捷回自由市场

`gohome-flow.mjs` 从自由市场房间 910000001（fieldLimit 含
`CANNOTMIGRATE`，旧 `@goto`/`@warp` 会拒绝）登录，发送 `/gohome`，
断言地图切回 910000000 且落点为 `in01` 传送门附近（x≈1332）。
这是 slash 命令的唯一放行项，客户端无需改动；`@gohome`/`!gohome`
也可用。运行前需把测试角色放到 910000001：

```sql
update characters c join accounts a on a.id=c.accountid
  set c.map=910000001, c.spawnpoint=0
  where a.name='fmflow1' and c.name='fmflow1Hero';
update accounts set loggedin=0 where name='fmflow1';
```

```bash
CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/gohome-flow.mjs
```

环境变量：`GOHOME_ACCOUNT/PASSWORD/CARD/START_MAP/URL/DEBUG_PORT`、
`CHROME_BIN`；产物在 `/tmp/maplestory-webui-*/`。

## 自由市场服务 NPC（佩里传送 / 科皮商店）

`fm-service-npcs.mjs` 验证 910000000 新增的两个服务 NPC（服务端 life
index 4/5）：从 sp 出生后走到 x≈300 打开佩里 9110000 的主菜单，
点击首分类进入维多利亚子菜单，确认文本含「维多利亚」；Escape 关闭后
走到 x≈600 打开科皮 9201116 的商店（shopid 1339），断言可见行含
飞镖/子弹/药水等本店物品，并真实点击购买 2030004 或 2030000，
库存 +1；最后再次打开佩里，选维多利亚→射手村，断言 warp 到
100000000。

对话选项由脚本从 y=140 起逐行物理点击，命中首项（默认 selected=0）
即确认。运行前需重置测试角色：

```sql
update accounts set loggedin=0 where name='test2';
update characters set map=910000000, spawnpoint=0 where id=219;
```

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-service-npcs.mjs
```

环境变量：`FM_SERVICE_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT`（默认
test2/test2、卡 0、本地 8000、9255）；产物在
`/tmp/maplestory-webui-*/`（perry-main、victoria、kopee-shop、
henesys 截图 + browser-console.log）。

## 自由市场玩法向导（PQ 招募 / BOSS 入口传送）

`fm-gameplay-guides.mjs` 验证 910000000 新增的两个向导 NPC（服务端 life
index 6/7）：从 sp 出生后走到 x≈850 打开 9010000「玩法向导」的组队任务
招募菜单，断言提示含「玩法向导」与「组队任务」；Escape 关闭后走到
x≈1150 打开 9010005「BOSS 向导」的入口菜单，断言提示含「BOSS 向导」与
「入口」；随后点击首选项「扎昆入口(50级+)」——211042300 不在脚本的
DANGEROUS_MAPS 危险名单内，单击直接 cm.warp，断言落图 211042300。

NPC 脚本为 `scripts/npc/9010000.js` / `9010005.js`（支持热更新），对话
选项由脚本从 y=140 起逐行物理点击，命中首项（默认 selected=0）即确认。
运行前需重置测试角色（结束后角色停在扎昆入口一带，会话落点为
211041900）：

```sql
update accounts set loggedin=0 where name='test2';
update characters set map=910000000, spawnpoint=0 where id=219;
```

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-gameplay-guides.mjs
```

环境变量：`FM_GUIDES_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT`（默认
test2/test2、卡 0、本地 8000、9256）；产物在
`/tmp/maplestory-webui-*/`（pq-menu、boss-menu、zakum-door 截图 +
browser-console.log）。

## 任务日志窗口（Q 键开合）

`questlog-flow.mjs` 验证 KeyAction::QUESTLOG 全链路：登录进图后按 Q
（GLFW 81）打开任务日志窗口（UIElement::QUESTLOG，`emplace<UIQuestLog>`），
断言 `msui_questlog_active=1` 并截图；`msui_questlog_rows` 返回非负整数
行数；再按 Q 经 TOGGLED 翻转关闭；第三次按 Q 重开后按 Escape 走
`send_key(escape)` 分支 deactivate。窗口实现位于
`IO/UITypes/Quest/UIQuestLog.{h,cpp}`，数据源 Character/QuestLog +
Quest.nx QuestInfo.img。

两个易错点：keymap 的 `key` 列是客户端 Keytable 的 MS 扫描码下标，
Q 是 16（GLFW 键码 81 对应 PAGE_DOWN 槽位）；KeyType 枚举 MENU=4/
ACTION=5，Q 必须绑 16→4/8，写成 ACTION/8 不会开窗。运行前需重置
测试角色：

```sql
update accounts set loggedin=0 where name='test2';
update characters set map=910000000, spawnpoint=0 where id=219;
update keymap set type=4, action=8 where characterid=219 and `key`=16;
```

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/questlog-flow.mjs
```

环境变量：`QUESTLOG_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT`（默认
test2/test2、卡 0、本地 8000、9257）；产物在
`/tmp/maplestory-webui-*/`（questlog-open、questlog-closed 截图 +
browser-console.log）。

## 圣甲术客户端生效流程

`iron-body-client-flow.mjs` 验证圣甲术（1001003）在真实 WASM 客户端内的完整生效链：登录 test1 账号 card0（test1Hero，战士系）→ 进图 → 按 I 键施放 → 右上角 buff 图标槽（`UIBuffList` 绘制于 `{width-50,40}`，32×32）出现新像素。判定带前置自检：施放前对图标槽连拍两帧必须逐字节一致（背景静态），否则判 INCONCLUSIVE（exit 3）退出，避免背景动画误报；施放后区域变化即 PASS（exit 0）。截图裁剪按 canvas `getBoundingClientRect` 做游戏坐标→CSS 坐标映射，适配页面缩放。

前置（一次性 SQL，角色迁至静态室内图 910000000 并绑键）：

```sql
INSERT INTO keymap (characterid, `key`, type, action)
  SELECT id, 23, 1, 1001003 FROM characters WHERE name='test1Hero';  -- key 23 = I
UPDATE characters SET map=910000000 WHERE name='test1Hero';
UPDATE accounts SET loggedin=0 WHERE name='test1';
```

注意：`skill-keymap-prep.py` 重跑会整表重建 keymap 并把角色迁回 40001，届时需重新执行上述绑定。与其它驱动共用 test1 账号，勿与其它 E2E 并发运行（服务端重复登录接管会踢掉旧会话）。协议层（伪客户端）回归见 `scripts/test_accounts/iron_body_buff_proto.mjs`（已挂入 `verify_all.sh` proto 阶段）。

施放间隔：圣甲术无 WZ `action` 节点 → `NoAction` 不进入 attacking 态，`KeyType::SKILL` 又不滤按键自动重复，曾出现按住键每 keydown 都发 `UseSkillPacket` 疯狂重施。现 `Combat::use_move` 对非攻击技能（teleport/flash jump 除外）按 `BUFF_CAST_INTERVAL_MS=1000ms` 节流（`Combat.cpp`，仅 FBR_NONE 成功施放后记录时间戳），成功施放后 1s 内同技能再按无效。

## 掉落拾取压测

`pickup-stress-e2e.mjs` 复现并回归"自己击杀的掉落有概率无法捡取"：登录
test1（test1Hero，出生点 40001 蜗牛狩猎场）→ 每轮用 Slash Blast（绑 S）
击杀蜗牛 → 人式走向最近掉落按住 Z 拾取（必要时左右扫动补救）→ 清空
统计进入下一轮。每轮记录直接拾取耗时/是否扫动/是否卡住；卡住（dropCount
恒定 + meso 不变，证明服务端从未发放拾取奖励）回合截图并连续采样留证。
退出码：0 = 无卡住回合（且有效回合 ≥3），1 = 存在卡住或流程失败。

```bash
E2E_ROUNDS=5 CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/pickup-stress-e2e.mjs
```

环境变量：`E2E_URL/E2E_ACCOUNT/E2E_PASSWORD/E2E_CHAR_INDEX/E2E_ROUNDS/
E2E_ARTIFACT_DIR/E2E_DEBUG_PORT`（默认本地 8000、test1、10 轮、
/tmp/pickup-stress、9330）；产物在 `/tmp/pickup-stress/`（轮次报告 +
截图 + browser-console.log）。运行前需先执行
`scripts/test_accounts/skill-keymap-prep.py` 恢复键位（Z 拾取/S 攻击）
与 40001 出生点（keymap 会被服务端存档覆盖为空）。与其它驱动共用
test1 账号，勿并发运行。

## 掉落拾取几何采样

`drop-reach-diagnose.mjs` 量化玩家坐标与最近可拾取掉落
（`msdrop_nearest_x/y`）的几何关系：先可选击杀刷掉落，再按 ~500ms
节奏补发 Z 并逐帧记录玩家/掉落/宠物坐标与 meso，结束后输出最小
|dx|/|dy|/距离、30/60/100px 内样本数与最近 10 帧样本，用于判断拾取
窗口是否可达（掉落长期滞留而采样距离普遍超出窗口 ⇒ 按 Z 的请求
不生效）。只测量不判定成败：正常结束恒退出 0，致命错误退出 1
（附 fatal 截图）。

```bash
E2E_KILL_SECONDS=5 E2E_DIAG_SECONDS=30 CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/drop-reach-diagnose.mjs
```

环境变量：`E2E_URL/E2E_ACCOUNT/E2E_PASSWORD/E2E_CHAR_INDEX/
E2E_KILL_SECONDS/E2E_DIAG_SECONDS/E2E_ARTIFACT_DIR/E2E_DEBUG_PORT`（默认
本地 8000、test1、不预击杀、30s、/tmp/drop-reach、9330）；产物在
`/tmp/drop-reach/`（样本 JSON + 截图）。与拾取压测同样依赖
`skill-keymap-prep.py` 准备键位，勿与其它 test1 E2E 并发。

## 自由市场组队任务传送冒烟（fm-pq-teleports）

登录→走到 FM 玩法向导 9010000→逐个点 13 个 PQ 标签传送→断言落点
地图 ID（招募图→入场图随机段按前缀断言）。13/13 PASS 为通过标准。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-pq-teleports.mjs
```

产物在 `/tmp/fm-pq-teleports/`。依赖 `pq-harness.mjs`（登录/对话/
DB 预置通用层，MySQL 凭据运行时从服务端 configuration.ini 解析）。

## 组队任务单人模式全链路（fm-pq-solo-flow）

每 PQ 独立会话：SQL 预置 test3（charId 223）回 910000000 并按等级段
set level→walkTowards 到招募 NPC（部分图配近门 spawn 省走位：Pirate
用 sp=next00、Treasure 用 sp=0）→Space 开对话→悬停扫描点「我想参加
组队任务」（CWKPQ 9201113 两段式远征：先创建远征再开始战斗）→单人
模式 sendYesNo 截图后点 YES→断言进入入场图。NPC 队友补位的主证据是
服务端 `[SoloPQ] instance <名> spawned N NPC teammates on map <id>`
日志（docker logs ms-game-server，入场后抓取），截图与
`msother_character_count` 仅作辅助；LudiMaze/BossRush（minPlayers=1，
`soloOptional`）天然单刷不触发补位，验证口径退化为入场图断言。
13/13 终态 PASS 为通过标准；单轮 115s 硬上限下长走位图需按
summary.txt 断点续跑（重跑单个 PQ 只删该目录 summary.txt）。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-pq-solo-flow.mjs
# 调试只跑部分（按给定顺序迭代）：FM_PQ_SOLO_ONLY=CWKPQ,OrbisPQ node ...
```

产物在 `/tmp/fm-pq-solo-flow/<NN>-PQ>/`（summary.txt+截图+console/
docker 日志）。辅助层：`pq-harness-{login,db,dialog,game,move}.mjs`
（`pq-harness.mjs` 为桶文件；对话按钮定位=客户端探针
`msnpctalk_button_field` 优先→png-probe 视觉→网格三级回退，兼容
post-Chaos 519px 对话框几何）、`fm-pq-nav.mjs`（各 PQ 攀爬/隐藏门
路线）、`cdp.mjs` 的 move()。服务端契约另见
`tools/verify/pq-solo-server-contract.mjs`（含 minPlayers 数值断言与 E 节 wz 招募图 life cy≤fh 斜面落点断言，
已登记 tools/verify/index.md）。

## 现金商城取出被拒重试 + 聊天栏默认收起（cash-shop-take-reject-retry）

登录进图后分四阶段验收两项修复：A 传统(pre-Chaos)状态栏聊天栏默认收起
（`msime_field_focused` 初始为 0、`mschatbar_open`=0；打开按钮展开、发送
后自动收起释放 IME，截图 a0/a1/a2 留档人工核对）；B 断言 DB 预填的 CASH
栏物品已随登录下发（`msinv_count(5)` ≥ 100；预填 5210000 券 ×127 于
1..127 号位——ItemFactory 以 `(byte)position` 读档，128 号位加载即溢出
丢弃，故 127 为 DB 预填上限）；C 进商城按 SN `20000016` 连买两件帽子
（各 3700 NX）：第 1 件双击取出断言成功落 128 号位把 CASH 栏补满，第 2 件
取出验证**拒绝**——server patch 0005 后满栏回 0x144（enableCSUse → 客户端
即时报错解锁），静默服则由操作超时兜底解锁，两路均要求
`mscashshop_pending` 归零、`mscashshop_require_reenter`=0、
`mscashshop_can_mutate`=1（不锁死商城、按钮仍可点）、错误弹窗可见可关闭、
被拒物品原位保留在仓库；D Escape 回游戏 `!clearslot cash` 清 CASH 栏
（1..100；穿戴覆盖位 101+ 不受影响）后重进商城重选仓库格重试双击断言取出
成功（locker 复原、`mscashshop_inventory_count` 最终 +2——第 1 件在
128 号位未被清空，截图 c1/d1）。A 先行、B-D 各阶段独立 try/catch 继续汇报
（`REJECT_FAIL_FAST=1` 失败即中止），保证聊天修复在商城流程中断时仍有
结论；重进商城后仓库整包重载会复位选中，脚本按 locker 追加序号重新
点选（跨页自动翻页）。运行前置：DB 预填 + `docker restart ms-game-server`。

### 双击时序与选中复位（e2e 稳定性要点，2026-10-30 定稿）

- **选中按压与双击首压必须间隔 ≥500ms**：`Window::is_doubleclick` 的合成
  窗口是 350ms/4px，`selectLockerSlot` 的按压若与 `takeFromLocker` 双击的
  press1 间隔不足，会被合成成一次游戏内双击直接触发 take；随后双击自身
  的 press2/press3 再合成第二个双击，若 0x68 已回、右面板切到 INVENTORY，
  第二个双击会把刚取出的物品 **PUT 回仓库**（take+put 净零，断言 locker
  计数永不收敛）。脚本在每次双击前 `sleep(500)` 隔离，phase C/D 全绿。
- **取出成功后右面板自动切 INVENTORY 并选中取出物**：take#2 前必须先点
  回 LOCKER 页签（canvas (579,43)，= `TAB_LOCKER_POS{508,34}` +
  `TAB_SIZE{142,18}` 中心，origin=(0,0)），且因 locker 行序前移一格、行号
  需 -1。
- **弹窗关闭用 Enter 而非坐标点击**：`UIOk::send_key` 处理 RETURN/ESC，
  键盘路径稳定；BtOK4 按钮坐标随文本高度浮动，硬编码易超时。
- **phase D 的 `!clearslot cash` 是 GM 命令**：角色 `gm=0` 时只当普通聊天
  打印、CASH 栏不会清空，重试取出会因满栏被拒且成功断言永不收敛
  （脚本已加生效快检：clearslot 后 `msinv_count(5)` 须 <100，否则报
  "is the character GM?" 中止）。跑该 e2e 前需 `gm=2`，跑完归零。
- **phase D retry 的成功 poll 需弹窗容错**：retry 双击偶发合成第二个
  TAKE（CASH 栏重新填满后被拒），poll 内先探测 `msui_notice_active`
  并 Enter 关闭再采样；retry 双击前同样 `sleep(500)` 隔离选行按压。
  2026-10-30 final34：42 PASS / 0 FAIL 全绿（A 聊天、B 预填、C 拒绝
  解锁、D 清仓重试取出 + 最终退出），DB 终态双通道印证（locker 4→2、
  CASH 栏 2 件 cap）。
- 服务端 CS 退出分支已补 `saveCharToDB(false)`（ChangeMapHandler），退出
  不再丢商城内变更；见 changelog 2026-10-30。**重置 locker SQL 必须等上
  一轮角色完全下线**（服务端异步退出 save 可能晚于超时击杀落盘，与
  DELETE+INSERT 竞态会产生重复行）。

```bash
CHROME_BIN=/usr/bin/chromium-browser node \
tools/verify/maplestory-webui/cash-shop-take-reject-retry.mjs
```

环境变量：`REJECT_DEBUG_PORT`（默认 9259）、`REJECT_URL`（默认
http://127.0.0.1:8000/web/index.html）、`REJECT_ACCOUNT`/`REJECT_PASSWORD`
（默认 test1/test1）、`REJECT_CARD`（角色卡索引 0..3，默认 1）、
`REJECT_ITEM_ID`/`REJECT_SN`/`REJECT_PRICE`（默认 1002000/20000016/3700）、
`REJECT_FAIL_FAST`（=1 阶段失败立即中止）。前置：账号 NX Credit ≥ 商品价。
