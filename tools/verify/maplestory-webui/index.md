# MapleStory WebUI 全流程

`run.mjs` 通过 Chromium DevTools Protocol 驱动真实 WASM 客户端，断言每次 UI 状态转换，并依次保存登录、世界选择、人物选择、中文建角和中文聊天证据。未设置 `E2E_URL` 时会探测 loopback 上的 8000/8001 Web 服务；自定义绑定必须显式传入地址。

```bash
E2E_URL=http://127.0.0.1:8001/web/index.html node tools/verify/maplestory-webui/run.mjs
```

浏览器必须支持 WebGL2。系统浏览器版本过旧时，通过 `CHROME_BIN` 指向兼容 Chromium；无图形会话时可用 `xvfb-run -a env E2E_HEADED=1 ...` 运行。
脚本会在加载客户端前验证 WebGL2，并且资产专项模式也必须等到 GLFW 窗口初始化成功且没有 fatal log 才能通过。
完整流程默认要求 P95 帧间隔不超过 34ms（覆盖正常的 30Hz 调度，但不接受 50ms 档位），可用 `E2E_MAX_P95_FRAME_MS` 收紧。

默认首次运行创建角色 `测试一`；已有该角色时设置 `E2E_CREATE_CHARACTER=0`。
设置 `E2E_REGISTER=1` 会先验证游戏内注册表单可填写和关闭，再刷新页面并执行独立的真实登录流程。

世界选择默认通过物理鼠标事件单击确认按钮；`E2E_WORLD_SELECT_ACTION` 可设为 `channel`、`dom-go` 或 `dom-channel`，分别验证物理双击频道、纯 DOM 确认和纯 DOM 双击兼容路径。设置 `E2E_STOP_AT_CHAR_SELECT=1` 可在确认进入角色选择后结束专项验收。

设置 `E2E_STOP_AT_GAME=1` 会选择已有角色，完成登录服到频道服重连、完整角色数据解析和首次地图前台素材加载后结束；该模式不会创建角色或发送聊天消息，适合上线首尾稳定样本。

设置 `E2E_CAPTURE_ATTACK_EFFECTS=1` 会在进入地图后使用普通攻击，并以 40 ms 最小节奏保存每次攻击的 20 帧画面，覆盖刺击、挥砍以及冷缓存加载后的延迟残影窗口；默认采集 12 次，可用 `E2E_ATTACK_SAMPLES` 调整。它可以与 `E2E_STOP_AT_GAME=1` 组合，用于冷缓存和热缓存的武器残影验收。

设置 `E2E_RETRY_CHARACTER=<未使用名称>` 会先提交非法名称并验证提示后的输入恢复，再提交指定名称进入外观定制，随后取消返回角色选择；该模式不会创建测试角色或进入游戏。

完整游戏流程使用独立浏览器缓存，并在进入地图前监听真实 `Texture::draw()` 驱动的前台 NX 范围请求。验收要求至少一个请求在发起前尚未驻留、素材遮罩实际出现，并将证据写入产物目录的 `natural-asset-loading.json`。`E2E_ASSET_ONLY=1` 仅运行独立 LazyFS 网络、失败和重试检查，不代替完整游戏触发验收。
