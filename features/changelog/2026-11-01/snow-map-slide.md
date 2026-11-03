# 冰封雪域：不穿防滑鞋的雪地打滑物理

上游 Journey 客户端只留下了 `PhysicsObject::ICE // TODO` 空壳，地图与
装备数据其实都带滑动标志，本轮把四个断点接通，实现官方 El Nath 雪地
表现：进入 `fs` 雪地图且未穿防滑鞋时，**行走速度与普通地图完全一致**，
只是松开方向键后会惯性滑行一小段距离，斜坡上仍保留顺势下滑分量；
穿上防滑鞋（Snowshoes）则完全不滑。

## 机制与参数

- 摩擦力按"是否施加水平力"区分（`move_ice` 内 `walking` 分支）：
  - **按住方向键行走时用常规 `FRICTION`(0.3)**，与普通地面完全相同，
    步行末速 1.2 px/帧不变——雪地**不会**越走越快；
  - **松开方向键后**才改用雪面摩擦 `SNOWFRICTION(0.1) × 地图fs`，
    fs=0.2 时为 0.02，从常规步速滑行约 25~30 px（约 1 个身位、
    ~60 帧）后停下，即官方"停键后打滑一小段"的手感。
- `SLOPEFACTOR` 斜坡项保持不变：雪地斜坡上重力分量持续做功，经典
  的"顺着坡停不下来、爬不上坡"手感即来自此项（平地无影响）。
- 滑行期间保持 WALK 状态（走路动画），`|hspeed| ≤ 0.25` 后才切
  STAND；`<0.1` 归零阈值沿用原物理。
- 只作用于本地玩家；Mob、掉落物、其他玩家保持 NORMAL 物理不变。

## 数据链路确认

- 地图侧：雪地图 `info` 下有 `<float name="fs" value="0.2"/>`，例
  `link_repos/MapleStory-Server/wz/Map.wz/Map/Map1/108010701.img.xml`
  （ElNathDungeon 雪谷系列）。
- 装备侧：防滑鞋鞋子 `info` 下有 `<int name="fs" value="10"/>`，如
  1072168~1072171、1072238/1072239、1072338（Shoes.txt 手册可查
  "won't be slipping around regardless of snow or ice"）。
- 服务端 `MapleItemInformationProvider.java:1270` 把装备 `fs > 0`
  映射为 `ItemConstants.SPIKES` 标志，语义一致。

## 改动位置

- `src/client/Gameplay/MapleMap/MapInfo.{h,cpp}`：解析 `info/fs`，
  新增 `is_slippery()` / `get_slip_factor()`。
- `src/client/Gameplay/Physics/Physics.{h,cpp}`：新增带滑动系数的
  构造函数与 `move_ice()`，替换 `ICE // TODO` 空壳。
- `src/client/Data/EquipData.{h,cpp}`：解析鞋子 `fs`，新增
  `is_snowshoe()`。
- `src/client/Character/Player.{h,cpp}`：`respawn` 增加 slippery 参，
  新增 `has_snowshoes()`（查 EQUIPPED 鞋位）/ `on_slick_ground()`。
- `src/client/Character/PlayerStates.cpp`：STAND/WALK/FALL 状态按
  `on_slick_ground()` 选 ICE/NORMAL；WALK 滑行期间不切 STAND。
- `src/client/Gameplay/Stage.cpp` / `StageInput.cpp`：构造 Physics
  时传入 `info/fs`，respawn 传 `mapinfo.is_slippery()`。

## 实测记录

- Docker WASM 全量构建通过（`docker_build_wasm.sh --jobs 16`，
  `[100%] Built target JourneyClient`）。
- 改动翻译单元 g++ 语法检查无新增告警/错误。
- e2e 实测（`tools/verify/maplestory-webui/snow-slide-flow.mjs`，El Nath
  211000000，fmflow1Hero lv60）：physicstype=1(ICE)；按住 → 1.6s 行走
  速度收敛到 1.26 px/帧（旧 bug ≈4.0，修复后=7.5×walkforce）；松键滑行
  21px、期间 stance 保持 34/35(WALK) 约 209ms 后停稳回 21/22(STAND)，
  与数值模拟（≈26px/≈60 帧）吻合。全部断言通过，产物
  `/tmp/snow-slide-flow/`（截图+采样 JSON）。
- 卧倒观察项（问五第 5 项）：prone 只能从静止（|hs|≤0.25px/帧）进入，
  进入后不再滑行 —— 雪面尾速趋零后才可卧倒，无额外滑行风险。
- 遗留人工待办：108010701 斜坡加速目测、穿 1072168 雪鞋后恢复 NORMAL
  物理（0）的对照实测。

## 相关文档

- 物理常量与斜坡因子历史见 `src/client/Gameplay/Physics/Physics.cpp`
  文件头（Journey 上游遗留 TODO 于本次补齐）。
