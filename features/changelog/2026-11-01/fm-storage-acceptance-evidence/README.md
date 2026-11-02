# FM 仓库 NPC 浏览器级验收证据归档（2026-11-01）

commit f0fd5b5「FM 仓库 NPC 全链路 e2e 与装备块对齐修复」的验收证据。
账号 test2（card0=test2ILA，world=4），FM 910000000 储物 NPC 1100000，
30 格 Trunk 窗口全矩阵，attempt 1 一次全绿（25 项 PASS）。

运行命令要点：`CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/fm-storage-flow.mjs`
（:8000 新构建；外层循环 /tmp/fm-retry10.sh 吸收 asyncify 楔死偶发，
本轮无楔死、未触发重试）。

## 文件说明

- fm-main12-evidence.log：完整 e2e 输出（25 PASS + attempt 1 rc=0
  ALL_CHECKS_PASSED）。关键断言：ETC 混存 diff=69.6（skip(30) 修复）、
  满仓拒存 notice ESC x1、取出 DB 命中 4000001 x5（页签索引而非装备）、
  EQUIP 图标 diff=7.9、存入后 front=15 dismiss x0（无误关窗）、
  存入 DB 4000003 x1。
- browser-console-filtered.log：非 LazyFS 噪声的浏览器 console——
  login→channel→STORAGE 全链路 stdout（含 Login failed reason=4 的
  凭据错排障痕迹）。
- screenshots.sha256：00-07 截图哈希（截图本体不入库；01=ETC tab、
  02=满仓 notice、03=取出后、04=装备取出后、05=现金商城存档点、
  06=存入后左列行1、07=存入后存档点）。

## 复算说明

harness 见 tools/verify/index.md fm-storage-flow 条目；重跑需
ms-web-server(:8000) / ms-ws-proxy(:8090) / ms-game-server（六口全监听）
/ assets-server(:8765) 在运行，storages.slots=30、FM plife 行 1100000
（0006 补丁 + DB 迁移）就位。
