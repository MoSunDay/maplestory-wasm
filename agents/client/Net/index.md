Commit: b2922f3eef7e2afa16fe954cee477d1ca3831e00

# 网络层

## 职责

管理客户端与 `link_repos/MapleStory-Server` 服务端的 TCP 连接（经 WebSocket 代理），处理封包加密/解密，将接收的封包按 opcode 路由到对应处理器。

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
- `disconnect()`: 主动关闭当前 WebSocket/TCP 连接并清空接收缓冲状态；系统菜单登出先发送标准 `PLAYER_DC(0x0C)`，再调用该入口关闭传输并结束客户端循环
- `is_connected()`: 连接状态

内部独立缓存 4 字节封包头和封包体，可在任意接收分片边界上继续解析，并拒绝小于 opcode 或超过最大缓冲区的长度。`connection_changed` 在 `reconnect()` 末尾置位，`process()` 在解出一个封包后检查，若已置位则丢弃旧连接剩余字节并复位，避免用新加密上下文解密旧流。使用 ASIO (非 WASM) 或 Winsock 作为底层 socket 实现。

WASM 的浏览器到 `ws-proxy` 连接支持从页面主机名或显式 `ProxyIP` 构造 IPv4、域名或带 URI 方括号的 IPv6 URL；每 25 秒发送一个零长度二进制 WebSocket 帧，只维持传输层活动，不向游戏 TCP 流注入数据。游戏协议仍由服务端 `PING` 和客户端 `PONG` 保活；若 Session 非主动断开，主循环停止并通过页面级一次性提示告知游戏已经退出。

### 重连路径与 UI 恢复契约

登录服到频道服的切换是必经的 `reconnect()` 点（两条独立 TCP 连接）。该路径的失败/竞态必须恢复 UI（`UI::enable()`），否则角色选择界面会卡在 `disable()` 禁用态：

- `ServerIPHandler`（`Handlers/LoginHandlers.cpp`）：收到 `SERVER_IP` → `reconnect()` 到下发的频道地址 → 仅当 `is_connected()` 才 `PlayerLoginPacket().dispatch()`，否则 `Console` 诊断 + `UI::enable()`。WASM 握手 `ws_recv` 有 15s 超时（替代原先无限等待），频道不可达时 `open()` 返回 false 触发此恢复分支
- `SetfieldHandler::set_field`（`Handlers/SetfieldHandlers.cpp`）：进入游戏前若角色选择界面已消失（重连竞态）或本地无该 cid，两处早退分支均 `Console` 诊断 + `UI::enable()`，而非静默 return
- 退出现金商城时，空 `CHANGE_MAP` 请求触发服务端 `CHANGE_CHANNEL`；`ChangeChannelHandler` 重连原频道并发送当前 cid 的 `PLAYER_LOGGEDIN`。随后 `SET_FIELD` 直接复用现有 `Player`，用完整角色快照重置服务端所有进度数据，不依赖已销毁的角色选择 UI

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

其他玩家的可见 Buff 按掩码位序完整解析后一次性提交到角色状态；截断封包在 `InPacket` 报错时不会留下部分斗气或属性充能状态。

其他玩家出生包 `SPAWN_PLAYER` 的头部由独立解析器读取 `charId:int`、`level:short` 和 `name:string`。linked server 的 custom-client 模式使用 16 位等级；若按旧版单字节读取，后续姓名长度会错位并使整个远端玩家出生包被丢弃，表现为能看到怪物和掉落变化但看不到具体玩家。

场地包 `CLOCK` / `STOP_CLOCK` 驱动游戏内场地计时器；`FIELD_EFFECT` 的 2、3、4 模式分别处理具名地图对象关闭、一次性视觉效果和场地音效。服务端 `Party1/Clear`、`Party1/Failed` 会按 `Sound.nx/Field.img` 兼容路径解析，组队任务开门则按地图对象的 `name` 精确停用。

怪物出生包 `SPAWN_MONSTER` / `SPAWN_MONSTER_CONTROL` 共用无应用状态依赖的 `SpawnMobPayload` 解析器。linked server 在坐标前固定写入 22 字节出生状态块；假怪显形包额外写入 6 字节。解析器按已存在的怪物 oid 区分显形布局，并完整消费坐标、姿态、foothold、出生特效、父怪物、队伍和物品特效尾段；未知布局或截断封包直接抛出 `PacketError`，不会以默认坐标生成怪物。

### 怪物动作、状态与雾协议

- 控制端通过 `MOVE_MONSTER(0xBC)` 上报递增 `moveId`、原始动作、技能 ID/等级、起点与标准移动片段。动作值 24–41 对应 `attack1..attack9`，42–59 对应技能 action1–action9，低位承载朝向。
- `MOVE_MONSTER_RESPONSE(0xF0)` 回显序列、服务端仇恨状态和当前 MP，并可下发一个绑定该序列的技能建议。客户端只接受与最新已发序列完全一致的响应；建议在实际施法上报后一次性清除。
- `MOVE_MONSTER(0xEF)` 广播携带服务端接受后的原始动作。`MobMovedHandler` 完整消费保留位、动作/技能、option、起点和移动片段，远端怪物不会从移动姿态猜测攻击。
- `TAKE_DAMAGE(0x30)` 的 0–8 表示对应普通攻击动作；linked server 只接受已授权、未过期、仍在 WZ 范围内且该角色尚未消费的命中。`-1` 继续表示接触伤害。玩家攻击伤害沿用 v83 的负数暴击编码，发送与接收均保留逐段暴击位，供硬皮等服务端规则判定。
- `APPLY/CANCEL_MONSTER_STATUS(0xF2/0xF3)`、`SPAWN/REMOVE_MIST(0x111/0x112)` 已注册真实处理器；状态包截断会抛出 `PacketError`，雾包按完整 owner/skill/level/delay/rectangle 布局消费。
- `GIVE_BUFF(0x20)` 同时支持普通技能 Buff 与怪物疾病条目。疾病使用 `value:short + mobSkillId:short + level:short + duration:int`，不会误按玩家技能 ID 解析。

### 好友与组队协议

`Handlers/Social` 处理 `BUDDYLIST(0x3F)`、`PARTY_OPERATION(0x3E)`、队伍值和成员 HP。好友解析器明确支持全量列表 mode 7、请求 mode 9、频道变化 mode 20 和容量 mode 21；添加、接受、拒绝/删除统一通过 `BUDDYLIST_MODIFY(0x82)` 发出。队伍处理器消费创建、邀请、成员快照、退出/踢出、队长变化和错误状态，并把窗口无关的完整状态提交给状态栏。

好友聊天 `MULTI_CHAT(0x77)` 携带当前好友角色 ID 列表；队伍聊天由服务端依据当前队伍路由。收到 `MULTICHAT` 后按类型更新消息计数并显示聊天行。好友与队伍的解析/状态转换可通过 `SocialProtocol` 和 `SocialState` 在不启动 UI 的情况下验证。

### 玩家交易协议

`PLAYER_INTERACTION(0x7B)` 复用 v83 交易模式：创建房间、邀请地图内角色、接受/拒绝、聊天、放入物品/金币、确认和退出。`TradeHandler` 消费 `PLAYER_INTERACTION(0x13A)` 的房间、参与者、物品、金币、确认与结束事件；交易物品负载由 `TradeProtocol` 完整消费普通物品、装备和宠物分支。所有成功或失败结论以服务端结果为准，客户端只维护展示状态和发送意图。

### 注册复用登录协议

游戏内注册不引入新 opcode：`UIRegister` 校验账号为 4–12 位 ASCII 字母数字、密码为 4–12 位非空白可打印 ASCII 后发送现有 `LOGIN_PASSWORD`。服务端对不存在的合法账号自动建号并返回 `LOGIN_STATUS=23`，客户端自动发送 `ACCEPT_TOS`，随后沿用登录成功和世界列表流程。注册失败由 `LoginHandlers` 回填到注册表单，不落入通用登录位图提示。

### InPacket / OutPacket

封包读写辅助:
- `InPacket`: 从字节流中按顺序读取不同类型字段 (`read_byte()`, `read_int()`, `read_string()` 等)；读取数值前先整体检查剩余长度，linked server 的 Java 定长 UTF-8 字段先编码再补零到固定字节数，客户端必须按线上字节宽度消费
- `OutPacket`: 构建封包字节流 (`write_byte()`, `write_int()`, `write_string()` 等)

登录 `CHARLIST` / 新建角色 / 进入地图复用 `LoginParser::parse_stats`。其字段宽度必须与 linked server 的 `addCharStats` 一致：角色名固定 13 字节，custom-client 模式的等级为 short；Evan 职业（2001、2200–2218）的剩余 SP 为多池表，其余职业为单个 short。宠物、队伍、戒指和现金礼物的 Java 定长字段遵循同一字节宽度规则。

角色创建在最终 `CREATE_CHAR` 前再次发送 `CHECK_CHAR_NAME`，避免外观定制期间名称被占用。创建 payload 严格按 linked server 当前 `USE_CUSTOM_CLIENT=true` 合约写入名称、`job/face/packedHair/skin/top/bottom/shoes/weapon` 八个整数和性别字节，其中 `packedHair = baseHair + hairColor`；纯编码函数为字段顺序和字节宽度提供回归边界。服务端对名称不可用或角色槽已满可能静默返回，对数据库插入失败则复用 `DELETE_CHAR_RESPONSE(state=9)`；`UICharCreation` 与 `DeleteCharResponseHandler` 会把这些结果收敛为可恢复状态，重新开放输入或定制控件。

`CharacterDataParser` 消费 `SET_FIELD` 的小游戏、三类戒指和新年贺卡附加数据。linked server 当前小游戏列表固定为空，非零计数属于不支持的合约变更并立即抛出 `PacketError`；新年贺卡无论是否为空都完整消费，保证后续区域信息与尾字段保持对齐。

自然恢复通过 `HEAL_OVER_TIME` 上报 HP/MP 增量；物品椅子使用 `USE_CHAIR`/`CANCEL_CHAIR`，其他角色的椅子外观由 `SHOW_CHAIR` 同步。本地 `CANCEL_CHAIR` 回执通过不回发封包的状态入口应用，避免服务端取消与客户端取消相互回声。

现金商城使用 `ENTER_CASHSHOP` 进入，由 `SET_CASH_SHOP` 建立完整角色快照和特殊商品状态；`QUERY_CASH_RESULT` 同步三类余额，`CASHSHOP_OPERATION` 处理商城仓库初始化、单品/礼包购买、仓库双向转移和明确错误。现金物品解析保留服务端唯一 `cash_id`，作为转移请求的稳定标识；取出响应若因物品分类未携带现金标志，则只在该响应上下文中沿用请求对应的 `cash_id`，同时仍按线上标志决定封包字段宽度，避免后续存回失去身份或解析错位。

NPC 商店操作 `NPC_SHOP_ACTION` 在原购买、单件出售、补充和退出模式后追加 mode 4，负载为当前物品栏分类字节；服务端以 `CONFIRM_SHOP_TRANSACTION(0x132)` 返回一次批量操作结果。客户端注册该回包，0/8 静默成功，现金物品出售等失败显示明确提示。

宠物链路使用 v83 原有协议：`SPAWN_PET(0x62)` 发送现金栏字节槽位，`MOVE_PET(0xA7)` 携带宠物唯一 ID 与标准移动片段，`PET_LOOT(0xAA)` 携带唯一 ID 和掉落 oid，`PET_AUTO_POT(0xAB)` 携带宠物索引、唯一 ID、USE 槽和物品 ID。`AUTO_HP_POT(0x150)` / `AUTO_MP_POT(0x151)` 回包恢复角色级药水选择；客户端只负责意图与显示，linked server 对每次拾取和服药做最终校验。

## 封包流程

### 发送流程
```
Client Code → OutPacket 构建 → Session::write()
→ Cryptography::encrypt() → Socket::write() → linked MapleStory Server
```

### 接收流程
```
linked MapleStory Server → Socket::read() → Session::read()
→ Cryptography::decrypt() → PacketSwitch::forward()
→ PacketHandler::handle() → Client Logic
```

## 子目录

| 目录 | 职责 |
|------|------|
| `Handlers/` | 按功能分类的封包处理器 (Login, Common, Setfield, Player, Inventory, Attack, Messaging, Social, Trade, NpcInteraction, CashShop 等) |
| `Handlers/Helpers/` | 封包解析辅助 (CharacterDataParser, ItemParser, LoginParser, MovementParser) |
| `Packets/` | 发送封包构建器 (LoginPackets, GameplayPackets, InventoryPackets, SocialPackets, TradePackets, CashShopPackets 等)；`CharacterCreation/` 保存可独立验证的建角 payload 编码 |
| `Protocol/` | 可独立验证的协议解析与编码，包括好友事件、远端玩家和怪物出生负载 |

## 依赖关系

- **内部依赖**: `Configuration` (连接参数), `Stage` (游戏状态更新)
- **外部依赖**: WebSocket 代理 (TCP 桥接), `link_repos/MapleStory-Server` 服务端
- **协议文档**: `docs/ms-network-protocol.md` 包含完整 opcode 和封包结构参考
