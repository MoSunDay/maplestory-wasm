# 汉化批次一：职业名 / 底栏菜单 / 聊天目标 / 系统消息 / 设置对话框

## 背景

首批"改字符串即生效"的客户端文案汉化（FreeType + DroidSansFallback
CJK 回退管线已就绪，零基础设施改动）。范围限于代码内 `Text` 绘制
字面量；NX 烤字图（HP/MP/EXP 条、post-Chaos 按钮贴图、登录错误整句
图）与服务端下发文本不在本批。

## 变更

- `9f402c9` fix(client)：SHOW_FOREIGN_EFFECT 长度守卫 `available()>=1/4`
  → `length()>=1/4`。`available()` 返回 bool，`>=4` 恒 false 触发
  -Werror（855371f 提交时未重编 WASM 的潜伏问题），并修复技能分支
  永不执行的回归。
- `9ee8da4` feat(client)：7 文件 108 行纯字面量替换——
  Job::get_name 64 条职业名（骑士团带 `(2转)~(4转)` 后缀、2000-2112
  →战神）；v83 prechaos 菜单 9 项；聊天目标 6 标签；Messages 12 条
  系统提示；游戏/宠物/按键设置三对话框（含宠物阈值动态文案
  "HP 不高于 N% 时自动恢复"与按键确认框三问）。

## 验收（浏览器 e2e，test1/战士角色 test1Warr）

- 阶段 A 标准 run.mjs 全流程通过（登录→世界→选角→进图→中文聊天）。
- 阶段 B 专项 7 项：6 PASS / 0 FAIL / 1 SKIP，fatal console 0：
  - 底栏职业名 PASS：运行时探针 `msstatusbar_stat_label`="战士"
    （旧英文构建同位为 Warrior）；
  - 按键设置 PASS：`msnotice_question` 直读"恢复默认按键设置？"
    "清除所有按键设置？"（运行时真值，非 OCR）；YesNo 一律选"否"，
    未破坏按键配置；
  - 聊天目标 SKIP：`UIChatBar.cpp:50-61` 目标标签整体在
    `statusbar::prechaos()` 分支，当前 UI.nx 含 StatusBar2.img
    （post-Chaos）则不渲染——串为休眠正确，切 pre-Chaos 资源集即
    生效（wasm 二进制已验 6/6）；
  - Menu/System 弹窗弹出与动作命中 PASS；post-Chaos 行标签为
    StatusBar2.img 贴图，代码中文串仅驱动 pre-Chaos 路径（语义如此，
    非回归）。
- 上线前对最终产物（HEAD=d1dbf54，wasm md5 99765b12，分发≡build）
  重跑全流程：全部 PASS，P95 帧间隔 33.3ms，浏览器异常 0。
  并行窗口的主树启动崩溃（fde7b03b，Base.nx 首块 call_indirect
  null）已由 d1dbf54 lazyfs Asyncify rewind 守卫修复并随本批上线。

## UI 自动化坐标结论（后续 e2e 复用）

- KeyConfig 按钮带在窗口内 **y383-399**（默认 154-211/删除 217-285/
  快捷键 291-…/取消/确定）；`MapleButton(src)` 单参构造锚定窗口原点
  (144,144)，子树在 **UIWindow2.img**（非 KeyConfig.img）。
- 网格扫掠 y448-504 会落键帽/动作网格（CANGRAB/GRABBING 吞点击），
  勿用于按钮探测；hover 差分被键位图标帧动画淹没不可用。
- Notice 弹窗热区 (270,216)-(530,263)，问句可经 `msnotice_question`
  直读，比 OCR 可靠。

## 构建教训

- emscripten 共享缓存卷缺 freetype port 时 emcc 联网拉取被断连 →
  cache.lock 死锁、并发构建全僵；解法为克隆热缓存卷
  （`pure-66a8454_emscripten-cache` → 专用卷）。
- link.txt 导出列表手工维护（CMakeLists 无新探针名），增量构建加
  导出需同步补链表，勿触发 re-configure。

## 已知边界

- Messages 类提示（如"MP 不足"）未在浏览器确定性触发（需特定技能/
  状态），同一 `Text` 渲染管线已被其余 6 项覆盖；
- 原始截图与排查证据在 /tmp/l10n-evidence/（易失），本档为其固化
  摘要；report.json 7 项终表随验收 agent 产物存档于当次 artifacts。
