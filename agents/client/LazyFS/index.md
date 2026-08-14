Commit: 12dd3160bc45b2fdf18da6c1f740a3e705002e0a

# 按需文件系统 (LazyFS)

## 职责

在 Emscripten 环境中实现游戏资源文件的按需加载。通过拦截文件系统 API，将 `.nx` 文件的读取请求转换为对资源服务器的 WebSocket 分块请求，避免下载整个资源文件。

## 边界

- **包含**: 文件系统拦截、WebSocket 分块请求、两级块缓存、与 JavaScript 桥接
- **不包含**: 游戏逻辑、NX 文件解析、封包网络协议

## 关键抽象

### LazyFS 命名空间 (`LazyFS.h`)

简洁的公共 API:
- `Initialize()`: 同步 C++ 侧块大小 (`LazyFileBackend::CHUNK_SIZE`) 到 JavaScript 层，并确定资源 WebSocket URL。URL 来源优先级: 显式配置或 query 参数 `assets_url` > `AssetsServerIP/Port/Protocol` 配置 > 按页面地址自动探测 (默认 8765 端口)；裸 IPv6 地址会自动补 URI 方括号
- `RegisterFile(filepath, url)`: 在虚拟文件系统中注册一个文件；注册时即通过 WebSocket `get_size` 获取文件大小，数据本身延迟到首次读取时加载
- `StartItemAssetPreload()`: 首次进入游戏后启动一次非阻塞物品素材预载；失败后再次进入游戏可重试

### 工作原理

1. 游戏启动时调用 `Initialize()`，与 JavaScript 层同步配置
2. `RegisterFile()` 注册所有 .nx 文件（如 `Base.nx`），并经 WebSocket 获取文件大小
3. 拦截 `fopen`/`fseek`/`fread` 调用，按块读取:
   - 首次访问某块时，通过 WebSocket 发送 `get_chunks` 批量请求到 assets-server
   - 服务端以二进制帧返回块数据（4 字节块索引 + 文件名 + 原始数据），`chunks_done` 标记批次结束
   - 返回的块写入内存缓存与 IndexedDB，后续访问直接命中缓存
4. 浏览器端连接由 `lazyfs_connection.js` 管理：并发调用共享同一个连接 Promise；每 25 秒通过只读 `get_size` 保活；意外断线按 1/2/4 秒重试并在新连接上重放未完成请求。重试耗尽后拒绝待处理任务，由前台素材错误态允许用户重新建连重试
5. 后台范围预取由固定 4 路队列调度；地图构造时收集当前地图所有静态与初始实体贴图并持续异步驻留，地图安全门完成后再预取完整 `String.nx`、`Item.nx` 和 `Character.nx` 元数据，避免两个大批次争抢首屏资源
6. 贴图实际进入绘制路径但范围尚未驻留时，原预取任务会提升为前台任务并越过后台并发队列；同步前台读取也使用同一套请求键去重
7. 地图进入期间始终显示带进度的阻塞遮罩，失败后停留并允许重试；正常游戏中的可见缺块/图集缺图也会冻结模拟并使用同一遮罩，纯后台预取不显示遮罩

### 缓存策略

- 内存块缓存：当前会话已请求的文件块
- 浏览器 IndexedDB 缓存：`lazyfs.js` 将文件块持久化到 IndexedDB（键带版本标签，跨浏览器重启有效）
- 缓存优先级：内存 → IndexedDB → WebSocket 请求
- 物品预载不设置应用级过期时间，并请求浏览器持久存储权限；浏览器仍可能因用户操作或存储压力回收数据
- 物品全量预载只在浏览器明确授予持久存储权限后运行；公网 HTTP 等不具备该能力的上下文标记为 `unavailable` 并跳过批量任务，继续使用标准按需读取，避免无持久收益的大批资源占用队列和内存；已启动预载的任一缓存写失败为 `failed`
- 所有已实例化纹理先异步预取对应范围，范围进入内存后再由图形层按帧预算解压并上传 WebGL，避免网络等待阻塞渲染循环

## 文件组织

| 文件 | 职责 |
|------|------|
| `LazyFS.h` / `LazyFS.cpp` | 公共 API 和初始化逻辑 |
| `LazyFileBackend.h` / `LazyFileBackend.cpp` | 文件后端，管理文件描述符，经 JS 桥接按块读取（支持 read/seek/stat 与 mmap 风格随机访问） |
| `LazyFileLoader.h` / `LazyFileLoader.cpp` | 单文件分块加载器，维护已加载块的 LRU 缓存 |
| `lazyfs_connection.js` | WebSocket 生命周期、保活、有限重连和未完成请求重放 |
| `lazyfs.js` | JavaScript 文件操作层，处理分块响应、预取与内存/IndexedDB 两级缓存 |

## 依赖关系

- **内部依赖**: Emscripten 文件系统 API (虚拟 FS)、Asyncify (异步等待 WebSocket 响应)
- **外部依赖**: assets-server (WebSocket 资源服务), 浏览器 IndexedDB API
