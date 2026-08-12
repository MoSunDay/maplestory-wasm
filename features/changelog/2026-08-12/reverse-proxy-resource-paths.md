Commit: 5059b422f9f2a19252e7536d09e173a4b69dfd7b

# 反向代理前缀下的 Web 资源寻址

## Context

WASM 入口页部署在带路径前缀的 iframe 反向代理下时，域名根绝对路径会绕过应用前缀，导致配置、字体和客户端脚本返回 404。

## Change Summary

- `web/index.html` 的 `config.json`、字体和 `JourneyClient.js` 改为相对页面位置寻址。
- `JourneyClient.wasm` 继续由 Emscripten 相对实际脚本目录加载，无需修改构建产物。
- 增加路径解析回归测试，覆盖带代理前缀的页面 URL。

## Impact Surface

- 本地 `/web/index.html` 资源位置不变。
- 反向代理部署会保留应用路径前缀，不再错误请求代理域名根目录。

## Related Docs

- [Web 基础设施](../../../agents/web/index.md)
