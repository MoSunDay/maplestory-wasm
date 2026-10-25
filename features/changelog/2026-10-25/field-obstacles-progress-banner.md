# 场景机关/进度横幅/BGM 切换封包接入

组队任务的机关卡关（OrbisPQ 陷阱、CWKPQ 石阵等）此前无解：客户端只
处理 `FIELD_EFFECT` 的个别模式，服务端 MaplePacketCreator 下发的机关
与进度封包在 `PacketSwitch` 里还是 NullHandler。本轮按服务端 opcode
对齐接入三类机关封包，并补上 PQ 进度横幅与 BGM 运行时切换：

| opcode | 封包 | 负载 | 客户端行为 |
|---|---|---|---|
| 139 | FIELD_OBSTACLE_ONOFF | string name + int state | `Stage::set_named_object_active` |
| 140 | FIELD_OBSTACLE_ONOFF_LIST | int count × (string, int) | 逐条下推同上 |
| 141 | FIELD_OBSTACLE_ALL_RESET | 无负载 | 新增 `Stage::reset_named_objects` |
| 122 | SCRIPT_PROGRESS_MESSAGE | string | 弹出 `UIProgressBanner` 进度横幅 |
| 138 | FIELD_EFFECT mode 6 | string bgm 路径 | 规范化后切换 BGM |

## 机关开/关/重置链路

`Net/Handlers/FieldObstacleHandlers.{h,cpp}`（57/84 行）按上表解析负载：

- ONOFF / ONOFF_LIST 走共用 `apply_obstacle_state`，转调
  `Stage::set_named_object_active(name, state == 1)`；仅当具名对象
  不存在时打印一行日志，周期性开关不会刷屏。
- ALL_RESET 调用新增的 `Stage::reset_named_objects()`，沿
  `Stage → MapTilesObjs → TilesObjs → Obj` 层层下推，把全部具名
  对象重置回地图默认的全激活状态。此前机关一旦被关闭就没有任何
  封包能把它恢复，机关图（门/陷阱/石阵）会永久卡关。

## 进度横幅 UIProgressBanner

`IO/UITypes/UIProgressBanner.{h,cpp}`（57/82 行）是新的非 TOGGLED
`UIElement`（`Type::PROGRESSBANNER`）：顶部一条全宽 24px 半透明黑条 +
居中黄字，显示 1 秒后进入 5 秒 Linear 淡出，淡出结束自 `deactivate`；
非 TOGGLED 意味着重复收到 122 时直接重建替换旧横幅。ESC 也可立即关闭。

## BGM 切换与路径规范化

`FIELD_EFFECT` mode 6（`SetfieldHandlers.cpp` 新分支）携带 BGM 路径
并运行时切换背景音乐。服务端脚本里路径有两种写法（`Bgm03/FormatX`
或 `Bgm03.img/FormatX`），为此新增纯函数辅助单元
`src/client/Audio/FieldMusicPath.{h,cpp}`：

| 输入 | 输出 |
|---|---|
| `Bgm03/FormatX` | `Bgm03.img/FormatX`（首个 `/` 前补 `.img`） |
| `Bgm03.img/FormatX` | 原样返回（已含 `.img`） |
| `Bgm03` | `Bgm03.img`（裸目录名） |
| 空串 | 空串（mode 6 提前返回并告警） |

## 文件清单

| 文件 | 行数 | 说明 |
|---|---|---|
| `Net/Handlers/FieldObstacleHandlers.{h,cpp}` | 57/84 | 机关三封包 + 122 进度横幅 handler |
| `IO/UITypes/UIProgressBanner.{h,cpp}` | 57/82 | 顶部进度横幅元素 |
| `Audio/FieldMusicPath.{h,cpp}` | 28/42 | `normalize_bgm_path` 纯函数 |
| `Net/PacketSwitch.cpp` | — | 4 个 NullHandler 换成真 handler |
| `Net/Handlers/SetfieldHandlers.cpp` | — | FIELD_EFFECT mode 6 分支 |
| `Gameplay/Stage.{h,cpp}`、`MapTilesObjs.{h,cpp}`、`Graphics/Obj.{h,cpp}` | — | `reset_named_objects` 下推链路 |

探针：`_msui_progressbanner_active`（ImeBridge.cpp + CMake 导出）；
机关开/关可复用已有的 `_msfield_object_active(name)`。

## 验证

BGM 路径规范化单测（命令见 `tools/verify/index.md`）：

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client \
  tools/verify/field-music-path.cpp \
  src/client/Audio/FieldMusicPath.cpp -o /tmp/field-music-path && \
  /tmp/field-music-path
```

```
ALL CHECKS PASSED
```

WASM 重编通过（`scripts/docker_build_wasm.sh`）。机关与横幅的运行时
验收可复用现有探针：`_msfield_object_active(name)` 断言机关对象
开/关，`_msui_progressbanner_active` 断言横幅出现与淡出。

## 相关文档

- [网络层](../../../agents/client/Net/index.md)
- [UI 系统](../../../agents/client/IO/index.md)
- [验证工具索引](../../../tools/verify/index.md)
