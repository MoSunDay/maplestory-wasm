# 佩里传送目的地蚂蚁洞Ⅰ（ant-tunnel）实测

> 说明：第 7 项传送点随后由并行改动（见 perry-ant-park-destination.md）
> 由 105050000 蚂蚁洞Ⅰ 调整为 105070001 蚂蚁广场；本文件保留该阶段的
> 实测证据与环境备忘，环境备忘对后续复跑仍然适用。

## 改动

自由市场传送 NPC「佩里 / 次元空间的帮手」（9110000，脚本
`link_repos/MapleStory-Server/scripts/npc/9110000.js`）维多利亚分类
（`CATEGORY_MAPS[0]`「维多利亚岛及周边」）第 7 项传送点 105050000
（蚂蚁洞Ⅰ，位于 105040300 林中之城之后）本次纳入端到端实测：

- `tools/verify/maplestory-webui/fm-service-npcs.mjs` 新增
  `FM_SERVICE_DEST` 开关：`henesys`（默认）/ `ant-tunnel`，
  文件共 368 行，`node --check` 通过；
- ant-tunnel 模式用探针直读对话框按钮几何（`msnpctalk_button_field`
  / `msnpctalk_element_field`，同 pq-harness-dialog.mjs 的模式），
  点循环按钮 6 次再点 OK——子菜单共 12 项（0..10 地图 + 返回主菜单），
  从 selected=0 出发 NEXT×6 与 PREV×6 都落在 index 6（±6 ≡ 6 mod 12），
  不依赖按钮方向枚举；index 6 即 105050000 蚂蚁洞Ⅰ。

## 前置确认

- 105050000 在客户端 `assets/String.nx`（`Map.img/victoria` 下
  mapName「蚂蚁洞Ⅰ」、streetName「迷宫」）与 `assets/Map.nx`
  （`Map/Map1/105050000.img`）中均存在；
- 不在 `DANGEROUS_MAPS` 名单内，无需进入前二次确认。

## 选项文本

选项经 `#m<mapid>#` 宏渲染为客户端本地地图名（中文），不触及
`replace_macros` 会吞掉选项文本开头 `#b`/`#r` 等字母格式 token 的坑
（该坑只影响分类名，本次未改分类名）。

## 实测记录

真实 WASM 客户端 + 真实服务端，两次全绿：

- `FM_SERVICE_DEST=ant-tunnel`：15 项 PASS，关键断言「循环 6 次选中
  蚂蚁洞Ⅰ，已点 OK」「传送到 105050000 蚂蚁洞Ⅰ（实测 105050000）」，
  产物 /tmp/maplestory-webui-zreYP5/（fm-service-ant-tunnel.png 等四图）；
- 默认回归：15 项 PASS，含原射手村 warp 100000000 断言与科皮
  （Kopee Relicseeker）购买 0→1，产物 /tmp/maplestory-webui-Boqmur/
  （fm-service-henesys.png 等四图）。

## 环境问题与处置（复跑备忘）

- 共享服务器上有其它验证进程周期性重置 test 系列账号（test2 密码被
  改写为 cost-12 bcrypt 导致 reason=4 登录失败）；本次克隆了专用账号
  fmverify01（account id=103）+ 角色 fmverifyA（character id=245，
  SQL 克隆自 test2ILA：map=910000000、spawnpoint=0、keymap 13 行）
  规避争夺。
- ms-game-server 曾因 7975 端口被旧实例占用导致频道 5 绑定失败
  （客户端选中该频道时报 Reconnect failed）；`docker restart
  ms-game-server` 后 8484/7575..7975 全部就绪。
- 高负载（load 4~5）下 CDP `Runtime.evaluate` 偶发 45s 超时，加
  `E2E_CDP_TIMEOUT_MS=120000` 后稳定。
- 重跑 SQL：`update accounts set loggedin=0 where name='<账号>';
  update characters set map=910000000, spawnpoint=0 where id=<角色id>;`
- 运行命令示例：

```bash
E2E_CDP_TIMEOUT_MS=120000 FM_SERVICE_ACCOUNT=fmverify01 \
FM_SERVICE_PASSWORD=test2 FM_SERVICE_CARD=0 \
CHROME_BIN=/usr/bin/chromium-browser [FM_SERVICE_DEST=ant-tunnel] \
node tools/verify/maplestory-webui/fm-service-npcs.mjs
```

## 生效方式

服务端重启并非必需：AbstractScriptManager 按 MapleClient 缓存脚本
引擎（`getInvocable` 先查 `c.getScriptEngine`），已在线的旧会话沿用
缓存脚本，而全新登录的新会话会由 FileReader 重新加载脚本。本次实测
即在未重启脚本缓存的情况下完成（中途仅因端口占用重启过一次容器）。
