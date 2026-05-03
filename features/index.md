# 功能能力索引

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 概述

MapleStory WASM 是一个运行于浏览器中的 MapleStory v83 客户端。所有功能通过 WebAssembly 在浏览器中实现，经由 WebSocket 代理与 Cosmic 服务端通信。

## 功能分组

### 登录与认证

- **账号登录**: 用户名/密码认证
- **世界选择**: 选择服务器大区和频道
- **角色创建/删除**: 创建新角色或删除已有角色
- **角色选择**: 选择角色进入游戏世界
- **PIC 认证**: 二次密码验证

相关模块: [网络层](agents/client/Net/index.md), [UI 系统](agents/client/IO/index.md)

### 游戏世界

- **地图加载与渲染**: 加载 .nx 地图数据，渲染背景、图块、对象
- **角色移动**: 行走、跳跃、攀爬、游泳等物理移动
- **传送门**: 地图间瞬移
- **NPC 交互**: 对话、商店、仓库等
- **反应器**: 可交互物体（如采集物）
- **掉落物**: 物品/meso 的拾取
- **小地图/世界地图**: 地图导航

相关模块: [游戏世界](agents/client/Gameplay/index.md), [图形渲染](agents/client/Graphics/index.md)

### 战斗系统

- **普通攻击**: 近战/远程攻击
- **技能使用**: 各职业技能施放
- **伤害数字显示**: 伤害数值渲染
- **Buff 管理**: 增益/减益效果
- **怪物 AI**: 怪物移动和行为

相关模块: [游戏世界](agents/client/Gameplay/index.md), [角色系统](agents/client/Character/index.md)

### 物品与装备

- **物品栏管理**: 消耗品、装备、其他物品
- **装备系统**: 穿戴/卸下装备，属性变更
- **商店交易**: NPC 商店购买/出售
- **仓库存储**: 物品存储

相关模块: [角色系统](agents/client/Character/index.md), [UI 系统](agents/client/IO/index.md)

### 技能系统

- **技能书管理**: 技能学习、升级
- **技能冷却**: 冷却计时
- **被动技能**: 被动效果计算

相关模块: [角色系统](agents/client/Character/index.md)

### 社交

- **聊天系统**: 全局、私聊、频道消息
- **组队**: 组队信息显示
- **状态消息**: 在线状态/Buddy 消息

相关模块: [UI 系统](agents/client/IO/index.md)

### 资源流式加载

- **LazyFS**: 按需通过 HTTP Range 请求加载 .nx 文件
- **浏览器缓存**: 本地 IndexedDB 缓存已加载资源，减少重复下载

相关模块: [按需文件系统](agents/client/LazyFS/index.md)

### 音频

- **BGM 播放**: 背景音乐
- **音效**: 攻击、技能等音效

## 变更记录

变更日志入口: [features/changelog/](features/changelog/)
