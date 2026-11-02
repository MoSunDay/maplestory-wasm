# CharSelectedHandler 缓存未命中静默 close 补 WARN 日志

## 背景

原 `docs/known-issues/2026-10-31-game-server-sql-seeding-pitfalls.md`（已
删除，要点并入 `docs/roadmap.md`「服务端种子与反馈事项」）尾部
"其他待反馈服务端的问题"第一条：`CharSelectedHandler` 对缓存未命中走
**静默 `closeSession`**，建议至少打日志。

排查链路：

- 选角封包到达时先过 `Server.haveCharacterEntry(accId, charId)`，查
  `accountChars` 缓存（仅 boot / 角色增删 / 转世界时更新）；
- 缓存查不到（直改 DB、缓存未装载等）即 `closeSession(session, true)`
  直接返回，**零日志**——客户端症状为"双击角色无声掉线"，服务端日志
  无任何对应线索，无法定位；
- 其后 `getCharacterWorldRefreshed`（补丁 0008 引入）虽已按 DB 刷新
  `worldChars`，但若在前面一步就被静默 close，则永远走不到。

同文件不存在"会话强杀后重放对话脚本"入口（known-issues 同节的
`[HACK] Not enough items available of Item:4031008` 竞态发生在 NPC
对话 handler），故无需加排查 INFO 日志。

## 变更

### linked server（`scripts/server-patches/0011-charselected-cache-miss-logging.patch`）

- `CharSelectedHandler.handlePacket`：仅在 `haveCharacterEntry` 未命中
  的静默 close 前补一条 WARN——
  `System.err.println("[WARN] CharSelectedHandler character cache miss: accountName=..., charId=..., world=... (no character entry in accountChars cache for accId=...), closing session")`，
  格式照抄同层 `Server.java` 既有 `[WARN]` 日志（补丁 0008 引入的
  `System.err.println` 约定，该 Handler 与 login 层均无 Logger）；
- 纯日志，4 行新增，零行为变更。

## 验证

- `diff -u` 备份对照生成补丁，checkout 根目录
  `patch -p1 -R --dry-run < scripts/server-patches/0011-charselected-cache-miss-logging.patch`
  干净回退（补丁与当前源码一致）；
- `grep -n 'character cache miss' src/net/server/handlers/login/CharSelectedHandler.java`
  命中新增日志行；
- `bash posix-compile.sh` 编译通过（仅既有 deprecation 警告）；
- `docker restart ms-game-server` 后容器 Up、`docker logs --since 3m`
  无新异常、无 bind 失败，登录口 8484 就绪监听。

## Related Docs

- [服务端补丁 0011](../../../scripts/server-patches/0011-charselected-cache-miss-logging.patch)
- [服务端补丁索引](../../../scripts/server-patches/README.md)
- [roadmap：服务端种子与反馈事项（原 SQL seeding 陷阱尾部待反馈问题已并入）](../../../docs/roadmap.md)
