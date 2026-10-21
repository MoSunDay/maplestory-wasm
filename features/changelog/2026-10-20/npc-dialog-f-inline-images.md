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
