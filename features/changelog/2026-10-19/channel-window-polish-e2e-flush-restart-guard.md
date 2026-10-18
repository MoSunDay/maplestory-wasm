# 频道窗遮盖打磨 / e2e 输出冲洗 / 服务端重启守卫

承接 `channel-window-disabled-cells-cover.md`（commit ebe5301）评审遗留的三项跟进。

## 1. 频道窗遮盖配色校准 + 整体平面化（客户端）

渲染管线对未纹理矩形不做变换（填充色 1:1 上屏），而 `layer:bg` 纹理路径
渲染偏亮：原取自 PPM 间隔均值的 (0.31,0.40,0.48) 实测偏亮约 6%。按截图
实测校准为 (0.26,0.35,0.44)。另发现 3px 行缝在渲染中为亮暖色分隔线
((130,118,101))，逐行 27px 遮盖会残留每 30px 一条的"隐形行"条纹——
`draw_channel_cover()` 重写为两块连续矩形（部分行余量 + 下方整块），
连分隔线一并盖掉，逻辑更简（边界：0/1/5/6/25/30 频道穷举核验）。

定位校准过程中确认 `Constants.h` 的 `VIEWYOFFSET=10`：游戏坐标整体下移
10px 上屏，bg 与遮盖同域平移，对齐无损（此前截图离线像素分析的 +10px
偏差由此解释）。

## 2. e2e 尾部输出冲洗（tools）

Node 管道 stdout 为异步写，进程退出即截断尾部：上一轮 e2e 实际成功
（exit 0、工件齐全）但 chat/artifacts 等 PASS 行丢失。`run.mjs` 结尾新增
`drainStdout()`（drain 事件 + 2s 兜底），失败退出与自然收尾两条路径均先
冲洗再退。本轮快验日志尾部三行（artifacts/console lines/flow completed）
已完整出现。

## 3. 服务端重启守卫（scripts）

新增 `scripts/game_server_restart.sh`：stop(30s) → 轮询直至游戏端口
非 LISTEN socket 清零（180s 超时，MINA 无 SO_REUSEADDR，TIME_WAIT 残留
会让新进程 `Address already in use` 丢频道，Reboot 7975 两次中招）→
start → 轮询验证 6 监听口（5 世界单频道 + 8484，120s 超时并在失败时
倾倒 ss 与容器日志）。

## 验证

- `E2E_STOP_AT_CHAR_SELECT=1` 快验 exit 0：频道窗截图（VIEWYOFFSET 校正
  映射）显示 格子行缝条纹消失、y310-421 全均匀、频道 1 板与 GoWorld
  按钮正常浮于遮盖之上。
- `node --check` / `bash -n` 语法校验通过。
