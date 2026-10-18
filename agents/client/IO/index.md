Commit: e683a36c8ac98d8f13d5a15928b91366c126879d

# UI 系统

## 职责

管理用户界面、输入处理和 UI 状态切换。`UI` 是顶层 UI 框架，通过 `UIState` 多态在登录、游戏和现金商城状态之间切换。

## 边界

- **包含**: UI 框架、屏幕组件、键盘/鼠标输入、光标管理
- **不包含**: 游戏世界渲染、网络通信、数据持久化

## 关键抽象

### UI (`UI.h`)

UI 框架单例。核心职责:
- `change_state(LOGIN/GAME)`: 切换无会话参数的登录/游戏状态
- `enter_cash_shop(model, female)`: 使用服务端会话快照和本地商品目录进入现金商城状态
- `update()` / `draw()`: 更新和渲染当前状态的 UI 元素
- 输入分发: `send_cursor()`, `send_key()`, `send_scroll()`, `doubleclick()`
- `emplace<T>()`: 动态创建 UI 元素
- `get_element<T>()`: 获取 UI 元素引用
- `has_element(type)` / `is_element_active(type)`: 分别查询缓存对象是否存在、界面是否实际显示；WASM 验收通过 active 状态将登录、世界、人物、建角、游戏和商城映射为稳定状态码，商城另以只读探针暴露 NX Credit、仓库数量、角色现金栏数量和请求中状态
- `msworldselect_enter`: 浏览器 Canvas `click`/`dblclick` 的世界选择兼容入口；与 GLFW 路径共用 `UIWorldSelect::enter_selected_channel()`，通过请求中状态保证同一动作只发送一次角色列表请求

### UIState (`UIState.h`)

UI 状态接口。三个具体实现:
- `UIStateLogin`: 登录流程状态（登录、世界选择、角色选择、角色创建）；世界选择进入角色选择时先挂载不读取 NX 的目标 UI 壳，来源 UI 保留到全部人物预览收敛后才移除，静态界面和人物预览资源均在 state 3 内继续准备；若 CHARLIST 解析、静态 UI 或顶层更新失败，则重新激活来源 UI、复位世界选择请求门闩并恢复输入；聚焦弹窗始终最后绘制，创建时清除并阻止底层重新取得鼠标捕获，同时暂时移除父输入框的浏览器 IME 覆盖层，保证提示可见、按钮可点
- `UIStateGame`: 游戏内状态（状态栏、背包、技能、聊天等）
- `UIStateCashShop`: 商城独占状态，管理商品、商城仓库、角色现金物品与弹窗输入

每个状态包含一组 `UIElement` 子元素，支持:
- `pre_add(type)`: 检查是否允许添加元素
- `remove(type)`: 移除元素
- `get(type)`: 获取元素

### UIElement (`UIElement.h`)

所有 UI 组件的基类。定义元素生命周期:
- `Type` 枚举: 标识元素类型
- `TOGGLED` / `FOCUSED`: 静态属性控制元素行为
- `update_screen()`: 屏幕尺寸变化回调
- 绘制、更新、输入处理等虚函数

### Keyboard / Cursor

- `Keyboard`: 键位绑定与映射 (`KeyConfig`, `KeyType`, `KeyAction`)
- `Cursor`: 光标状态与渲染

## UI 组件分类

### 登录流程 UI (`UITypes/`)
| 文件 | 职责 |
|------|------|
| `UILogin` | 账号密码登录界面 |
| `UIRegister` | 游戏内账号注册表单；复用 `Basic.img/Notice6` NX 面板、输入框和按钮素材，校验 4–12 位账号/密码后走登录协议自动建号 |
| `UILoginNotice` | 登录通知弹窗 |
| `UILoginWait` | 登录等待动画 |
| `UIWorldSelect` | 大区/频道选择；单世界使用服务端实际世界 ID，客户端仅暴露首个可用频道并固定选择零基频道 0；确认按钮和双击频道共用幂等请求入口，请求失败返回时复位门闩以允许重试，连接已断开时直接提示刷新 |
| `UICharSelect` | 角色选择；构造器只保存角色数据并立即形成可挂载壳，首个更新帧加载静态 UI，之后每帧最多构造一个延迟贴图预览，顺序为已选角色、当前页、其余角色；预览以 `PENDING`、`READY`、`FAILED` 三态收敛，构造或更新单个外观失败会记录角色与装备上下文、清除该预览、继续其他预览并计入完成数，绘制失败只警告并跳过该角色画面；全部收敛后恢复 UI，失败角色仍可选择并进入游戏；顶层 UI 失败则返回来源界面而非留下禁用的角色选择页 |
| `UICharCreation` | 角色创建；名称输入先做协议与常见非法 ID 检查，服务端负责最终合法性与唯一性；最终确认前重新检查名称，创建无回包时自动复查，拒绝、协议错误或超时均恢复输入/定制控件而不锁死界面 |

### 游戏内 UI (`UITypes/`)
| 文件 | 职责 |
|------|------|
| `UIStatusBar` | 底部状态栏；持有好友与队伍状态并向窗口、聊天及验收探针提供统一读写入口；Menu/System 使用 `StatusBar2.img/mainBar` 原生弹层，可用项派发到现有窗口，缺失能力保持禁用；游戏内无其他窗口时 Escape 打开 System 并默认选中退出，Enter 发送标准登出、重连登录服并回到账号登录页 |
| `UIStatsInfo` | 角色属性窗口；加点按钮直接采用 NX 内置 origin 定位，使可见加号与鼠标命中区域落在对应属性行 |
| `UISkillBook` | 技能书窗口；使用与四行技能布局匹配的 175×289 紧凑 NX 面板，并按 Explorer/Cygnus/Aran 的实际转职阶段开放可切换页签 |
| `UIEquipInventory` | 装备栏窗口；宠物页显示已选择的 HP/MP 药水，并从 `Stage` 当前已召唤宠物与已装备能力解析启用项；现金栏仅持有未召唤宠物不会误显示为已启用，窗口接受 USE 药水拖放 |
| `UIItemInventory` | 物品栏窗口；现金栏宠物可双击召唤，USE 药水可拖到宠物 HP/MP 槽并同步服务端角色键位选择 |
| `Pet/UIPetOptions` | 独立高对比宠物自动补给面板；分别设置并本地持久化 1%–99% 的 HP/MP 阈值，从当前 USE 背包候选中选择恢复药品或 `OFF`，确认时重新校验并通过既有键位协议持久化角色药品选择 |
| `UIBuffList` | Buff 列表显示 |
| `UIMiniMap` / `UIWorldMap` | 小地图/世界地图 |
| `Social/UIBuddy` | 独立好友窗口；展示容量、在线状态和待处理请求，提供添加、接受、拒绝、选择与删除入口，默认快捷键为 R |
| `UIChatBar` / `Chat/UIChatBarSocial` | 聊天输入与社交命令；`/buddy` 向当前好友 ID 集合发送消息，`/party` 管理队伍，`/pt` 发送队聊；每次提交后释放输入焦点，保证连续命令不丢失 |
| `Trade/UIPlayerContext` / `Trade/UITrade` | 地图玩家右键菜单与双人交易室；邀请/接受后展示双方形象、最多 9 个物品、金币、交易聊天和确认状态，物品从背包拖入，双方确认后才完成交易 |
| `CharacterInfo/UIUserInfo` | 双击自己或地图内其他玩家打开的人物资料卡；人物面板使用原版 271×190 布局，宠物面板在正下方展开为 271×387，可切换最多三只已召唤宠物；其他玩家卡片保留交易入口并提供人气增减，响应提示写入聊天区而不覆盖卡片字段 |
| `UINpcTalk` | NPC 对话界面 |
| `Field/UIFieldClock` | 场地计时器；处理服务端当前时间与单调倒计时，并响应 `STOP_CLOCK` 隐藏 |
| `UIShop` | NPC 商店界面；购买堆叠物品时数量默认值 `1` 处于全选态，首次输入直接替换；售卖数量输入超过当前堆叠时立即回填上限；批量售卖复用 `SELL ITEM` 按钮素材并位于单件售卖按钮上方，不遮挡金币栏，二次确认后一键售卖当前分类全部非现金物品；现金 Tab 保留但不展示可售物品 |
| `UIStorage` | 仓库界面 |
| `CashShop/UICashShop` | 现金商城；严格复用 `UIWindow2.img/Shop` 的 465×328 老版素材，以左商品目录、右商城仓库/角色现金栏双栏布局展示余额、搜索和翻页，处理真实购买及双向转移；商品 SN/价格/礼包读取随 WASM 内嵌的 linked-server v83 清单，筛选、分页和模式循环由无 UI 依赖的纯函数模块提供 |
| `UIParty` | 组队界面；展示完整成员快照和 HP，支持邀请、接受/拒绝、选中成员、踢人、转让队长及退出/解散；地图内点击远端玩家可选中队员或预填邀请名 |
| `UIKeyConfig` | 键位设置；从 `UIWindow2.img/KeyConfig` 加载完整键盘面板，并以同一份当前素材布局驱动映射图标绘制、点击和拖放命中 |
| `UINotice` | 系统通知；数字输入弹窗默认全选预填值，首次字符输入、退格或粘贴会替换预填值，并可按调用场景启用输入期上限钳制 |
| `UISoftKey` | 虚拟按键 (移动端) |

默认空格键的 `NPCCHAT` 动作会在人物附近选择距离最近、允许键盘交互的 NPC 并发送标准对话请求；鼠标双击 NPC 的路径保持不变。

### 通用组件 (`Components/`)
| 文件 | 职责 |
|------|------|
| `Button` / `MapleButton` / `TwoSpriteButton` / `AreaButton` | 各种按钮 |
| `Icon` / `IconCover` | 图标显示 |
| `Gauge` / `Slider` | 进度条/滑块 |
| `Textfield` | 文本输入框（UTF-8 按码点编辑，限长按字节以兼容协议）；支持一次性全选替换，并与 WASM 隐藏 textarea 的选择范围同步 |
| `ChatBalloon` / `Nametag` | 聊天气泡/名字标签 |
| `Tooltip` / `EquipTooltip` / `ItemTooltip` / `SkillTooltip` / `MapTooltip` | 各种悬浮提示 |
| `ScrollingNotice` | 滚动公告 |

### IME 桥接 (`ImeBridge.h`)

WASM 下把文本输入委托给浏览器隐藏 textarea（`web/index.html` 的
`#ime-input`），由浏览器原生输入法提供候选词窗口。C++ 通过
`EM_ASM` 调 JS 的 `MapleWasmIME.onFocus/onBlur/onText/onSelectAll`，JS 通过导出函数
`msime_input`（整段文本 + UTF-16 光标）与 `msime_key`（白名单控制键）回传。
浏览器侧实现位于 `web/ime_input.js`：`compositionstart` 到候选词确认期间，
`input` 和 `selectionchange` 只保留 DOM preedit，不调用 `msime_input`；
`compositionend` 后以最终非合成 `input` 为主、下一任务为兼容回退同步候选结果。
合成期的物理键事件不向 GLFW 窗口气泡，回显快照去重避免 DOM/C++ 循环同步；
全选时先记录选择起点快照，防止 `selectionchange` 回传后立即折叠选择范围。
密码字段（`crypt > 0`）不走桥接，保持纯键盘路径。非 WASM 平台为空实现。

同一 WASM 桥还提供只读 CHARLIST 探针：解析/挂载/静态 UI/预览阶段、当前角色索引、当前装备 ID、剩余封包字节、预览完成数与全局输入启用状态。`READY` 与 `FAILED` 预览都会计入完成数；浏览器验收要求正常路径最终阶段为 `READY` 且完成数等于角色总数，预览收敛前与选角请求等待期保持输入锁；顶层失败路径必须回到可操作的来源 UI。用这些字段可区分封包错位、UI 安装失败、具体外观加载停点、失败恢复和重复解锁。

### 社交状态 (`Social/SocialState`)

好友、待处理好友请求、队伍成员、待处理组队邀请和消息计数由无 UI 依赖的纯状态转换函数维护。网络处理器只解析事件并提交新状态，好友/组队窗口和聊天栏只消费状态；这避免窗口是否打开影响协议同步，也使离线/上线、接受/拒绝和队长变化可独立验证。

### 玩家交易状态 (`Trade/TradeState`)

交易室把双方名称、9 格物品、金币、确认标记和最近 6 条消息集中在无网络依赖的状态对象中。UI 负责输入数量、拖放和展示，服务端回包负责确认物品/金币是否实际进入交易栏；任一方确认后都禁用继续追加，退出、拒绝或错误会释放文本焦点并恢复普通 UI 输入。

## 依赖关系

- **内部依赖**: [图形渲染](../Graphics/index.md) (渲染), [角色系统](../Character/index.md) (数据展示)
- **外部依赖**: `link_repos/MapleStory-Server` 服务端 (操作请求)
