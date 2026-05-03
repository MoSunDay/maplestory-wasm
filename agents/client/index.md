# WASM 客户端

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

将 C++ MapleStory v83 客户端编译为 WebAssembly，使其可在浏览器中运行。客户端负责游戏渲染、用户交互、网络通信和资源加载。

## 边界

- **包含**: 游戏逻辑、渲染、UI、网络、资源加载
- **不包含**: 服务端逻辑、账号系统、Nexon 官方内容分发

## 架构

客户端以单例模式组织核心子系统，入口点为 `Journey.cpp`。各子系统通过单例模式互相引用，无依赖注入层。

### 核心子系统

```
Journey.cpp (入口)
├── GraphicsGL    - 图形渲染引擎 (OpenGL/WebGL)
├── UI            - UI 框架 (登录/游戏状态切换)
│   └── Stage     - 游戏阶段 (地图、角色、实体)
├── Session       - 网络会话 (TCP/WASM Socket)
│   ├── Cryptography - AES + 自定义加密
│   └── PacketSwitch - 封包路由
├── LazyFS        - 按需文件系统
└── Configuration - 本地配置持久化
```

## 关键抽象

- **单例模式**: 所有核心子系统（`GraphicsGL`, `UI`, `Stage`, `Session`, `Configuration`）均为单例，通过 `Singleton<T>` 模板实现
- **UI 状态机**: `UI` 通过 `UIState` 多态在 `LOGIN` 和 `GAME` 两态之间切换，每态包含各自的 `UIElement` 集合
- **封包路由**: `PacketSwitch` 维护 500 个槽位的 `PacketHandler` 数组，按 opcode 索引分发封包
- **LazyFS 拦截层**: 在 Emscripten 文件系统层拦截 `fopen`/`fseek`/`fread` 调用，通过 HTTP Range 请求按需读取 .nx 文件

## 依赖关系

- **内部依赖**: `nlnx` (NX 文件读取), `stb` (图片/字体渲染), `lz4` (压缩), `GL/glew` (OpenGL), `FreeType` (字体)
- **外部依赖**: Cosmic 服务端 (封包通信), LazyFS WS 服务端 (资源流), WebSocket 代理 (TCP 桥接)

## 编译

通过 Emscripten CMake 工具链编译为 `JourneyClient.js` + `JourneyClient.wasm`。编译选项:
- `JOURNEY_USE_CRYPTO`: 启用 AES/Custom 加密
- `JOURNEY_USE_ASIO`: 在非 WASM 平台使用 ASIO 网络库
- `JOURNEY_PRINT_WARNINGS`: 控制台输出警告

## 子模块

| 子模块 | 路径 | 职责 |
|--------|------|------|
| [网络层](Net/index.md) | `Net/` | 会话管理、加密、封包收发与路由 |
| [游戏世界](Gameplay/index.md) | `Gameplay/` | 地图、战斗、物理、实体管理 |
| [角色系统](Character/index.md) | `Character/` | 玩家角色、属性、技能、装备、外观 |
| [UI 系统](IO/index.md) | `IO/` | 输入、UI状态、界面组件 |
| [图形渲染](Graphics/index.md) | `Graphics/` | OpenGL 渲染、纹理、精灵、文字 |
| [按需文件系统](LazyFS/index.md) | `LazyFS/` | 运行时按需加载游戏资源 |

## 辅助模块

| 模块 | 路径 | 职责 |
|------|------|------|
| Data | `Data/` | 从 NX 文件解析物品/技能/职业静态数据 |
| Audio | `Audio/` | 音效与 BGM 播放 |
| Template | `Template/` | 通用模板工具 (Point, Rectangle, Singleton, Cache 等) |
| Util | `Util/` | 通用工具 (Hash, NxFiles 路径, QuadTree, Randomizer) |
| fonts | `fonts/` | 内嵌字体资源 (Arial) |
