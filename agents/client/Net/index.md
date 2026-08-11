# 网络层

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

管理客户端与 Cosmic 服务端的 TCP 连接（经 WebSocket 代理），处理封包加密/解密，将接收的封包按 opcode 路由到对应处理器。

## 边界

- **包含**: 会话生命周期、加密层、封包解析、封包路由、封包构建
- **不包含**: 服务端逻辑、业务数据定义、UI 逻辑

## 关键抽象

### Session (`Session.h`)

网络会话单例。管理连接的整个生命周期:
- `init()`: 从配置获取 host/port 并建立连接
- `write(bytes, length)`: 发送加密封包
- `read()`: 帧循环中轮询接收，解密后分发
- `reconnect(address, port)`: 断开旧连接并建立新连接（用于登录→频道切换）。内部经 `resolve_channel_address` 对本地频道地址做配置 IP 替换；末尾置位 `connection_changed` 标志
- `is_connected()`: 连接状态

内部维护接收缓冲区、位置指针、连接状态，以及 `connection_changed` 标志：`reconnect()` 末尾置位，`process()` 在解出一个封包后检查，若已置位则丢弃缓冲区剩余字节并复位——防止在 `forward()` 期间触发重连后，用新加密上下文继续解密旧连接的尾部字节（跨连接污染）。使用 ASIO (非 WASM) 或 Winsock 作为底层 socket 实现。

### 重连路径与 UI 恢复契约

登录服到频道服的切换是必经的 `reconnect()` 点（两条独立 TCP 连接）。该路径的失败/竞态必须恢复 UI（`UI::enable()`），否则角色选择界面会卡在 `disable()` 禁用态：

- `ServerIPHandler`（`Handlers/LoginHandlers.cpp`）：收到 `SERVER_IP` → `reconnect()` 到下发的频道地址 → 仅当 `is_connected()` 才 `PlayerLoginPacket().dispatch()`，否则 `Console` 诊断 + `UI::enable()`。WASM 握手 `ws_recv` 有 15s 超时（替代原先无限等待），频道不可达时 `open()` 返回 false 触发此恢复分支
- `SetfieldHandler::set_field`（`Handlers/SetfieldHandlers.cpp`）：进入游戏前若角色选择界面已消失（重连竞态）或本地无该 cid，两处早退分支均 `Console` 诊断 + `UI::enable()`，而非静默 return

### Cryptography (`Cryptography.h`)

两层加密:
1. **MapleAESOFB**: AES-OFB 模式，使用固定 Key + 会话 IV
2. **MapleCustomEncryption**: 6 轮固定置换，无密钥

发送顺序: Custom → AES → 4 字节头
接收顺序: 读 4 字节头 → AES → Custom → 解析

### PacketSwitch (`PacketSwitch.h`)

封包路由器。维护 500 个槽位的 `PacketHandler` 数组:
- `forward(bytes, length)`: 读取头部 opcode，调用对应 handler
- `emplace<Opcode, HandlerType>()`: 注册 handler

### PacketHandler (`PacketHandler.h`)

封包处理器接口。每个 handler 处理特定 opcode 的业务逻辑。各 handler 分门别类放在 `Handlers/` 目录。

### InPacket / OutPacket

封包读写辅助:
- `InPacket`: 从字节流中按顺序读取不同类型字段 (`read_byte()`, `read_int()`, `read_string()` 等)
- `OutPacket`: 构建封包字节流 (`write_byte()`, `write_int()`, `write_string()` 等)

## 封包流程

### 发送流程
```
Client Code → OutPacket 构建 → Session::write()
→ Cryptography::encrypt() → Socket::write() → Cosmic Server
```

### 接收流程
```
Cosmic Server → Socket::read() → Session::read()
→ Cryptography::decrypt() → PacketSwitch::forward()
→ PacketHandler::handle() → Client Logic
```

## 子目录

| 目录 | 职责 |
|------|------|
| `Handlers/` | 按功能分类的封包处理器 (Login, Common, Setfield, Player, Inventory, Attack, Messaging, NpcInteraction 等) |
| `Handlers/Helpers/` | 封包解析辅助 (ItemParser, LoginParser, MovementParser) |
| `Packets/` | 发送封包构建器 (LoginPackets, GameplayPackets, InventoryPackets 等) |

## 依赖关系

- **内部依赖**: `Configuration` (连接参数), `Stage` (游戏状态更新)
- **外部依赖**: WebSocket 代理 (TCP 桥接), Cosmic 服务端
- **协议文档**: `docs/ms-network-protocol.md` 包含完整 opcode 和封包结构参考
