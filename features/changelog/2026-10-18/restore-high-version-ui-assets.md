# UI 素材回归高版本（README 配方恢复）

## Context

README 规定客户端资产配方为：v83 全套 WZ，**唯独 UI.wz 取自 v153+**。
此前全量转换时误将 v83 `UI.wz` 一并转出并覆盖 `assets/UI.nx`，
随后又为 pre-Chaos 节点集做了整套运行时适配。本次按用户要求回归配方：
UI 整体使用高版本素材，仅职业技能与自由市场及其传送地图保持 v83。

## Change Summary

- `assets/UI.nx` 换回高版本（1.15GB，post-Chaos，源自 node02
  `/data/msclient/MapleStory-Client/UI.nx`）。客户端运行时探测
  `Login.img/WorldSelect/BtChannel`（UiAssetVersion.h）自动切回
  post-Chaos 分支，无需改代码。
- 职业技能保持 v83：`Skill.nx`（职业图标/技能数据，如 `100.img`）
  与技能窗口节点 `UIWindow.img/Skill`（175×289 紧凑面板，高版本
  UI.nx 内仍保留）均确认存在，`UISkillBook` 读取路径不变。
- 自由市场及传送地图保持 v83：`Map.nx` 内 `910000000/910000001`
  等 FM 地图齐全，FM 传送沿用 v83 NPC 脚本。
- 防再犯：`wz/UI.wz` 改名 `UI.wz.v83-excluded`，全量转换
  （`*.wz` glob）不会再把 v83 UI 覆盖回高版本 UI.nx；
  v83 版 UI.nx 备份于 `/tmp/nx-backup/UI.nx.v83-final`。

## Verification

- `nxdump2` 节点级验证：post-Chaos 标记、`StatusBar2.img/mainBar`、
  `UIWindow2.img`、`UIWindow.img/Skill{,/Tab/enabled/0..4}`、
  `Map/Map9/91000000{0,1}.img`、`Skill.nx/100.img`。
- e2e 全绿（E2E_CREATE_CHARACTER=0）：login→world→char-select
  （3/3 预览含中文名）→双击进图→地图门→p95 16.7ms→拼音中文聊天，
  0 浏览器异常。

## Follow-up（同日第二笔）

- 四件套游戏内窗口双世代恢复（bea8a45）：人物属性（UIWindow4/Stat）、
  背包（UIWindow2/Item）、快捷键设置（UIWindow2/KeyConfig + 复活
  KeyConfigLayout）、装备（UIWindow2/Equip）；v83 路径原样保留。
  游戏内探针验证：四窗口+技能书区域差分 32~76%，0 报错。
- 服务器默认经验 3 倍：ServerConstants.EXP_RATE 5→3（dist 常量池
  Integer 3 实证），world.ini 无覆盖即默认生效；@rates 中央对话框
  渲染验证。
