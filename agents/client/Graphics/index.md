# 图形渲染

Commit: bc0234fe7c7f53322453e7bdd79564d9aca4cd8b

## 职责

基于 OpenGL/WebGL 的 2D 游戏渲染引擎。管理 8192x8192 图集、精灵绘制、文字渲染（FreeType）、场景帧缓冲。

## 边界

- **包含**: OpenGL/WebGL 封装、纹理图集、精灵绘制、文字排版渲染、色彩/几何工具
- **不包含**: 游戏逻辑、资源加载、UI 布局

## 关键抽象

### GraphicsGL (`GraphicsGL.h`)

渲染引擎单例。核心职责:
- `init()`: 初始化 OpenGL 状态、着色器、图集、字体
- `draw(bmp, rect, color, angle)`: 将位图四边形加入渲染队列
- `drawtext(args, text, layout, font, color, back)`: 文字渲染
- `drawrectangle()` / `drawscreenfill()`: 矩形/全屏填充
- `flush(opacity)`: 批量提交所有四边形到 GPU

内部使用一个 8192x8192 的纹理图集 (`GLuint atlas`) 管理所有位图，通过 QuadTree 分配图集空间。渲染时通过 VBO/IBO 批量提交四边形。

### 纹理图集 (`GraphicsGL::addbitmap`)

- 位图通过 `addbitmap()` 注册到图集
- 图集空间由 `QuadTree<Leftover>` 管理，按需分配/回收
- 当图集空间不足时调用 `clear()` 清空

### Text (`Text.h`)

文字渲染系统:
- `Text::Layout`: 文字排版结果，由 `LayoutBuilder` 构建
- 支持格式化文本（颜色/字体切换标记）
- 支持左对齐/右对齐/居中

通过 FreeType 加载字体，渲染字形到图集，支持 Regular 和 Bold 两种字体。

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
GraphicsGL::draw() → 创建 Quad 加入 quads 列表
GraphicsGL::drawtext() → 创建 Quad 加入 quads 列表
...
GraphicsGL::flush() → 绑定 atlas 纹理 → 提交 VBO/IBO → glDrawElements
```

所有绘制操作在 `flush()` 之前都是 CPU 端的四边形积累；`flush()` 一次提交到 GPU。

## 依赖关系

- **内部依赖**: `nlnx::bitmap` (位图数据), FreeType (字体渲染)
- **外部依赖**: OpenGL 3.0+ / WebGL 2.0, GLEW
