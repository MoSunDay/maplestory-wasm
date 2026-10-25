# 宠物浏览器级验收证据归档（2026-10-25）

commit 66a8454f「宠物同参跟随 + 批量扫拾」的浏览器级验收证据归档；账号 accept1/accept1Hero，地图 40001。
运行命令要点：`E2E_PET_OPTIONS=pickup|follow E2E_ACCOUNT=accept1 E2E_CREATE_CHARACTER=0 CHROME_BIN=/usr/bin/chromium-browser node tools/verify/maplestory-webui/run.mjs`

## 文件说明

- pickup-evidence.json（pickup 模式）：服务端确认拾取 0→3、精确 OID 1000000029 移除 1ms、finalTargetOid=0；follow 子门槛 petP95 4≤4.1、距离 40、0 瞬移、0 越权跳跃。
- pickup-trace.json（pickup 模式）：拾取轨迹采样（targetOids/confirmedTargetOid/removal 时序）。
- follow-evidence.json（follow 模式）：基态同参 petP95=playerP95=4、距离 33、3 折返、0 瞬移、0 越权、recovered。
- follow-buff-evidence.json（follow 模式）：装备临时提速（SPEED 140/JUMP 127，测试后已还原 8/7）下 playerP95=5=petP95，动态 MovementProfile 链背书。

## 复算说明

harness 见 tools/verify/index.md 与 tools/verify/maplestory-webui/index.md；重跑需游戏服务器/web-server/ws-proxy 在运行。

## SHA256

b3215c12f2e2c94bb8a4c3a4859a1848618a380bb19e209aeb95e29d522c83a8  pickup-evidence.json
df87d81e79cb978f5097e57f6e979f008ad729a34574f30eb61043cbe9fbc310  pickup-trace.json
913a47f9c4301c1b2555cc2910b721327167877e415511503ca2eaf50090d575  follow-evidence.json
a8bc45beb0974485c4b07273f90ef43d169b20046ba85821c85e35f7d66b18fa  follow-buff-evidence.json
