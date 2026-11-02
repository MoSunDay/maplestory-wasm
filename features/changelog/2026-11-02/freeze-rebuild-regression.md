# P0 冻结重建回归：交付门禁闭环（冻结点 HEAD=081ff80）

> 记录冻结日的交付门禁执行全程：干净源树全量重建 → 产物纯度三连 →
> 部署 → 服务端冷重启 → 回归矩阵 → 基线采样。本条不含代码变更，仅
> 运维透明记录；观察窗起点 **2026-11-02 23:00:06 +0800**，窗内判据见
> `docs/roadmap.md`「交付观察窗（24h）」。

## 干净源树全量重建

- 冻结点：HEAD=081ff80（docs(webui): 补录历史验证脚本 28 项到 index
  分册，roadmap #3 挂接看门狗 changelog）。
- 源树来源：`git archive` 导出冻结点（不含工作区脏文件与构建产物），
  送入 docker wasm-builder（emsdk 4.0.21）全量重建。
- 重建窗口：22:50:13 → 22:51:51（含产物落盘）。

## 产物纯度三连与部署

| 检查 | 结果 |
|---|---|
| `ms-disconnect-banner` 符号出现次数 | 1（断线看门狗在包内） |
| `scheduleBootWarmup` 符号出现次数 | 1（预热拆分在包内） |
| 并发实验 token | 0（无实验性残留） |

- 旧产物备份至 `build.bak-pre-freeze/`（21:28 版本）后部署
  `build/JourneyClient.{js,wasm}`；部署产物 md5 与干净构建输出一致
  （js=0606e3de…、wasm=0b3bdf31…），ms-web-server `:8000` HTTP 200
  （`/` 与 `/build/JourneyClient.js` 均 200）。

## 服务端冷重启与端口

- `scripts/game_server_restart.sh` 冷重启 ms-game-server。
- 观察窗 24h 起点 **2026-11-02 23:00:06 +0800**；重启后
  8484（登录）+ 7575/7975（世界/频道）全 LISTEN。

## 回归矩阵（全绿）

| 项 | 结果 |
|---|---|
| lazyfs-connection 单测 | PASS |
| 冷缓存登录 + 选角 | PASS（82s） |
| 完整进图 run.mjs（canonical） | PASS（P95 16.7ms） |
| cash-shop-equip-permanent-flow | PASS（23 断言，"ok":true） |
| 暖缓存复跑 | PASS（108s → 44s） |

冻结回归另发现 cash-shop-equip-permanent-flow 在预热拆分
（47550c8）后未等 assets idle 即输入、连跑第二次卡登录（reason=4），
已按 run.mjs 同位置等价修复并实跑全绿，见 59d73a0。

## 基线采样（观察窗对照用）

- `MySQLSyntaxErrorException` 计数 = 0（rank 保留字补丁 0009 生效）。
- `maplestory.characters` 行数 = 52。
- rank 总榜基线已采样（1-45 名非零、rankMove 有值，判据见 roadmap ①）。
- SaveChar 存档活跃：`/data/MapleStory-Server/logs/2026-11-02/players/
  SaveChar.txt` 持续有写入（容器挂载 /data/MapleStory-Server → /srv，
  仓库 `link_repos/MapleStory-Server` 为其符号链接）。

## 运维透明记录

- 测试账号 test1（id63）：冻结回归前 NX +7400 补给，本次商城流程
  实耗 3700（余 7150）；无 DB 手工造数，余额经游戏内商城链路消耗。

## 观察项交接

- PLAYER_LOGGEDIN 进图时每次 1 次 collations 混用 SQLException
  （不阻塞、存档正常，存量服务端疑点）：列入 24h 观察窗跟踪，见
  `docs/roadmap.md`「观察项」新增条目。
