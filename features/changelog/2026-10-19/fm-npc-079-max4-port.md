# FM(910000000) NPC 阵容照搬 079MAX4：弗兰德里 + 妮娜(9270052) 客户端立绘注入

## 背景

079MAX4-2023年度版的自由市场入口 NPC 来自其数据库表 `wz_customlife`（本仓库
Java 服务端不支持该表）：仅 2 个 NPC——9030000 弗兰德里 与 9270052 妮娜
（TMS 医生助理，装备回收脚本）。v83 侧原为 4 个 NPC
（9030000/9030100/9250025/9000069），且 v83 `Npc.wz` 与 `String.wz` 均无
9270052 定义，079 客户端 `data/Npc/9270052.img` 为自定义半加密格式
（结构明文、名字走 WZ XOR 流密码 + KMS key、canvas 为 ARGB4444+zlib flush）。

## 变更

服务端（`link_repos/MapleStory-Server`，非 git 仓库，手工同步）：

- `wz/Map.wz/Map/Map9/910000000.img.xml`：life 4→2。9030000 保留 079 的
  x=1590/rx 定点语义，fh 适配到 v83 覆盖该点的 fh94(1582..1658, y34)；
  9270052 新增于 x=34/fh82(0..90, y34)（079 fh81 线段不覆盖 x=34，取同
  平台相邻 fh）；删除 9030100/9250025/9000069。
- `wz/Npc.wz/9270052.img.xml`：079 原样拷贝。
- `wz/String.wz/Npc.img.xml`：按数字序插入妮娜（name/n0/n1/func 原文）。
- `scripts/npc/9270052.js` + `scripts/npc/lib/9270052_data.js`：079 脚本
  拆分移植；兼容改写：角色积分 `getjf3/setjf3` → 账号域
  `getRewardPoints/setRewardPoints`，带 `#L` 菜单的 `sendOk`→`sendSimple`。

客户端（`assets/`，均已留 `*.pre-9270052-backup`）：

- `Npc.nx`：注入 `9270052.img`（info/stand 0-5/say 0-4，11 帧 85×78，
  ARGB4444→RGBA8888 + LZ4，origin(38,77)，delay 600/120）；子节点按
  `nl::node::get_child` 二分查找要求有序插入。
- `String.nx`：注入铭牌条目（妮娜/医生助理）；重序列化为标准 writer 布局。

工具（新增，均纯函数、<400 行）：

- `tools/extract_079_data.py`：079MAX4 data/*.img 解密提取（含格式规范
  `link_repos/.ref-079max4/extracted/FORMAT.md`）。
- `tools/nx/inject_npc_nx.py` / `tools/nx/verify_npc_nx.py`：NX 注入/校验；
  `tools/nx/nxppm.cpp`：客户端 NX 读取路径的最小编译载体。
- `scripts/wz-converter/README.md`：补充"再生成后须重注入"守则。

## 验证

- 服务端：4 世界 wz 全量加载无错（XML 良构、JS `node --check` 通过、
  life 恰 2 NPC 且 (x,fh,y) 落在所引线段上）。
- 客户端：`nxppm`（真实 `nl::file/node/bitmap` 代码）实测 9270052 各帧
  像素与源 ARGB4444 逐字节全等；既有 NPC/String 子树零变化（35/35）；
  运行中的 assets-server（stale-reload）get_size/尾块与磁盘逐字节一致。
- 已知限制：兑换表 16 个 v83 缺失道具（1112763+ 戒指等）图标为空、不可兑；
  积分由 079 角色域改为账号域（同账号共享），待用户追认。

## 待办

浏览器端到端冒烟（两 NPC 显示/对话/一笔回收）；用户追认积分作用域与
fh 坐标适配两点偏差。
