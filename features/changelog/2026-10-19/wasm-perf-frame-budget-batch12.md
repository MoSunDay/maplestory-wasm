# WASM 每帧开销优化：第一批 + 第二批

渲染架构本身健康（单 atlas、每帧 1 次 glDrawElements、着色器一次编译），
本批针对"每帧重复做本可缓存的事"与"全局构建配置未收紧"两类 CPU 浪费。

## 第一批（低风险缓存与构建收紧）

- **小地图标记动画缓存**（`IO/UITypes/UIMiniMap.*`）：`draw()` /
  `draw_movable_markers()` 原先每帧构造 `Animation`（NX 节点查找 + 堆分配），
  提升为成员 `portal_marker / npc_marker / other_marker`，构造函数一次性
  初始化。绘制停留在 frame 0，与原先每帧重建的行为一致。
- **flush() 索引缓冲复用**（`Graphics/GraphicsGL.cpp`）：索引内容只依赖
  quad 数，改为容量增长式共享 IBO（增长 1.5 倍、`GL_STATIC_DRAW` 上传），
  每帧仅 `glDrawElements`；删除每帧 ~6×quads 次 `push_back` 与重复
  `glBufferData`。
- **ASSERTIONS 门控**（`CMakeLists.txt`）：`-sASSERTIONS=1` 仅在非
  Release 构建启用。JS glue 283.7KB → 205.8KB（-27.5%）。
- **优化等级钉死 + LTO**（`build_wasm.sh` / `CMakeLists.txt`）：
  显式传 `-O3 -DNDEBUG`（C 与 C++），新增 `WASM_LTO` 选项（默认 ON），
  zlib/freetype port 自动以 LTO 变体重建。
- **帧时间探针**（`Journey.cpp`）：主循环原有的死代码 FPS 统计块改为
  真实滚动平均，导出 `_msframe_avg_us`（ccall 可读）。

## 第二批（系统性热路径）

- **LazyFS chunk 缓存 O(1) 化**（`LazyFS/LazyFileLoader.*`）：新增按
  chunk index 的直接索引表 `chunk_slots_`（open 时按文件大小分配，驱逐时
  置空），命中路径从最多 160 块线性扫描变为一次下标访问。NX 节点查找是
  全局乘数，此改动让所有 `node["x"]` 二分受益。
- **FootholdTree 改 vector**（`Gameplay/Physics/FootholdTree.*`）：fhid 为
  密集 uint16，`unordered_map<uint16_t, Foothold>` 改为按 id 下标的
  `vector<Foothold>`；`get_fh` 从哈希查找 + `.at` 变为一次边界检查 +
  下标。每移动对象每帧 8–12 次查询全部受益。
- **StatusBar 文本缓存**（`IO/UITypes/UIStatusBar.*`）：`ensure_stat_labels()`
  按 (格式, level/hp/mp/maxhp/maxmp/exp/百分比) 失效缓存，`draw()` /
  `draw_prechaos()` 直接绘制缓存串；数值不变时不再每帧 5+ 组
  `std::to_string` 拼接与 Charset 重排。

## 验证

- `scripts/docker_build_wasm.sh -j 16`：Release + LTO 全量构建通过；
  wasm 10.68MB → 10.75MB，JS glue 283.7KB → 205.8KB。
- `node scripts/e2e_ime.mjs`（无头 Chrome 驱动真实 WASM 客户端）：13/13
  PASS，覆盖 wasm 引导、LazyFS 流式加载、CJK 像素级渲染（间接验证
  flush 索引复用与主循环改动）。
- `node scripts/e2e_utf8_protocol.mjs`（CHARNAME 限 VARCHAR(13) 字节内）：
  登录/世界列表/角色列表/中文名建角全部通过；末步 logout-close 依赖
  服务端主动断开，属既有服务端行为，与本次客户端改动无关。
- 探针实测：登录界面 `msframe_avg_us ≈ 16665`（60fps，vsync 上限）。

## 未做（第三批，需专门验证手段）

- ASYNCIFY 插桩收窄（`ASYNCIFY_ONLY/REMOVE`）与 EM_ASM 轮询改 JS 直写
  HEAP32：改错 allowlist 会让异步路径挂起，需要覆盖登录/换图/资源阻塞的
  脚本化游戏内 e2e 后再动。
- `_ms*` 探针导出门控：e2e JS 与 `web/*.js` 无条件 ccall 这些导出，
  门控需同步拆分 harness，收益（体积）有限。
- MapObjects 视口剔除：可见性判定已有 `Animation::is_visible` 基础，
  但剔除策略改变渲染行为，需游戏内视觉回归。
