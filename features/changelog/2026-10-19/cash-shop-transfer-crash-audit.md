# 商城道具取出崩溃排查

## Context

用户反馈商城道具在取出/操作时崩溃。排查范围限定在现金商城仓库与角色背包之间的
转移链路（`0x0D` 取出、`0x0E` 放回），覆盖普通道具、装备、CASH 道具、宠物、
戒指与真实戒指等目录类型；坐骑数据路径做了静态审计，未纳入最终浏览器通过项。

## 根因一：客户端 `std::length_error`（已修复）

2026-10-18 的浏览器异常为 `std::length_error: vector`，调用栈落在 Asyncify
包装的导出函数内。对应时间点服务端没有商城封包异常，客户端也没有收到错误响应。

根因是 `assets-server` 缓存了 NX 文件长度，而本地化脚本原地追加改写了
`String.nx`，客户端拿到新 header + 旧长度后被截断，字符串表越界。修复见
`assets-server-stale-nx-reload.md`（按需重载磁盘上已改写的 NX 条目）；修复后
商城全链路 e2e 浏览器异常为 0。

## 根因二：服务端取出悬空 ringid 触发 NPE（linked server，未在本仓库修改）

客户端侧没有对应崩溃，但服务端日志可复现另一条独立问题：

1. `CashOperationHandler` 的 `0x0D` 先 `chr.getInventory(...).addItem(item)`，
   再发送 `takeFromCashInventory`（`0x68`）。
2. 若 `item instanceof Equip && equip.getRingId() >= 0`，紧接着调用
   `MapleRing.loadFromDb(ringid)` 和 `chr.addPlayerRing(ring)`。
3. 当 `rings` 表没有该 `ringid` 行时，`loadFromDb` 返回 `null`，
   `addPlayerRing(null)` 在 `MapleCharacter.addPlayerRing` 读取 `getItemId()`
   时抛 `NullPointerException`。
4. 因为 `0x68` 已经先发出，客户端会正常显示取出成功，所以浏览器 e2e 仍为
   PASS；异常只出现在服务端 `CashOperationHandler.txt`。

角色登录装载路径已经在 `MapleCharacter` 中对 `loadFromDb` 的 null 做了跳过，
商城取出路径缺少同样的防护。正常服务端 `createRing` 会同时写入两条 `rings`
行，因此该问题需要损坏/缺失的关系数据才会触发；建议 linked server 在
`CashOperationHandler` 补齐 null 防护，并校验 `inventoryequipment.ringid`
引用完整性。

## Verification

- 普通/装备/CASH/宠物/戒指/真实戒指等取出与放回流程 e2e 均为
  `PASS ... flow completed`，`browser exceptions: 0`。
- 悬空 ringid 场景客户端 PASS，服务端稳定复现
  `Cannot invoke "client.MapleRing.getItemId()" because "<parameter1>" is null`。
- 商城纯函数与素材契约测试：`PASS cash shop view policy`、`PASS modern
  CashShop.img previews/badges/currency/locker-cover`。
