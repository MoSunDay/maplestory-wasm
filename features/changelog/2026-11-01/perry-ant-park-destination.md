# 佩里传送目的地由蚂蚁洞Ⅰ改为蚂蚁广场

## 改动

自由市场传送 NPC「佩里 / 次元空间的帮手」（9110000，脚本
`link_repos/MapleStory-Server/scripts/npc/9110000.js`）维多利亚分类
（`CATEGORY_MAPS[0]`「维多利亚岛及周边」）第 7 项传送点：

- 由 `105050000`（蚂蚁洞Ⅰ）改为 `105070001`（蚂蚁广场）；
- 位置不变（仍在 `105040300` 林中之城之后），分类项数与其余分类、
  危险地图确认与状态机逻辑均未改动。

## 前置确认

- `105070001` 在客户端 `assets/String.nx`（`Map.img/victoria` 下
  mapName「蚂蚁广场」、streetName「迷宫」）与 `assets/Map.nx`
  （`Map/Map1/105070001.img`）中均存在；
- 不在 `DANGEROUS_MAPS` 名单内，无需进入前二次确认。

## 选项文本

选项经 `#m<mapid>#` 宏渲染为客户端本地地图名（中文），不触及
`replace_macros` 会吞掉选项文本开头 `#b`/`#r` 等字母格式 token 的坑
（该坑只影响分类名，本次未改分类名）。

## 验证

- `node --check link_repos/MapleStory-Server/scripts/npc/9110000.js` 通过；
- `tools/verify/maplestory-webui/fm-service-npcs.mjs` 的 `ant-tunnel`
  目的地同步更名为 `ant-park`（mapId 105070001、label 蚂蚁广场、
  optionIndex 仍为 6）：子菜单仍是 12 项，按钮循环 ±6 ≡ 6 (mod 12)
  的推导不受影响；
- 端到端实测：使用专用账号 fmverify01 / 角色 fmverifyA（id 245），
  `FM_SERVICE_DEST=ant-park` 全流程 15 项 PASS，关键断言「循环 6 次
  选中蚂蚁广场，已点 OK」「传送到 105070001 蚂蚁广场（实测 105070001）」，
  产物 /tmp/maplestory-webui-WBZgH0/（fm-service-ant-park.png 等截图）。
  实测运行于 2026-11-01 晚间最终客户端构建（build/JourneyClient.wasm
  19:22 产物）。

## 生效方式

服务器端重启并非必需：AbstractScriptManager.getInvocable 按
MapleClient 缓存脚本引擎（先查 `c.getScriptEngine`），同账号旧会话
命中缓存、沿用已缓存的旧脚本；而全新登录的新会话会由 FileReader
重新加载脚本文件，因此不重载/重启也能生效——本次实测即是在未重启
的情况下完成。
