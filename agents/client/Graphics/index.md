Commit: b42ba604e68b4f517c782a65c9b3994a0342ec60

# 图形渲染

## 职责

基于 OpenGL/WebGL 的 2D 游戏渲染引擎。管理 8192x8192 图集、精灵绘制、文字渲染（FreeType）、场景帧缓冲。

## 边界

- **包含**: OpenGL/WebGL 封装、纹理异步准备、纹理图集、精灵绘制、文字排版渲染、色彩/几何工具
- **不包含**: 游戏逻辑、NX 分块传输、UI 布局

## 关键抽象

### GraphicsGL (`GraphicsGL.h`)

渲染引擎单例。核心职责:
- `init()`: 初始化 OpenGL 状态、着色器、图集、字体
- `draw(bmp, rect, color, angle)`: 将位图四边形加入渲染队列
- `drawtext(args, text, layout, font, color, back)`: 文字渲染
- `drawrectangle()` / `drawscreenfill()`: 矩形/全屏填充
- `flush(opacity)`: 批量提交所有四边形到 GPU

文字排版与绘制成员（`createlayout`/`LayoutBuilder`/`drawtext`）定义在
`GraphicsGLText.cpp`（自 `GraphicsGL.cpp` 拆出以满足文件行数限制，行为不变）。

内部使用一个 8192x8192 的纹理图集 (`GLuint atlas`) 管理所有位图，通过 QuadTree 分配图集空间。渲染时通过 VBO/IBO 批量提交四边形。

### 纹理图集 (`GraphicsGL::addbitmap`)

- WASM 中 `Texture` 实例化后通过 `queuebitmap()` 静默预取，主循环每帧调用 `preparebitmaps()`；普通运行使用 2ms、阻塞加载使用 8ms 软预算批量解压和上传
- `beginbitmapbatch()`/`bitmapbatchprogress()` 在地图切换期间收集所有新实例化贴图；全图贴图要求 NX 范围驻留，首屏可见贴图还要求已经进入 WebGL 图集
- 首屏静态地图对象、图块和背景会把可见动画的全部帧提升为高优先级，避免安全门释放后紧接着因下一动画帧再次停顿
- 位图准备分为 `BACKGROUND`、`MAP_REQUIRED`、`BLOCKING_VISIBLE`、`TRANSIENT_EFFECT`：地图必需素材参与安全门，可交互 UI 缺图才暂停玩法，剑光和掉落物等瞬态内容只提升网络/GPU 优先级
- `Texture::prepare_effect()` 与 `draw_effect()` 使用非阻塞瞬态路径；已在普通队列中的同一位图会原地提升，仍保持全局去重，网络失败才升级到可重试的前台错误态；`draw_effect()` 仅在非零透明贴图实际加入绘制队列时返回成功，供一次性特效锁存真实呈现
- `Texture`、`Frame` 与 `Animation` 提供有效性和图集驻留检查，一次性动画可以等待全部帧就绪后再推进，避免异步下载消耗播放窗口
- `Animation/FrameProperties.h` 统一解释 NX 透明度端点：缺少 `a0` 但存在 `a1` 时从默认 255 过渡到 `a1`，不会把仅声明淡出终点的帧误判为全程透明
- 首次绘制时尚未就绪的普通纹理会进入阻塞可见队列；WASM 主循环只在显式 `BLOCKING_VISIBLE` 集合未清空时冻结 Stage/UI 并显示素材遮罩，优先队列本身不再等同于玩法阻塞
- 非 WASM 平台仍通过 `addbitmap()` 同步注册到图集
- 图集空间由 `QuadTree<Leftover>` 管理，按需分配/回收
- 当图集空间不足时调用 `clear()` 清空

### FontCache (`FontCache.h`)

字形缓存单例，负责 UTF-8/Unicode 文字的字形供给:
- 每个字号一个主 face（Regular/Bold Arial，保持原版观感）+ 共享的
  DroidSansFallback face（覆盖 CJK 等主 face 缺失的文字）
- 按需光栅化：字形首次使用时才渲染进图集（ASCII 32-127 启动时预烘焙）
- 所有 face 都缺字时写入占位字形（有前进量、不可见），保证排版稳定

### Text (`Text.h`)

文字渲染系统:
- `Text::Layout`: 文字排版结果，由 `LayoutBuilder` 构建；按码点切分，
  `advances` 为字节索引（延续字节复用引导字节的 advance）
- 支持格式化文本（颜色/字体切换标记）
- 支持左对齐/右对齐/居中

字形经 FontCache 光栅化后放入图集：字体区从图集底边向上分配
（`fontborder` 自底向上推进，着色器按 `texpos.y >= fontregion` 区分字体/位图区），
位图区则从第 1 行向下增长，两区互不侵占。

### 辅助类

| 类 | 职责 |
|------|------|
| `Texture` | 纹理加载与管理 |
| `Sprite` | 精灵（从 NX 节点加载动画帧） |
| `Animation` | 精灵动画播放 |
| `EffectLayer` | 特效层（渲染效果叠加） |
| `Color` | 颜色工具（RGBA 和混合） |
| `Geometry` | 几何工具（线条、矩形） |
| `DrawArgument` | 绘制变换参数封装 |

## 渲染流程

```
GraphicsGL::preparebitmaps() → 按帧预算准备异步位图
GraphicsGL::draw() → 已就绪位图创建 Quad 加入 quads 列表
GraphicsGL::drawtext() → 创建 Quad 加入 quads 列表
...
GraphicsGL::flush() → 绑定 atlas 纹理 → 提交 VBO/IBO → glDrawElements
```

所有绘制操作在 `flush()` 之前都是 CPU 端的四边形积累；`flush()` 一次提交到 GPU。

## 依赖关系

- **内部依赖**: `nlnx::bitmap` (位图数据), FreeType (字体渲染)
- **外部依赖**: OpenGL 3.0+ / WebGL 2.0, GLEW
