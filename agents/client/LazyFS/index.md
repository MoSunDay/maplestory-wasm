# 按需文件系统 (LazyFS)

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

在 Emscripten 环境中实现游戏资源文件的按需加载。通过拦截文件系统 API，将 `.nx` 文件的读取请求转换为 HTTP Range 请求，避免下载整个资源文件。

## 边界

- **包含**: 文件系统拦截、HTTP Range 请求、内存缓存、与 JavaScript 桥接
- **不包含**: 游戏逻辑、NX 文件解析、网络协议

## 关键抽象

### LazyFS 命名空间 (`LazyFS.h`)

简洁的公共 API:
- `Initialize()`: 与 JavaScript 层同步配置，初始化拦截层
- `RegisterFile(filepath, url)`: 注册一个虚拟文件，指定其在 Emscripten 虚拟文件系统中的路径和实际的 HTTP URL

### 工作原理

1. 游戏启动时，通过 JavaScript 获取配置（host/port/protocol）
2. 在 `RegisterFile()` 中注册所有 .nx 文件（如 `Base.nx` → `/assets/Base.nx`）
3. 拦截 `fopen`/`fseek`/`fread` 调用:
   - 第一次访问时，发送 HTTP Range 请求到 LazyFS assets_server.py
   - 将返回的数据缓存到内存
   - 后续访问直接返回缓存数据
4. 通过 JavaScript 层 (`lazyfs.js`) 处理浏览器端的 HTTP 请求和缓存

### 缓存策略

- 内存块缓存：已请求的文件片段存储在内存中
- 浏览器 IndexedDB 缓存：`lazyfs.js` 可选地将完整文件缓存到浏览器 IndexedDB
- 缓存优先级：内存 → IndexedDB → HTTP 请求

## 文件组织

| 文件 | 职责 |
|------|------|
| `LazyFS.h` / `LazyFS.cpp` | 公共 API 和初始化逻辑 |
| `LazyFileBackend.h` / `LazyFileBackend.cpp` | 文件后端抽象，管理文件描述符和回调 |
| `LazyFileLoader.h` / `LazyFileLoader.cpp` | 异步文件加载器，管理 HTTP 请求和回调 |
| `lazyfs.js` | JavaScript 桥接层，处理浏览器端 HTTP 请求和 IndexedDB 缓存 |

## 依赖关系

- **内部依赖**: Emscripten 文件系统 API (虚拟 FS)
- **外部依赖**: assets_server.py (WebSocket 资源服务), 浏览器 IndexedDB API
