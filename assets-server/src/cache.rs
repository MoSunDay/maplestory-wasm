//! Immutable in-memory cache for NX assets.

use std::collections::{HashMap, HashSet};
use std::ffi::OsString;
use std::io;
use std::io::{Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::UNIX_EPOCH;

const ASSET_SUBDIRECTORIES: [&str; 5] = ["", "assets", "serverAssets", "wz", "data"];

#[derive(Debug)]
pub struct CachedAsset {
    storage: AssetStorage,
    size: u64,
    version: i64,
}

#[derive(Debug)]
enum AssetStorage {
    Memory(Arc<[u8]>),
    Disk(PathBuf),
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
            AssetStorage::Disk(path) => {
                let path = path.clone();
                tokio::task::spawn_blocking(move || read_disk_chunk(&path, start, count))
                    .await
                    .map_err(|err| io::Error::other(format!("NX chunk reader panicked: {err}")))?
            }
        }
    }
}

#[derive(Debug, Default)]
pub struct AssetCache {
    entries: HashMap<String, CachedAsset>,
    total_bytes: u64,
    resident_bytes: u64,
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
        let mut total_bytes = 0u64;
        let mut resident_bytes = 0u64;

        for path in paths {
            let filename = utf8_filename(&path)?;
            let metadata = std::fs::metadata(&path).map_err(|err| {
                io::Error::new(
                    err.kind(),
                    format!("cannot inspect NX file '{}': {err}", path.display()),
                )
            })?;
            let version = modified_version(&metadata);
            let size = metadata.len();
            total_bytes = total_bytes
                .checked_add(size)
                .ok_or_else(|| io::Error::other("NX cache size overflow"))?;
            let storage = if cache_all {
                let data: Arc<[u8]> = std::fs::read(&path)
                    .map_err(|err| {
                        io::Error::new(
                            err.kind(),
                            format!("cannot cache NX file '{}': {err}", path.display()),
                        )
                    })?
                    .into();
                resident_bytes = resident_bytes
                    .checked_add(data.len() as u64)
                    .ok_or_else(|| io::Error::other("resident NX cache size overflow"))?;
                AssetStorage::Memory(data)
            } else {
                AssetStorage::Disk(path)
            };
            entries.insert(
                filename,
                CachedAsset {
                    storage,
                    size,
                    version,
                },
            );
        }

        Ok(Self {
            entries,
            total_bytes,
            resident_bytes,
        })
    }

    pub fn get(&self, requested: &str) -> Option<&CachedAsset> {
        let filename = Path::new(requested).file_name()?.to_str()?;
        if filename == ".." {
            return None;
        }
        self.entries.get(filename)
    }

    pub fn file_count(&self) -> usize {
        self.entries.len()
    }

    pub fn total_bytes(&self) -> u64 {
        self.total_bytes
    }

    pub fn resident_bytes(&self) -> u64 {
        self.resident_bytes
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
