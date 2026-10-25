# 斗气球不闪烁：整环呼吸式透明度 + 随高度连续的环绕明暗

## 背景

用户报告：英雄系（Crusader/Hero，斗气 1111002 与进阶斗气 1120003）叠珠后
环绕的斗气球一颗颗硬闪——约每 0.3 秒从亮猛跳回暗一次；另外每颗珠绕过身前
/身后切换时亮度也会"啪"地跳一档。

根因（Skill.nx 已核对）：

- `111.img/skill/1111002/state` 与 `1120003/state` 的珠帧（"1".."5"）全部带
  `a0=64, a1=255, delay=300`。`sample_orb()` 在帧内对 a0→a1 线性插值，而下一
  帧又从 a0 重新起步：透明度每 300ms 走一遍 25%→100%，随后瞬间跳回 25%，
  逐帧回绕成 0.75 的锯齿突变，肉眼即硬闪。
- `draw_orbs()` 里 `shade = near_side ? 1.0f : 0.65f` 按 `sin(angle)` 的符号
  硬切远近两档，珠越过轨道水平直径的瞬间亮度跳变 0.35，形成第二处闪跳。

## 变更

- 新增 `src/client/Character/Effects/ComboOrbPulse.h`（header-only 纯函数，
  namespace `jrc::combo_pulse`，风格同 `DeathTombOrbit.h`）：
  - `breathe(t, duration)`：整环余弦呼吸包络 `0.5 - 0.5*cos(TAU*phase)`，
    值域 [0,1]，t=0 与 t=duration 处均为 0 且取模回绕后逐位相等（整环连续，
    无帧边界跳变）；duration<=0 返回 0。
  - `depth_shade(height)`：`FAR_SHADE + (1-FAR_SHADE)*(0.5+0.5*height)`，
    远端 0.65、近端 1.0、轴心 0.825，随轨道高度连续过渡。
- `src/client/Character/Effects/ComboOrbs.cpp`：
  - `ComboVisuals` 新增 `glow_min`/`glow_max`（各帧 a0 最小值 / a1 最大值，
    默认 255，缺 props 的数据保持恒定不闪），`load_visuals()` 收帧后一次算出。
  - `sample_orb()` 改为收 `ComboVisuals`：保留按 delay 的选帧（图案仍在轮换），
    透明度弃用帧内 a0→a1 锯齿插值，改为
    `glow_min + (glow_max - glow_min) * breathe(t, duration)` 整环呼吸；删除
    死掉的帧内 progress 计算，[0,255] 钳制不变。
  - `draw_orbs()` 先远后近两遍的绘制顺序与 `near_side` 分遍判定保持不变，
    `shade` 改为 `combo_pulse::depth_shade(std::sin(angle))`，过轴不再跳档。
- 轨道相位、半径 34、逐珠错帧 `local_time`、光环（aura）绘制均未改动。

## 验证

- 新增回归 `tools/verify/combo-orb-pulse.cpp`：breathe 端点/峰值、整环与取模
  回绕逐位相等、非法时长返回 0、25ms 步进单调升降、值域 [0,1]、渲染透明度
  （64..255）相邻 25ms 样本最大步长 < 0.05（旧锯齿为 0.75 瞬跳）、depth_shade
  端点/轴心值与过轴连续，退出码 0：

  `c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client tools/verify/combo-orb-pulse.cpp -o /tmp/combo-orb-pulse && /tmp/combo-orb-pulse`

- `c++ -std=c++17 -fsyntax-only -Isrc/client -Isrc -Isrc/nlnx src/client/Character/Effects/ComboOrbs.cpp`
  通过：无错误，仅 Point/DrawArgument 既有 narrowing 告警且与改动前逐条一致
  （本机无 emcc/em++，用系统 g++ 11.4 做语法检查）。
