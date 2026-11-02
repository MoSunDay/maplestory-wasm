# 修复排行 Worker 每小时整点 MySQL 保留字异常（RANK）

## 背景

`docker logs ms-game-server` 每到整点固定抛：

```
com.mysql.jdbc.exceptions.jdbc4.MySQLSyntaxErrorException: You have an error
in your SQL syntax; ... right syntax to use near 'rank = 1, rankMove = 0
WHERE id = 1' at line 1
    at net.server.worker.RankingLoginWorker.updateRanking(RankingLoginWorker.java:74)
```

排查链路：

- `Server.java` 以 `RANKING_INTERVAL` = 60min 注册 RankingLoginWorker，
  初始延迟对齐下一整点——异常精确出现在每个小时边界；
- `RANK` 自 MySQL 8.0 起是保留字（窗口函数），而 worker 总榜分支
  （job == -1）拼出的预编译语句 `UPDATE characters SET rank = ?,
  rankMove = ? WHERE id = ?` 未给 rank 加反引号 → 服务端 prepare 即抛
  1064，整个事务回滚，characters.rank / rankMove 列从不更新；
- 职业榜分支（jobRank / jobRankMove，非保留字）、总榜 SELECT（限定名
  `c.rank`，合法）与 `resetMoveRank`（rankMove 非保留字）均正常，
  错误文本也证明只有总榜 UPDATE 失败。

## 变更

### linked server（`scripts/server-patches/0009-ranking-worker-rank-reserved-word.patch`）

- `RankingLoginWorker.java updateRanking()`：仅总榜分支的 UPDATE 列名加
  反引号，`rank = ?, rankMove = ?` → `` `rank` = ?, `rankMove` = ? ``；
  jobRank 分支、SELECT、resetMoveRank 一律不动，1 行最小 diff。

## 验证

- 编译前对 live DB 回滚事务实测（凭据取自 configuration.ini 的
  DB_USER/DB_PASS，`MYSQL_PWD` 注入、不打印口令）：
  - 新写法 ``START TRANSACTION; UPDATE characters SET `rank` = 1,
    `rankMove` = 0 WHERE id = 243; ROLLBACK;`` → ROW_COUNT()=1，
    回滚后 test1Warr（id=243）rank/rankMove 原值不变；
  - 旧写法同语句 → `ERROR 1064 ... near 'rank = 1, rankMove = 0 WHERE
    id = 243'`，与 worker 日志逐字一致（日志中 `WHERE id = 1` 为整点
    首个角色）；
- `bash posix-compile.sh` 编译通过（仅既有 deprecation 警告），
  `dist/net/server/worker/RankingLoginWorker.class` 常量池含
  `` `rank` = ?, `rankMove` = ? `` 字面量；
- `docker restart ms-game-server` 后容器 Up、登录口 8484 LISTEN
  （/proc/net/tcp6 :2124 = 8484），日志出现 "MapleStory is now online"，
  重启后零 MySQLSyntaxErrorException；
- 下一整点 tick（2026-11-02 21:00 CST）由后续值班观察确认不再抛异常。

## Related Docs

- [服务端补丁 0009](../../../scripts/server-patches/0009-ranking-worker-rank-reserved-word.patch)
- [服务端补丁索引](../../../scripts/server-patches/README.md)
