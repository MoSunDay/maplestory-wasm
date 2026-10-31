# Asyncify 楔死根因与私有 e2e 栈 runbook（2026-10-31）

## 现象：三种楔死形态

宠物温饱度 e2e（`E2E_PET_OPTIONS=hunger`）在登录后 UI 输入触发 lazy-fetch
的时刻偶发挂起，同一构建多次复跑表现出三种形态：

1. **abort**：glue `handleSleep` 在 `state==Unwinding` 收到新挂起走
   `abort('invalid state')`，控制台出现 abort 栈；
2. **OOB 风暴**：rewind 缓冲被释放后仍在重放（`fetch_data` 返回后
   `doRewind` 抛异常被 `try{wakeUp(1)}catch(e){}` 吞掉），后续随机
   wasm 越界读；
3. **忙自旋楔死**：渲染器主线程冻结（单 chrome 进程 600%+ CPU 或完全
   空闲两种姿势都见过），事件循环停转，CDP `Runtime.evaluate`
   永不返回，e2e 无输出悬挂。

## 根因链（已定性，/emsdk 回归前无法根治）

Emscripten Asyncify 只有**单 rewind 缓冲**；本客户端存在多个并发挂起源：

- `SockInternal.cpp:336` `ws_recv` 5ms 轮询 `emscripten_sleep`；
- `SockInternal.cpp:228` `connect` 自旋 `emscripten_sleep(5)`；
- `LazyFS` lazy-read 的 `fetch_data` EM_ASM 异步取资源。

`CMakeLists.txt:35` 的 `ASYNCIFY_IMPORTS` 含 `emscripten_sleep`，因此
socket 层与资源层共享同一 rewind 状态机。单缓冲下并发 park/wake 无
JS 侧完全解：state machine 冲突即触发上述三形态。socket 层三方案
（per-call malloc、延迟 free、gate 排队）实验均已回退——socket 非首因。

## 已落地缓解

- `src/client/LazyFS/lazyfs.js` `readFileData` 守卫：Asyncify
  `currData || state` 非空闲时直接 `return null` 跳过本次 lazy-read，
  显著降低 abort/OOB 概率（b105/b106 两次全流程通过均基于此守卫）；
- `tools/verify/maplestory-webui/support/async_glue_instrument.py`：
  只读埋点注入 glue（wrap abort/callUserCallback/handleSleep，
  `window.__asyncLog` 上限 500 条，`[ASYNC-STUCK]` 每 4s 打印卡死轮询）。
  楔死时 CDP 取不到数据，但 run.mjs `finally` 会把 console 刷进
  `browser-console.log`，事后可复盘 PARK/WAKE 序列；
- 解析容错：opcode 198（SHOW_FOREIGN_EFFECT）短包 Stack underflow 噪声
  已修（855371f，`available()>=4` 才进技能分支），属独立预存在缺口。

## 待编译根治（/emsdk 回归后）

源码已就位，需重编 wasm 生效并先过 boot 冒烟（其前身热补丁版曾致
boot 期 null function 回归，勿默认绿）：

- `src/client/LazyFS/lazyfs_sync_cache.js`：同步缓存命中不挂起 +
  warmupAfterPark（73 行）；
- `src/client/LazyFS/LazyFileBackend.cpp`：`fetch_data` EM_ASM 同步快路径；
- `src/client/CMakeLists.txt:47`：pre-js 接线。

## 私有 e2e 栈搭建（不动仓库 build/，防外部重建覆盖）

    # 1) 独立目录拷贝 web/ 与构建产物
    mkdir -p /tmp/e2e-fix && cp -r web /tmp/e2e-fix/web
    mkdir -p /tmp/e2e-fix/build && cp build/JourneyClient.{js,wasm} /tmp/e2e-fix/build/
    # 2) 起 Range 服务器（stdlib http.server 不支持 206，wasm 分块下载会失败）
    python3 tools/verify/maplestory-webui/support/rangesrv.py /tmp/e2e-fix 8899
    # 3) 可选：注入 Asyncify 埋点（先备份原 glue）
    cp /tmp/e2e-fix/build/JourneyClient.js /tmp/backup-JourneyClient.js
    python3 tools/verify/maplestory-webui/support/async_glue_instrument.py \
        /tmp/e2e-fix/build/JourneyClient.js
    # 4) 跑 e2e（E2E_URL 指向私有栈）

## 环境坑（本会话实证）

- **跨工具调用的后台进程必被回收**：`nohup`/`setsid` 均无效；唯一存活
  方式是让长命令跑满 150s 被 harness 自动转后台监管。e2e 复跑应把
  rangesrv 启动、夹具 SQL、run.mjs 串进**同一条命令**；
- **pkill 自杀**：匹配串含自身命令行时用 `pkill -f 'patter[n]'` 括号法；
  且当前 shell 也跑在 internal-process-supervisor 下，全局 kill
  supervisor 会杀掉自己；
- **`E2E_CDP_TIMEOUT_MS` 勿用 900000**：cdp.mjs 每条命令有该值硬超时，
  设 15 分钟会让楔死静默悬挂。推荐 `120000`（慢 evaluate 有余量、
  楔死 2 分钟内失败可见）；
- rangesrv 若用 `timeout N` 包裹，到期即断流（wasm 下载中途 404/
  连接拒绝），按 e2e 时长给足。

## 证据（会话级，/tmp 易失）

- b105：49 PASS，唯一失败为 opcode-198 噪声触发 fatalRuntimeLogs；
- b106（全绿）：47 PASS / 0 FAIL / exit 0，关键断言链
  `pet hungry state probe fullness=18` → `hungry pet strike held
  (drops=2, pickups 0..0)` → `pet fed fullness=48 hungry=0` →
  `fed pet resumed looting (pickups 0 -> 2)`，截图 20a（现金栏红!
  角标）/20b（宠物面板 HUNGRY! 状态行）各 ~330-370KB；
- 复跑命令模板见 `tools/verify/maplestory-webui/index.md` 宠物段落。
