# 角色选择切到 079MAX4 位图界面：原版窗框/花名册素材注入，扁平界面保留为回退

## 背景（Context）

- 2023 重做后选人屏是程序化扁平面板，与 079 客户端的原版外观差距明显。
  目标是在零协议改动、e2e 锚点不变的前提下，用 079 原版登录素材还原该屏。
- 079 的 `UI/Login.img` 与 `login.img`（Back）需要先转成 NX 才能作为客户端
  素材使用，而 079 `Login.img` 的 800x600 画布是分块 zlib + KMS XOR，
  既有提取脚本（base64 内嵌的 0x8000 密钥前缀 + 单段解压）读不出来。

## 改动（Change Summary）

- 素材注入：新增 `tools/nx/inject_charselect_079_nx.py`，把三棵 079 子树
  改名注入 `assets/UI.nx` 的 `Login.img`——`CharSelect079`（完整 079
  CharSelect 子树，含 charInfo/charInfo1/按钮/翻页箭头/圆盘）、`frame079`
  （800x600 登录窗框，原点 400,300）、`selectWorld079`（150x57 世界横幅）；
  `assets/Map.nx` 注入 `Back/login079`（38 层视差 `back` + `ani`）。子节点
  按名字节序排序（nlnx 用二分查找解析），注入前保留
  `*.pre-079charselect-backup` 备份，备份存在时拒绝重跑。世界选择/登录的
  既有素材节点未改动。
- 提取链路：`tools/img079_key.py` 改为从新增的 `tools/data/key_kms.bin` 读取完整的
  `key_kms[0x20000]` 表（原先只有 base64 内嵌的 0x8000 前缀，够 Map/Npc，
  不够 079 `Login.img` 的 25KB 画布分块）；`tools/img079_parser.py` 新增分块
  zlib 画布解压（`[u32 长度][KMS XOR 片段]`，每块从密钥偏移 0 重启 XOR、
  共用同一 inflater），并保留单段 zlib / 单段 XOR 分支；
  `tools/extract_079_data.py` 的 BGRA→RGBA 改切片实现，全屏背景转换不再逐
  像素循环。
- 布局：新增 `src/client/IO/CharacterSelect/CharSelect079Layout.h`（纯常量，
  无类）——800x600 画布、窗框内部 x41-756 / y40-488、四槽 120px 花名册、
  161x188 charInfo1 卡（锚点 625,305、烘焙原点 45,57）及其文字行基线、
  横幅/计数/按钮/翻页箭头锚点、`rank_icon_pos()` 升降箭头定位。
- 绘制/初始化拆分：`UICharSelectInit.cpp` 负责节点挂载与两代 UI 的选择，
  `UICharSelectDraw.cpp` 负责绘制，`UICharSelect.cpp` 只留 update/输入
  （768 → 572 行）；`UICharSelectNodes.h` 增加 `root079()/frame079()/
  select_world079()/back079()/info_board()/job_icon()/page_arrow()` 等空安全
  取节点，任意节点缺失只降级为 Console 警告。
- 回退：`has_bitmap_ui()` 在 `CharSelect079/charInfo1` 与 `Back/login079`
  缺失时仍走原 2023 扁平绘制；两代共用同一套 e2e 锚点（落地点
  (130+120i,270)、热区 (100+120i,160,60x120)、翻页箭头、建角点击 250,515）。
- `src/nlnx/bitmap.cpp`：LZ4 读取改为先读 4 字节压缩长度、再按该精确长度读
  块，不再用 `4*w*h` 上界。归档内最后一张位图（如 `frame079`）用上界会读到
  文件尾之外，`io_read` 拒绝短读、图片静默不加载。
- E2E 分派：`ImeBridge.cpp` 新增只读探针 `msui_charselect_bitmap`（1=079 位图
  屏，0=扁平回退）；`run.mjs` 据此调用新的
  `tools/verify/maplestory-webui/layout-pixels-charselect079.mjs`
  （`assertCharSelectBitmapLayout`）或沿用 2023 断言，并新增
  `E2E_SKIP_CHAR_SELECT_LAYOUT=1` 跳过。位图契约按色度而非亮度判定（饱和
  橙色窗框边、蓝色卡片带、粉色按钮板对橙色下框带的 B-G 差、横幅板与上方
  天空的亮度差、左右翻页箭头与其上方天空的亮度差）；`layout-pixels.mjs`
  把采样收敛为单次 readPixels 的 `sampleChannels`，亮度口径保持原样。
- 新增原生契约测试 `tools/verify/charselect-079-geometry.cpp`（布局常量、
  卡片文字行基线、rank_icon_pos、花名册/热区/圆盘、横幅/计数、按钮/箭头）。
- 辅助流程入口：新增
  `tools/verify/maplestory-webui/support/char-select-entry.mjs`，按
  `msui_charselect_bitmap` 在扁平屏 (656,453) 与 079 页脚 (487,516) 之间
  选择 BtSelect 锚点；辛迪缺席、NPC 对话重开、空屋、二转教官四个流程改为
  调用它，不再硬编码扁平屏坐标。

## 验证（Verification）

- 素材注入与复核：`python3 tools/nx/inject_charselect_079_nx.py` 注入后，
  `python3 tools/verify/charselect-079-nx.py` 只读复核——三棵 UI 子树与
  `Back/login079` 与 079 源归档逐节点一致、父节点仍按名有序、整包节点/
  位图计数相对备份的增量为脚本登记的 UI +360/+91、Map +633/+160。
- 原生几何契约：
  `c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client -Isrc tools/verify/charselect-079-geometry.cpp -o /tmp/charselect079-geometry && /tmp/charselect079-geometry`
  → `PASS charselect 079 bitmap geometry (800x600)`。
- 浏览器 E2E：`E2E_STOP_AT_CHAR_SELECT=1 node tools/verify/maplestory-webui/run.mjs`
  在探针返回 1 时按位图契约采样，输出
  `PASS  char-select 079 bitmap layout {...}`（帧边、卡片带、粉色按钮板、
  横幅、页箭头全部命中）。
- 全文 E2E（不带 stop/skip）：`E2E_URL=http://127.0.0.1:8000/web/index.html
  node tools/verify/maplestory-webui/run.mjs` 输出 `INFO  char-select layout
  contract: 079 bitmap (probe=1)` 与
  `PASS  char-select 079 bitmap layout {...}`，随后完成建角、进图、中文
  聊天，末尾 `PASS  MapleStory WebUI flow completed`（browser exceptions 0）。
- 位图模式辅助流程：`henesys-cindy-gone-flow.mjs` 与
  `npc-dialog-reopen-flow.mjs` 均 `ALL PASS`——两者都先单击卡片、再经
  共享锚点单击 079 页脚 BtSelect 进图。

## 影响面（Impact Surface）

- 仅客户端角色选择界面、`nlnx` 位图解码与 079 素材注入工具；登录、世界
  选择、建角、协议、DB 均不变。
- `nlnx/bitmap.cpp` 的读取方式修正对全部 NX 位图生效，非归档末位位图的解码
  结果不变。
- 注入脚本一次性：`assets/UI.nx`、`assets/Map.nx` 的节点表被重写，回滚需
  用备份文件恢复。

## 已知限制（Known limitations）

- 位图屏完全依赖注入素材；未注入的 `UI.nx`/`Map.nx` 上仍回退到 2023 扁平
  界面（探针返回 0），外观与 079 不一致。
- 卡片下半的排名/职业排名两行只在存在长卡 `charInfo1` 时绘制；只有短卡
  `charInfo` 的变体下这两行留空。
- 布局按 800x600 原版画布写死；花名册仍是 `PAGESIZE = 4` 的单页、120px
  间距。
