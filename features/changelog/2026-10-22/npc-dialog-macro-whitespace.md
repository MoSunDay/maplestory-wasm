# NPC 对话宏与空白归一化（#i/#v/#s/#q/#c + 脚本缩进）

## 根因

- linked server checkout 的 NPC 脚本
  （`link_repos/MapleStory-Server/scripts/npc`）
  混用多种对话宏：`#i<itemid>#`/`#v<itemid>#` 物品图标、
  `#s<skillid>#` 技能图标、`#q<skillid>#` 技能名、
  `#c<itemid>#` 持有数量，以及既有的 `#f/#F<path>#` 内嵌图片。
- 本轮之前扫描器只识别 `#f/#F` 路径宏和 `#b/#k/#e` 等格式 token；
  `#i/#v/#s` 落入通用字母 token 分支，连同后面的数字 id 一起被吞掉，
  图标静默消失。`#q/#c` 也没有进入命名引用表，`#c` 还容易与颜色
  标签混淆。
- 脚本来自 Windows 编辑器，同时存在 CRLF、单独 CR 和 LF 三种换行；
  行首有 `\t\t`/`\t\t\t\t` 缩进，行内还有用于对齐的连续空格。
  这些字节原样传给 `Text`，表现为参差缩进和成片空白。

## 实现

- `src/client/IO/UITypes/UINpcTalkWhitespace.{h,cpp}`（新增，纯函数）：
  `normalize_dialogue_whitespace` 把 CRLF/CR/LF 统一为 `\n`，逐行
  去掉首尾 ASCII 空格/制表符，把行内连续空格或制表符折叠为一个空格，
  删除首尾空行并把连续空行折叠为一行；UTF-8 字节和 `#..#` token 正文
  原样保留。`strip_npc_tokens` 与 `replace_macros` 在返回前调用它。
- `src/client/IO/UITypes/UINpcTalkIconData.{h,cpp}`（新增）：
  `item_icon_texture`/`skill_icon_texture` 封装
  `ItemData::get_icon(false)`（无投影图标）与
  `SkillData::get_icon(NORMAL)`。把重数据头从 `UINpcTalkImages.cpp`
  隔离，独立 verifier 以 `--gc-sections` 链接时无需拖入数据缓存。
- `src/client/IO/UITypes/UINpcTalkImages.{h,cpp}`：新增
  `try_parse_icon_token`，识别 `#i/#v`（ITEM）与 `#s`（SKILL），
  大小写不敏感，接受前导零和闭合 `#` 前最多一个 `:`；空 body、
  非数字、双冒号/尾随其他字符及超过 `INT32_MAX` 的 id 一律拒绝。
  `#F` 与 `#f` 同样按内嵌图片识别。`extract_embed_sprites` 统一把
  `#f/#F/#i/#v/#s` 替换为按显示宽度向上取整的空格占位并返回带
  byte 位置的 sprite；物品/技能图标走 `NPC_ICON_MAX_HEIGHT = 17`
  （A12M 行高），`embed_display_size` 按原始宽高比整数换算显示尺寸，
  `#f` 路径图传 0 保持 native。未解析的图标按 kind+id 只告警一次，
  告警总数上限为 `MAX_UNRESOLVED_LOGS = 20`。
- `src/client/IO/UITypes/UINpcTalkText.cpp`：`copy_icon_token` 在通用
  字母 token 剥离之前处理图标 token，KEEP 时原样透传给 sprite 提取，
  STRIP 时整段删除，数字 id 不再泄漏到正文。`#c<digits>#` 在格式
  token 处理之前解析为物品数量；`q/Q` 加入
  `#m/#t/#z/#p/#o` 命名引用扫描。
- `src/client/IO/UITypes/UINpcTalkRefs.{h,cpp}`：`q/Q` 查 `SkillData`
  技能名；`c/C` 用 `InventoryType::by_item_id` 找到对应栏位，遍历所有
  槽位累加同一 itemid 的数量。`#c` 对缺失/越界 id 返回 `"0"`（0 是
  有效数量，不记为 unresolved）。
- `src/client/IO/UITypes/UINpcTalk.cpp`：`draw_embed_sprites` 按
  sprite 的 `display/native` 做 x/y 缩放，占位宽度使用显示宽度；
  绘制 y 由
  `embed_sprite_y(line_baseline, native_height, origin_y, display_height)`
  按纹理 origin 计算，把贴图视觉底边放在所在行基线，修正物品图标
  origin 在底边时整体高一行的问题。`#f` 图片按原尺寸嵌入。17px
  限高与 origin 公式细节见同日 `npc-dialog-icon-height-cap.md`。

## 边界情况

- `#i1132012:#`（可选尾冒号）、`#v04032424#`（前导零）、
  `#I/#V/#S`（大写）均可解析；溢出 id 不会截断成另一个 id。
- `#q1000#`/`#c4000000#` 属于文本宏，`try_parse_icon_token` 明确
  返回 false，不会画出图标。
- 正文中的 `#B/#I/#V` 在缺少数字或闭合 `#` 时不会被误判为图标
  token，仍按原有格式/字母 token 规则处理。
- 选项缺少 `#l` 时，`find_next_selection_tag` 以下一个
  `#L<digits>#` 或文本末尾作为边界；`#Ll#` 不匹配 `#L<digits>#`，
  不会生成空 id 选项，而是留在文本流中按普通 token 规则处理。

## 验证

```bash
# 空白归一化纯函数回归
c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client \
    tools/verify/npc-dialog-whitespace.cpp \
    src/client/IO/UITypes/UINpcTalkWhitespace.cpp \
    -o /tmp/npc-dialog-whitespace
/tmp/npc-dialog-whitespace
# ALL CHECKS PASSED (npc dialogue whitespace)

# 独立 token/路径/origin 回归（tools/verify/npc-embed-path-nx.cpp，
# 按 tools/verify/index.md 的独立构建方式补 lz4.c）
c++ -std=c++17 -Wall -Wextra -Werror -Wno-narrowing \
    -Wno-unused-function -ffunction-sections -fdata-sections \
    -Isrc/client -Isrc -Isrc/nlnx -Isrc/client/includes \
    tools/verify/npc-embed-path-nx.cpp \
    src/client/IO/UITypes/UINpcTalkImages.cpp \
    src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp \
    src/nlnx/nx.cpp src/client/includes/lz4.c \
    -Wl,--gc-sections -o /tmp/npc-embed-path-nx
/tmp/npc-embed-path-nx assets/UI.nx assets/Effect.nx \
    assets/Item.nx assets/Map.nx
# ALL CHECKS PASSED
# 实测位图尺寸：15x15 / 142x18 / 11x10 / 27x30 / 190x11

# WASM 构建（WARNINGS_AS_ERRORS=ON）
./scripts/docker_build_wasm.sh -j 36
# Build finished successfully.

# 真实客户端 E2E：妮娜回收中心菜单
node tools/verify/maplestory-webui/npc-embed-icons.mjs
# PROMPT>> "亲爱的玩家 [fmflow1Hero],欢迎来到回收中心\n\n您目前拥有回收积分 ：0"
# PASS：无路径泄漏、无 unresolved 告警；#f 菜单爱心仍为 native 11x10，
# 图标带 chroma 109.88 vs 灰阶 24.92

# 空白检查
git diff --check
# 无输出（clean）
```

- 临时像素探针 `/tmp/npc-list.png`：点击第一个选项后 prompt 为
  `可回收物品如下:`；物品图标约 17px 高，行间有可见间隙，物品名与
  `#c` 数量在同一基线，无跨行遮挡（修复前 32px native 图标连成一条
  彩色带）。
- 服务端脚本属于外部 linked checkout
  `link_repos/MapleStory-Server`，本轮只读排查，未修改任何脚本、SQL
  或服务端代码。WASM 构建保持 `WARNINGS_AS_ERRORS=ON`，无警告通过。
