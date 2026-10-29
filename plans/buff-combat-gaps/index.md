# 减益映射与受击随机数遗留缺口

## 当前快照

- 最后更新：2026-10-28
- 当前状态：待启动
- 一句话目标：补齐服务端减益（WEAKEN 等）到客户端 ActiveBuffs 的映射，并为受击/动作伤害引入 v83 随机系数。
- 当前进展：仅登记（源自圣甲术修复评审结论），未动工。
- 下一步：见 todos/。
- 当前 blocker：无。
- 成功标准：见各 TODO 验证手段。
- 非目标：不改动本次已修复的 WDEF 抵扣系数 0.7 与 buff 施放节流。

## 背景

圣甲术（1001003）受击伤害与施放节流修复评审（2026-10-28）判定的两项非阻塞缺口，
不影响本次修复正确性，另立任务跟进。

## 环境阻塞记录（2026-10-29）

节流 e2e（iron-body-cast-throttle.mjs）当日未能完成运行时验证：共享
game-server 处于崩溃重启循环（对端会话热修其未提交的 Java 补丁），客户端
wasm 在登录流被断开后阻塞等待封包字节，主线程冻结、CDP 超时。对照实验
（--host-resolver-rules 屏蔽 8090）证明页面本身可 72s+ 稳定运行，冻结
100% 来自服务端连接路径。服务端稳定后重跑：

    UPDATE maplestory.accounts SET loggedin=0 WHERE name='ibtest';
    CHROME_BIN=/usr/bin/chromium-browser E2E_ACCOUNT=ibtest E2E_PASSWORD=test1 \
      node tools/verify/maplestory-webui/iron-body-cast-throttle.mjs

ibtest 账号（ibHero，密码同 test1）为圣甲术 e2e 专用克隆，避免与其他
会话争抢 test1 登录态。

### 23:40 复查：阻塞性质已定位为协议中间态

当晚 23:17 服务端重启后进入稳定但**不发 WORLD_LIST** 的状态（78 次登录全卡
world select，含对端自己的测试循环；assets-server/ws-proxy/客户端二进制/
探针导出均已逐一验证健康，详见
docs/known-issues/2026-10-29-game-server-restart-client-freeze.md）。
属对端登录协议改造的中间破损态。复跑前置判据追加：

    docker logs ms-game-server --since 10m 2>&1 | grep -c "WORLD_LIST"  # 须 >0

通过后依上节命令重跑；PASS 后再补 iron-body-client-flow.mjs（buff 图标）
与 scripts/test_accounts/verify_all.sh all --skip-visual 全量回归。
