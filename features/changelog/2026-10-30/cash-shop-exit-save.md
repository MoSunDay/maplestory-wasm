# 现金商城退出丢档：退出分支补 saveCharToDB

## 症状

商城内任意变更——取出（take）、放回（put）、购买（buy）——在**退出商城
回游戏**后全部丢失；重进商城看到的是进店前的旧数据。

## 根因（link_repos/MapleStory-Server）

`ChangeMapHandler` 的空包分支（CS 退出）只做 `getCashShop().open(false)` +
channel change，随后下发的换线封包让客户端重连、**从数据库整建角色**
（`MapleCharacter.readFromDB` 重建 CashShop 并按 `type=3 AND accountid`
重载 locker）。进入商城时 `EnterCashShopHandler` 末尾有
`mc.saveCharToDB()`，退出分支却漏了 save——商城内的内存变更从未落盘，
重连即回滚。

## 修复

`src/net/server/channel/handlers/ChangeMapHandler.java` CS 退出分支在
`open(false)` 后补 `chr.saveCharToDB(false)`。`saveCharToDB` 内
`cashshop.save(con)`（:8763）无 isOpen 守卫，locker 的 DELETE+INSERT 与
CASH 余额、wishlist 均随该 save 落盘。

## 验证

`cash-shop-take-reject-retry.mjs`（tools/verify/maplestory-webui/）phase D：
退出商城 → `!clearslot cash` → 重进 → locker 计数与退出前一致、重试取出
成功；服务端 `[take-debug]`（验证期临时插桩，已回退）确认 addResult 落位。

补丁留档：`scripts/server-patches/0006-cash-shop-exit-save.patch`（含应用
与验证命令）。

## 备注（测试床竞态，非产品 bug）

e2e 之间以 SQL 重置 locker（`type=3 AND accountid` DELETE+INSERT）时，
若上一轮被超时击杀的浏览器其**服务端异步退出 save** 尚未落盘，会在重置
之后写回旧数据（曾观察到 5210000 出现两行）。重置前应确认上一轮角色已
完全下线（`Attempting to save` 不再新增），或重启 ms-game-server。
