Commit: 802f41b5f43829f8f6d2af45334d37e3fac95596

# 公开完整的版本化部署制品

## Context

下载服务原先只提供客户端 WASM/JS、服务端 JAR 和 `ws-proxy`。浏览器 IME 修复依赖额外的 `web/index.html` 和 `web/ime_input.js`，NPC 商店批量售卖又同时依赖客户端封包与 linked server 处理。只替换旧的两个客户端文件不能构成可生效的部署集。

## Change Summary

- 以客户端 `802f41b` 和 linked server `2d6e38cf` 组成版本目录 `versions/802f41b-2d6e38cf/`。
- 客户端包同时携带 `JourneyClient.js`、`JourneyClient.wasm` 与完整 `web/` 静态入口，包含 IME 合成输入隔离逻辑。
- 完整部署包额外携带 `web-server`、`ws-proxy`、`assets-server`、全量编译的 `MapleStory-Server.jar` 和 Java `cores/` 依赖。
- 公开根目录为每个可单独拉取的文件保留稳定名称，并增加客户端包、完整部署包、`VERSION` 和双层 SHA-256 清单。
- 发布前的四个公开制品保留在 `versions/legacy-before-802f41b-2d6e38cf/` 以便回滚。

## Impact Surface

- `:48562` 的 IPv4/IPv6 下载目录、稳定文件名和版本化子目录。
- 通过文件服务拉取客户端、Web 静态入口、Rust 服务和 Java 服务端的部署流程。
- 不切换正在运行的 `/data00/maplestory-wasm-deploy/current`，也不修改数据库。

## Validation

- Emscripten 4.0.21 Release clean build 成功；客户端 NPC 售卖策略验证通过。
- linked server 使用 Java 11 全树编译，NPC 售卖、组队奖励与会话保护验证通过；JAR 包含 `NPCShopHandler`、`MapleShop`、`NpcShopSalePolicy`、`PartyRewardPolicy` 和 `SessionSaveFence`。
- `ws-proxy` release 测试 10/10 通过。
- 从 HTTP 公开地址逐件下载后，稳定文件清单和解压包内清单均全部校验通过；Range 请求返回精确 16 字节。

## Notes / Compatibility

- 下载目录不提供源码、凭据或数据库配置。
- Java 服务运行仍需部署环境已有的 `wz/`、`scripts/` 和数据库配置；完整部署包不复制这些数据。
- 从已下载包启动的 NPC 商店 UI 专项未能走进武器店，因此未将该轮记为 UI 功能通过；纯策略、JAR 内容和之前完成的真实 UI 验收仍是本版本的功能证据。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
- [NPC 商店批量售卖](npc-shop-bulk-sale.md)
- [IME preedit 隔离](ime-preedit-isolation.md)
