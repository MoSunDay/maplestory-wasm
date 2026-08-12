Commit: 3ac87b184f131b8c3c5e496384cbb6fa827435d8

# 紧凑技能窗口与物品素材预载

## Context

技能窗口使用了比现有四行内容更宽的新版背景，物品图片首次读取 NX 数据块时还会同步等待网络，造成明显停顿。

## Change Summary

- 技能窗口改用与现有布局匹配的 175×289 NX 面板。
- 进入游戏后后台持久预载物品相关 NX 数据，实际装备图片采用异步范围预取。
- 位图上传只读取和解压一次源数据。

## Impact Surface

- 技能书窗口尺寸与命中边界。
- LazyFS 内存缓存、IndexedDB 持久缓存和物品纹理上传流程。

## Notes / Compatibility

- 不修改 NX 素材或服务端协议。
- 缓存键继续包含资源版本；应用不设置过期时间，但浏览器仍保留最终存储管理权。

## Related Docs

- [UI 系统](../../../agents/client/IO/index.md)
- [按需文件系统](../../../agents/client/LazyFS/index.md)
