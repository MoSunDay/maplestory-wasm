# NPC 对话与物品名称全面中文化

## Context

客户端渲染链路（FreeType + DroidSansFallback CJK 回退、UTF-8 排版、浏览器 IME）早已支持中文，但游戏内容仍残留大量英文：`String.nx` 中约 650 条物品/NPC/怪物/地图/宠物名称为英文或韩文残留；linked server 的 712 个 NPC 脚本与 260 个 Quest 脚本对话全部为英文。

## Change Summary

- 新增 `scripts/nx-l10n/`：NX 字符串表原地重写工具（`apply.py`）与三份翻译数据模块。工具在文件尾部追加新字符串数据与偏移数组，并 patch header 的 `string_offset`，节点表与字符串索引不动。
- 对 `nxFiles/String.nx` 与 `assets/String.nx` 应用 519 条翻译：Cash/Consume/Eqp/Etc/Ins/Map/Mob/Npc/Pet 各表的 `name`/`mapName`/`streetName` 条目（含 `MISSING NAME` 之外的卷轴、结婚道具、Masteria 装备、活动地图与韩服残留 NPC 名）。
- linked server `scripts/npc/`（633 个含对话文件，3888 处 `cm.send*`）与 `scripts/quest/`（249 个含对话文件）的对话文本全部译为简体中文；格式宏（`#b/#k/#L#l/#m/#p/#t` 等）、`\r\n`、拼接结构、逻辑与注释逐字节保留。
- 跨文件口令校验同步：`npc/211101{7,8,9}.js` 的 `"my love Phyllia"` 与 `npc/2091009.js` 的 `"Actions speak louder than words"` 改为与 quest 题面一致的中文口令，避免任务卡死。
- 客户端 `src/client/Net/Handlers/NpcInteractionHandlers.cpp`：仓库/商店错误提示 11 条译为中文。

## Impact Surface

- 客户端物品悬停提示、商城、NPC 对话标题、地图/怪物/宠物名称、小地图地名。
- linked server NPC/Quest 对话（服务器为外部 checkout，不在本仓 Git 管理内）。
- 不改变 NX 节点结构、封包格式、数据库与构建产物。

## Validation

- `python3 scripts/nx-l10n/apply.py <nx> --dry-run`：纯英文残留仅 `BOSS`（内部测试怪，有意保留）。
- 重写后的 `String.nx`：81197 条字符串全部 UTF-8 可解码，抽查 `2040045=头盔防御卷轴50%`、`4031367=爱的证明`、`9201142=女巫马拉迪`、`680000110=白色婚礼圣坛/阿莫利亚`。
- 972 个 JS 全量 `node --check`：仅 `npc/9900000.js` 失败，为原有 Rhino E4X `for each(` 语法（服务器 Nashorn 支持，Node 不支持），与翻译无关。
- 英文残留扫描：`send*` 调用中仅注释内英文与宠物指令关键字（`sit`/`bad` 等，需与指令数据一致）。

## Notes / Compatibility

- `String.nx` 修改后需重启 assets-server（启动快照缓存）；服务器脚本修改后建议重启 `maplestory-cosmic` 服务使 Nashorn 重新加载脚本。
- 隐藏门密码 `Open Sesame`（ThiefPassword）、`Francis is a genius Puppeteer!`（PupeteerPassword）保留英文：无任务题面来源，属探索彩蛋。
- 翻译数据模块按 `zh_cash_consume.py` / `zh_eqp_etc.py` / `zh_ins_map_misc.py` 拆分，单文件均在 400 行内。

## Related Docs

- [中文/Unicode 输入与渲染](../../input-localization/index.md)
- [网络层](../../../agents/client/Net/index.md)
- [Linked 服务端部署](../../../agents/web/index.md)
