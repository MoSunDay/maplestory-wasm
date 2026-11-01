# 技能窗口技能名消失 + 终极攻击攻击次数（2 段 / 8 只）

## 背景

用户报告两处问题：

1. 技能窗口（K）里**所有技能名都不显示**，且启动时偶发控制台
   `null function` 崩溃；
2. 终极攻击（Final Attack，1100002/1200003/1300002）**只打 1 下、
   只命中 1 只**，与服务端期望的追加攻击不符（`AbstractDealDamageHandler`
   对 FA 技能特判 `mobCount = 15`），且低级阶段完全没有 2 段效果。

## 根因

### 一、技能名：Asyncify 单 rewind 缓冲 + 同步快路径

`SkillData` 构造时读 `String.nx/Skill.img/<id>/name`，走 LazyFS 的
`open()`/`fetch_data()`。Asyncify 只有一份 rewind 缓冲，唤醒时**从栈底
整帧重放**，于是 `open()`/`fetch_data()` 会再执行一遍：

- 8b5cde5 的 `readCachedSync` 快路径对重放也生效，挂起读的 chunk 此时
  已在 `chunkCache`，快路径直接返回数据 → 重放**越过**记录帧继续跑 →
  后续 `dynCall` 拿到失效索引（`null function`），或读到尚未取回的字节 →
  `Skill.img` 的字符串缓存被写成空名，技能栏整列变空；
- d1dbf54 全禁快路径后，重放中的**非挂起**读又必须 `handleSleep`，单缓冲下
  变成从栈底无限重放（忙自旋冻死）；
- 旧 `readCachedSync` 跨 chunk 恒返回 null，大节点表（常跨 2MB chunk）
  永远进不了快路径，进一步放大上面两条。

### 二、终极攻击：只在 30 级写了 `attackCount`/`mobCount`

079MAX4 数据集里 FA 技能仅 **30 级**带 `attackCount=2`/`mobCount=8`，
1-29 级两个键都缺失；客户端 `SkillData.cpp`、服务端
`MapleStatEffect.getInt` 都默认 1，所以非满级终极攻击只有 1 段 1 只。
客户端与服务端数据是两份（NX 归档 + 服务端 wz XML），必须同时打。

## 变更

### LazyFS（技能名 / 启动崩溃）

- `src/client/LazyFS/LazyFileBackend.cpp`
  - `fetch_data`：读取 `Asyncify.state`。state 2 (Rewinding) 且
    `Module.LazyFS.pendingSyncRead` 命中本读 → 走 `handleSleep`，让重放
    停在记录帧；state 2 的**其它**读 → 只查 `readCachedSync`，命中同步
    返回数据，未命中 `replayFailures++` 后返回 0（由调用方在 Normal 态重试）；
    快路径仅用于 state 0/1。
  - `open()` 的 size 取回同理：重放的 `open` 只在 size 已缓存时同步返回，
    不再二次挂起；`pendingSyncRead` 用 `'size#'+path` 与读操作区分键。
- `src/client/LazyFS/lazyfs_sync_cache.js`
  - `readCachedSync` 支持**跨 chunk 拼接**（首/中/尾段分别处理，中间段长度
    不足 `CHUNK_SIZE` 视为截断，返回 null 交回异步路径）。
  - 删除 `warmupAfterPark`：在重放中追加异步请求只会污染单 rewind 缓冲。

### 终极攻击数据（NX + 服务端 XML 双侧）

- 新增 `tools/nx/patch_final_attack_counts_nx.py`
  - `assets/Skill.nx`：对 1100002/1200003/1300002 的 1-29 级 level 节点插入
    `attackCount=2`/`mobCount=8`（nlnx 按字节序二分查子节点，保持块有序紧凑），
    越界值强制回 2/8，重建归档并保留 `.pre-fa-counts-backup`。
  - `link_repos/MapleStory-Server/wz/Skill.wz/{110,120,130}.img.xml`：在每级
    `</imgdir>` 前插入/改写两个 `<int>` 标签，其余字节不动。
  - 幂等：已满足 2/8 的 level 不写；写完双侧回读断言。
- `tools/verify/final-attack-combo-nx.cpp`：断言从「1 级依赖默认 1 段 1 只」
  改为「每级 `attackCount=2` 且 `mobCount=8`」。

## 验证

- 启动：`boot-trace` 干净构建 `boot ok, no crash`，`Asyncify.state=0`、
  `pendingSyncRead=null`、`replayFailures=0`。
- 技能名：登录态临时探针直接调用 `SkillData::get(id).get_name()`
  （与 `UISkillBook` 同一条 LazyFS 路径），1100002 / 1200003 / 1300002 /
  1001004 / 1101004 全部返回非空（"终极剑"/"终极钝器"/"终极枪"/"强力攻击"/
  "快速剑"）；首次调用会在 `handleSleep` 挂起，重试命中缓存返回数据，
  不再出现空名。探针验证后已移除，正式构建无调试符号。
- 终极攻击 NX 运行时读取：`SkillData::get(id).get_stats(level)` 在
  1100002 lvl1/7/29、1200003 lvl14、1300002 lvl20 均为
  `attackcount=2, mobcount=8`。
- 数据侧：`patch_final_attack_counts_nx.py --dry-run` 双侧 no-op（NX 每技能
  58 节点、XML 每文件 30×2 标签已就位）；
  `final-attack-combo-nx assets/Skill.nx` 全绿（exit 0）。
- 服务端 rz 变更只需重启容器（`posix-compile.sh` 只编译 java，wz 运行时读）。

## 备注

- 进图阶段仍会遇到 Chromium/SwiftShader renderer 与 GPU 进程互等 `condvar`
  的环境级死锁（与本修复无关，A/B 关闭快路径同样冻结，`/proc` 见 GPU
  进程 300-600% CPU），游戏内截图验证因此未做；技能名/攻次数据改用上述
  运行时探针取证。
- 详细复盘见 `docs/known-issues/2026-10-31-asyncify-wedge-and-private-e2e-stack.md`
  末尾「后续结论」。
