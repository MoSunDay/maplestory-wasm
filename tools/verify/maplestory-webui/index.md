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

设置 `E2E_STOP_AT_GAME=1` 会选择已有角色，完成登录服到频道服重连、完整角色数据解析和首次地图前台素材加载后结束；该模式不会创建角色或发送聊天消息，适合上线首尾稳定样本。

设置 `E2E_REQUIRE_MOBS=1` 会要求当前地图至少实例化一只服务端怪物，用于覆盖怪物构造和进图稳定性。测试角色仍在新手起始地图 `10000` 时，可同时设置 `E2E_ROUTE_TUTORIAL_MOB_MAP=1`，脚本会通过正常传送门依次走到首个有怪地图 `40000`；该路线会由游戏服务正常保存角色所在地图，不直接操作数据库。

设置 `E2E_CASH_SHOP=1` 会在进入地图后打开老版商城，搜索 linked server 固定商品 SN `80000002`，完成真实购买、商城仓库取出、角色现金栏存回和退出商城，并通过 WASM 只读探针逐步核对 NX Credit、仓库和角色现金栏数量。该流程会产生真实测试账号数据；运行前应确保账号离线且至少有 1 NX Credit。

设置 `E2E_NPC_SHOP=1` 会进入勇士部落装备店，验证售卖数量超过堆叠上限时立即回填上限、当前 Tab 一键售卖的二次确认，以及现金 Tab 保留但不展示可售物品。流程只取消确认，不会实际售出角色物品；角色已在店内时同时设置 `E2E_NPC_SHOP_INSIDE=1`。已知问题：`E2E_NPC_SHOP=1` 环境敏感：走位后商店可能不弹出（NPC 对话框正常）；商店几何/页签/售物行验证请用 gm-shop-flow.mjs（需 characters.gm≥2）。

设置 `E2E_CAPTURE_ATTACK_EFFECTS=1` 会在进入地图后使用普通攻击，通过 CDP screencast 连续保存真实合成帧，并断言攻击期间没有素材遮罩或全局资源阻塞；默认采集 20 次，可用 `E2E_ATTACK_SAMPLES` 调整。只读 WASM 探针会逐帧记录刺击/挥砍姿态、身体帧以及非零透明残影是否实际提交渲染，并要求两类攻击均覆盖。它可以与 `E2E_STOP_AT_GAME=1` 组合，用于冷缓存和热缓存的武器残影验收；合成帧仍需抽样确认剑光形状与人物图层观感。

设置 `E2E_PET_OPTIONS=1` 会打开宠物自动补给设置，再让角色移动并通过只读探针验证首只宠物的正常速度、跳跃样本及 300px 唯一瞬移门槛。生产等价验收分为两个阶段：`E2E_PET_OPTIONS=configure` 在 Perion 药店通过正常 NPC 商店购买可用药品、选择 HP/MP 药品并保存 98% 阈值；`E2E_PET_OPTIONS=runtime` 验证重连加载的药品设置、真实资源恢复、真实怪物掉落目标转换和服务端确认的本角色宠物拾取。`E2E_PET_OPTIONS=pickup` 可让新测试角色只验证 Brown Puppy 的内建能力、真实拾取和 300px 跟随恢复，不依赖已有药品设置。`E2E_PET_OPTIONS=follow` 不修改补给配置或拾取物品，只逐帧验证宠物斜坡贴合、真实受阻跳跃因果与 300px 安全绳。配置与运行阶段可通过 `E2E_PROFILE_DIR` 复用浏览器本地阈值设置，`E2E_ARTIFACT_DIR` 固定各阶段证据目录。

设置 `E2E_CHARACTER_INFO=1` 会双击本人角色，验证人物信息响应、卡片关闭，以及存在已召唤宠物时的宠物面板展开。`social-flow.mjs` 另用双账号验证远端人物信息卡片和原有右键交易菜单共存；自动化只读取人气状态，不发送会持久化的人气增减请求。

设置 `E2E_RETRY_CHARACTER=<未使用名称>` 会先提交非法名称，断言原生提示实际出现并验证关闭提示后的输入恢复，再提交指定名称进入外观定制，随后取消返回角色选择；该模式默认不会创建测试角色或进入游戏。与 `E2E_STOP_AFTER_CHARACTER_CREATE=1` 组合时会执行最终创建，等待新角色响应返回角色选择后结束，适合隔离建角验收。

完整游戏流程使用独立浏览器缓存，并在进入地图前监听真实 `Texture::draw()` 驱动的前台 NX 范围请求。验收要求至少一个请求在发起前尚未驻留、素材遮罩实际出现，并将证据写入产物目录的 `natural-asset-loading.json`。`E2E_ASSET_ONLY=1` 仅运行独立 LazyFS 网络、失败和重试检查，不代替完整游戏触发验收。

## 好友与组队双账号流程

`social-flow.mjs` 要求两个独立测试账号，驱动两个真实 WASM 客户端完成好友请求/接受/拒绝/删除、好友聊天、离线/上线同步，以及组队创建、邀请拒绝、重新邀请加入、队聊、HP、队长转让、踢出和解散。流程通过正常 UI 清理其好友和队伍状态，不直接改数据库。

```bash
PRIMARY_ACCOUNT=... PRIMARY_PASSWORD=... PRIMARY_CHARACTER=... \
SOCIAL_ACCOUNT=... SOCIAL_PASSWORD=... SOCIAL_CHARACTER=... \
E2E_URL=http://127.0.0.1:8001/web/index.html \
node tools/verify/maplestory-webui/social-flow.mjs
```

## GM 商店几何目检流程

`gm-shop-flow.mjs` 不走位、不找 NPC，登录后直接用 GM 聊天命令 `!gmshop`（服务端 HeavenMS GmShopCommand，gm2 级，打开 shop 1337）稳定打开商店窗口，再用页面内 WebGL readPixels 做几何断言：窗口左边缘竖线（game x≈174 列的相邻列亮度差分 > 40 且显著高于两侧参照列）、USE 页签切换（双页签区均值差均 > 3 直接通过；实测 EQUIP 页签选中/未选中位图均值差仅 ~1.5，此时退回可逆切换验证：USE 页签名均值先显著变化、点回 EQUIP 后复原），以及售物行区域（game x410..610 y196..232）的暗像素目检（实测空槽框最暗 ~119、物品描边近 0，阈值取 100）。开店检测刻意不用暗色标题条启发式（会误报）。

```bash
GM_SHOP_ACCOUNT=test1 GM_SHOP_PASSWORD=test1 GM_SHOP_CARD=1 \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/gm-shop-flow.mjs
```

前置条件：所选角色 `characters.gm` ≥ 2（是 characters 表的 gm 列而非 accounts；`update characters set gm=2 where id=<该角色id>;` 验后还原 0）。账号 test1/test1 实测登录进的是账号 test1test1(id 26)，角色列表 card0=e2efix1 card1=wt1210 card2=验证员一，与直觉的账号 25 不同；`GM_SHOP_CARD`（角色卡索引 0..3，默认 1）选卡前先核对该账号名下角色。环境变量：`CHROME_BIN`（默认 /usr/bin/chromium-browser）、`GM_SHOP_DEBUG_PORT`（默认 9248）、`GM_SHOP_ACCOUNT`/`GM_SHOP_PASSWORD`（默认 test1/test1）、`GM_SHOP_CARD`（默认 1）、`GM_SHOP_URL`（默认 http://127.0.0.1:8000/web/index.html）。
