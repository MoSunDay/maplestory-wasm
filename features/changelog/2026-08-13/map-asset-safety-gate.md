Commit: 207b59c38ce4d3481f8c83b38480af94c6cf29cc

# 地图素材安全门与持续预取

## Context

进入地图后仍可能有地图、实体或下一动画帧素材尚未从 NX 加载。客户端此前会继续推进物理、怪物和碰撞，玩家可能在黑块或特效缺失期间已经受伤；单纯后台预取也无法保证首屏完整。

## Change Summary

- `Stage` 新增 `LOADING` 状态：构造地图并接收初始实体，但不推进物理、AI、战斗、碰撞或 UI 输入。
- 延迟地图过渡完成的 `PLAYER_UPDATE`，直到全地图已实例化贴图 NX 范围驻留、BGM 范围读取完成、首屏及当前实体可见贴图进入 WebGL 图集，并经过 250ms 资源集合稳定窗。
- 地图构造出的其余贴图继续由 4 路 LazyFS 队列异步预取；首屏静态动画全帧提升为前台优先，物品全量预载推迟到地图安全门完成后启动。
- 统一素材 Loading 层展示进度并阻塞 Canvas；60 秒无进展进入错误态，重试会重新调度缺失 NX 范围和图集任务。
- 正常游戏中若新出现的可见素材未准备好，也会先释放持续动作并冻结 Stage/UI 更新，素材完成后自动恢复。

## Validation

- 地图安全门纯函数覆盖驻留、可见图集、稳定窗、动态集合、超时与重试。
- LazyFS 连接恢复验证通过。
- Docker WASM release 构建和三个 Rust Web 服务 release 构建无警告通过。
- WebUI E2E 已扩展为进入游戏后显式等待 `msstage_loading == 0`；当前机器 Chromium 90 无 WebGL2，浏览器执行受环境能力阻塞。

## Impact Surface

WASM 地图状态机、图形异步队列、LazyFS 调度顺序、输入路由、BGM 预备、浏览器 Loading 层和 WebUI 验收。
