# 一转 NPC 修复与疾风步冷却 1 小时

## 问题 1：点击武术教练（NPC 1022000）无反应

- 断点：服务端脚本 `scripts/npc/1022000.js:50` 调 `cm.getFirstJobStatRequirement(jobType)`
  抛 `TypeError: ... is not a function`（logs/2026-10-18/error/game/npcs/1022000.txt，
  24 次）。`NPCScriptManager` 捕获后不发任何包 → 客户端"点了没反应"。
- 根因：该方法在 `AbstractPlayerInteraction` 里是 `static`，而独立版 nashorn
  （org.openjdk.nashorn 15.4）**不通过实例引用暴露静态方法**。用最小类实验实证：
  `t.smethod()` → NOT A FUNCTION，`t.imethod()` → 正常。仓库先例
  `numberWithCommas` 即为此专门包了实例壳。
- 修复：`src/scripting/AbstractPlayerInteraction.java` 去掉 `static`（调用方全部是
  npc/quest 脚本，无 Java 静态调用者）。波及恢复：战士 1022000、法师 1032001、
  弓手 1012100、飞侠 1052001、海盗 1090000、骑士团 9201123~9201127 及任务
  20101-20105 全部一转链路。
- 验证：重编译后 `javap` 确认实例方法；以真实编译类（AIProbe 子类）跑 nashorn：
  `exposed: true`、`getFirstJobStatRequirement(1)` → `"STR 35"`；容器重启后
  0 ERROR 在线。

## 问题 2：疾风步（0001002）冷却 60s → 3600s（1 小时）

| 侧 | 文件 | 变更 |
|----|------|------|
| 服务端 | `wz/Skill.wz/000.img.xml` | 0001002 level 1/2/3 `cooltime` 60→3600（相邻 0001001=120、0001005=7200 未动） |
| 客户端 | `assets/Skill.nx` | 同 3 个整型节点就地改写（新增通用工具 `tools/nx/patch_nx_int.py`，int64 LE 原位补丁，assets-server 按 size/mtime 自动重载） |
| 探针 | `tools/verify/skill-2023-port-nx.cpp` | 期望值 60→3600 并注明 ops override；复核 RESULT: PASSED |

已知外观限制：String.nx 技能描述文本"再次使用时间间隔：1分"为字符串表内容，
本工具不改写字符串；实际冷却以 Skill 数据为准。

## 复现

```bash
# 服务端
cd link_repos/MapleStory-Server && bash posix-compile.sh && docker restart ms-game-server
# 客户端 NX（三级各一次）
python3 tools/nx/patch_nx_int.py assets/Skill.nx 000.img/skill/0001002/level/1/cooltime 3600 --expect 60
# 探针复核
c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/skill-2023-port-nx.cpp \
  src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/skill-2023-port-nx && /tmp/skill-2023-port-nx assets/Skill.nx
```
