# 自由市场入口 BGM 不刷新（/gohome）

## 背景

玩家在野外地图执行 `/gohome` 回到自由市场入口（910000000）后，背景音乐
没有切换，上一张地图的 BGM 一直播放。相邻的 910000001~910000022 等图
切图换曲均正常，仅 `/gohome` 的落点图异常。

## 根因

`910000000.img` 是注入的 079 版自由市场地图，其属性（bgm、VRLeft、town、
swim、mapMark 等）**平铺在 img 根节点下**，没有 v83 标准的 `info` 子节点
（提取源 `link_repos/.ref-079max4/extracted/map079/910000000.json` 即为
平铺结构，与 20ac30d 修复的 img079_parser 扁平化问题同源）。

客户端 `MapInfo` 构造时固定读 `src["info"]["bgm"]`，拿到空节点后拼出
`".img/"`；`Music::play()` 对该路径 `resolve()` 失败提前返回——发生在
停掉旧曲之前，于是旧图 BGM 永久续播，表现为"背景音乐没刷新"。

## 变更

- `src/client/Gameplay/MapleMap/MapInfo.cpp`：`src["info"]` 不存在时回退
  用地图节点本身读取属性，兼容平铺结构的注入地图。
- 附带恢复该图此前同样失效的字段：VR 镜头边界（-1320~2437 / -2073~225，
  数值已核对）、town/swim/mapMark/fieldLimit/recovery 等。

客户端读取路径其余不动：`back`/`portal`/`foothold`/`ladderRope` 等本就
平铺，两套结构通用。

## 验证

- 数据核对（python + tools/nx 解析器）：`assets/Map.nx` 中 910000000 无
  `info` 子节点且 `bgm=Bgm00/FloralLife` 平铺于根；910000001~910000022、
  000000000、100000000、211040000 均为标准 `info` 嵌套结构。
- `assets/Sound.nx` 存在 `Bgm00.img/FloralLife`，修复后路径可解析播放。
- 宿主机 `g++ -fsyntax-only` 通过；`scripts/docker_build_wasm.sh` 完整
  WASM 构建成功。
- 游戏内手测：从野外图 `/gohome` 应切到 FloralLife；与 910000001
  （同曲）之间往返曲目连续属预期。
