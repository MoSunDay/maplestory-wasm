Commit: cc039ba9798debb450c69b1dd6990f2d353f1178

# 武器刺击与挥砍残影完整播放

## Context

普通近战的刺击残影通常在姿态第 1 帧触发，挥砍残影通常在第 2 帧触发。WASM 冷缓存下，Character NX 位图可能在攻击动作结束后才到达；旧逻辑仍按姿态时间推进一次性动画，并要求当前姿态继续处于触发帧，因此挥砍光效会被永久丢弃。等级超过残影素材最高档的武器还会直接选到不存在的档位。

## Change Summary

- 残影先锁存触发状态，位图未驻留时暂停动画；素材到达后即使角色已恢复站立也会补播。
- 短时可见位图通过独立优先队列准备，并在普通预取已排队时提升同一任务，维持每帧最多一次解压上传。
- 标准武器残影按精确姿态选择不高于请求等级的最近可用档位；Skill NX 无有效位图时明确记录问题并回退标准残影。
- 支持同一姿态中的多个数字触发节点，且不把 `charge` 等非数字节点误作动画。
- 新增纯状态验证、589 把近战武器的 Character NX 审计，以及浏览器普通攻击逐帧采样。

## Impact Surface

- 角色普通攻击和近战技能的 Character/Skill NX 残影选择与播放。
- WASM 位图前台请求、图集准备优先级及动画就绪判断。
- WebUI 攻击特效验收。

## Validation

- NX 审计通过：589 把武器、2534 个精确攻击姿态无缺失，160 个高等级姿态正确回退到已有素材档。
- Docker WASM Release 构建、Rust workspace 测试、Clippy `-D warnings` 与三项 Rust Release 构建通过。
- 浏览器完整流程和 12 次冷/热攻击采样通过；无浏览器异常、`Packet Error`、`Stack underflow` 或残影结构错误。

## Notes / Compatibility

仅修改客户端；不修改 Cosmic、数据库、环境变量或 `assets/`。普通纹理仍使用原有后台队列，优先级只影响显式标记的短时可见效果。

## Related Docs

- [角色系统](../../../agents/client/Character/index.md)
- [图形渲染](../../../agents/client/Graphics/index.md)
