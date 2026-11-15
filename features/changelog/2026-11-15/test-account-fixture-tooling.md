# 测试账号夹具工具化：Hero 装备袋契约、门日志留档与 loopback 静态服务

## 背景（Context）

- `scripts/test_accounts/` 的角色夹具此前只覆盖已穿戴装备；自动化的
  `equip-stat-flow.mjs` 期望值继承自手工维护的 test1Hero：Hero 穿裤
  （1062000）不穿上衣，且 EQUIP 背包 2..5 槽还有四件未穿装备，夹具需要
  原样固化这组 legacy DB 行（1062000 在本地 wz XML 无条目）。
- clean-HEAD e2e 复验需要一台支持 HTTP Range 的静态资产服务器：stdlib
  `python3 -m http.server` 对 Range 请求返回 200 全量，而客户端分片 wasm
  加载器要求 206，3 次重试后以 "WASM module did not initialize" 失败。

## 改动（Change Summary）

- `scripts/test_accounts/plans.py`：`SLOT_POSITION` 补 `pants`（-6）；
  固化 `HERO_PANTS` 与 `HERO_BAG_EQUIPS` 契约（legacy DB 行逐字段原样）；
  `_body_slot` 让 Hero 保持裤装形态，`plan_bag_equips` 输出背包四件装备，
  `_stat_key_to_req` 容忍 wz 信息缺失（info 为 None 时需求按 0）。
- `scripts/test_accounts/create_accounts.py`：抽出 `_equip_stats_insert`
  供已穿/背包装备共用；新增 `bag_item_sql`（inventorytype=1 固定槽位写
  inventoryitems + inventoryequipment）；`build_sql` 逐角色写入背包装备。
- `scripts/test_accounts/verify_all.sh`：equip / cooppet 两阶段的门输出由
  纯 `tail` 改为 `tee /tmp/<flow>-last.log` 后再 `tail`，失败可回溯完整
  日志，不再只有最后几行。
- `scripts/test_accounts/serve_clean.py`（新增）：最小 206 Partial Content
  静态服务器，用法 `python3 serve_clean.py <port> <root>`（默认 8010）；
  支持 Range/后缀 -N/416/HEAD 与目录穿越防护，**只绑定 127.0.0.1**。
- 凭据零硬编码：DB 连接信息运行时解析 linked server `configuration.ini`
  （URL/DB_USER/DB_PASS），账号密码取 `MS_TEST_ACCOUNT_PASSWORD`（未设置
  时为账号名本身），仅以 HeavenMS 接受的 sha512 形式入库。

## 验证（Verification）

- `python3 -m py_compile scripts/test_accounts/create_accounts.py
  scripts/test_accounts/plans.py scripts/test_accounts/serve_clean.py`。
- `bash -n scripts/test_accounts/verify_all.sh`。
- `E2E_PET_OPTIONS=pickup E2E_REQUIRE_MOBS=1 E2E_ROUTE_TUTORIAL_MOB_MAP=1
  node tools/verify/maplestory-webui/run.mjs` 全绿：建角、夹具装备/背包
  落库、现金栏召唤、内建能力、拾取与跟随恢复均由这套夹具供数。

## 影响面（Impact Surface）

- 仅测试夹具与验证脚本，不改客户端/服务端行为、协议或 DB schema。
- 12 职业夹具中只有 Hero 携带裤装与背包装备，其余职业蓝图不变。
- `serve_clean.py` 只服务 loopback 上的 clean-HEAD 资产副本，不对外暴露。
