Commit: fde7b1761b51ea4cf6d9c5ff2902b63506a86088

# 逻辑结构

Commit: 589b388fb88255db718e2ccd67b1e0a219fdd1f1

## 概述

MapleStory WASM 是 MapleStory v83 客户端的 WebAssembly 移植。C++ 客户端经 Emscripten 编译为 WASM，在浏览器中完成游戏渲染、交互与协议通信；Rust Web 服务层负责分发 WASM 产物、桥接游戏封包（WebSocket → TCP）并按需流式提供 `.nx` 游戏资源。客户端设计为与本地 linked checkout `link_repos/MapleStory-Server` 配合运行。

构建、部署与环境约定见根目录 [AGENTS.md](AGENTS.md)（构建/运行入口），网络协议细节见 [docs/ms-network-protocol.md](docs/ms-network-protocol.md)。

## 模块索引

| 模块 | 路径 | 职责 |
|------|------|------|
| [WASM 客户端](agents/client/index.md) | `src/client/` | 核心 C++ 客户端，编译为 WASM |
| [NoLifeNx 库](agents/nlnx/index.md) | `src/nlnx/` | NX 文件格式读取库 |
| [Web 基础设施](agents/web/index.md) | `web-server/` `ws-proxy/` `assets-server/` `web/` | Rust Web 服务：HTTP 服务器、WebSocket 代理、资源流 |

## 客户端子模块

| 子模块 | 路径 | 职责 |
|--------|------|------|
| [网络层](agents/client/Net/index.md) | `src/client/Net/` | 会话管理、加密、封包收发与路由 |
| [游戏世界](agents/client/Gameplay/index.md) | `src/client/Gameplay/` | 地图、战斗、物理、实体管理 |
| [角色系统](agents/client/Character/index.md) | `src/client/Character/` | 玩家角色、属性、技能、装备、外观 |
| [UI 系统](agents/client/IO/index.md) | `src/client/IO/` | 输入、UI 状态、界面组件 |
| [图形渲染](agents/client/Graphics/index.md) | `src/client/Graphics/` | OpenGL 渲染、纹理、精灵、文字 |
| [按需文件系统](agents/client/LazyFS/index.md) | `src/client/LazyFS/` | 运行时按需加载游戏资源 |

## 功能地图

用户可见能力的分组与说明见 [features/index.md](features/index.md)。
