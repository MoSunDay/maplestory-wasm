Commit: 10fb657e2799392e1a15e8a68d1a989a5dcb6967

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
- `has_element(type)`: 只读查询元素是否存在；WASM 验收通过导出的 `msui_state` 将登录、世界、人物、建角和游戏状态映射为稳定状态码

### UIState (`UIState.h`)

UI 状态接口。三个具体实现:
- `UIStateLogin`: 登录流程状态（登录、世界选择、角色选择、角色创建）
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
| `UIWorldSelect` | 大区/频道选择；单世界使用服务端实际世界 ID，频道面板居中，开放按钮严格按服务端频道数绘制并支持双击进入；其余槽位沿用统一关闭背景 |
| `UICharSelect` | 角色选择；绘制角色槽计数、人物平台和对应属性面板字段 |
| `UICharCreation` | 角色创建；名称校验采用可恢复的局部请求状态，拒绝或超时后保留输入并恢复焦点，服务端仍负责最终合法性与唯一性校验 |

### 游戏内 UI (`UITypes/`)
| 文件 | 职责 |
|------|------|
| `UIStatusBar` | 底部状态栏；Menu/System 使用 `StatusBar2.img/mainBar` 原生弹层，可用项派发到现有窗口，缺失能力保持禁用，支持互斥切换、Escape 和外部点击关闭 |
| `UIStatsInfo` | 角色属性窗口；加点按钮直接采用 NX 内置 origin 定位，使可见加号与鼠标命中区域落在对应属性行 |
| `UISkillBook` | 技能书窗口；使用与四行技能布局匹配的 175×289 紧凑 NX 面板，窗口边界与可见背景一致 |
| `UIEquipInventory` | 装备栏窗口 |
| `UIItemInventory` | 物品栏窗口 |
| `UIBuffList` | Buff 列表显示 |
| `UIMiniMap` / `UIWorldMap` | 小地图/世界地图 |
| `UIChatBar` | 聊天输入栏 |
| `UINpcTalk` | NPC 对话界面 |
| `UIShop` | NPC 商店界面 |
| `UIStorage` | 仓库界面 |
| `CashShop/UICashShop` | 现金商城；展示余额和 NX 商品目录，处理购买及商城仓库双向转移 |
| `UIParty` | 组队界面 |
| `UIKeyConfig` | 键位设置；从 `UIWindow2.img/KeyConfig` 加载完整键盘面板，并以同一份当前素材布局驱动映射图标绘制、点击和拖放命中 |
| `UINotice` | 系统通知 |
| `UISoftKey` | 虚拟按键 (移动端) |

### 通用组件 (`Components/`)
| 文件 | 职责 |
|------|------|
| `Button` / `MapleButton` / `TwoSpriteButton` / `AreaButton` | 各种按钮 |
| `Icon` / `IconCover` | 图标显示 |
| `Gauge` / `Slider` | 进度条/滑块 |
| `Textfield` | 文本输入框（UTF-8 按码点编辑，限长按字节以兼容协议） |
| `ChatBalloon` / `Nametag` | 聊天气泡/名字标签 |
| `Tooltip` / `EquipTooltip` / `ItemTooltip` / `SkillTooltip` / `MapTooltip` | 各种悬浮提示 |
| `ScrollingNotice` | 滚动公告 |

### IME 桥接 (`ImeBridge.h`)

WASM 下把文本输入委托给浏览器隐藏 textarea（`web/index.html` 的
`#ime-input`），由浏览器原生输入法提供候选词窗口。C++ 通过
`EM_ASM` 调 JS 的 `MapleWasmIME.onFocus/onBlur/onText`，JS 通过导出函数
`msime_input`（整段文本 + UTF-16 光标）与 `msime_key`（白名单控制键）回传。
密码字段（`crypt > 0`）不走桥接，保持纯键盘路径。非 WASM 平台为空实现。

## 依赖关系

- **内部依赖**: [图形渲染](../Graphics/index.md) (渲染), [角色系统](../Character/index.md) (数据展示)
- **外部依赖**: Cosmic 服务端 (操作请求)
