// Synchronous read fast-path for LazyFS.
//
// Asyncify has a single rewind buffer, so two concurrently suspended wasm
// chains corrupt each other. readFileData is always async (IndexedDB), so
// every wasm read suspends — including reads whose data already sits in the
// in-memory chunk cache. This module adds a synchronous lookup used by the
// fetch_data trampoline: when every needed byte is already resident, the
// trampoline copies it straight into the wasm heap and never calls
// Asyncify.handleSleep, which removes the suspension entirely.
//
// When the guard rejects a re-entrant read (another chain is parked), the
// rejection schedules a pure-JS warmup that waits for the wasm side to go
// idle and then loads the chunks into the memory cache, so the wasm retry on
// a later frame hits the synchronous fast-path.
(function (global) {
	'use strict';

	function install(LazyFS) {
		if (!LazyFS || LazyFS.readCachedSync) {
			return;
		}

		// Returns a Uint8Array view of [offset, offset+length) of filepath
		// when the whole range is covered by resident memory chunks and the
		// file metadata (size, hence version tag) is already known.
		// Returns null whenever anything would need an async fetch.
		LazyFS.readCachedSync = function (filepath, offset, length) {
			const fileEntry = this.files.get(filepath);
			if (!fileEntry || fileEntry.size === null || length <= 0) {
				return null;
			}
			const start = Math.floor(offset / this.CHUNK_SIZE);
			const end = Math.floor((offset + length - 1) / this.CHUNK_SIZE);
			if (start !== end) {
				return null; // multi-chunk spans keep the async path
			}
			const versionTag = this.getFileVersionTag(fileEntry);
			const chunk = this.chunkCache.get(this.getChunkCacheKey(filepath, versionTag, start));
			if (!chunk) {
				return null;
			}
			const inChunk = offset % this.CHUNK_SIZE;
			if (inChunk + length > chunk.length) {
				return null;
			}
			return chunk.subarray(inChunk, inChunk + length);
		};

		// Fire-and-forget warmup for ranges the wasm side asked about while
		// another chain was parked. Pure JS: no wasm export is touched while
		// Asyncify is busy, and the actual readFileData only starts once the
		// wasm state is idle again.
		LazyFS.warmupAfterPark = function (filepath, offset, length) {
			const attempt = () => {
				if (typeof Asyncify !== 'undefined' && (Asyncify.currData || Asyncify.state)) {
					setTimeout(attempt, 30);
					return;
				}
				this.readFileData(filepath, offset, length, false).catch(() => {});
			};
			setTimeout(attempt, 0);
		};
	}

	const target = global.Module && global.Module.LazyFS ? global.Module.LazyFS : null;
	if (target) {
		install(target);
	} else {
		const queue = global.__lazyfsSyncCacheQueue || (global.__lazyfsSyncCacheQueue = []);
		queue.push(install);
		global.__lazyfsInstallSyncCache = install;
	}
})(typeof window !== 'undefined' ? window : globalThis);
