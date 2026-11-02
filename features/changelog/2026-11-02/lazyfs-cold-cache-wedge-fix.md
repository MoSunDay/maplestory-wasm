# LazyFS 冷缓存 Asyncify 楔死修复（boot 预热 + tick 钳制 + wakeUp 分级）

背景与根因链见 `docs/known-issues/2026-10-31-asyncify-wedge-and-private-e2e-stack.md`。
本轮针对**冷缓存首次启动**场景落地三处修复并完成冷/暖 e2e 验证。

## 症状

冷缓存（IndexedDB 无 chunk）登录进图后渲染线程偶发冻结：无 abort 栈、
无 OOB，事件循环停转；暖缓存不复现。控制台可见 boot 期
`characterMetadataEnd` 相关读取失败与后续零取块。

## 根因（三个叠加缺口）

1. **boot 预热被 persist 门控**：`startItemAssetPreload` 等待
   `requestPersistentStorage()` 授权才预热。授权通常被拒（需用户手势
   且冷启动首访必拒），于是 boot 期零预热；进图后全部变成首读，与
   socket 层 `emscripten_sleep` 挂起并发挤兑单 rewind 缓冲。
2. **`characterMetadataEnd` 依赖 Asyncify 挂起态下的 `readFileData`**：
   `readFileData` 的防护守卫在 Asyncify 挂起/回放中直接 `return null`
   （该守卫本身是 10-31 落地的 abort/OOB 缓解），boot 期调用必失败，
   元数据残缺加重进图后的补偿读取。
3. **`'unwind'` 哨兵被误判致命**：`fetch_data` 回调里裸
   `try{wakeUp(n)}catch(e){}` 吞掉一切异常。但 emscripten 主循环
   （`simulate_infinite_loop=1`）的 `throw "unwind"` 会穿过同步
   rewind 回放——它是**正常哨兵**。旧 wakeUpOrReload 雏形曾把任何
   异常当致命触发 `location.reload()`，反而把正常回放 reload 掉。

## 修复

### 1. `src/client/LazyFS/lazyfs.js`

- 新增 `scheduleBootWarmup`：Character/Item/String 三个 nx 文件的
  size 一经 `getFileSize` 获知即调度后台预热（幂等，只调度一次）；
  `getFileSize` 尾部 `.finally(() => this.scheduleBootWarmup())`。
- `startItemAssetPreload` 解除 persist 门控：`requestPersistentStorage`
  改 best-effort（失败仅提示 "Persistent browser storage not
  granted"，预热照常）。
- `characterMetadataEnd` 改为 `prefetchFileRange('Character.nx', 0, 40)`
  + 从 chunkCache 解析元数据，与 Asyncify 挂起态完全解耦。

### 2. `src/client/Journey.cpp`（main_tick 累加器）

落后帧累加器钳制 `MAX_CATCH_UP_STEPS = 2`（×timestep 8ms）：rewind
回放窗口内滴答时间戳可能回跳/跳变，无钳制时累加器积累巨额帧债务，
主循环陷入追帧忙自旋；超出钳制直接丢弃。

### 3. `src/client/LazyFS/LazyFileBackend.cpp`（fetch_data EM_ASM）

`makeWakeUpOrReload(wakeUp)` 工厂**必须定义在 handleSleep 回调内部**
（闭包捕获该次 wakeUp；定义在外层会 `ReferenceError: wakeUp is not
defined`）。行为分级：

- 异常为 `'unwind'`（或 message 含 unwind）→ 静默忽略（主循环哨兵）；
- 其他异常 → `console.error` + `sessionStorage
  ('lazyfsWakeUpReloaded')` 一次性防循环标记 + `location.reload()`。

4 处 `try{wakeUp(x)}catch(e){}` 全部替换为该工厂。

### 单测契约更新：`tools/verify/lazyfs-connection.mjs`

旧断言 "persist()=false → 预热 skipped/unavailable" 与新契约冲突，
更新为**无授权仍预热**；新增 `testBootWarmupSchedulesOnce`（幂等 +
不 gate persist）。

## 构建坑（重要）

- pre-js（`lazyfs.js`）**不是 make 的链接依赖**：只改 pre-js 时 make
  判定无需重链接，产物不含新代码。必须
  `touch src/client/LazyFS/LazyFileBackend.cpp` 强制重链接。
- 验证产物包含新逻辑时用**代码字符串**（如 grep
  `"Character.nx is not registered"`），-O3 会剥离注释。
- 构建入口 `./scripts/docker_build_wasm.sh`（docker compose run
  wasm-builder，本地无 emcc），包裹
  `systemd-run --unit=<name> --collect` 防止后台进程被回收。

## 冷缓存验证方法（snap chromium 专属坑）

snap chromium 看到的 `/tmp` 映射到宿主
`/tmp/snap-private-tmp/snap.chromium/tmp/`（注意多一层 `tmp/`）——
**wipe 宿主 `/tmp/lazyfs-fix` 不是冷缓存**。真冷缓存：

    rm -rf /tmp/snap-private-tmp/snap.chromium/tmp/lazyfs-fix

验证标准：browser-console 有 ~200 条 `🌐 NET` 取块日志（若只有个位
数说明跑的是暖缓存）。暖缓存回归用非 snap 的
`google-chrome-stable`（`/tmp` 非私有挂载，profile 目录
`E2E_PROFILE_DIR` 直接持久化），`systemd-run --property=PrivateTmp=no`
包裹，同一 profile 跑两次（第一次填缓存，第二次取证）。

## 验证证据

命令模板（e2e 账号 test1/test1，`E2E_CREATE_CHARACTER=0
E2E_STOP_AT_GAME=1` 走 登录→选角→进图→探针）：

    E2E_URL=http://127.0.0.1:8000/web/index.html \
    CHROME_BIN=/usr/bin/chromium E2E_ACCOUNT=test1 E2E_PASSWORD=test1 \
    E2E_CREATE_CHARACTER=0 E2E_STOP_AT_GAME=1 \
    E2E_ARTIFACT_DIR=/tmp/lazyfs-fix/cold-run-N \
    node tools/verify/maplestory-webui/run.mjs

冷启动（wipe snap-private-tmp 后，探针门 34 PASS/次）：

| 跑次 | 进图耗时 | NET 取块 | reload/异常 |
|---|---|---|---|
| cold-1 | 86s | 198 | 0 / 0 |
| cold-2 | 69s | 200 | 0 / 0 |
| cold-3 | 68s | 199 | 0 / 0 |

每次均有 "Persistent browser storage not granted" + "Item asset
preload complete"（boot 预热在无授权下照常完成的直接证据）。

暖缓存回归（google-chrome-stable，同一 profile 跑两次）：

| 跑次 | 角色 | 进图耗时 | NET | CACHE | reload/异常 |
|---|---|---|---|---|---|
| warm-a | 填缓存 | 44s | 196 | 1 | 0 / 0 |
| warm-b | **回归取证** | 22s | 3 | 321 | 0 / 0 |

warm-b 仅剩 3 次 NET 均为 Character.nx 外观分支 chunk（warm-a 未触
达的区域），34 探针全 PASS。

证据文件（/tmp 会话级易失）：`/tmp/lazyfs-fix/`（cold-run-{1,2,3}/
截图+console、warm-run-{a,b}/、logs/、cold-probe-evidence.txt、
build-backup/）。

## 残余风险（未根治项）

- `open()` 的文件 size 仍是打开时快照，文件变大需重开才可见；
- socket 层（`ws_recv` 5ms 轮询 / `connect` 自旋）与 LazyFS 共享单
  rewind 缓冲的并发 park/wake 结构未变，Asyncify 单缓冲本质限制
  仍在（/emsdk 回归后的根治方案见 known-issues 文档）；
- wakeUp 真异常现走一次性 reload 兜底，属降级而非根治。

## 改动位置

| 文件 | 改动 |
|---|---|
| `src/client/LazyFS/lazyfs.js` | scheduleBootWarmup / persist 解耦 / characterMetadataEnd 管线化（恰 800 行上限） |
| `src/client/Journey.cpp` | main_tick 累加器钳制 MAX_CATCH_UP_STEPS=2 |
| `src/client/LazyFS/LazyFileBackend.cpp` | fetch_data EM_ASM 内 makeWakeUpOrReload（unwind 哨兵忽略 + 一次性 reload 兜底） |
| `tools/verify/lazyfs-connection.mjs` | 无授权仍预热契约 + testBootWarmupSchedulesOnce |
| `docs/known-issues/2026-10-31-asyncify-wedge-and-private-e2e-stack.md` | 状态更新段（修复落地 + 残余风险） |
