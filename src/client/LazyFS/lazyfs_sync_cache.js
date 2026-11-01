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
// The fast path is only consulted while Asyncify is idle or unwinding. During
// a rewind (state 2) LazyFileBackend bypasses it, because the resumed read has
// to replay through handleSleep to stop at its recorded frame.
(function (global) {
	'use strict';

	function install(LazyFS) {
		if (!LazyFS || LazyFS.readCachedSync) {
			return;
		}

		// Returns a Uint8Array view (or stitched copy for multi-chunk
		// ranges) of [offset, offset+length) of filepath when the whole
		// range is covered by resident memory chunks and the file metadata
		// (size, hence version tag) is already known.
		// Returns null whenever anything would need an async fetch.
		//
		// Multi-chunk stitching matters for Asyncify rewind replays: a read
		// re-executed during a replay must return synchronously or the replay
		// restarts forever (the documented asyncify busy-wedge). Large node
		// tables are read as spans across chunk boundaries, so spans covered
		// by resident chunks must also hit this fast path.
		LazyFS.readCachedSync = function (filepath, offset, length) {
			const fileEntry = this.files.get(filepath);
			if (!fileEntry || fileEntry.size === null || length <= 0) {
				return null;
			}
			const start = Math.floor(offset / this.CHUNK_SIZE);
			const end = Math.floor((offset + length - 1) / this.CHUNK_SIZE);
			const versionTag = this.getFileVersionTag(fileEntry);
			// Collect every covered chunk first; any miss keeps the async path.
			const chunks = [];
			for (let i = start; i <= end; i++) {
				const chunk = this.chunkCache.get(this.getChunkCacheKey(filepath, versionTag, i));
				if (!chunk || !chunk.length) {
					return null;
				}
				chunks.push(chunk);
			}
			if (start === end) {
				const inChunk = offset % this.CHUNK_SIZE;
				const chunk = chunks[0];
				if (inChunk + length > chunk.length) {
					return null;
				}
				return chunk.subarray(inChunk, inChunk + length);
			}
			// Stitch [offset, offset+length) across chunk boundaries. An
			// interior chunk shorter than CHUNK_SIZE means a truncated fetch;
			// reject the whole range so the async path stays authoritative.
			for (let i = 0; i < chunks.length - 1; i++) {
				if (chunks[i].length !== this.CHUNK_SIZE) {
					return null;
				}
			}
			const result = new Uint8Array(length);
			let pos = 0;
			for (let i = start; i <= end; i++) {
				const chunk = chunks[i - start];
				const from = (i === start) ? offset % this.CHUNK_SIZE : 0;
				const take = Math.min(chunk.length - from, length - pos);
				if (take <= 0) {
					return null; // range not covered by resident data
				}
				result.set(chunk.subarray(from, from + take), pos);
				pos += take;
			}
			return pos === length ? result : null;
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
