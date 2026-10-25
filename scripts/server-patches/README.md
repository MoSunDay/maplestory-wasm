# 服务端源码补丁（server-patches）

`link_repos/MapleStory-Server` 是本地 linked checkout（目录内无 `.git`，
父仓也不跟踪其内容）。凡是对服务端 **Java 源码** 的修改都无法随父仓
提交持久化——重新 checkout / 同步该目录后修改即丢失。

因此服务端源码改动必须：

1. 在本目录以编号补丁形式留档（`NNNN-<slug>.patch`，统一 diff，可在
   checkout 根目录 `patch -p1` 重放）；
2. 补丁头部注释写明背景、应用与验证命令；
3. 对应的父仓 changelog 引用补丁文件路径；
4. 若改动影响运行产物（`dist/`），重放后需 `bash posix-compile.sh`
   并重启容器 `ms-game-server`（`docker restart ms-game-server`，
   unless-stopped 策略，登录口 8484）。

## 已知差异（相对上游 HeavenMS checkout）

| 补丁 | 内容 | 状态 |
|------|------|------|
| [0001-fix-iron-body-skill-id-constants.patch](0001-fix-iron-body-skill-id-constants.patch) | 圣甲术 `Swordsman.IRON_BODY` 1000003→1001003；`Rogue.NIMBLE_BODY` 4001000→4000000（未引用常量对齐） | 已应用于当前 checkout 并重编入 dist（2026-10-25） |

注意：`Bowmaster.ARMOR_CRASH = 3121010` 经审计为不可达死代码
（v83 无此技能槽位），**有意保留**，不做补丁。
