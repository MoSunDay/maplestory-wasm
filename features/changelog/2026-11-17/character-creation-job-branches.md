# 建角四职业循环选择：战神长枪/海盗指虎起始武器与出生图 E2E 断言

## 背景（Context）

- 建角界面此前仅在封包层固定发送 `job=1`（Explorer），服务端自定义客户端模式
  实际支持 0=Cygnus / 1=Explorer / 2=Aran / 3=Pirate 四个分支，界面无从选择。
- v83 `MakeCharInfo` 池只覆盖冒险家武器，战神（长枪 1442079）与海盗
  （指虎 1482000 / 短枪 1492000）需要钉死各自的起始武器池。
- 服务端配套缺口：`job == 3` 无 creator；`createCharacter` 返回 -1/-3 时静默
  无应答，客户端建角界面永久挂起；Aran 任务链脚本引用上游 YamlConfig 断链。

## 改动（Change Summary）

- 新增 `src/client/IO/UITypes/CharacterCreation/JobOption.h`：纯函数职业域
  ——wire code 枚举、中文标签、按职业钉死的 `weapon_pool()`、循环切换
  `cycle()`（Explorer → Cygnus → Aran → Pirate，支持左右双向）。
- `UICharCreation.cpp/.h`：新增 BT_CHARC_JOBL/JOBR 职业循环按钮；武器池与
  `CreateCharPacket` 的 job 字段改为跟随所选职业（`weapon_choices()[weapon]`
  而非基础池同索引元素）；绘制职业标签。
- `ImeBridge.cpp` + `CMakeLists.txt`：导出新探针 `msui_character_creation_job`
  （E2E 断言建角界面当前职业的 wire code）。
- 服务端补丁 `scripts/server-patches/0023`（Pirate creator / -1/-3 回
  deleteCharResponse(0,9) / `useFullAranSkillset()` 暴露）与 `0024`
  （Aran 任务链 21101/21201/21302 分支判断改走该方法 + 914000000
  onUserEnter 解锁 UI 钩子），README 索引同步登记（均已应用于 2026-11-16）。
- E2E：`run.mjs` 新增 `E2E_CHAR_JOB` 环境变量（默认 explorer 零额外点击，
  保持旧跑法不变），按 wire code 断言循环点击次数、出生图
  （explorer 10000 / cygnus 130030000 / aran 914000000 / pirate 120000000），
  非默认职业翻页进入新建角色卡片；非 explorer 跳过新手图 mob 走查。
- DB 事实核查夹具：`scripts/game_server_character_weapon.sh`（查装备栏
  position=-11 武器位 itemid，口令运行时读服务端配置）+
  `support/char-weapon-fixture.mjs`（Aran=1442079 / Pirate=1482000）；
  `scripts/game_server_test_account_slots.sh` 查账号建角槽位。
- 契约测试 `tools/verify/character-creation-packet.cpp`：全部四个 job code
  的线上编码逐字段校验，Explorer 保留全字段字节序锁。
- 文档：`docs/ms-network-protocol.md` job 字段补 Pirate=3；
  `features/index.md` 角色创建条目更新。

## 验证（Verification）

- `cargo test --workspace` / `cargo clippy --workspace --all-targets -- -D warnings`
  / `cargo build --workspace` 全绿。
- `character-creation-packet` 四职业线序契约测试通过。
- E2E（README 登记口径）：`E2E_CHAR_JOB=pirate` → wire 3、出生图 120000000、
  DB 武器位 1482000；`E2E_CHAR_JOB=aran` → wire 2、出生图 914000000、
  DB 武器位 1442079。

## 影响面（Impact Surface）

- 客户端建角界面与 CREATE_CHAR 封包 job/weapon 字段；默认 explorer 路径
  行为与点击次数不变。
- 服务端改动仅存在于 link_repos 检出（补丁留档可重放），重放后需重编 dist
  并重启容器。
- 其余界面、协议、DB schema 均不变。
