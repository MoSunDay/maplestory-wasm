# NPC 对话内嵌图片宏 `#f<路径>#`：妮娜对话乱码修复

## 根因

服务端脚本（`link_repos/MapleStory-Server/scripts/npc/9270052.js` 等）在
对话文本里用 `#fUI/GuildMark/Mark/Pattern/00004014/11#` 这类宏内嵌小图片。
客户端 `UINpcTalk` 的宏扫描器（`strip_npc_tokens` / `replace_macros` /
`parse_selections`）只识别 `#b/#k/#e` 等 NEXON 风格标签，不认识 `#f`，
于是把整段资源路径当普通文本送进 `Text` 布局，渲染成一串乱码英文。

## 实现

- `src/client/IO/UITypes/UINpcTalkImages.h/.cpp`（新增，64/221 行）：
  纯函数模块。`try_parse_embed_token` 解析 `#f...#` token；
  `resolve_embed_node` 从任意 nx 根按 `/` 路径解析（支持 `UI.nx` /
  `Item.nx` 等跨文件前缀）；`extract_embed_sprites` 把文本里的 token
  原地替换为按图标宽度换算的空格占位（byte 位置不变，后续 advance
  查询与最终布局一致），返回 `NpcEmbedSprite{byte_pos, x_offset, texture}`
  列表；未解析路径打印 `[npc] unresolved inline image` 告警。
- `src/client/IO/UITypes/UINpcTalkText.h/.cpp`：三个扫描函数加
  `EmbedImages` 枚举（STRIP=默认剔除整个 token；KEEP=透传 `#f..#`），
  UINpcTalk 用 KEEP 保证占位符宽度参与换行。
- `src/client/IO/UITypes/UINpcTalk.h/.cpp`：`change_text` 两条分支
  （say / selection）先 `extract_embed_sprites` 再建 `Text`；`draw()`
  新增 `draw_embed_sprites`：x = 文本列 + `label.advance(byte_pos)` +
  微调，y = `text_y + 行基线 - 图标高`（底边贴行基线，与 drawtext 的
  `chy = y + ay - bt` 同一坐标系）；`embed_line_baseline` 由布局
  word 区间反查 token 所在行。
- `src/client/Graphics/Text.h/.cpp`：暴露 `const Layout& get_layout()`。

注意全屏存在 `Constants::VIEWYOFFSET = 10`（着色器把所有 quad 画到
`canvas_y = passed_y + 10`），文本与图标同受该平移，因此"底边贴基线"
公式无需补偿；曾用截图/页面采样双重实测确认两者对齐一致。

## 验证

`tools/verify/maplestory-webui/npc-embed-icons.mjs`（250 行）全绿：

- `msnpc_prompt` = `"   亲爱的玩家 [fmflow1Hero],欢迎来到回收中心
  \r\n\r\n<105 空格占位>\r\n您目前拥有回收积分 ：0\r\n…"`，无路径泄漏；
- 页面内 readPixels：图标带 y255-269（game 坐标）chroma 108.34 vs
  灰阶文本参照 5.62；x300..560 十二列全部染色（17 连排 15×15 四方印
  + 问候行尾 3 枚，pitch 15 与 UI.nx 位图尺寸一致）；
- Escape 关闭后同带 chroma 回落 108→70（残留为世界底色）；
- console 无 unresolved 告警、无浏览器错误；LazyFS 异步就绪由
  20s 色带重试覆盖（首帧 0/21，数秒后 21/21）。

## 追加：全 NX 域解析与独立回归（同日）

- `resolve_embed_node` 的域前缀从 UI/Effect 两域扩展为全部 16 个顶层
  NX 文件（base/character/effect/etc/item/map/mob/morph/npc/quest/
  reactor/skill/sound/string/tamingmob/ui），大小写不敏感；未知域返回
  空 node（`domain_root()`）。服务端脚本里 `#fItem/...#`、`#fMap/...#`
  等不再依赖手工加域。
- 新增首帧回退：部分路径在新版 UI.nx 中是帧容器而非位图
  （`UIWindow.img/QuestIcon/3` 的位图在子节点 `0`），解析到容器时
  自动取 `0` 帧；实测 `UIWindow2.img/QuestIcon/3/0` 为 142×18。
- `extract_embed_sprites` 签名改为
  `(source, advance_per_space, out_text)`：空格步长由调用方
  `UINpcTalk.cpp` 的 `npc_space_advance()`（A12M 空格 advance）注入，
  模块不再依赖 `FontCache`/`Text`，可在无图形状态下链接测试。
- 新增 `tools/verify/npc-embed-path-nx.cpp`（220 行）：token 识别
  （中段 token、120 字节上限、`##` 转义、颜色标签近似串拒绝）、
  `.img` 补全、UIWindow2 迁移回退、首帧回退、大小写不敏感域、
  Item/Map 跨域真实位图尺寸（15×15 / 142×18 / 11×10 / 27×30 /
  190×11）及未知域/缺节点空返回；以 `--gc-sections` 剥离
  Texture/FontCache，位图只保留元数据桩。ALL CHECKS PASSED。
- `src/client/Graphics/Color.h` 补 `<cstddef>`，显式提供 `size_t`
  （独立编译该模块时不再报 `size_t does not name a type`）。

重建 WASM 后复跑端到端回归：`npc-embed-icons.mjs`
（prompt 无路径泄漏、105 空格占位、图标带 chroma 108.34 vs 灰阶 5.62、
12/12 列染色、关闭回落）PASS；`fm-map-flow.mjs` 15/15 PASS（出生点、
妮娜对话开关、四层平台/门传送、自由市场<1> 往返、返回落点）。
