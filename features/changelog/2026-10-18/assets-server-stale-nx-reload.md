# assets-server: 按需重载磁盘上被改写的 NX 文件

## 问题

`AssetCache` 只在进程启动时 `fs::metadata` 索引各 NX 文件的大小与 mtime。任何启动后的就地改写(如 `scripts/nx-l10n` 对 `String.nx` 的 append + header patch)都不会被发现:客户端按 `get_size` 返回的旧长度请求分块,拿到「新 header + 旧长度」的截断流,NX 字符串表越界,进图链路抛 `std::length_error "vector"`,只能靠重启容器恢复。

## 修复

`assets-server/src/cache.rs`:

- `CachedAsset` 持有源文件 `path`,新增 `build`(从磁盘重建条目)与 `is_stale`(stat 比对 size/mtime)。
- `AssetCache.entries` 改为 `RwLock<HashMap<String, Arc<CachedAsset>>>`;`get()` 快路径只读锁 + 一次 stat,发现过期即持写锁重建该条目(双重检查避免并发重复加载),stat 失败时继续服务旧副本。
- `total_bytes()`/`resident_bytes()` 改为实时聚合,重载后保持准确。
- 回归测试 `reloads_entry_after_in_place_rewrite`:加载后改写文件,`get` 立即反映新 size/内容(Memory 与 Disk 两种模式)。

## 验证

- `cargo test` 8/8 通过,clippy 无新增告警。
- 运行时 WS 探测:`get_size String.nx` 返回 9059572(与磁盘一致);`touch` 后 version 变化,无需重启。
- 完整浏览器 e2e(`E2E_CASH_SHOP=1`)再次全绿,0 异常。
