# 频道选择窗口隐藏烘焙的禁用频道格

## 问题

单频道化（每世界 `channels=1`）后，频道选择窗口仍显示"频道 2 及之后的
禁用样式"灰色格子。

根因：post-Chaos（v153+）`UI.nx` 的 `Login.img/WorldSelect/BtChannel/layer:bg`
（396x275）把 **5 列 x 6 行、共 30 个带编号的灰色频道格整幅烘焙进背景位图**；
客户端按 `channelcount` 只在其上绘制高亮频道板（`button:N` 的 origin 已按
槽位编码偏移，全部以窗口锚点定位），未覆盖的烘焙格即呈现为"禁用频道"。

像素取证（新增 `tools/nx/nxppm.cpp` 导出 PPM+alpha）：
- 网格带位于位图 (22,76)-(374,253)，格 68x27，步距 (71,30)；
- 各 button:N origin：(-22,-76)、(-93,-76)、(-22,-106)…… 与格位一致；
- 面板底色采样 ≈ (80,102,122)，网格带 alpha 全不透明。

## 变更（客户端，git 提交）

| 文件 | 变更 |
|------|------|
| `src/client/IO/UITypes/UIWorldSelect.cpp` | `draw()` 改为 sprites → 遮盖 → buttons 三段；新增 `draw_channel_cover()`：post-Chaos 分支按行计算已用格数，用 `GraphicsGL::drawrectangle` 以面板色 (0.31,0.40,0.48) 填充无频道板的格带区域（0 频道全遮、N 频道保留前 N 格、行满跳过） |
| `src/client/IO/UITypes/UIWorldSelect.h` | 新增 `postch_window` 标记与 `draw_channel_cover()` 声明 |
| `tools/nx/nxppm.cpp` | 新增 NX 位图节点 → PPM(P6)+PGM(alpha) 导出工具（像素级素材取证用） |

pre-Chaos（v83）分支不受影响（走 `chBackgrn`，无烘焙网格）。

## 验证

- `bash scripts/docker_build_wasm.sh` 重建后 e2e
  `tools/verify/maplestory-webui/run.mjs` 全流程通过（exit 0）：
  频道窗截图仅剩频道 1 高亮板，右侧/下方网格带为均匀面板色，灰色编号格消失；
  中文聊天、按需资源加载等断言均 PASS。
- 附带运维修复：Reboot 世界 7975 端口此前因旧进程 TIME_WAIT 未绑定，
  等待 socket 清零后干净重启，5 世界单频道（7575/7675/7775/7875/7975）
  + 登录口 8484 全部监听，e2e 角色（世界 4）可正常进图。
