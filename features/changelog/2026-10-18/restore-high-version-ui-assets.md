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
