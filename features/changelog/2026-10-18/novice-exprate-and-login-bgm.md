# 新手经验倍率统一 3x 与登录界面 BGM 切换修复

日期：2026-10-18

## 问题

1. **经验不是 3 倍**：源码 `EXP_RATE = 3` 且 dist 编译产物中世界倍率确为 `iconst_3`，
   但 `ServerConstants.USE_ENFORCE_NOVICE_EXPRATE = true`（编译期常量）使
   `MapleCharacter.hasNoviceExpRate()` 对初心者职业（job 0/1000/2000）且 ≤10 级的
   角色强制 `getExpRate() = 1`。新建测试角色全部命中该区间，表现为 1x。
2. **退出回登录界面音乐不变**：WASM 音频管线要求先 `Music::prepare()`（→
   `wasm_audio_register_clip`）注册 clip 再 `play()`，否则 JS 端 `playMusic` 在
   `if (!clip) return` 处静默早退。全工程只有地图 BGM 走过 prepare（Stage.cpp:128、
   StageLoading.cpp:170），登录 BGM `BgmUI.img/Title`（UILogin.cpp:38）从未注册；
   且早退发生在停掉 `state.currentMusic` 之前，导致退出到登录后地图 BGM 继续循环。

## 修复

| 文件 | 变更 |
|------|------|
| `link_repos/MapleStory-Server/src/constants/ServerConstants.java:88` | `USE_ENFORCE_NOVICE_EXPRATE` true→false，全服统一按世界倍率 3x |
| `src/client/Audio/Audio.cpp`（`Music::play()` WASM 分支） | 播放前补调 `prepare()`，play 自足（decodeClip 幂等，重复注册无开销） |
| `src/client/Audio/Audio.cpp`（JS `playMusic`） | 停旧曲 + musicToken 递增提前到 clip 查询之前，clip 缺失时不再残留旧 BGM |

## 生效与验证

```bash
# 服务端：重编译 + 重启（/srv 即本仓库 link_repos/MapleStory-Server 的 bind mount）
cd link_repos/MapleStory-Server && bash posix-compile.sh
javap -c -p dist/client/MapleCharacter.class | grep -A2 hasNoviceExpRate   # 期望 iconst_0
docker restart ms-game-server
docker logs ms-game-server --since 120s    # "Skills: Loaded in 0.288 seconds"、5 世界在线

# 客户端：重建 WASM（ms-web-server 整仓挂载，build/ 产物即时生效）
bash scripts/docker_build_wasm.sh
```

游戏内复核：新手号杀怪经验 = 3 × 怪物经验（`@rates` 不再显示 novice rate）；
菜单退出回登录界面 BGM 切换为 `BgmUI.img/Title`，冷启动登录界面亦有音乐。
