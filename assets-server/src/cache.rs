//! Immutable in-memory cache for NX assets.

use std::collections::{HashMap, HashSet};
use std::ffi::OsString;
use std::io;
use std::io::{Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock};
use std::time::UNIX_EPOCH;

const ASSET_SUBDIRECTORIES: [&str; 5] = ["", "assets", "serverAssets", "wz", "data"];

#[derive(Debug)]
pub struct CachedAsset {
    path: PathBuf,
    storage: AssetStorage,
    size: u64,
    version: i64,
}

#[derive(Debug)]
enum AssetStorage {
    Memory(Arc<[u8]>),
    Disk,
}

impl CachedAsset {
    pub fn size(&self) -> usize {
        self.size as usize
    }

    pub fn version(&self) -> i64 {
        self.version
    }

    pub async fn read_chunk(&self, index: u64, chunk_size: u64) -> io::Result<Vec<u8>> {
        let Some((start, count)) = chunk_range(self.size, index, chunk_size) else {
            return Ok(Vec::new());
        };

        match &self.storage {
            AssetStorage::Memory(data) => {
                let start = start as usize;
                Ok(data[start..start + count as usize].to_vec())
            }
            AssetStorage::Disk => {
                let path = self.path.clone();
                tokio::task::spawn_blocking(move || read_disk_chunk(&path, start, count))
                    .await
                    .map_err(|err| io::Error::other(format!("NX chunk reader panicked: {err}")))?
            }
        }
    }

    fn build(path: PathBuf, cache_all: bool) -> io::Result<Self> {
        let metadata = std::fs::metadata(&path).map_err(|err| {
            io::Error::new(
                err.kind(),
                format!("cannot inspect NX file '{}': {err}", path.display()),
            )
        })?;
        let version = modified_version(&metadata);
        let size = metadata.len();
        let storage = if cache_all {
            let data: Arc<[u8]> = std::fs::read(&path)
                .map_err(|err| {
                    io::Error::new(
                        err.kind(),
                        format!("cannot cache NX file '{}': {err}", path.display()),
                    )
                })?
                .into();
            AssetStorage::Memory(data)
        } else {
            AssetStorage::Disk
        };
        Ok(Self {
            path,
            storage,
            size,
            version,
        })
    }

    fn is_stale(&self) -> bool {
        match std::fs::metadata(&self.path) {
            Ok(metadata) => metadata.len() != self.size || modified_version(&metadata) != self.version,
            Err(_) => false,
        }
    }
}

#[derive(Debug, Default)]
pub struct AssetCache {
    entries: RwLock<HashMap<String, Arc<CachedAsset>>>,
    cache_all: bool,
}

impl AssetCache {
    pub fn load(root: &Path, cache_all: bool) -> io::Result<Self> {
        let paths = discover_nx_files(root)?;
        if paths.is_empty() {
            return Err(io::Error::new(
                io::ErrorKind::NotFound,
                "no NX files found in the asset search directories",
            ));
        }
        let mut entries = HashMap::with_capacity(paths.len());
        for path in paths {
            let filename = utf8_filename(&path)?;
            let asset = CachedAsset::build(path, cache_all)?;
            entries.insert(filename, Arc::new(asset));
        }

        Ok(Self {
            entries: RwLock::new(entries),
            cache_all,
        })
    }

    /// Returns the asset, transparently reloading entries whose file
    /// changed on disk (size or mtime) after startup. In-place NX
    /// rewrites otherwise served a stale size and truncated chunks.
    pub fn get(&self, requested: &str) -> Option<Arc<CachedAsset>> {
        let filename = Path::new(requested).file_name()?.to_str()?;
        if filename == ".." {
            return None;
        }
        let asset = self.entries.read().unwrap().get(filename)?.clone();
        if !asset.is_stale() {
            return Some(asset);
        }
        self.refresh(filename, asset)
    }

    fn refresh(&self, filename: &str, stale: Arc<CachedAsset>) -> Option<Arc<CachedAsset>> {
        let mut entries = self.entries.write().unwrap();
        if !entries
            .get(filename)
            .is_some_and(|current| Arc::ptr_eq(current, &stale))
        {
            // Another request already refreshed this entry.
            return entries.get(filename).cloned();
        }
        match CachedAsset::build(stale.path.clone(), self.cache_all) {
            Ok(asset) => {
                let asset = Arc::new(asset);
                entries.insert(filename.to_owned(), asset.clone());
                Some(asset)
            }
            Err(_) => Some(stale),
        }
    }

    pub fn file_count(&self) -> usize {
        self.entries.read().unwrap().len()
    }

    pub fn total_bytes(&self) -> u64 {
        self.entries
            .read()
            .unwrap()
            .values()
            .map(|asset| asset.size)
            .sum()
    }

    pub fn resident_bytes(&self) -> u64 {
        self.entries
            .read()
            .unwrap()
            .values()
            .map(|asset| match &asset.storage {
                AssetStorage::Memory(data) => data.len() as u64,
                AssetStorage::Disk => 0,
            })
            .sum()
    }
}

fn chunk_range(file_size: u64, index: u64, chunk_size: u64) -> Option<(u64, u64)> {
    if chunk_size == 0 {
        return None;
    }
    let start = index.checked_mul(chunk_size)?;
    if start >= file_size {
        return None;
    }
    Some((start, (file_size - start).min(chunk_size)))
}

fn read_disk_chunk(path: &Path, start: u64, count: u64) -> io::Result<Vec<u8>> {
    let mut file = std::fs::File::open(path)?;
    file.seek(SeekFrom::Start(start))?;
    let mut buffer = Vec::with_capacity(count as usize);
    file.take(count).read_to_end(&mut buffer)?;
    Ok(buffer)
}

fn discover_nx_files(root: &Path) -> io::Result<Vec<PathBuf>> {
    let mut files = Vec::new();
    let mut seen = HashSet::<OsString>::new();

    for subdirectory in ASSET_SUBDIRECTORIES {
        let directory = if subdirectory.is_empty() {
            root.to_owned()
        } else {
            root.join(subdirectory)
        };
        if !directory.is_dir() {
            continue;
        }

        let mut directory_files = Vec::new();
        for entry in std::fs::read_dir(&directory)? {
            let path = entry?.path();
            if path.is_file() && has_nx_extension(&path) {
                directory_files.push(path);
            }
        }
        directory_files.sort();

        for path in directory_files {
            let Some(filename) = path.file_name().map(ToOwned::to_owned) else {
                continue;
            };
            // Match the original resolution order: the first basename found
            // across root/assets/serverAssets/wz/data is the served file.
            if seen.insert(filename) {
                files.push(path);
            }
        }
    }

    Ok(files)
}

fn has_nx_extension(path: &Path) -> bool {
    path.extension()
        .and_then(|extension| extension.to_str())
        .is_some_and(|extension| extension.eq_ignore_ascii_case("nx"))
}

fn utf8_filename(path: &Path) -> io::Result<String> {
    path.file_name()
        .and_then(|filename| filename.to_str())
        .map(ToOwned::to_owned)
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "NX filename is not UTF-8"))
}

fn modified_version(metadata: &std::fs::Metadata) -> i64 {
    metadata
        .modified()
        .ok()
        .and_then(|mtime| mtime.duration_since(UNIX_EPOCH).ok())
        .map(|elapsed| elapsed.as_nanos() as i64)
        .unwrap_or(-1)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn indexes_all_nx_files_and_respects_resolution_order() {
        let root = tempfile::tempdir().unwrap();
        std::fs::create_dir(root.path().join("assets")).unwrap();
        std::fs::write(root.path().join("UI.nx"), b"root").unwrap();
        std::fs::write(root.path().join("assets/UI.nx"), b"shadowed").unwrap();
        std::fs::write(root.path().join("assets/Map.NX"), b"map").unwrap();
        std::fs::write(root.path().join("ignore.txt"), b"ignored").unwrap();

        let cache = AssetCache::load(root.path(), true).unwrap();
        assert_eq!(cache.file_count(), 2);
        assert_eq!(cache.total_bytes(), 7);
        assert_eq!(cache.resident_bytes(), 7);
        assert!(cache.get("ignore.txt").is_none());
    }

    #[tokio::test]
    async fn reloads_entry_after_in_place_rewrite() {
        let root = tempfile::tempdir().unwrap();
        let file = root.path().join("UI.nx");
        std::fs::write(&file, b"0123456789").unwrap();
        let memory = AssetCache::load(root.path(), true).unwrap();
        let disk = AssetCache::load(root.path(), false).unwrap();

        // Simulate an in-place rewrite that appends data and bumps mtime.
        std::fs::write(&file, b"0123456789-append").unwrap();

        for cache in [&memory, &disk] {
            let asset = cache.get("UI.nx").unwrap();
            assert_eq!(asset.size(), 17);
            assert_eq!(
                asset.read_chunk(0, 17).await.unwrap(),
                b"0123456789-append".to_vec()
            );
        }
        assert_eq!(memory.total_bytes(), 17);
        assert_eq!(memory.resident_bytes(), 17);
    }

    #[tokio::test]
    async fn memory_and_disk_modes_read_identical_safe_chunks() {
        let root = tempfile::tempdir().unwrap();
        std::fs::write(root.path().join("UI.nx"), b"0123456789").unwrap();
        let memory = AssetCache::load(root.path(), true).unwrap();
        let disk = AssetCache::load(root.path(), false).unwrap();
        assert_eq!(memory.resident_bytes(), 10);
        assert_eq!(disk.resident_bytes(), 0);

        for cache in [&memory, &disk] {
            let asset = cache.get("UI.nx").unwrap();
            assert_eq!(asset.read_chunk(1, 4).await.unwrap(), b"4567");
            assert_eq!(asset.read_chunk(2, 4).await.unwrap(), b"89");
            assert!(asset.read_chunk(3, 4).await.unwrap().is_empty());
            assert!(asset.read_chunk(u64::MAX, 4).await.unwrap().is_empty());
            assert!(asset.read_chunk(0, 0).await.unwrap().is_empty());
        }
    }
}
