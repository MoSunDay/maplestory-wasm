# 验证工具索引

- [MapleStory WebUI 全流程](maplestory-webui/index.md)
- LazyFS 连接恢复：`node tools/verify/lazyfs-connection.mjs`
- 自然恢复规则：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/natural-recovery.cpp src/client/Character/Recovery/NaturalRecovery.cpp -o /tmp/natural-recovery && /tmp/natural-recovery`
- 死亡幽灵轨迹：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/death-tomb-orbit.cpp -o /tmp/death-tomb-orbit && /tmp/death-tomb-orbit`
- 死亡墓碑落地点：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/death-tomb-ground.cpp -o /tmp/death-tomb-ground && /tmp/death-tomb-ground`
- 攻击输入与特效选择：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/attack-effect-selection.cpp -o /tmp/attack-effect-selection && /tmp/attack-effect-selection`
- 远端 Buff 掩码顺序：`c++ -std=c++17 -Isrc/client -Isrc tools/verify/foreign-buff-mask.cpp -o /tmp/foreign-buff-mask && /tmp/foreign-buff-mask`
- 攻击 NX 结构审计：`c++ -std=c++17 -Isrc/client -Isrc -Isrc/nlnx tools/verify/attack-effect-nx.cpp src/nlnx/file.cpp src/nlnx/node.cpp src/nlnx/audio.cpp -o /tmp/attack-effect-nx && /tmp/attack-effect-nx Skill.nx`
