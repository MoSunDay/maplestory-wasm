# 仓库（Storage）封包 slots 字段字节宽度错位修复

## 现象

仓库窗口打开后左列恒空：服务端 DB 明明有仓库物品（type=2 行），客户端却一个
都不显示；存入后物品从背包消失但左列不出现，取出动作因无物品可选而无法经 UI
触发——仓库物品被静默丢弃（无任何客户端报错/封包解析异常）。

## 根因（跨仓库协议错位）

服务端 `link_repos/MapleStory-Server/src/tools/MaplePacketCreator.java` 的
STORAGE 系列封包全部以 `writeShort(slots)` 写 2 字节，而 v83 客户端
`src/client/Net/Handlers/NpcInteractionHandlers.cpp` 的 `StorageHandler` 对
全部下行 mode 均按 byte 读 slots：

| mode | 服务端封包 | 客户端解析 |
|------|-----------|-----------|
| 0x16 | `getStorage` | `read_byte()`（全量开窗） |
| 0x13 | `mesoStorage` | `read_byte()`（金币存取） |
| 0x0D | `storeStorage` | `read_byte()`（存入） |
| 0x09 | `takeOutStorage` | `read_byte()`（取出） |
| 0x0F | `arrangeStorage` | `read_byte()`（整理） |

错位 1 字节后客户端后续字段全部左移：0x16/0x0D/0x09 的 count 恰好读到尾部
zero short 的高位字节 = 0 → 物品列表整段被丢弃，窗口"正常"打开但恒空。

## 变更（均为服务端，补丁 0002）

`MaplePacketCreator.java` 五处 `mplew.writeShort(slots);` → `mplew.write(slots);`
（行 3935 getStorage / 3972 mesoStorage / 3985 storeStorage / 4001
takeOutStorage / 4017 arrangeStorage）。`MaplePacketLittleEndianWriter.write(int)`
写单字节，与客户端 `read_byte()` 对齐；slots 为 short 且上限 48 < 128，无截断。

有意不改：行 8480/8489/8498 三处 `writeShort(slots)` 属 CASHSHOP_OPERATION
（`showBoughtInventorySlots` 0x60 / `showBoughtStorageSlots` 0x62 /
`showBoughtCharacterSlot` 0x64），非 STORAGE 封包，且客户端
`CashShopOperationHandler` 无这三个子码的解析（落入 default 提示），不构成
字节宽度错位缺陷。

## 验证

- `bash posix-compile.sh` 重编 `dist/` 后 `docker restart ms-game-server`；
- `tools/verify/maplestory-webui/storage-deposit-flow.mjs` 由"已知缺陷签名
  断言"翻转为真实链路断言：SQL 注入仓库 ETC 物品 → 开窗左列行0 可见 →
  双击取出（左列清空 + DB 断言背包出现）→ 存入（左列出现 + 右列清空 +
  DB 断言仓库侧）→ 重开仓 0x16 左列可见，ALL CHECKS PASSED；
- `storage-flow.mjs`（开窗/布局/关窗）复跑无回归。
