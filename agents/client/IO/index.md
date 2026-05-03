# UI 系统

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

管理用户界面、输入处理和 UI 状态切换。`UI` 是顶层 UI 框架，通过 `UIState` 多态在登录状态和游戏状态之间切换。

## 边界

- **包含**: UI 框架、屏幕组件、键盘/鼠标输入、光标管理
- **不包含**: 游戏世界渲染、网络通信、数据持久化

## 关键抽象

### UI (`UI.h`)

UI 框架单例。核心职责:
- `change_state(LOGIN/GAME)`: 切换 UI 状态（登录界面/游戏界面）
- `update()` / `draw()`: 更新和渲染当前状态的 UI 元素
- 输入分发: `send_cursor()`, `send_key()`, `send_scroll()`, `doubleclick()`
- `emplace<T>()`: 动态创建 UI 元素
- `get_element<T>()`: 获取 UI 元素引用

### UIState (`UIState.h`)

UI 状态接口。两个具体实现:
- `UIStateLogin`: 登录流程状态（登录、世界选择、角色选择、角色创建）
- `UIStateGame`: 游戏内状态（状态栏、背包、技能、聊天等）

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
| `UILoginNotice` | 登录通知弹窗 |
| `UILoginWait` | 登录等待动画 |
| `UIWorldSelect` | 大区/频道选择 |
| `UICharSelect` | 角色选择 |
| `UICharCreation` | 角色创建 |

### 游戏内 UI (`UITypes/`)
| 文件 | 职责 |
|------|------|
| `UIStatusBar` | 底部状态栏 (HP/MP/EXP/等级) |
| `UIStatsInfo` | 角色属性窗口 |
| `UISkillBook` | 技能书窗口 |
| `UIEquipInventory` | 装备栏窗口 |
| `UIItemInventory` | 物品栏窗口 |
| `UIBuffList` | Buff 列表显示 |
| `UIMiniMap` / `UIWorldMap` | 小地图/世界地图 |
| `UIChatBar` | 聊天输入栏 |
| `UINpcTalk` | NPC 对话界面 |
| `UIShop` | NPC 商店界面 |
| `UIStorage` | 仓库界面 |
| `UIParty` | 组队界面 |
| `UIKeyConfig` | 键位设置 |
| `UINotice` | 系统通知 |
| `UISoftKey` | 虚拟按键 (移动端) |

### 通用组件 (`Components/`)
| 文件 | 职责 |
|------|------|
| `Button` / `MapleButton` / `TwoSpriteButton` / `AreaButton` | 各种按钮 |
| `Icon` / `IconCover` | 图标显示 |
| `Gauge` / `Slider` | 进度条/滑块 |
| `Textfield` | 文本输入框 |
| `ChatBalloon` / `Nametag` | 聊天气泡/名字标签 |
| `Tooltip` / `EquipTooltip` / `ItemTooltip` / `SkillTooltip` / `MapTooltip` | 各种悬浮提示 |
| `ScrollingNotice` | 滚动公告 |

## 依赖关系

- **内部依赖**: [图形渲染](../Graphics/index.md) (渲染), [角色系统](../Character/index.md) (数据展示)
- **外部依赖**: Cosmic 服务端 (操作请求)
