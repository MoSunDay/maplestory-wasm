# NPC 对话内嵌图标限高与基线对齐

## 背景

服务端对话脚本用 `#i/#v<itemid>#`、`#s<skillid>#` 内嵌物品/技能图标。
这些图标原生多为 32×32（技能约 26×30），直接按原尺寸画进 A12M 对话
文本会把相邻选项行顶开、互相遮挡。`#f<路径>#` 图片是策划排版元素
（分隔线、菜单爱心等），必须保持原始尺寸，不能一起缩放。

## 变更

- `src/client/IO/UITypes/UINpcTalkImages.cpp`：新增
  `NPC_ICON_MAX_HEIGHT = 17`，与 `FontCache::linespace` 的 A12M 行高
  （12px 字形 ×1.35 + 1）保持一致；纯函数 `embed_display_size` 按
  原始宽高比整数四舍五入算显示尺寸，宽高最小 1px；`append_sprite`
  按**显示宽度**写入空格占位（`ceil(width / 空格advance)`），保证
  换行宽度和实际渲染一致。只有 `#i/#v/#s` 传入
  `NPC_ICON_MAX_HEIGHT`；`#f` 分支传 0，保持 native 尺寸。
- `src/client/IO/UITypes/UINpcTalkImages.h`：`NpcEmbedSprite` 增加
  `width/height`（显示尺寸），绘制层据此缩放；文档明确图标限高、
  `#f` 不限高的语义。
- `src/client/IO/UITypes/UINpcTalk.cpp`：`draw_embed_sprites` 以
  `display/native` 作为 x/y 缩放，并把贴图**视觉底边**放在文本行
  基线上。新增纯函数 `embed_sprite_y` 处理纹理 origin：物品图标
  origin 通常在底边（如 27×30 的 `Consume/0200.img/02000000/info/icon`
  origin=(-3,30)），此时绘制点就是基线；`#f` 图片 origin 在左上时
  绘制点为 `基线 - 显示高度`。旧实现统一用 `基线 - 高度`，会让
  底边 origin 的图标整体高出一行。
- `tools/verify/npc-embed-path-nx.cpp`：补 12 组 `embed_display_size`
  断言（32×32/27×30/35×32/40×39/33×27/26×30 图标限高、142×18 与
  11×10 的 `#f` 保持 native、1px 最小宽度、退化高度）和 8 组
  `embed_sprite_y` 断言（底边 origin、左上 origin、缩放取整、退化高度）。

## 验证

```bash
# 独立 token/path/origin 回归
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

# WASM 构建
./scripts/docker_build_wasm.sh -j 36
# Build finished successfully.

# 真实客户端：妮娜菜单爱心（#f，native 11×10）仍为彩色，
# 对话选项行不重叠
node tools/verify/maplestory-webui/npc-embed-icons.mjs
# PASS ... band chroma 109.88 vs grey 24.92
```

像素探针（临时脚本 `#v` 选项列表截图）显示物品图标底边与同行中文
文本基线对齐（误差 ≤1px，个别行受字形与透明边距影响），图标列宽约
17px，相邻行间距约 19px（17px 行高 + 2px 选项间距），不再跨行遮挡。
