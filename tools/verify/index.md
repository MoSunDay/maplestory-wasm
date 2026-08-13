# 验证工具索引

- [MapleStory WebUI 全流程](maplestory-webui/index.md)
- LazyFS 连接恢复：`node tools/verify/lazyfs-connection.mjs`
- 自然恢复规则：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/natural-recovery.cpp src/client/Character/Recovery/NaturalRecovery.cpp -o /tmp/natural-recovery && /tmp/natural-recovery`
- 死亡幽灵轨迹：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/death-tomb-orbit.cpp -o /tmp/death-tomb-orbit && /tmp/death-tomb-orbit`
- 死亡墓碑落地点：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/death-tomb-ground.cpp -o /tmp/death-tomb-ground && /tmp/death-tomb-ground`
- 攻击输入与特效选择：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/attack-effect-selection.cpp -o /tmp/attack-effect-selection && /tmp/attack-effect-selection`
- 武器残影档位与播放状态：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/afterimage-playback.cpp -o /tmp/afterimage-playback && /tmp/afterimage-playback`
- 远端 Buff 掩码顺序：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/foreign-buff-mask.cpp -o /tmp/foreign-buff-mask && /tmp/foreign-buff-mask`
- 世界选择单频道策略与确认按钮资源：`c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/world-select.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/world-select && /tmp/world-select UI.nx`
- 世界选择浏览器点击兼容：`node tools/verify/world-select-input.cjs`
- Cosmic 角色列表（UTF-8 定长名、等级字节、Evan SP 表）：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/login-parser.cpp src/client/Net/InPacket.cpp src/client/Net/Handlers/Helpers/LoginParser.cpp -o /tmp/login-parser && /tmp/login-parser`
- Cosmic 完整角色数据（Java 定长 UTF-8、戒指、新年贺卡、小游戏门禁）：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/character-data-parser.cpp src/client/Net/InPacket.cpp src/client/Net/Handlers/Helpers/CharacterDataParser.cpp -o /tmp/character-data-parser && /tmp/character-data-parser`
- 角色创建名称策略、最终确认与失败恢复状态：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/character-creation-flow.cpp -o /tmp/character-creation-flow && /tmp/character-creation-flow`
- 攻击 NX 结构审计：`c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/attack-effect-nx.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/attack-effect-nx && /tmp/attack-effect-nx Skill.nx`
- 近战武器残影 NX 审计：`c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/weapon-afterimage-nx.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/weapon-afterimage-nx && /tmp/weapon-afterimage-nx Character.nx`
