// FM 组队任务(PQ) e2e 流程的共享 harness（纯函数，无 class）。
//
// 2026-10-26 起按职责拆分为五个子模块（各 ≤400 行），本文件仅做聚合
// re-export，保持 fm-pq-teleports / fm-pq-solo-flow 等消费方导入不变：
//   * pq-harness-db.mjs     —— DB 凭据解析（configuration.ini，不硬编码）、
//                              SQL 预置、账号释放等待；
//   * pq-harness-game.mjs   —— 游戏态探针（probe/msmap_id/msplayer_x…）、
//                              waitFor、停滞看门狗、素材就绪截图；
//   * pq-harness-login.mjs  —— 账号/密码输入、登录进游戏、浏览器会话管理；
//   * pq-harness-move.mjs   —— walkTowards 走位（含起跳跨台阶）、同层逼近
//                              NPC、Space 开对话 / Escape 关闭、聊天发送；
//   * pq-harness-dialog.mjs —— 选项行悬停扫描（ORANGE 重绘采样）与
//                              YES/OK 按钮视觉定位（png-probe 像素解码）。
export * from './pq-harness-db.mjs';
export * from './pq-harness-game.mjs';
export * from './pq-harness-login.mjs';
export * from './pq-harness-move.mjs';
export * from './pq-harness-dialog.mjs';
