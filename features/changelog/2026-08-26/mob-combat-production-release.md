Commit: 39f0688277ec5a1926757da0e6f2c80bc1b781f6

# 怪物战斗双端生产发布

## Context

怪物仇恨与动作授权同时改变 WASM 客户端表现和 linked server 裁决，两端必须使用同一发布基线，不能只替换客户端文件或服务端类。

## Change Summary

- 生产版本切换为 `39f0688-f572fa605`，客户端逻辑基线为 `b2922f3`，服务端逻辑基线为 `a88add4de`。
- 文件服务器公开不可变版本目录、客户端包、完整部署包和逐文件 SHA-256 清单；稳定下载名原子指向该版本。
- Web 与 Cosmic 服务同步重启，WebSocket 代理和素材服务因二进制未变化而保持原进程。

## Impact Surface

生产 Web 入口、WASM 客户端、linked server 运行类、`:48562` 制品下载目录以及对应回滚点。

## Validation

- clean worktree Release 构建和服务端纯策略验证通过。
- 完整部署包内外两层 SHA-256、IPv4/IPv6 下载和 WASM Range 206 通过。
- 生产 WebGL2 流程完成登录、进入地图 `40000` 并实例化 3 只怪物，浏览器异常为 0。
- 发布审计与回滚步骤记录在 `/data00/maplestory-wasm-deploy/rollback-journal-39f0688-f572fa605.md`。

## Notes / Compatibility

- 未变更数据库结构、数据、凭据或环境变量。
- 上一版本 `197feb9-ec839130`、服务端 class 备份和旧公开制品均保留，可独立回滚。

## Related Docs

- [怪物仇恨战斗](../../mob-combat/)
- [版本化部署制品](../2026-08-21/versioned-deployment-artifacts.md)
- [怪物战斗实现](mob-aggro-combat.md)
