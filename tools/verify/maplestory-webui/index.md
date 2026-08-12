# MapleStory WebUI 全流程

`run.mjs` 通过 Chromium DevTools Protocol 驱动真实 WASM 客户端，断言每次 UI 状态转换，并依次保存登录、世界选择、人物选择、中文建角和中文聊天证据。未设置 `E2E_URL` 时会探测 loopback 上的 8000/8001 Web 服务；自定义绑定必须显式传入地址。

```bash
E2E_URL=http://127.0.0.1:8001/web/index.html node tools/verify/maplestory-webui/run.mjs
```

浏览器必须支持 WebGL2。系统浏览器版本过旧时，通过 `CHROME_BIN` 指向兼容 Chromium；无图形会话时可用 `xvfb-run -a env E2E_HEADED=1 ...` 运行。

默认首次运行创建角色 `测试一`；已有该角色时设置 `E2E_CREATE_CHARACTER=0`。
设置 `E2E_REGISTER=1` 会先验证游戏内注册表单可填写和关闭，再刷新页面并执行独立的真实登录流程。

设置 `E2E_RETRY_CHARACTER=<未使用名称>` 会先提交非法名称并验证提示后的输入恢复，再提交指定名称进入外观定制，随后取消返回角色选择；该模式不会创建测试角色或进入游戏。
