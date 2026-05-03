# NoLifeNx 库

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

独立的 C++ 库，用于读取 MapleStory `.nx` (NoLife) 文件格式。提供 NX 文件的打开、节点遍历、位图/音频提取功能。

## 边界

- **包含**: NX 文件格式解析、节点树遍历、类型化值读取、位图/音频提取
- **不包含**: 游戏逻辑、网络通信、渲染

## 关键抽象

- **`nl::node`**: NX 树节点的访问器，提供类型化子节点访问 (`x()`, `y()`, `get<T>()`) 和音频/位图提取
- **`nl::file`**: NX 文件打开和管理入口
- **`nl::bitmap`**: 位图数据封装
- **`nl::audio`**: 音频数据封装

## 主要流程

1. `nl::file::open(path)` 打开 .nx 文件
2. 通过 `nl::file::root()` 获取根节点
3. 通过 `node::operator[](name)` 或 `node::operator[](index)` 遍历子节点
4. 使用 `node::x()` 返回子节点数，`node::y()` 返回子节点迭代
5. 通过 `node::get<T>()` 或 `node.get<T>(name)` 读取类型化值
6. 通过 `node::bitmap()` / `node::audio()` 提取多媒体资源

## 依赖关系

- **库依赖**: `lz4` (LZ4 解压缩)
- **使用者**: `src/client/` (通过 `nxfwd.hpp` 等头文件引用)
- **无网络/UI/文件系统依赖**: 纯解析库

## 文件组织

| 文件 | 职责 |
|------|------|
| `nx.hpp` / `nx.cpp` | 主入口，打开/管理 NX 文件 |
| `node.hpp` / `node.cpp` | 节点访问 API |
| `file.hpp` / `file.cpp` | NX 文件格式解析 (header, node tables) |
| `bitmap.hpp` / `bitmap.cpp` | 位图提取 |
| `audio.hpp` / `audio.cpp` | 音频提取 |
| `nxfwd.hpp` | 前向声明 |
| `file_impl.hpp` / `node_impl.hpp` | 内部实现细节 |
