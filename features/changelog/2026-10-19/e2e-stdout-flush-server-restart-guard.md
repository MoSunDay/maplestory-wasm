# e2e 输出冲洗 / 服务端重启守卫

## 1. e2e 尾部输出冲洗（tools）

Node 管道 stdout 为异步写，进程退出即截断尾部：上一轮 e2e 实际成功
（exit 0、工件齐全）但 chat/artifacts 等 PASS 行丢失。`run.mjs` 结尾新增
`drainStdout()`（drain 事件 + 2s 兜底），失败退出与自然收尾两条路径均先
冲洗再退。本轮快验日志尾部三行（artifacts/console lines/flow completed）
已完整出现。

## 2. 服务端重启守卫（scripts）

新增 `scripts/game_server_restart.sh`：stop(30s) → 轮询直至游戏端口
非 LISTEN socket 清零（180s 超时，MINA 无 SO_REUSEADDR，TIME_WAIT 残留
会让新进程 `Address already in use` 丢频道，Reboot 7975 两次中招）→
start → 轮询验证 6 监听口（5 世界单频道 + 8484，120s 超时并在失败时
倾倒 ss 与容器日志）。

## 验证

- `node --check` / `bash -n` 语法校验通过。
