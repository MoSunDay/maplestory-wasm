# 状态栏快捷栏（v83 quickSlot）技能绑定

## 背景

v83 的 `StatusBar.img/base/quickSlot` 是一个 151×80 的 4×2 面板，右锚
在底部 HUD 上。此前客户端只绘制面板与八枚键帽：槽位不能绑定技能，也
不能点击释放，更不显示服务端下发的冷却剩余时间。本次补齐快捷栏的技能
绑定（点击与拖拽）、点击使用、右键清空和冷却倒计时，复用现有的
`Combat::use_move` 施法路径，同时保留 v83 原本的 HUD 外观。

## 变更

- `src/client/IO/UITypes/StatusBar/QuickSlotModel.h`（新增）：与图形
  解耦的纯函数/纯数据模型。`jrc::quickslot::State` 持有
  `std::array<int32_t, 8> skills`（0 表示空槽）、`hovered`（-1 表示
  面板外）与 `press_id`（同一次按下只处理一次）；`index_at` 按 4 列
  × 2 行的命中带把面板内坐标映射到 0..7；`skill_at` 对越界下标返回
  0；`assign` 拒绝越界槽位与非正技能 ID；`clear` 对越界槽位为
  no-op。
- `src/client/IO/UITypes/StatusBar/QuickSlotBar.h/.cpp`（新增）：只负责
  绘制。先画 151×80 面板；已绑定槽位取
  `SkillData::get(id).get_icon(SkillData::NORMAL)` 的技能图标，空槽画
  `StatusBar.img/key/<0..7>` 键帽；两者都以
  `statusbar::quickslot_key_center(i)` 的槽位中心对齐。槽位处于冷却时，
  再覆盖 32×32、alpha 0.55 的黑色 `ColorBox`，并用 `Text::A11B`
  （`CENTER`、白色、maxwidth 32）把剩余秒数画在遮罩中央；没有冷却的
  槽位不画遮罩，空槽不受影响。
- `src/client/Character/Player.h/.cpp`、`PlayerCooldown.cpp`：新增按技能
  ID 记录的冷却表
  `std::unordered_map<int32_t, int32_t>`，内部单位为毫秒。
  `add_cooldown(skill_id, seconds)` 接收服务端下发的**秒**值并转成毫秒，
  非正值会清除对应条目；`has_cooldown` 判断剩余时间是否大于 0；
  `get_cooldown_seconds` 向上取整返回剩余秒数；`update_cooldowns` 由
  `Player::update` 每帧调用，按 `Constants::TIMESTEP` 递减并删除到期项。
  实现放在独立的 `PlayerCooldown.cpp`，避免继续增长 `Player.cpp`。
- `src/client/Net/Handlers/SetfieldHandlers.cpp` /
  `PlayerHandlers.cpp`：`COOLDOWN` 与 field 冷却列表按
  `int skill_id + short seconds` 解析后写入 `Player` 冷却表，客户端冷却
  只由服务端驱动，不在本地施法时自行计时。
- `src/client/IO/UITypes/StatusBar/StatusBarPreChaos.h`：提供
  `QUICKSLOT_WIDTH/HEIGHT`、右锚的 `quickslot_pos()`（x =
  `Constants::viewwidth() - 151`，y = 71 - 80）与八槽中心表；新增
  `quickslot_range()`，让高出状态栏 9px 的面板顶边也参与命中。
- `src/client/IO/UITypes/UIStatusBar.h/.cpp`、
  `StatusBar/UIStatusBarPreChaosDraw.cpp`：持有
  `quickslot::State`、面板与八枚键帽贴图，以及每槽一个冷却 `Text`；
  绘制时调用 `quickslot::draw`；`update_quickslot_cooldowns` 每帧读取
  已绑定技能在 `Player` 中的剩余秒数并刷新文本；`send_cursor` 处理
  悬停、点击绑定与点击使用，`send_icon` 处理从技能簿拖入，`rightclick`
  处理清空。快捷栏的命中判断先于底部状态栏按钮；v83 量条与快捷栏的
  整帧绘制移入 `UIStatusBarPreChaosDraw.cpp`，便于继续维护。
- `src/client/IO/UITypes/UISkillBook.h/.cpp`：保留 `selected_skill`
  与“只接受已学习且非被动技能”的选中逻辑；**不再**绘制选中的 32×32
  白色高亮。Escape 关闭技能簿时清空选中；`SkillDragType` 通过
  `get_skill_id` 把技能 ID 暴露给拖拽落点。
- `src/client/IO/Components/Icon.h/.cpp`：`Icon::Type` 增加虚函数
  `get_skill_id()`，默认返回 0，`Icon::get_skill_id()` 转发；只有技能
  拖拽图标覆盖为非零技能 ID，其他图标不会被快捷栏误接收。
- `src/client/IO/UIStateGame.cpp`：快捷栏作为常驻 HUD 控件路由。左键
  （`send_cursor`）与右键（`rightclick`）在快捷栏范围内都直达
  `UIStatusbar`；`drop_icon` 先检查快捷栏范围，使技能拖拽在技能簿仍
  聚焦时也能落到槽位。

布局在 800×600 与 1280×720 视口下核对：完整 151×80 面板（比 71 高的
状态栏高出 9px）保持右锚，八个槽位的技能图标与键帽均按槽位中心居中
绘制，没有穿插或裁切。HUD 背景恢复为 v83 的单张
`StatusBar.img/base/backgrnd`，不做分段重绘；技能簿也恢复无高亮的原版
观感，本轮外观回退不影响下面的绑定与冷却功能。

## 交互

- 悬停槽位：已绑定技能弹出与技能簿一致的技能 tooltip（名称、等级、
  精通等级、到期时间）；空槽、面板外或离开底部 HUD 时清除。
- 绑定：打开技能簿，点击一个已学习的非被动技能图标选中它，再在技能簿
  仍处于激活状态时点击快捷栏任一槽位，即把该技能写入并覆盖原值；也可
  直接从技能图标拖到槽位，走 `UIStateGame::drop_icon` →
  `UIStatusbar::send_icon`。两种方式都会清除技能簿选中并立即按新图标
  重绘。点击绑定的同一次按下由 `press_id` 去重，不会重复触发。
- 使用：没有选中技能时，点击已绑定槽位调用
  `Stage::get().get_combat().use_move(skill_id)`，与键盘施法共用服务端
  冷却、武器限制与禁止原因提示；点击空槽无效果。
- 冷却显示：施法后服务端通过 `COOLDOWN` 下发技能 ID 与秒数，`Player`
  在每帧更新中递减；快捷栏按槽位的绑定技能读取剩余秒数（向上取整），
  在技能图标上叠加黑色半透明遮罩和白色数字。数字水平居中、垂直居中
  于遮罩，冷却结束或清空槽位后遮罩与数字一并消失。
- 清空：右键已绑定槽位恢复键帽，并清除该槽的悬停 tooltip 与冷却数字。
- 越界：面板外或命中带外得到下标 -1，后续查询/写入均被模型拒绝，
  不会越界写数组；对空槽右键直接返回。

## 限制

- 快捷栏只接受技能，不支持物品、表情或键位设置里的按键动作；图标
  与是否可选（被动判定）都来自 `SkillData`。
- 绑定状态仅存在于客户端内存（`UIStatusbar` 内的
  `jrc::quickslot::State`），重登后清空，没有服务端回填。
- 不发包：v83 的 `CHANGE_QUICKSLOT` 为 0x00B7；当前 linked server 的
  `RecvOpcode` 中没有 0xB7，`SendOpcode` 的 0xB7 是
  `REMOVE_DRAGON`，服务端代码里也没有 `CHANGE_QUICKSLOT` 的处理器。
  因此客户端不发送该包，绑定不会持久化，服务端也无从得知客户端的
  槽位内容。
- 选中阶段只接受已学习且非被动的技能；模型本身只校验技能 ID > 0，
  不做等级或被动状态的二次校验。拖拽入口同样依赖技能簿的选中/拖拽
  类型，不会接收背包物品或表情。
- 冷却值以服务端为准，客户端只做显示倒计时，不发送本地冷却；到期后
  本地标签会先消失，真正能否再次施法仍由服务端判定。
- 冷却文本按整秒展示，因向上取整，实际剩余不足 1 秒时仍可能显示 1；
  这是显示精度而非计时误差。
- 浏览器 E2E 已覆盖 800x600 与 1280x720 两档；仍未验证重登后的
  绑定持久化（绑定只在客户端内存，见上）。

## 验证

- `./scripts/build_wasm.sh`（Release）编译通过，新增模型与绘制代码
  随 WASM 客户端一起构建。
- 新增纯模型回归 `tools/verify/quickslot-model.cpp`：覆盖八个槽位中心
  与 4×2 命中带的边界、面板外返回 -1、`assign`/`clear` 对越界槽位及
  非法技能 ID 的拒绝、`skill_at` 对空槽与越界下标返回 0；本地编译
  运行 PASS。
- 浏览器内交互 E2E 已由
  `tools/verify/maplestory-webui/quickslot-flow.mjs` 覆盖（800x600 与
  1280x720 两档，`QS_PRESET=0|1`）：真实登录进入 test1Hero 后打开
  技能书、选中技能（不断言具体服务端技能 id）、slot0
  点击绑定/悬停 tooltip/点击使用/右键清空、slot7 拖拽绑定/右键清空，
  随后绑定英雄的意志（1121011）并施法，等待服务端冷却生效后断言
  `msquickslot_cooldown(0) > 0` 且冷却标签长度大于 0，移开鼠标避免
  tooltip 遮挡后截图。脚本在每次 use/assign 之后、右键之前等待
  `msasset_blocking === 0`：施法（`Combat::use_move`）触发的 LazyFS
  按需流式加载会阻塞输入，blocking 期间派发的右键会被整击吞掉，
  该等待是必要语义而非仅调试辅助。截图像素核对显示白色剩余秒数位于
  32×32 黑色遮罩中央；因每次运行与截图的时序不同，不断言具体数值。
  重登后清空仍只经代码路径核对。

## 验收

2026-10-24 最终验收通过：Release WASM 构建成功；`quickslot-model.cpp`
纯模型回归 PASS；`QS_PRESET=0`（800x600）与 `QS_PRESET=1`（1280x720）
两档真实浏览器 E2E 均 PASS，覆盖点击绑定、拖拽绑定、施法、服务端冷却
生效（`msquickslot_cooldown(0) > 0`、冷却标签长度 2）与右键清空；
浏览器实际加载的 `JourneyClient.wasm` 与本地构建产物 sha256 一致。
