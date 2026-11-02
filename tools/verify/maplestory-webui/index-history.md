# MapleStory WebUI 历史脚本补录（分册）

> 承接 `index.md` 的补录分册：早期落地、仍可复跑的历史验证脚本与 PQ
> harness 拆分层的完整条目（主册保持精简不超行数上限）。条目格式与主册
> 一致：`## 中文名流程` + 说明 + 环境变量/运行命令（默认值取自各脚本
> 头注释与代码）。并发会话在途脚本（aqua-swim-flow / snow-slide-flow /
> cash-cover-look-flow，untracked）不收录本册。速查目录见主册
> 「历史脚本补录（目录）」一节。

## 建卡 OK 按钮定位探针（cc-probe）

`cc-probe.mjs`：建角名字录入后逐候选坐标点击，观察
`msui_character_creation_customizing` 翻位与 NameCharPacket，定位 pre-Chaos
建卡 BtYes 真实命中区（OK 艺术在 x≈385..465、y≈298..346，(520,310) 落在
取消键区）。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/cc-probe.mjs
```

环境变量：`CC_PROBE_DEBUG_PORT`（9261）、`CC_PROBE_URL`
（http://127.0.0.1:8000/web/index.html）、`CHROME_BIN`。

## 双击选卡探针（dc-probe）

`dc-probe.mjs`：停在选角卡现场 dump IME 桥状态、activeElement、卡面命中
元素与 `msui_state/msui_enabled/mscharlist_*` 全景，再以不同节奏双击逐秒
采样，定位"双击卡无 CHAR_SELECT"类输入到达问题（ImeBridge blur +
pointer-events:none 修复的实证探针）。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/dc-probe.mjs
```

环境变量：`DC_PROBE_DEBUG_PORT`（9251）、`DC_PROBE_URL`、
`DC_PROBE_ACCOUNT/PASSWORD`（test1/test1）、`DC_PROBE_CARD`（1）、
`DC_PROBE_GAP`（两次点击间隔 ms，250）。

## 登录凭据实证探针（login-probe）

`login-probe.mjs`：`typeAscii('test1')x2` 后登录现场截图（用户名明文、
密码星号计数）并 dump IME 桥，配合 `SELECT id,name FROM accounts WHERE
loggedin=1` 一锤定音凭据拼接问题（曾实证落在账号 test1test1）。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/login-probe.mjs
```

环境变量：`LOGIN_PROBE_DEBUG_PORT`（9249）、`LOGIN_PROBE_URL`、
`LOGIN_PROBE_ACCOUNT/PASSWORD`（test1/test1）、`LOGIN_PROBE_SUBMIT`
（默认提交；=0 只填不提交）。

## 干净登出 helper（logout）

`logout.mjs`：e2e 收尾公共库——游戏内退出（ESC → 系统菜单预选 LOGOUT →
Enter → PLAYER_DC(0x0C) → `Session::disconnect()` 真实 WS Close）替代杀
浏览器，消除半开 TCP 让服务端 2-3 分钟才清 `accounts.loggedin` 的
anti-multiclient 残留。导出 `logoutToLogin` 与 `waitForAccountLoggedOut`
（DB 轮询），被 storage 系列脚本复用；无独立入口。

## 圣甲术施放节流回归（iron-body-cast-throttle）

`iron-body-cast-throttle.mjs` 验证 `Combat::BUFF_CAST_INTERVAL_MS=1000`
节流：冷却窗内连按 `mscombat_last_buffcast(1001003)` 时间戳不变（全部被
节流），1.1s 后再按时间戳前进（恢复非永久封锁）。exit 0=PASS/1=FAIL/
2=HARNESS；前置同圣甲术章节（keymap I 键、map=910000000、跑前
`loggedin=0`）。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/iron-body-cast-throttle.mjs
```

环境变量：`IB_DEBUG_PORT`（9275）、`IB_URL`、`IB_ARTIFACT_DIR`
（/tmp/iron-body-throttle）、`E2E_ACCOUNT/E2E_PASSWORD`（test1/test1）。

## 逐角色技能动画验证（skill-anim-flow）

`skill-anim-flow.mjs`：按 keymap 逐技能取证——keyHold 600ms 后每 100ms
采样 1.5s 的 stance/frame/afterimage/怪数与伤害数字区亮度，判据 frame
变化、stance≠0 或 afterimage、screencast ≥3 帧、亮度变化。前置
`scripts/test_accounts/skill-keymap-prep.py`（角色迁 40001、技能绑字母键、
写出 /tmp/skill_keymap.json）；世界选择直接点 GO(510,216) 进 Reboot。

```bash
E2E_ACCOUNT=test1 E2E_KEYMAP_JSON=/tmp/skill_keymap.json \
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/skill-anim-flow.mjs
```

环境变量：`E2E_ACCOUNT`（test1）、`E2E_PASSWORD`（默认同账号）、
`E2E_CHAR_INDEX`（0-3，0）、`E2E_KEYMAP_JSON`、`E2E_ARTIFACT_DIR`
（/tmp/skill-anim）、`E2E_DEBUG_PORT`（9310）、`E2E_URL`、
`E2E_DAMAGE_LUMA_DELTA`（4）、`E2E_DRY_RUN=1`（只验 roster）。

## Hero 斗气连招端到端（hero-combo-e2e）

`hero-combo-e2e.mjs`：一次性实测 Combo Attack → Slash Blast 蓄气 → Coma
消耗；DB 预检把 test1Hero 放 40001 并从 keymap 解析技能键（Combo Attack
是 serverBuff，缺失时补空闲字母键，report 标 provisioned）。复用
cdp.mjs 与 runtime-flow.mjs 探针。

```bash
node tools/verify/maplestory-webui/hero-combo-e2e.mjs [--headed] [--port 9320]
```

环境变量：`E2E_ACCOUNT/E2E_PASSWORD/E2E_CHAR/E2E_CARD`
（test1/test1/test1Hero/0）、`E2E_DEBUG_PORT`（9320）、`E2E_ARTIFACT_DIR`
（/tmp/hero-combo-e2e）、`MS_SERVER_CONFIG`、`CHROME_BIN`、`E2E_HEADED=1`。

## 装备防御结算实测（equip-stat-flow）

`equip-stat-flow.mjs`：登录后经 InventoryStatProbe 读 EQUIPPED/EQUIP 槽位
与 WDEF/MDEF 总值，对照 DB 已穿（8 件）/背包（4 件）装备，判定 SetField
是否把背包装备误计入 EQUIPPED（误计会使穿与不穿受击伤害一致）。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/equip-stat-flow.mjs
```

环境变量：`E2E_URL`（http://127.0.0.1:8000/web/index.html）、
`E2E_ACCOUNT/E2E_PASSWORD`（test1/test1）、`CHROME_BIN`；登录链复用
pq-harness-login.mjs。

## NPC 对话重开回归（npc-dialog-reopen-flow）

`npc-dialog-reopen-flow.mjs` 验证 2026-10-29 对话 toggle 失步修复
（makeactive + 键盘/鼠标泄漏守卫）：射手村 1012000 正向对照——Space 首弹
对话、打开期间再按 Space 4s 内 active 恒 1、Escape 关、再 Space 可重开
（"无法再对话"消失）。前置：`update accounts set loggedin=0 where
name='test1'`、`update characters set map=100000000, spawnpoint=0 where
name='test1Hero'`，SPACE(57)→ACTION/NPCCHAT 绑定必须存在。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/npc-dialog-reopen-flow.mjs
```

环境变量：`NPCRE_DEBUG_PORT`（9263）、`NPCRE_URL`、
`NPCRE_ACCOUNT/PASSWORD`（test1/test1）、`NPCRE_CARD`（0）、
`NPCRE_ARTIFACT_DIR`。

## 二转教官全流程（second-job-instructor-flow）

`second-job-instructor-flow.mjs`：验证对话 toggle 修复在真实二转脚本上的
行为并走通 接受考验 → 传送 108000300 → 同事 NPC 1072004 对话可再开。复跑
Runbook（characterslots、`sp` 逗号格式、拉高 maxhp、每轮重插
queststatus/信、`loggedin=0`）见脚本头注释与
`features/changelog/2026-10-30/second-job-instructor-e2e.md`。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/second-job-instructor-flow.mjs
```

环境变量：`SJ_ACCOUNT/SJ_PASSWORD`（test1/test1）、`SJ_URL`、
`SJ_DEBUG_PORT`（9264，勿与 9259/9263 冲突）、`SJ_CARD`（0）、
`SJ_ARTIFACT_DIR`。

## 仓库窗口开合布局（storage-flow）

`storage-flow.mjs`：130000200 储物 NPC 1100000 开仓后验证 STORAGE 窗
`msui_front_at(400,240)==15` 且为完整 463 宽列表窗（修复前只渲染 263px
残骸）、列表面板亮带/中央暗带像素采样、ETC tab 点按不关窗、BtExit 关窗。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/storage-flow.mjs
```

环境变量：`STORAGE_DEBUG_PORT`（9296）、`STORAGE_URL`、
`STORAGE_ACCOUNT/STORAGE_PASSWORD`（test2/test2）、`STORAGE_CARD`（0）。

## 仓库取出存入动作全链路（storage-deposit-flow）

`storage-deposit-flow.mjs` 覆盖 CP_Storage 封包闭环（storage-flow 只测
开窗布局）：前置 SQL 注入仓库 4000001 x5 / 背包 ETC 4000000 x3 → 像素
diff 断言 0x16 全量与镜像 → 双击左列行0 TAKE_OUT 整堆取出 → 右列非 EQUIP
数量>1 二次点击弹 UIEnterNumber，Enter=STORE 存入 → 进现金商城落档 → DB
断言终态。账号 test2 卡0 test2ILA。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/storage-deposit-flow.mjs
```

环境变量：`STORAGE_DEPOSIT_DEBUG_PORT`（9333）、`STORAGE_DEPOSIT_URL`、
`STORAGE_DEPOSIT_PROFILE/STORAGE_DEPOSIT_ARTIFACT_DIR`
（/tmp/maplestory-webui-deposit、/tmp/storage-deposit-flow）、
`MS_SERVER_CONFIG`、`STORAGE_ACCOUNT/STORAGE_PASSWORD/STORAGE_CARD`
（test2/test2/0）。

## 自由市场仓库 NPC 全链路（fm-storage-flow）

`fm-storage-flow.mjs` 在 910000000 验证 plife 注入的仓库 NPC 1100000：
Space 开仓、仓库混装备后 0x16 解析不串位（修复前 skip(28) 错读）、取出按
"页签内索引"命中（修复前桶映射错位）、满仓被 0x11 拒、空位存入成功、
ESC 关窗。复用 logout.mjs 与 support/fm-map-retry.mjs。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/fm-storage-flow.mjs
```

环境变量：`FM_STORAGE_DEBUG_PORT`（9298）、`FM_STORAGE_URL`、
`FM_STORAGE_PROFILE/FM_STORAGE_ARTIFACT_DIR`
（/tmp/maplestory-webui-fm-storage、/tmp/fm-storage-flow）、
`MS_SERVER_CONFIG`、`STORAGE_ACCOUNT/STORAGE_PASSWORD/STORAGE_CARD`
（test2/test2/0）。

## 宠物自动化链路库（pet-automation-flow）

`pet-automation-flow.mjs` 是 run.mjs `E2E_PET_OPTIONS` 宠物专项复用库
（无独立入口）：`verifyPetAutomationFlow(..., mode)` 支持 configure（预置
HP/MP 药水并配置自动喝药/拾取/跟随）、runtime、pickup（掉落拾取与精确
OID 失效——drop 移除后目标清零）、follow；另导出 `ensurePetSummoned` /
`openItemInventory` / `openOptions`。运行：`E2E_PET_OPTIONS=pickup node
tools/verify/maplestory-webui/run.mjs`；`E2E_PET_HP_ITEM`/`E2E_PET_MP_ITEM`
（默认 2000000/2000003）覆盖药水 id。

## 宠物饥饿数据夹具（pet-hunger-fixture）

`pet-hunger-fixture.mjs`：把现金栏第 1 格新手宠 5000001 温饱度写为低于
饥饿阈值 40 的指定值并向 USE 栏注入食品 2120000；结束按业务键（characterid
+ inventorytype + itemid）还原——服务端存档会重写主键，不能复用旧主键。

```bash
E2E_PET_OPTIONS=hunger E2E_PET_HUNGER="10,10" \
node tools/verify/maplestory-webui/run.mjs
```

环境变量：`E2E_PET_HUNGER="fullness,foodQty"`（如 "10,10"，格式错误
throw）、`MS_SERVER_CONFIG`（默认 linked server configuration.ini，
凭据运行时解析不落盘）。

## 宠物温饱饥饿全链路（pet-hunger-flow）

`pet-hunger-flow.mjs` 导出 `verifyPetHungerFlow`，由 run.mjs
`E2E_PET_OPTIONS=hunger` 阶段调用（配合 pet-hunger-fixture）：断言温饱度/
饥饿态/等级全链路（存档 → 登录解析 → `get_pet_by_cash_id` →
pet_vitality）；饥饿期截图 HUNGRY! 与红色 ! 角标；饿宠罢工（拾取计数不
涨）；喂食 +30 解除饥饿；复工后掉落被捡走。

## 宠物提速数据夹具（pet-stat-boost）

`pet-stat-boost.mjs`：把绝对 speed/jump 装备值写入选定角色 -7 鞋位供
pickup/follow 提速验收；结束按业务键（characterid + inventorytype=-1 +
position=-7）新鲜定位还原并断言命中 1 行（存档重写 inventoryitemid）。

```bash
E2E_PET_OPTIONS=pickup E2E_PET_SHOE_STATS="40,27" \
node tools/verify/maplestory-webui/run.mjs
```

环境变量：`E2E_PET_SHOE_STATS="speed,jump"`（绝对装备值，需与
E2E_PET_OPTIONS 同用）、`MS_SERVER_CONFIG`。

## 魔法密林渲染性能验证（ellinia-perf-flow）

`ellinia-perf-flow.mjs`：SQL 预置 map=101000000 登录进图后采样 LazyFS
空闲窗口网络请求增量（queuebitmap 去重后静止无新 fetch）与 rAF 帧时间
p95/max + longtask（与 run.mjs `E2E_MAX_P95_FRAME_MS` 同口径）。背景见
`features/changelog/2026-10-19/ellinia-map-render-cpu-hotspots.md`。

```bash
CHROME_BIN=/usr/bin/chromium-browser \
node tools/verify/maplestory-webui/ellinia-perf-flow.mjs
```

环境变量：`ELLINIA_ACCOUNT/PASSWORD/CARD/URL/DEBUG_PORT`（test1/test1/
0/:8000/9266）、`ELLINIA_MAP`（101000000）、`ELLINIA_SETTLE_SECONDS`（8）、
`ELLINIA_SAMPLE_SECONDS`（12）、`ELLINIA_MAX_P95_FRAME_MS`（34）、
`ELLINIA_MAX_LONG_TASK_MS`（100）、`ELLINIA_MAX_IDLE_NET_REQUESTS`（0）。

## 游戏内分辨率切换验收（resolution-flow）

`resolution-flow.mjs` 验证 800x600 ↔ 1280x720 真实渲染面切换：boot 期
localStorage 恢复、Game Options 菜单接线、取消语义、canvas backing size、
GL viewport、居中设计画布原点、持久化与 800x600 回环。

```bash
node tools/verify/maplestory-webui/resolution-flow.mjs
```

环境变量：`RESOLUTION_CHROME_BIN`/`CHROME_BIN`
（/usr/bin/chromium-browser）、`RESOLUTION_URL`（:8000/web/index.html）、
`RESOLUTION_DEBUG_PORT`（9251）、`RESOLUTION_ACCOUNT/PASSWORD`
（test1/test1）、`RESOLUTION_CARD`（1，须为 0..3）。

## PQ harness 拆分层（pq-harness-game/login/db/move/dialog）

`pq-harness.mjs` 于 2026-10-26 按职责拆为五个内聚模块，fm-pq-* 与其他
进图脚本按需组合引用：

- `pq-harness-game.mjs`：游戏态探针 / 通用等待 / 停滞看门狗层（`probe`、
  `waitFor`、`uiState`、`cardCenter`、`noteProgress` 等 11 个导出）；
- `pq-harness-login.mjs`：登录进图与会话管理层（`loginAccountText`
  确定性账号输入、`attemptLogin`、`loginAndEnterGame`、`startSession`；
  密码框走 GLFW 键盘通道，共享 CDP 键盘事件偶发被丢需逐键确认）；
- `pq-harness-db.mjs`：数据库预置层（`readDbConfig`/`sqlExec`/
  `sqlQuery`；凭据运行时从 configuration.ini 解析，绝不硬编码）；
- `pq-harness-move.mjs`：走位 / NPC 对话开闭 / 聊天层（`walkTowards`、
  `approachNpcLevel`、`openNpcAt`、`openNpc`）；
- `pq-harness-dialog.mjs`：对话框交互层——选项行悬停扫描（ORANGE 重绘
  采样）与 YES/OK 按钮视觉定位（依赖 png-probe 像素解码）。

## 纯函数 PNG 解码与按钮定位（png-probe）

`png-probe.mjs`：无依赖纯函数式 PNG 解码（`parseChunks`/`decodePng`/
`pixelAt`）+ 对话框按钮定位（`findDialogBox`/`findDialogButtons`）。动机：
UINpcTalk 按钮从右缘布局、y 随文本高度变化（同一 OK 曾在 y310 与 y394+），
固定网格点击会落空；被 pq-harness-dialog.mjs 复用。

## run.mjs 复用库（runtime-flow / ime-chat-flow / user-info-flow）

- `runtime-flow.mjs`：`resolvePageUrl()`（E2E_URL，缺省探测 loopback
  8000/8001 并校验可达）、`createRuntimeProbes`（帧间隔/地图/玩家探针）、
  `createAssetIdleWaiter`、`enterTutorialMobMap`/`enterTutorialLootMap`；
  hero-combo-e2e 等独立脚本也复用；
- `ime-chat-flow.mjs`：`verifyImeChatFlow(driver, chatMessage)`——Enter
  聚焦聊天后浏览器 IME 接管（拼音 preedit 不进游戏、按键不泄漏、候选提交
  同步进 `#ime-input`）；
- `user-info-flow.mjs`：`verifyUserInfoFlow`——双击自己角色开信息卡
  （`userInfoActive`/`userInfoTargetId` 命中自身 playerId）。

