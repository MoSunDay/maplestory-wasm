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

设置 `E2E_PET_OPTIONS=1` 会打开宠物自动补给设置，再让角色移动并通过只读探针验证首只宠物的正常速度、跳跃样本及 300px 唯一瞬移门槛。生产等价验收分为两个阶段：`E2E_PET_OPTIONS=configure` 在 Perion 药店通过正常 NPC 商店购买可用药品、选择 HP/MP 药品并保存 98% 阈值；`E2E_PET_OPTIONS=runtime` 验证重连加载的药品设置、真实资源恢复、真实怪物掉落目标转换和服务端确认的本角色宠物拾取。`E2E_PET_OPTIONS=pickup` 可让新测试角色只验证 Brown Puppy 的内建能力、真实拾取和 300px 跟随恢复，不依赖已有药品设置。`E2E_PET_OPTIONS=follow` 不修改补给配置或拾取物品，只逐帧验证宠物斜坡贴合、真实受阻跳跃因果与 300px 安全绳。配置与运行阶段可通过 `E2E_PROFILE_DIR` 复用浏览器本地阈值设置，`E2E_ARTIFACT_DIR` 固定各阶段证据目录。

设置 `E2E_CHARACTER_INFO=1` 会双击本人角色，验证人物信息响应、卡片关闭，以及存在已召唤宠物时的宠物面板展开。`social-flow.mjs` 另用双账号验证远端人物信息卡片，以及卡片 `BtParty` 与右键菜单「组队邀请」行两条组队入口（右键菜单「交易」行的进房取证见下文交易段）；自动化只读取人气状态，不发送会持久化的人气增减请求。

设置 `E2E_RETRY_CHARACTER=<未使用名称>` 会先提交非法名称，断言原生提示实际出现并验证关闭提示后的输入恢复，再提交指定名称进入外观定制，随后取消返回角色选择；该模式默认不会创建测试角色或进入游戏。与 `E2E_STOP_AFTER_CHARACTER_CREATE=1` 组合时会执行最终创建，等待新角色响应返回角色选择后结束，适合隔离建角验收。

完整游戏流程使用独立浏览器缓存，并在进入地图前监听真实 `Texture::draw()` 驱动的前台 NX 范围请求。验收要求至少一个请求在发起前尚未驻留、素材遮罩实际出现，并将证据写入产物目录的 `natural-asset-loading.json`。`E2E_ASSET_ONLY=1` 仅运行独立 LazyFS 网络、失败和重试检查，不代替完整游戏触发验收。

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

`cash-shop-equip-flow.mjs` 登录进图后点击状态栏商城按钮进入现金商城，专项目检「装备」分类二级菜单：点击分类栏「装备」展开浮层（`mscashshop_equip_menu_open`/`mscashshop_equip_sub`/`mscashshop_filtered_count` 只读探针驱动状态断言），依次选择「武器」「帽子」「全部装备」核对过滤数量（实测全部 1984 件中装备 1455、武器 233、帽子 338，切回全部装备数量复原），子分类选中后浮层自动收起（同一项再点也收起），「×」随时可收起且过滤保持、再次点击「装备」可重新展开/收起（toggle），切回「全部」分类时子分类复位且目录数量恢复初始值，最后 Escape 退出商城回游戏。视觉层用页面内 WebGL readPixels 断言选中行为暖橙高亮（R>G>B 且 R-B>40，实测 rgb≈184/111/37）而未选中行为深色（≈26/28/34），菜单收起后原「武器」行带不再出现暖色高亮（卡面 artwork 偏蓝 rgb≈178/186/196）。

```bash
CASH_EQUIP_ACCOUNT=test1 CASH_EQUIP_PASSWORD=test1 CASH_EQUIP_CARD=1 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/cash-shop-equip-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`CASH_EQUIP_DEBUG_PORT`（默认 9249）、`CASH_EQUIP_ACCOUNT`/`CASH_EQUIP_PASSWORD`（默认 test1/test1）、`CASH_EQUIP_CARD`（角色卡索引 0..3，默认 1，选卡前先核对该账号名下角色）、`CASH_EQUIP_URL`（默认 http://127.0.0.1:8000/web/index.html）。无需 GM 等级；登录偶发 reason=7（账号残留 loggedin 状态）时重跑即可。

## 现金商城试穿流程

`cash-shop-tryon-flow.mjs` 登录进图后进入现金商城，专项目检累计试穿（「试穿不生效」修复的验收）：选中「装备→帽子」子分类后点击目录卡入试穿集（`mscashshop_tryon_count`/`mscashshop_highlighted_item`/`mscashshop_preview_cap` 只读探针驱动断言），点击同槽第二顶帽子验证覆盖语义（计数仍为 1、预览帽切换为第二顶且不同于首顶和玩家基线帽），换「上衣」子分类再点一卡验证跨槽累计（计数 2、帽子预览保持），最后点击推荐面板「重置」按钮清空试穿集并还原玩家原始外观。视觉层用页面内 WebGL readPixels 采样人像带：换帽时均值亮度变化 ≥3、加上衣时降采样网格差异率 ≥2%（实测 5%）、重置后亮度回到基线 ±1.5。退出商城回游戏收尾。

```bash
TRYON_FLOW_ACCOUNT=test1 TRYON_FLOW_PASSWORD=test1 TRYON_FLOW_CARD=1 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-tryon-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`TRYON_FLOW_DEBUG_PORT`（默认 9250）、`TRYON_FLOW_ACCOUNT`/`TRYON_FLOW_PASSWORD`（默认 test1/test1）、`TRYON_FLOW_CARD`（角色卡索引 0..3，默认 1）、`TRYON_FLOW_URL`（默认 http://127.0.0.1:8000/web/index.html）。测试动态读取当前角色穿戴帽作为重置基线（实测 1002000）；帽子子分类至少需要两件同槽在售帽，若换测试账号或商品表需同步调整。

## 现金商城取出装备并穿戴流程

`cash-shop-take-equip-flow.mjs` 覆盖「仓库双击取出后消失且未穿戴」修复：搜索并购买现金帽 SN `20000016`（物品 1002000，3700 NX），确认购买进入商城仓库；对选中仓库格双击后断言仓库数量减一、角色现金栏数量加一且请求已结算，证明双击只取出一次、没有被成功后的自动选中触发逆向存回。Escape 回图后通过状态栏 ITEMS 菜单打开物品栏（带重试以吸收资产加载抖动），用 `msiteminventory_slot_at` 校验 EQUIP 槽命中坐标后双击，断言 `msinventory_equipped(1)` 为 1002000、`msinventory_equipped_marker(1)`（装备现金 ID 标记）发生变化，且同 ID 换装时 EQUIP 数量不变也会被识别。测试会真实消耗 NX 并留下仓库/背包数据，运行前需角色离线且 EQUIP 栏至少有一个空位。

```bash
TAKE_EQUIP_ACCOUNT=test1 TAKE_EQUIP_PASSWORD=test1 TAKE_EQUIP_CARD=1 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/cash-shop-take-equip-flow.mjs
```

环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`TAKE_EQUIP_DEBUG_PORT`（默认 9251）、`TAKE_EQUIP_ACCOUNT`/`TAKE_EQUIP_PASSWORD`（默认 test1/test1）、`TAKE_EQUIP_CARD`（角色卡索引 0..3，默认 1）、`TAKE_EQUIP_ITEM_ID`（默认 1002000）、`TAKE_EQUIP_SN`（默认 20000016）、`TAKE_EQUIP_PRICE`（默认 3700）、`TAKE_EQUIP_CAP_SLOT`（默认 1）、`TAKE_EQUIP_URL`（默认 http://127.0.0.1:8000/web/index.html）。

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

`npc-embed-icons.mjs` 验证妮娜（9270052，自由市场 910000000，出生点向右走到
x≈34）对话里 `#fUI/GuildMark/Mark/...#` 内嵌图片宏的端到端渲染：

- 文本层：`msnpc_prompt` 不得泄漏 `UI/`、`GuildMark` 等资源路径子串，
  且出现 `#f` 精灵按宽度换算出的空格占位长跑（实测 105 空格）；
- 像素层：页面内 readPixels 在对话文本列（game x300..620 y215..290）按
  5px 横带扫描，自动锁定最彩带为图标行（实测 y255-269、pitch 15 的
  17 连排 15×15 四方印 + 行尾 3 枚），要求其 chroma 显著高于灰阶文本
  参照（实测 108 vs 5.6），且 x300..560 分 12 列全部染色；
- 差分层：Escape 关闭对话后同带 chroma 须回落（世界底色残留约 70，
  开对话时 108，差 ≥10 断言通过）；
- 卫生层：console 无 `unresolved inline image` 告警、无浏览器错误。

图标纹理经 LazyFS 异步入图集，脚本对色带扫描带 20s 重试（首帧
ready=0/21，数秒后 21/21）。每次运行前需
`update accounts set loggedin=0 where name='fmflow1';`。
环境变量：`NPC_EMBED_ACCOUNT/PASSWORD/CARD/X/URL`、`CHROME_BIN`，
产物在 `/tmp/maplestory-webui-*/`（open/closed 截图 + npc-embed-icons.json）。
