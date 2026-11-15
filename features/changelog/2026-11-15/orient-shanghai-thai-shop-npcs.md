# 东方神州/泰国商店 NPC 售卖功能恢复

## 背景

[BGM 与 NPC 兜底](2026-11-14/orient-bgm-and-npc-fallback.md)落地后，上海/
泰国的商店 NPC（上海 9310001 李女士/9310002 张老板/9310003 金女士，泰国
9250007 丹/9250008 帕恩/9250009 诺伊）仍只有 fallback 台词——`shops`/
`shopitems` 无记录，`hasShop()` 为假，无法买卖。装备/防具/药品售卖需要
保留，因此按 079 参考库同类商店模板补齐商店数据。

> **非硬加的依据**：六家 NPC 的商人身份均为官方数据自带，非本项目杜撰——
> 泰国三家 String.wz `Npc.img` 的 `func` 字段官方标注即 武器店(9250007)/
> 药店(9250008)/防具店(9250009)；上海三家 `func` 为空（ChinaMS 数据缺省），
> 但官方中文台词明确身份（见 [2026-11-14 兜底交付](2026-11-14/orient-bgm-and-npc-fallback.md)、
> `tools/npc_chinese_dialogue.py`）：李女士「要是没钱在这儿买武器，就请回吧！」、
> 张老板「我在这儿卖防具已经十年了」、金女士「欢迎光临~请问想买点什么？」。
> 079 参考库本身没有这六家的 shops 记录（连 orient 地图都是该 2023 年包
> 才补的），故物品表取自同年代同职能模板商人，价格原样沿用。

## 变更

### 1. 商店数据（服务端 SQL，非本仓库文件）

`link_repos/MapleStory-Server/sql/custom_orient_shops.sql`（356 行，
幂等：DELETE 六家 shopid/shopitems + `ON DUPLICATE KEY UPDATE`）：

- shopid == npcid（沿用 hub 惯例）；position 104+4k；
- 模板映射（079 CongMS干净数据.sql 新加坡同类商店，物品与价格原样沿用）：
  - 9310001 李女士·武器 ← shop 78（9330048，48 件）
  - 9310002 张老板·防具 ← shop 79（9330047，86 件）
  - 9310003 金女士·杂货 ← shop 95（9270021，30 件）
  - 9250007 丹·武器 ← shop 62（9270019，42 件）
  - 9250008 帕恩·杂货 ← shop 60（9270022，30 件）
  - 9250009 诺伊·防具 ← shop 63（9270020，83 件）
- 共 319 件，全部通过双侧存在性校验后入库；已应用到库并
  `docker restart ms-game-server`（`MapleShopFactory.getShopForNPC` 懒加载
  + 负缓存，插库后必须重启）。

应用命令（凭据不入仓库）：

```bash
mysql -u<user> -p maplestory < link_repos/MapleStory-Server/sql/custom_orient_shops.sql
docker restart ms-game-server   # 随后确认 8484/7575/7675/7775/7875/7975 全监听
```

### 2. 生成与校验工具（本仓库）

- `tools/build_orient_shops_sql.py`：从 079 参考库解析模板商店 → 逐 itemid
  双侧校验 → 生成上述 SQL（`--dry-run` 只报告）。**319/319 全过，0 丢弃**。
  校验规则（踩坑记录，首版因命名规范误读全部误报）：
  - 服务端 `Item.wz/Consume/<02XX>.img.xml` 条目名是 **8 位零填充**
    （`name="02000003"`）；`Character.wz/<Type>/` 文件名同为 8 位零填充；
  - 服务端 `String.wz/Consume.img.xml`/`Eqp.img.xml` 用**裸 id**
    （`name="1302000"`），与 Item/Character 两处不同；
  - 客户端 `Item.nx` 节点名 8 位零填充（`02000003`）；`Character.nx`
    装备节点名是 `<iid:08d>.img`（`01302008.img`，见 `ItemData` 构造：
    `"0"+to_string(itemid)+".img"`）；`String.nx` 用裸 id；
  - `Path.stem` 只去最后一个后缀（`01302008.img.xml` → `01302008.img`）；
    NX 字符串表是 bytes，需 decode。

### 3. E2E 全链路（本仓库）

`tools/verify/maplestory-webui/orient-shop-flow.mjs`：六家逐店登录
（orient1/char 289）→ 走位/跳层 → Space → 断言商店 UI、商品行、EXIT
按钮、Escape 关闭。**6/6 全过（43 PASS/0 FAIL），连跑两轮**。首屏商品
（服务端 `ORDER BY position DESC`）：

| NPC | 分类 | rows[0..4] |
| --- | --- | --- |
| 9310003/9250008 | 杂货 | 2330000 2070000 2061000 2060000 2050003 |
| 9310002 | 防具 | 1002004 1040000 1040085 1041084 1041085 |
| 9310001/9250007 | 武器 | 1302008 1302004 1302009 1312005 1312006 |
| 9250009 | 防具 | 1040000 1040085 1041084 1041085 1050000 |

## 排障记录

- **商店打不开第一因：站位层差**。NPC 对话要求 |dx|<=120 且 |dy|<=80
  （`MapNpcs::talk_to_nearest`）。泰国三家都站在 y=68 栈道/桅顶，玩家走
  的底部是 y=152，dy=-84 刚好越界。上海 9310002/9310003 在街面 y=316 可
  直达，9310001 在遮阳棚 y=233。
- **跳层方案**：跳跃高度实测 ~82px。底部 152 → 栈道 68（84px）可借平台
  边缘直接落；上海街面 316 → fh72(273) → fh152(233) 两段；泰国帕恩
  fh57(91) → 西栈道(68) → fh18 桅顶(9) 三段。起跳前必须先按住方向键
  获得水平速度（原地起跳再按方向的水平位移不够落边）。
- **服务端登出会把 `spawnpoint` 重置回 0**（`MapleCharacter` 存档
  UPDATE）：每轮登录前必须重写（harness `prepCharacter` 已覆盖）。
  上海用 sp3（落到街面），泰国 sp0 落土丘滑坡到底部。
- **冷启动首登竞态**：全新浏览器档案第一次登录时 `assetsIdle` 可能早于
  地图字段初始化（`msmap_id=0`），先等 `msmap_id != 0` 再比对地图。
- **`ADD_NEW_CHAR_ENTRY`/SET_FIELD 已发但商店没开时先看服务端日志**
  `Received Packet: NPC_TALK`：没有该包说明客户端根本没触发（层差/输入
  被吃），有包再查 shops 数据与负缓存。
- 预期行为：六家 NPC 因 `hasShop()` 命中不再显示 0021 的 fallback 台词
  （`NPCTalkHandler` 商店优先于兜底脚本）。

## 文件

- 新增 `tools/build_orient_shops_sql.py`（207 行）
- 新增 `tools/verify/maplestory-webui/orient-shop-flow.mjs`（185 行）
- `tools/verify/maplestory-webui/index-history.md` 登记 orient-shop-flow
- 服务端（外部 checkout，不入本仓库）：`sql/custom_orient_shops.sql`
