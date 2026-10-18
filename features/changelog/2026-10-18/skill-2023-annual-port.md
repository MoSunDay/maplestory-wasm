# 2023 年度版（079MAX4）技能数据双侧落地

## 背景

`link_repos/2023年度冒险岛(1).7z`（8.4GB）内含 `079MAX4-2023年度版`：
服务端（CongMS 079，XML dump）与客户端（解包 `data/skill/*.img`）。
本仓库此前两侧均为 v83 技能数据，本变更将其替换为 2023 年度版，
使技能配置在客户端（Skill.nx）与服务端（wz/Skill.wz）同时生效。

## 生效集判定

- CongMS jar（dist/079.jar）字符串仅引用 `Skill.wz`，
  启动参数 `-Dnet.sf.odinms.wzpath=wz` + Windows 大小写不敏感
  → 包内 `wz/skill.wz/` 为生效集；`22Skill.wz`/`skill22.wz` 无引用，休眠备用。
- 原版 `配置.ini` 为 `CongMS.expRate = 10`；本仓库维持 **EXP_RATE = 3**
  （`ServerConstants.java:138`，`world.ini` 无 exprate 覆盖，5 世界全部 3x）。

## 服务端（link_repos/MapleStory-Server）

- 原 `wz/Skill.wz/` 全量备份至 `wz/Skill.wz.v83-backup.tar.gz`
  （含历史残留 `000.img.xml.bak`、`000.img.xml.local-pre-port`，一并归档后清除）。
- 拷入 2023 的 75 个 `<job>.img.xml`；从备份恢复 v83 `910.img.xml`
  （GM 技能 9101000-9101005/9101008，2023 集缺失，保留以免 GM 命令失效）。
- 兼容性扫描结论：HaRepacker 结构与 v83 同构；纯 getInt 字段无非数字
  string（NFE 面 = 0）；`damage` 字符串型两侧一致（getIntConvert 兼容）；
  lt/rb 全为 vector 且成对；技能节点名全数字 → `loadAllSkills` 不会异常。

## 客户端（assets/Skill.nx）

- 旧文件备份为 `assets/Skill.nx.v83-backup`。
- 管线：2023 客户端 `data/skill/*.img`（75 个，205MB，含完整特效位图）
  → NoLifeWzToNx `--client --lz4hc` 逐 img 转 nx
  → 新增 `tools/nx/merge_skill_nx.py` 合并为单一 Skill.nx
  （合成根 + 76 个 `<id>.img` wrapper；字符串去重；从旧 Skill.nx 剪枝
  携带 `910.img` 子树，DAG 共享节点全保留、仅剔除不可达位图/音频）。
- 产物 262.7MB / 238,737 节点 / 14,449 位图，二分查找有序性保持。

## 验证

`tools/verify/skill-2023-port-nx.cpp`（登记于 tools/verify/index.md）：
wrapper 清单（76）、2023 调校值（0001002 lv1 mpCon=4/time=4/speed=10/
cooltime=60；1000001 lv1 x=4/y=3；MobSkill/100 lv1 x=115/mpCon=5/
interval=40）、GM 携带（9101000 等 7 个）、全树非降序、位图数 >14000。
服务端安装后 XML 与上述值逐字段一致。

## 已知差异（非阻塞）

- Evan（2210+）等 v83 后期职业不在 2023 集；KoC(1xxx)/Aran(2xxx) 自制数据在。
- 2023 定制数据存在 5 处重复子名（21100002/effect0、4111002/effect、
  5121001/hit 各级帧 "2"），二分查找命中其一，渲染上个别帧不可达。
- `220.img` 一处非数字 `attackCount` → 服务端 getIntConvert 兜底 1，
  启动日志有一次 NFE 堆栈噪音。
- MobSkill 扁平布局与 MobSkillFactory 目录约定不符为存量问题，非本次回归。

## 复现

```bash
7z x 'link_repos/2023年度冒险岛(1).7z' '*/服务端/wz/skill.wz/*' '*/客户端(登录器在bin内)/data/skill/*'
.cache/tools/bin/NoLifeWzToNx --client --lz4hc <skill目录>/*.img
python3 tools/nx/merge_skill_nx.py -o Skill.nx <各img的nx> assets/Skill.nx.v83-backup::910.img
c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/skill-2023-port-nx.cpp \
    src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/skill-2023-port-nx
/tmp/skill-2023-port-nx Skill.nx
```
