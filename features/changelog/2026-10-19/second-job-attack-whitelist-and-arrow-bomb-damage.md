# 二转攻击技能白名单补齐 + 爆炸箭伤害双侧落地

## 背景

用户报告：1) 多个职业的二转技能攻击无效；2) 勇士仍有动画缺失、攻击计算无效。

排查结论（数据源均与 `link_repos/2023年度冒险岛(1).7z` 内 079MAX4 原始包逐字节比对一致，
服务端 `wz/Skill.wz/` 移植无缺失）：

- **主根因在客户端**：`Data/SkillData.cpp flags_of()` 白名单按 v83 技能集硬编码，
  079MAX4 数据集里大量二转攻击技能不在表内 → `is_attack()`=false → 走 buff 分支，
  只有特效+UseSkillPacket，无攻击判定、无伤害、且 `NoAction` 导致**无身体动作**（动画缺失）。
- **勇士两症状同源**：1111007 防御崩坏 / 1111008 虎咆哮未列入白名单（SHOUT 常量早已定义
  但从未登记 flags）。勇士一转**强力攻击 1001004 在 079MAX4 数据集中被移除**
  （原包源 XML 即无此技能，非移植丢失），白名单残留条目因数据缺失不可用，属数据集设定。
- **爆炸箭 3101005 伤害字段缺失**：079MAX4 将逐级伤害放在 level 节点 `x` 字段
  （72→220%），无 `damage`/`mad` → 客户端伤害乘数 0（每击 1 点），
  服务端 `damage` 默认 100 → 伤害上限校验（`calcDmgMax = base * damage/100`）偏低，
  高等级正常伤害有 DAMAGE_HACK 误判风险。

## 变更

### 客户端（src/client）

- `Character/SkillId.h`：新增 17 个技能常量（命名对齐服务端 `constants/skills/*.java`）：
  COLD_BEAM/THUNDERBOLT(2201004/5)、HOLY_ARROW(2301005)、ARROW_BOMB(3101005)、
  IRON_ARROW(3201005)、STEAL/SAVAGE_BLOW(4201004/5)、BACK_SPIN_BLOW/DOUBLE_UPPERCUT/
  CORKSCREW_BLOW(5101002-4)、INVISIBLE_SHOT/GRENADE/BLANK_SHOT/RECOIL_SHOT(5201001/2/4/6)、
  ARMOR_CRASH(1111007)、MAGIC_CRASH(1211009)、POWER_CRASH(1311007)。
- `Data/SkillData.cpp`：`flags_of()` 补 18 条 ATTACK（远程类带 RANGED）；
  stats 解析增加 damage≤0 时回退读 `x`/100（爆炸箭）。已全量扫描确认攻击技能中
  仅 3101005 命中该回退（1111007 无 x，按每击 1 点崩防语义）。

### 服务端（link_repos/MapleStory-Server）

- `wz/Skill.wz/310.img.xml`：3101005 全部 30 级物化 `<int name="damage" value="{x}"/>`
  （lv1=72 … lv30=220），与客户端 `x` 兜底语义一致；ElementTree 校验合法、
  damage==x 逐级相等；其它 img 未动（220.img md5 与源包一致）。
- 重启加载验证：`Skills: Loaded in 0.358s`，无异常。

### 运维

- `scripts/game_server_restart.sh`：端口释放等待从 5 个游戏端口扩到含 8484（登录端口）。
  本次实测旧脚本重启时 8484 处于 TIME_WAIT，登录线程 `Address already in use` 绑定失败
  （通道在线但无法登录）；修复后 6/6 监听完整拉起。

## 验证

- 新增 `tools/verify/second-job-attack-nx.cpp`（登记于 tools/verify/index.md）：
  18 技能存在且 lv1 有 damage/mad/x>0（1111007 特例无伤害字段）；3101005 全 30 级 x>0
  且 lv1=72/lv30=220；100.img 无 1001004；111.img 恰含 1111002/1111005/1111007/1111008；
  全部含 hit/effect/finish 动画节点。RESULT: PASSED。
- 回归 `skill-2023-port-nx`：PASSED。
- `scripts/docker_build_wasm.sh` 全量构建通过。
- 服务端 `game_server_restart.sh` 重启后 6/6 端口监听、docker logs 无异常。

## 已知限制

- 群体治愈 2301002 未标记为攻击：对不死系伤害走服务端专用公式（INT/魔法推算），
  客户端无对应公式，贸然本地计算有超出服务端上限触发 autoban 的风险；治疗功能不受影响。
- 魔法系 079 语义 `mad`="攻击力+N"（扁平魔攻加成），客户端暂按基础魔法区间显示，
  不叠加该加成（数值偏低但可用；服务端上限按 `calcDmgMax *= mad` 宽松，不会被拒）。
- 回旋斩 4201005 数据无 mobCount → 双侧默认 1 目标（数据集设定，双侧一致）。
- 投弹攻击 5201002 keydown 蓄力机制未实现，按键即按当前级别伤害攻击一次。
- 强力攻击 1001004 / 混乱术 4001002 等在 079MAX4 数据集中不存在，白名单残留条目无害。

## 复现

```bash
c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/second-job-attack-nx.cpp \
    src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/second-job-attack-nx
/tmp/second-job-attack-nx assets/Skill.nx
bash scripts/docker_build_wasm.sh
bash scripts/game_server_restart.sh
```
