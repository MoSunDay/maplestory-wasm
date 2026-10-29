# TODO-001：WEAKEN 等减益映射到 ActiveBuffs

- 状态：待启动
- 优先级：P2
- 需求目标：服务端下发的 WEAKEN/SEAL/DARKNESS/CURSE 等减益在客户端形成可见、可查询的 ActiveBuffs 条目。
- 核心实现：`src/client/Character/Buff.cpp` 的 statmask→Buff 映射表补充缺失减益项（WEAKEN 掩码已在 `Buff.cpp:103`，但无对应 ActiveBuff 生成路径），使 `PlayerHandlers.cpp:254`、`StageInput.cpp:161` 的 `has_buff(Buffstat::WEAKEN)` 消费端真正生效。
- 关键要素：映射不重复、不覆盖既有增益；buff 过期清理同步；图标与 UI 提示与增益一致。
- 前置依赖：无。
- 当前进展：未动工。
- 验证手段：伪客户端发 GiveBuff(WEAKEN) 断言 `has_buff`；真实客户端 e2e 观察减益图标。
- 下一步：盘点 `Buff.cpp` 掩码表与 `ActiveBuffs` 生成路径的完整差集。
- 备注：源自圣甲术修复评审；不影响 WDEF 数值修复本身。
