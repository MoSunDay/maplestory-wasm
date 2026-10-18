# 解封 test 账号与自动封禁开关配置化

## 问题

账号 `test` 被 autoban 误封：`Skill: 1001005; Player MP: 20; MP Needed: 30`
（Slash Blast 低 MP 时攻击包到达服务端，MPCON 检查 25 点/30s 累计自动封号，
属客户端拦截竞态导致的误判，非外挂）。用户要求解封并提供关闭自动封禁的配置。

## 变更（linked `/data/MapleStory-Server`，非 git 管理，posix-compile.sh + 容器重启生效）

| 文件 | 变更 |
|------|------|
| `configuration.ini` | 新增 `AUTOBAN=false` |
| `src/constants/ServerConstants.java` | `USE_AUTOBAN` 由 `static final` 改为 static 字段；static 块从 configuration.ini 读取 `AUTOBAN`（缺省 true 保持原行为） |

覆盖面：`AutobanManager.addPoint`（累计计分路径）、`AutobanManager:116`、
`AutobanFactory:87/99`（FAST_ATTACK/MOB_COUNT/FIX_DAMAGE/GACHA_EXP/HIGH_HP_HEALING
等直接封禁路径）——全部经 `ServerConstants.USE_AUTOBAN` 门控，false 时全量关闭。
`USE_AUTOBAN_LOG` 不受影响（仍可看日志）。

## 数据库

```sql
UPDATE accounts SET banned=0, banreason=NULL WHERE id=24;  -- test
```
`macbans`/`ipbans`/`hwidbans` 已核查为空，无残留。

## 验证

- 编译后独立 runner（CWD=服务端目录，触发 static 块读 ini）输出
  `USE_AUTOBAN=false`；
- 容器干净重启（stop -t 30 → start）：10/10 频道 + 8484 在线；
- `test` 账号 banned=0。

## 回滚

`configuration.ini` 删掉 `AUTOBAN=false`（或缺省 true）+ 重启即恢复自动封禁。
