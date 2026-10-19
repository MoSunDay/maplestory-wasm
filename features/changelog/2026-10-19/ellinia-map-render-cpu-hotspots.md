# 魔法密林渲染热点：平铺背景逐格剔除 + 待载位图去重前置

## 背景

魔法密林（101000000 及室内图 1010003xx/4xx）CPU 持续打满，其他地图正常。
用 `tools/nx/map_render_load.py` 对 `assets/Map.nx` 全量扫描定位到两个与
地图数据直接相关的热点：

- 城镇 526 个 obj（全游戏第二）、44 层背景（21 层平铺）、8432 段 foothold
  （每列平均 12.9 段，射手村仅 2.1），全部走无剔除的逐帧遍历路径；
- 室内图（Grendel 图书馆等）36 层背景中 24 层为"动画 + type-4 平铺"，
  `cy=0` 时平铺间距回退为精灵高度（7px），每帧展开约 2200 个 tile 单元，
  其中约 98% 落在屏幕外，但仍完整执行插值、矩形计算与贴图簿记。

## 变更

- `src/client/Graphics/Animation.{h,cpp}`：新增 `is_visible(args, alpha)`，
  与 `draw()` 使用同一帧（`frame.get(alpha)`）和同一缩放包装计算目标矩形
  后做 `isonscreen` 判定，供调用方提前跳过屏外工作。
- `src/client/Gameplay/MapleMap/MapBackgrounds.cpp`：`Background::draw` 的
  平铺循环先 `is_visible` 再 `animation.draw`，屏外 tile 只花一次矩形测试。
- `src/client/Graphics/GraphicsGL.cpp`：`queuebitmap` 把 pending 去重挪到
  `request()/prioritize()/prefetch()`（EM_ASM → JS 桥）之前；同一张待载
  位图被上百个 tile 每帧重绘时不再逐帧跨 JS 桥重建 key 字符串、扫描 JS
  预取队列。fetch 抽为文件内 `fetch_pending_bitmap`，仅在入队一次与
  "后台 lane 升级为优先 lane"时调用；加载门升级记账
  （`pending_blocking_bitmap_ids`）仍先于去重执行，失败恢复路径
  （`msmap_retry` → `retrybitmapbatch`）不受影响。

## 影响

- 室内图背景 tile 绘制调用 2202 → 约 46/帧（静态视差位置的近似统计，
  剔除率 97.9%）；城镇 110 → 50（54.5%）。
- 待载位图的每帧 EM_ASM 调用从"屏内出现次数"降为 0（去重命中）。
- 行为兼容：屏内 tile 绘制路径不变；TRANSIENT_EFFECT 获取失败仍按
  71fd0fa 的非阻塞语义静默跳过；BLOCKING_VISIBLE 失败仍走素材遮罩 +
  重试按钮。

## 验证

- `c++ -std=c++17 -Isrc/client -Isrc tools/verify/bitmap-load-class.cpp` 通过；
- `tools/verify/animation-frame-properties.cpp` 通过；
- 三个改动 TU 在 `-DMS_PLATFORM_WASM` 语法检查通过（宿主无 emsdk，用
  docker emscripten 缓存里的 GLES3/KHR/freetype 头 + 桩 emscripten.h）。

- WASM 全量重建（`docker compose run wasm-builder ./scripts/build_wasm.sh -j 8`，
  强制重编 3 个改动 TU 后链接）`Build finished successfully`，ms-web-server
  实际下发新产物（Content-Length 283678 与 build/JourneyClient.js 一致）；
- 运行时验收 `tools/verify/maplestory-webui/ellinia-perf-flow.mjs`（真实登录
  进图 + 12s 空闲采样；角色经 SQL 临时迁图，测后复原 100010000）：
  - 101000000 城镇：500 帧 p95=33.3ms（阈值 34）、0 longtask、空闲网络
    请求增量 0；
  - 101000300 室内（平铺最重场景）：630 帧（≈52.5fps）max=33.3ms、
    0 longtask、请求增量 0 —— 修复后满载地图静止时 LazyFS 不再产生持续
    fetch，帧预算达标；
- 回归：`E2E_STOP_AT_GAME=1 run.mjs` 标准 17 项全过、0 浏览器异常。

## 后续（未做，按性价比排序）

- `TilesObjs` 按位置粗粒度视口跳过（update/draw 均全量遍历 526 obj）；
- `get_fhid_below` 每列预排序/提前终止；
- `nl::bitmap::data()` 增加 LZ4 解压缓存。
