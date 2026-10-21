# 桌面客户端（Electron 发行包）

> 目标读者：参与桌面端打包、发布与联调的开发者。
> 相关文档：[../README.md](../README.md)（整体架构）、[ms-network-protocol.md](ms-network-protocol.md)（协议细节）。

## 1. 概述

桌面客户端是一个**纯客户端发行包**：Electron 壳 + WASM 客户端 + 三个本地 Rust 服务
（`web-server` / `ws-proxy` / `assets-server`），开箱即玩、无需安装本地 Web 栈。

- **不含游戏服务器**：游戏服务器通过登录界面的「服务器地址」接入，由玩家/运营方自行部署
  （见 [link_repos/MapleStory-Server](../link_repos/MapleStory-Server) 的本地联动部署）。
- **本地服务仅监听 `127.0.0.1`**，端口在启动时动态分配，不对外暴露、不触发防火墙放行弹窗。
- **资源获取两条路**：打包内置 `assets/*.nx`（经本地 `assets-server` 以 WebSocket 流式提供），
  或在设置里填写远程资源服务器 URL（按需下载，适合瘦身发行包）。

## 2. 运行时架构

```
┌───────────────────────────────────────────────────────────────┐
│ Electron 主进程 (desktop/main)                                 │
│                                                               │
│  - 启动时读取/写入设置: userData/settings.json                  │
│  - 拉起本地服务(全部绑定 127.0.0.1 + 动态端口):                │
│      web-server     --directory <msweb>      (HTTP 站点)       │
│      ws-proxy       --ws-port  <动态>        (仅远程服务器场景) │
│      assets-server  --port     <动态>        (仅本地 .nx 场景) │
│  - webRequest 虚拟化 /web/config.json:                        │
│      把服务器地址/端口/资源 URL 注入给页面, 磁盘上无该文件      │
└───────────────┬───────────────────────────────────────────────┘
                │ loadURL http://127.0.0.1:<webPort>/web/index.html
┌───────────────▼───────────────────────────────────────────────┐
│ BrowserWindow (渲染进程)                                       │
│   web/ 页面 + JourneyClient.js + JourneyClient.wasm            │
│   - HTTP:   静态资源 / LazyFS 分片 ← 本地 web-server           │
│   - WSS/WS: 游戏封包 ← 本地 ws-proxy(桥接远程 TCP) 或 远端 ws  │
│   - WSS/WS: .nx 资源流   ← 本地 assets-server 或 远端资源服务  │
└───────────────────────────────────────────────────────────────┘
```

要点：

- 页面永远从本地 `web-server` 加载（响应带 `Cross-Origin-Opener-Policy: same-origin` 与
  `Cross-Origin-Embedder-Policy: require-corp`，满足 SharedArrayBuffer 前置条件）。
- `/web/config.json` 由主进程 `webRequest` 拦截并虚拟生成，写入当前会话解析出的连接参数；
  因此渲染层无需感知「本地桥接 vs 远端 ws」的差异。

## 3. 「服务器地址」解析规则

| 用户输入 | 解析结果 |
|---|---|
| `ms.example.com` | 走**本地 ws-proxy** 桥接到 `ms.example.com:8484`（默认登录端口 8484） |
| `host:7575` | 同上，但桥接到指定端口 `host:7575` |
| `ws://host:8080` / `wss://host:8080` | 直连**远端 ws-proxy**（省略端口时默认 `8080`），其落点目标为 `host:8484` |
| 资源服务器 URL（可选）`ws(s)://host[:port]` | `.nx` 资源改为从远端流式加载；`ws` 省略端口默认 `8765`，`wss` 默认 `443`。**留空**时使用打包内 `assets/*.nx`，经本地 `assets-server` 提供 |

解析失败的输入会在登录界面就地提示，不会拉起多余服务。

## 4. 目录布局

```
desktop/                     # Electron 壳（本仓库的发行层）
├── main/                    # 主进程源码（服务生命周期、设置、webRequest 虚拟化）
├── scripts/
│   ├── assemble.sh          # 组装打包资源（--bin-dir/--target/--out）
│   └── smoke_services.mjs   # 服务冒烟测试（见第 6 节）
├── tests/                   # 单元测试 + 三层 e2e（见下）
├── package.json             # scripts: assemble / dist / dist:dir / start
└── resources/               # assemble.sh 产物（不入库）
    ├── msweb/               # 站点根：web/ + build/JourneyClient.* + assets/*.nx(可选)
    └── bin/<os>-<arch>/     # 原生服务: web-server / ws-proxy / assets-server(.exe)
```

- 打包目标标签 `linux-x64 | linux-arm64 | darwin-x64 | darwin-arm64 | windows-x64 | windows-arm64`，
  与 `resources/bin/<target>/` 一一对应。
- **开发模式**不需要组装：主进程在未发现 `resources/` 时直接使用仓库根作为站点根、
  `target/release/` 作为服务二进制目录（`npm start` 即此路径）。

## 5. 本地开发与打包

前置条件：`node >= 18` + npm、Rust stable（cargo）、Emscripten（仅构建 WASM 时需要，emsdk 4.0.21）。

```bash
# 1) 安装桌面端依赖
cd desktop && npm ci

# 2) 构建 WASM 客户端（仓库根执行，产出 build/JourneyClient.{js,wasm}）
./scripts/build_wasm.sh

# 3) 原生编译三个本地服务（产出 target/release/*）
cargo build --release -p web-server -p ws-proxy -p assets-server

# 4) 组装打包资源到 desktop/resources/
npm run assemble            # = bash scripts/assemble.sh（默认 --bin-dir target/release）

# 5) 运行
npm start                   # 开发运行（Electron）
npm run dist:dir            # 免安装目录打包（本机平台，快速验证）
npm run dist                # electron-builder 正式分发（nsis/dmg/AppImage + zip）
```

自动化测试（无头 Linux，root 环境需追加 `--no-sandbox`）：

```bash
npm test                       # 单元测试（node --test，纯函数解析/配置生成）

# e2e 第一层：开发模式注入链路（仓库根即站点）
E2E_SITE_ROOT=$PWD E2E_BIN_DIR=target/release \
  xvfb-run -a npx electron tests/e2e-main.js --no-sandbox

# e2e 第二层：真实打包产物（先 npm run dist:dir；预置 settings 后断言
#   动态端口、COOP/COEP、虚拟 config.json、退出时服务回收）
node tests/e2e-packaged.mjs

# e2e 第三层：首次启动设置流程（空配置 → 设置窗口 → 保存 →
#   settings.json 落盘 → 游戏窗口 + 远端 ws 代理配置注入）
node tests/e2e-settings-flow.mjs
```

> Electron 33 兼容性备忘（均已处理，遇升级需回归上述 e2e）：
> 1. `file://` 页面默认注入 `default-src 'self'` CSP，禁止内联脚本/样式，
>    因此 settings-ui 拆分为 `index.html + app.js + style.css` 三个文件；
> 2. 本环境 `app.resourcesPath` 为 `undefined`，`paths.js` 回退到
>    `process.resourcesPath`；
> 3. `BrowserWindow#setWindowOpenHandler` 不存在，统一使用
>    `webContents.setWindowOpenHandler`。

## 6. 冒烟测试

`desktop/scripts/smoke_services.mjs`（纯 Node、零依赖）验证打包出的服务真的能起、能服务页面：

```bash
node scripts/smoke_services.mjs \
  --bin-dir resources/bin/linux-x64 \
  --site   resources/msweb \
  [--web-port 8000]
```

断言内容：

1. `web-server` 15s 内就绪，`GET /web/index.html` 返回 **200**，且响应头
   `Cross-Origin-Opener-Policy: same-origin`、`Cross-Origin-Embedder-Policy: require-corp`（大小写不敏感、值精确匹配）；
2. `ws-proxy` 10s 内 TCP 可连；
3. 若 `<site>/assets/` 存在至少一个 `*.nx`，则 `assets-server` 10s 内 TCP 可连；否则打印
   `SKIP assets-server (no local .nx)`。

成功输出 `SMOKE_OK {…}`（JSON 摘要）；失败打印 `SMOKE_FAIL` 与各进程 stderr 尾部后以退出码 1 结束。
该步骤已接入 CI（见下节），本地排障同样可用。

## 7. CI 发布（.github/workflows/desktop-release.yml）

- **触发**：推送 `v*.*.*` 标签，或手动 `workflow_dispatch`；并发组 `desktop-release-<ref>`，新运行取消旧运行。
- **`wasm` job**（ubuntu, 45m）：emsdk 4.0.21 + `./scripts/build_wasm.sh`，产物
  `build/JourneyClient.{js,wasm}` 以 artifact `wasm-client` 共享，四个平台只编译一次。
- **`package` job**（matrix, 60m, fail-fast 关闭）：`linux-x64 / darwin-x64 / darwin-arm64 / windows-x64`
  各自 `cargo build --release` 原生编译三服务 → `npm ci` → `assemble.sh --target <tag>` →
  `smoke_services.mjs` 冒烟 → `electron-builder --publish never`（`CSC_IDENTITY_AUTO_DISCOVERY=false`
  跳过 macOS 签名）→ 上传 `desktop/dist/` 下的 AppImage/dmg/zip/exe/blockmap。
  Windows 的 assemble 步骤显式 `shell: bash`（git-bash）。
- **`release` job**：仅标签触发，汇总 `desktop-*` artifacts 后用
  `softprops/action-gh-release@v2` 附到 GitHub Release（自动生成变更说明）。

## 8. 风险与对策

| 风险 | 对策 |
|---|---|
| 协议版本错配（客户端 v83 版本与服务端 fork 演进不一致） | Release 说明中**标注配套 server commit**；后续加版本握手（见路线图） |
| 发行包无本地 `.nx` 且用户未填资源 URL | 主进程启动预检：缺资源时引导到设置页填写资源服务器 URL，而不是黑屏 |
| 防火墙/安全软件弹窗 | 本地服务**只绑定 `127.0.0.1`**（环回流量通常不触发放行提示） |
| macOS 未签名/未公证 | 首次运行引导「右键 → 打开」；工程已预留 entitlements 与公证配置位 |
| COOP/COEP 缺失导致 SharedArrayBuffer 不可用 | 冒烟测试硬性断言两个响应头，CI 拦截回归 |

## 9. 路线图（Phase 2 / 3）

- **Phase 2 — 体验完善**：`.nx` 打包开关（瘦身版发行包）、窗口位置/大小记忆、系统托盘、
  自动更新（electron-updater 对接 GitHub Releases）。
- **Phase 3 — 稳定性**：崩溃日志收集与上报、客户端/服务端版本握手（登录前校验协议版本，
  错配时给出明确提示与配套 server commit 指引）。
