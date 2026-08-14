/**
 * LazyFS - True Lazy Filesystem for WebAssembly
 * WebSocket-based asset streaming with IndexedDB persistent caching
 * 
 * This file is loaded via --pre-js before Module initialization
 * 
 * Features:
 * - IndexedDB persistent cache (survives browser restarts!)
 * - WebSocket connection for streaming file chunks
 * - Batched chunk requests for efficient pipelining
 * - In-memory chunk caching for current session
 * - First load: Download from server, cache locally
 * - Future loads: Instant from local cache
 */

// Define LazyFS before Module is initialized
var LazyFS = {
	...LazyFSConnection,
	// Configuration
	CHUNK_SIZE: 0, // Set dynamically from C++
	ASSETS_WS_URL: null, // Set from C++ or auto-detect
	DB_NAME: 'LazyFS_Cache',
	DB_VERSION: 1,
	STORE_NAME: 'chunks',

	// IndexedDB connection
	db: null,
	dbReady: false,
	dbInitPromise: null,
	inFlightBatches: new Map(),
	inFlightPrefetches: new Map(),
	foregroundRequests: new Map(),
	prefetchQueue: [],
	activePrefetches: 0,
	MAX_CONCURRENT_PREFETCHES: 4,
	pendingCacheWrites: new Set(),
	cacheWriteStats: {
		attempted: 0,
		succeeded: 0,
		failed: 0,
		lastError: null,
	},
	// In-memory chunk cache: Map<"file:size:chunk", Uint8Array>
	chunkCache: new Map(),
	// File registry: Map<filepath, { size }>
	files: new Map(),
	itemAssetPreloadPromise: null,
	itemAssetPreloadState: 'idle',
	// ========== FETCH STATISTICS ==========
	stats: {
		totalRequests: 0,
		totalBytes: 0,
		cacheHits: 0,
		cacheMisses: 0,
		startTime: null,
	},

	logFetch: function (filepath, chunkIndex, bytes, fromCache) {
		if (!this.stats.startTime) {
			this.stats.startTime = performance.now();
		}

		if (fromCache) {
			this.stats.cacheHits++;
		} else {
			this.stats.cacheMisses++;
			this.stats.totalRequests++;
			this.stats.totalBytes += bytes;
		}

		const elapsed = ((performance.now() - this.stats.startTime) / 1000).toFixed(2);
		const source = fromCache ? '💾 CACHE' : '🌐 NET';
		console.debug(`[LazyFS] [${elapsed}s] ${source}: ${filepath} chunk ${chunkIndex} (${(bytes / 1024 / 1024).toFixed(2)} MB)`);
	},

	printStats: function () {
		console.log('========== LazyFS Statistics ==========');
		console.log('Network requests:', this.stats.totalRequests);
		console.log('Network data:', (this.stats.totalBytes / 1024 / 1024).toFixed(2), 'MB');
		console.log('Cache hits:', this.stats.cacheHits);
		console.log('Cache misses:', this.stats.cacheMisses);
		console.log('Cache hit rate:', ((this.stats.cacheHits / (this.stats.cacheHits + this.stats.cacheMisses)) * 100).toFixed(1), '%');
		if (this.stats.startTime) {
			console.log('Time since first fetch:', ((performance.now() - this.stats.startTime) / 1000).toFixed(2), 's');
		}
		console.log('========================================');
	},

	// ========== INDEXEDDB CACHE ==========

	/**
	 * Build a cache key scoped by file version so swapped files with the same
	 * filename don't reuse stale chunks from older versions.
	 */
	getChunkCacheKey: function (filename, versionTag, chunkIndex) {
		return `${filename}:${versionTag}:${chunkIndex}`;
	},

	getFileVersionTag: function (fileEntry) {
		if (!fileEntry) {
			return 'unknown';
		}
		if (fileEntry.version !== null && fileEntry.version !== undefined) {
			return `v${fileEntry.version}`;
		}
		if (fileEntry.size !== null && fileEntry.size !== undefined) {
			return `s${fileEntry.size}`;
		}
		return 'unknown';
	},

	/**
	 * Initialize IndexedDB cache
	 */
	initDB: function () {
		if (this.dbInitPromise) {
			return this.dbInitPromise;
		}

		this.dbInitPromise = new Promise((resolve, reject) => {
			console.log('[LazyFS] Initializing IndexedDB cache...');

			const request = indexedDB.open(this.DB_NAME, this.DB_VERSION);

			request.onerror = (event) => {
				console.error('[LazyFS] IndexedDB error:', event.target.error);
				this.dbReady = false;
				resolve(false); // Don't reject, just continue without cache
			};

			request.onsuccess = (event) => {
				this.db = event.target.result;
				this.dbReady = true;
				console.log('[LazyFS] IndexedDB cache ready');
				resolve(true);
			};

			request.onupgradeneeded = (event) => {
				const db = event.target.result;
				// Create object store for chunks: key = "filename:chunkIndex"
				if (!db.objectStoreNames.contains(this.STORE_NAME)) {
					db.createObjectStore(this.STORE_NAME);
					console.log('[LazyFS] Created IndexedDB object store');
				}
			};
		});

		return this.dbInitPromise;
	},

	/**
	 * Get chunk from IndexedDB cache
	 */
	getCachedChunk: function (filename, versionTag, chunkIndex) {
		if (!this.dbReady || !this.db) {
			return Promise.resolve(null);
		}

		return new Promise((resolve) => {
			try {
				const tx = this.db.transaction(this.STORE_NAME, 'readonly');
				const store = tx.objectStore(this.STORE_NAME);
				const key = this.getChunkCacheKey(filename, versionTag, chunkIndex);
				const request = store.get(key);

				request.onsuccess = () => {
					resolve(request.result || null);
				};

				request.onerror = () => {
					resolve(null);
				};
			} catch (e) {
				resolve(null);
			}
		});
	},

	/**
	 * Store chunk in IndexedDB cache
	 */
	setCachedChunk: function (filename, versionTag, chunkIndex, data) {
		if (!this.dbReady || !this.db) {
			return Promise.reject(new Error('IndexedDB cache is unavailable'));
		}

		return new Promise((resolve, reject) => {
			try {
				const tx = this.db.transaction(this.STORE_NAME, 'readwrite');
				const store = tx.objectStore(this.STORE_NAME);
				const key = this.getChunkCacheKey(filename, versionTag, chunkIndex);
				store.put(data, key);

				tx.oncomplete = () => resolve(true);
				tx.onerror = () => reject(tx.error || new Error('IndexedDB cache transaction failed'));
				tx.onabort = () => reject(tx.error || new Error('IndexedDB cache transaction aborted'));
			} catch (e) {
				reject(e);
			}
		});
	},

	trackCacheWrite: function (write) {
		this.cacheWriteStats.attempted++;
		const tracked = write.then(() => {
			this.cacheWriteStats.succeeded++;
			return true;
		}, error => {
			this.cacheWriteStats.failed++;
			this.cacheWriteStats.lastError = error instanceof Error ? error.message : String(error);
			return false;
		});
		this.pendingCacheWrites.add(tracked);
		tracked.finally(() => this.pendingCacheWrites.delete(tracked));
		return tracked;
	},

	awaitCacheWrites: async function () {
		while (this.pendingCacheWrites.size > 0) {
			await Promise.all(Array.from(this.pendingCacheWrites));
		}
	},

	/**
	 * Clear the entire IndexedDB cache
	 */
	clearCache: function () {
		if (!this.dbReady || !this.db) {
			return Promise.resolve(false);
		}

		return new Promise((resolve) => {
			try {
				const tx = this.db.transaction(this.STORE_NAME, 'readwrite');
				const store = tx.objectStore(this.STORE_NAME);
				store.clear();

				tx.oncomplete = () => {
					console.log('[LazyFS] Cache cleared');
					resolve(true);
				};
				tx.onerror = () => resolve(false);
			} catch (e) {
				resolve(false);
			}
		});
	},

	/**
	 * Handle incoming WebSocket message (text or binary)
	 */
	handleMessage: function (data) {
		// Check if binary frame
		if (data instanceof ArrayBuffer) {
			this.handleBinaryChunk(data);
			return;
		}

		// Text frame - parse as JSON
		try {
			const msg = JSON.parse(data);

			if (msg.type === 'size') {
				if (this.files.has(msg.file)) {
					const fileEntry = this.files.get(msg.file);
					fileEntry.version = msg.version;
				}
				// File size response
				const key = `size:${msg.file}`;
				this.resolvePendingRequest(key, msg.size);
			} else if (msg.type === 'chunks_done') {
				// Batch request completed
				const key = `batch:${msg.file}:${msg.start}:${msg.end}`;
				this.resolvePendingRequest(key);
			} else if (msg.type === 'error') {
				console.error('[LazyFS] Server error:', msg.message);
			}
		} catch (e) {
			console.error('[LazyFS] Error parsing message:', e);
		}
	},

	/**
	 * Handle binary chunk data
	 * Format: [4 bytes: chunk index LE] [1 byte: filename len] [filename] [data]
	 */
	handleBinaryChunk: function (buffer) {
		const view = new DataView(buffer);

		// Parse header
		const chunkIndex = view.getUint32(0, true); // little-endian
		const filenameLen = view.getUint8(4);
		const filenameBytes = new Uint8Array(buffer, 5, filenameLen);
		const filename = new TextDecoder().decode(filenameBytes);

		// Extract chunk data
		const dataStart = 5 + filenameLen;
		const chunkData = new Uint8Array(buffer, dataStart);

		// Store in cache (memory)
		const fileEntry = this.files.get(filename);
		const versionTag = this.getFileVersionTag(fileEntry);
		const cacheKey = this.getChunkCacheKey(filename, versionTag, chunkIndex);
		this.chunkCache.set(cacheKey, chunkData);

		// Store in cache (persistent)
		if (versionTag !== 'unknown') {
			this.trackCacheWrite(this.setCachedChunk(filename, versionTag, chunkIndex, chunkData));
		}

		// Log
		this.logFetch(filename, chunkIndex, chunkData.length, false);

		// Resolve any pending request for this specific chunk
		const pendingKey = `chunk:${filename}:${chunkIndex}`;
		this.resolvePendingRequest(pendingKey, chunkData);
	},

	// ========== FILE OPERATIONS ==========

	/**
	 * Register a file for lazy loading
	 */
	registerFile: function (filepath, url) {
		console.log('[LazyFS] Registering file:', filepath);
		this.files.set(filepath, {
			size: null,
			version: null,
		});
	},

	/**
	 * Get file size via WebSocket
	 */
	getFileSize: async function (filepath) {
		const key = `size:${filepath}`;
		return this.request(key, {
				type: 'get_size',
				file: filepath
			}, 10000);
	},

	/**
	 * Fetch a batch of chunks
	 */
	fetchChunks: async function (filepath, startChunk, endChunk) {
		const key = `batch:${filepath}:${startChunk}:${endChunk}`;
		if (this.inFlightBatches.has(key)) {
			return this.inFlightBatches.get(key);
		}

		const request = this.request(key, {
				type: 'get_chunks',
				file: filepath,
				start: startChunk,
				end: endChunk,
				chunk_size: this.CHUNK_SIZE
			}, 60000);
		this.inFlightBatches.set(key, request);
		try {
			return await request;
		} finally {
			this.inFlightBatches.delete(key);
		}
	},

	/**
	 * Keep an NX range resident and persist every missing chunk. Background
	 * work fetches one chunk at a time and yields so interactive reads are not
	 * queued behind a large preload batch.
	 */
	prefetchFileRange: function (filepath, offset, length) {
		if (this.terminalConnectionFailure) {
			return Promise.reject(new Error('LazyFS connection is no longer recoverable'));
		}
		const key = `${filepath}:${offset}:${length}`;
		if (this.inFlightPrefetches.has(key)) {
			return this.inFlightPrefetches.get(key);
		}

		let resolveRequest;
		let rejectRequest;
		const request = new Promise((resolve, reject) => {
			resolveRequest = resolve;
			rejectRequest = reject;
		});
		this.inFlightPrefetches.set(key, request);
		const clear = () => this.inFlightPrefetches.delete(key);
		request.then(clear, clear);
		this.prefetchQueue.push({ key, filepath, offset, length, resolveRequest, rejectRequest });
		this.drainPrefetchQueue();
		return request;
	},

	startPrefetchTask: function (task) {
		this.activePrefetches++;
		this.prefetchFileRangeInternal(task.filepath, task.offset, task.length)
			.then(task.resolveRequest, task.rejectRequest)
			.finally(() => {
				this.activePrefetches--;
				this.drainPrefetchQueue();
			});
	},

	drainPrefetchQueue: function () {
		while (this.activePrefetches < this.MAX_CONCURRENT_PREFETCHES && this.prefetchQueue.length > 0) {
			this.startPrefetchTask(this.prefetchQueue.shift());
		}
	},

	promotePrefetch: function (filepath, offset, length) {
		const key = `${filepath}:${offset}:${length}`;
		const index = this.prefetchQueue.findIndex(task => task.key === key);
		if (index >= 0) {
			const [task] = this.prefetchQueue.splice(index, 1);
			// Foreground work may temporarily exceed the background concurrency cap.
			this.startPrefetchTask(task);
		}
	},

	runForegroundRequest: function (key, operation) {
		const existing = this.foregroundRequests.get(key);
		if (existing) {
			return existing.promise;
		}

		let resolveRequest;
		const entry = {
			promise: new Promise(resolve => { resolveRequest = resolve; })
		};
		const attempt = (isRetry = false) => {
			if (isRetry && this.terminalConnectionFailure && !this.resetConnectionFailure()) {
				return;
			}
			window.MapleAssetLoading?.begin(key);
			Promise.resolve()
				.then(operation)
				.then(result => {
					this.foregroundRequests.delete(key);
					window.MapleAssetLoading?.end(key);
					resolveRequest(result);
				})
				.catch(error => {
					console.error('[LazyFS] Foreground asset request failed:', error);
					window.MapleAssetLoading?.fail(key, () => attempt(true));
				});
		};
		this.foregroundRequests.set(key, entry);
		attempt();
		return entry.promise;
	},

	requestForegroundFileRange: function (filepath, offset, length) {
		if (this.isFileRangeResident(filepath, offset, length)) {
			return Promise.resolve();
		}
		const key = `range:${filepath}:${offset}:${length}`;
		return this.runForegroundRequest(key, () => {
			const request = this.prefetchFileRange(filepath, offset, length);
			this.promotePrefetch(filepath, offset, length);
			return request;
		});
	},

	prefetchFileRangeInternal: async function (filepath, offset, length) {
		if (this.terminalConnectionFailure) {
			throw new Error('LazyFS connection is no longer recoverable');
		}
		const fileEntry = this.files.get(filepath);
		if (!fileEntry || !fileEntry.size || length <= 0) {
			throw new Error(`Cannot preload unregistered file: ${filepath}`);
		}

		await this.initDB();
		const rangeEnd = Math.min(fileEntry.size, offset + length);
		const startChunk = Math.floor(offset / this.CHUNK_SIZE);
		const endChunk = Math.floor((rangeEnd - 1) / this.CHUNK_SIZE);
		const versionTag = this.getFileVersionTag(fileEntry);

		for (let chunkIndex = startChunk; chunkIndex <= endChunk; chunkIndex++) {
			if (this.terminalConnectionFailure) {
				throw new Error('LazyFS connection is no longer recoverable');
			}
			const cacheKey = this.getChunkCacheKey(filepath, versionTag, chunkIndex);
			if (!this.chunkCache.has(cacheKey)) {
				const cachedData = await this.getCachedChunk(filepath, versionTag, chunkIndex);
				if (cachedData) {
					this.chunkCache.set(cacheKey, cachedData);
					this.logFetch(filepath, chunkIndex, cachedData.length, true);
				} else {
					await this.fetchChunks(filepath, chunkIndex, chunkIndex);
				}
			}

			await new Promise(resolve => setTimeout(resolve, 0));
		}
	},

	isFileRangeResident: function (filepath, offset, length) {
		const fileEntry = this.files.get(filepath);
		if (!fileEntry || !fileEntry.size || length <= 0) {
			return false;
		}

		const rangeEnd = Math.min(fileEntry.size, offset + length);
		const startChunk = Math.floor(offset / this.CHUNK_SIZE);
		const endChunk = Math.floor((rangeEnd - 1) / this.CHUNK_SIZE);
		const versionTag = this.getFileVersionTag(fileEntry);
		for (let chunkIndex = startChunk; chunkIndex <= endChunk; chunkIndex++) {
			const cacheKey = this.getChunkCacheKey(filepath, versionTag, chunkIndex);
			if (!this.chunkCache.has(cacheKey)) {
				return false;
			}
		}
		return true;
	},

	requestPersistentStorage: async function () {
		if (!navigator.storage || !navigator.storage.persist) {
			console.warn('[LazyFS] Persistent browser storage API is unavailable');
			return false;
		}

		try {
			const granted = await navigator.storage.persist();
			console.log(`[LazyFS] Persistent browser storage ${granted ? 'granted' : 'not granted'}`);
			return granted;
		} catch (error) {
			console.error('[LazyFS] Persistent storage request failed:', error);
			return false;
		}
	},

	characterMetadataEnd: async function () {
		const header = await this.readFileData('Character.nx', 0, 40, false);
		if (!header || header.length < 40) {
			throw new Error('Character.nx header is unavailable');
		}

		const view = new DataView(header.buffer, header.byteOffset, header.byteLength);
		const bitmapCount = view.getUint32(28, true);
		const bitmapOffsetLow = view.getUint32(32, true);
		const bitmapOffsetHigh = view.getUint32(36, true);
		const bitmapOffset = bitmapOffsetLow + bitmapOffsetHigh * 0x100000000;
		return bitmapOffset + bitmapCount * 8;
	},

	/**
	 * Start once after entering the game. Item and String are retained in full;
	 * Character metadata is retained up front, and normal LazyFS reads retain
	 * every encountered equipment image chunk without an expiry timer.
	 */
	startItemAssetPreload: function () {
		if (this.itemAssetPreloadPromise) {
			return this.itemAssetPreloadPromise;
		}

		this.itemAssetPreloadState = 'running';
		this.itemAssetPreloadResult = null;
		this.itemAssetPreloadPromise = new Promise(resolve => setTimeout(resolve, 0))
			.then(async () => {
				const failureBaseline = this.cacheWriteStats.failed;
				const attemptBaseline = this.cacheWriteStats.attempted;
				const persistent = await this.requestPersistentStorage();
				if (!persistent) {
					this.itemAssetPreloadResult = Object.freeze({
						persistent: false,
						attempted: 0,
						succeeded: 0,
						failed: 0,
						reason: 'persistent-storage-unavailable',
					});
					this.itemAssetPreloadState = 'unavailable';
					console.warn('[LazyFS] Persistent item asset preload unavailable; bulk preload skipped');
					return;
				}
				const stringEntry = this.files.get('String.nx');
				const itemEntry = this.files.get('Item.nx');
				if (!stringEntry || !itemEntry) {
					throw new Error('Item preload files are not registered');
				}
				await this.prefetchFileRange('String.nx', 0, stringEntry.size);
				await this.prefetchFileRange('Item.nx', 0, itemEntry.size);
				const metadataEnd = await this.characterMetadataEnd();
				await this.prefetchFileRange('Character.nx', 0, metadataEnd);
				await this.awaitCacheWrites();
				const failed = this.cacheWriteStats.failed - failureBaseline;
				const attempted = this.cacheWriteStats.attempted - attemptBaseline;
				this.itemAssetPreloadResult = Object.freeze({
					persistent,
					attempted,
					succeeded: attempted - failed,
					failed,
				});
				if (failed > 0) {
					throw new Error(`${failed} IndexedDB cache writes failed`);
				}
				this.itemAssetPreloadState = 'complete';
				console.log(`[LazyFS] Item asset preload ${this.itemAssetPreloadState}`);
			})
			.catch(error => {
				this.itemAssetPreloadState = 'failed';
				this.itemAssetPreloadPromise = null;
				console.error('[LazyFS] Persistent item asset preload failed:', error);
				throw error;
			});

		// Keep the task detached from the WASM call while still reporting errors.
		this.itemAssetPreloadPromise.catch(() => {});
		return this.itemAssetPreloadPromise;
	},

	/**
	 * Get a chunk from cache or fetch it
	 */
	getChunk: async function (filepath, chunkIndex) {
		const fileEntry = this.files.get(filepath);
		const versionTag = this.getFileVersionTag(fileEntry);
		const cacheKey = this.getChunkCacheKey(filepath, versionTag, chunkIndex);

		// Check cache first
		if (this.chunkCache.has(cacheKey)) {
			return this.chunkCache.get(cacheKey);
		}

		const key = `chunk:${filepath}:${chunkIndex}`;
		return this.request(key, {
				type: 'get_chunk',
				file: filepath,
				index: chunkIndex,
				chunk_size: this.CHUNK_SIZE
			}, 30000);
	},

	/**
	 * Read data from a file at a specific offset
	 * This is called by Emscripten's FS when the file is read
	 */
	readFileData: async function (filepath, offset, length, showBlockingOverlay = true) {
		const fileEntry = this.files.get(filepath);
		if (!fileEntry) {
			console.error('[LazyFS] File not registered:', filepath);
			return null;
		}

		// Initialize DB if needed
		if (!this.dbReady && !this.dbInitPromise) {
			await this.initDB();
		} else if (this.dbInitPromise) {
			await this.dbInitPromise;
		}

		// Fetch file size if not already known
		if (fileEntry.size === null) {
			try {
				fileEntry.size = await this.getFileSize(filepath);
				if (fileEntry.size < 0) {
					console.error('[LazyFS] File not found:', filepath);
					return null;
				}
				console.log('[LazyFS] File size:', filepath, fileEntry.size, 'bytes');
			} catch (e) {
				console.error('[LazyFS] Failed to get file size:', e);
				return null;
			}
		}

		// Calculate which chunks we need
		const startChunk = Math.floor(offset / this.CHUNK_SIZE);
		const endChunk = Math.floor((offset + length - 1) / this.CHUNK_SIZE);

		// Identify chunks that need fetching
		let needsFetch = false;
		let missingStart = -1;
		let missingEnd = -1;
		const batches = []; // Array of {start, end}

		for (let chunkIdx = startChunk; chunkIdx <= endChunk; chunkIdx++) {
			const versionTag = this.getFileVersionTag(fileEntry);
			const cacheKey = this.getChunkCacheKey(filepath, versionTag, chunkIdx);

			// Check memory cache first
			if (this.chunkCache.has(cacheKey)) {
				this.logFetch(filepath, chunkIdx, this.CHUNK_SIZE, true); // Log cache hit

				// End current missing range if any
				if (missingStart !== -1) {
					batches.push({ start: missingStart, end: missingEnd });
					missingStart = -1;
					missingEnd = -1;
				}
				continue;
			}

			// Check IndexedDB
			const cachedData = await this.getCachedChunk(filepath, versionTag, chunkIdx);
			if (cachedData) {
				// Success! Load to memory
				this.chunkCache.set(cacheKey, cachedData);
				this.logFetch(filepath, chunkIdx, cachedData.length, true); // Log cache hit

				// End current missing range
				if (missingStart !== -1) {
					batches.push({ start: missingStart, end: missingEnd });
					missingStart = -1;
					missingEnd = -1;
				}
				continue;
			}

			// Not in any cache - needs fetch
			if (missingStart === -1) {
				missingStart = chunkIdx;
				missingEnd = chunkIdx;
			} else {
				missingEnd = chunkIdx;
			}
			needsFetch = true;
		}

		// Push final batch if open
		if (missingStart !== -1) {
			batches.push({ start: missingStart, end: missingEnd });
		}

		// Fetch missing batches
		if (needsFetch) {
			const fetchMissing = () => Promise.all(batches.map(batch =>
				this.fetchChunks(filepath, batch.start, batch.end)
			));
			try {
				const canShowRetry = showBlockingOverlay &&
					window.MapleAssetLoading?.canInteract?.();
				if (canShowRetry) {
					const key = `read:${filepath}:${batches.map(batch => `${batch.start}-${batch.end}`).join(',')}`;
					await this.runForegroundRequest(key, fetchMissing);
				} else {
					await fetchMissing();
				}
			} catch (e) {
				console.error('[LazyFS] Failed to fetch chunks:', e);
				return null;
			}
		}

		// Buffer for result
		const result = new Uint8Array(length);
		let resultOffset = 0;

		// Assemble from cached chunks
		for (let chunkIdx = startChunk; chunkIdx <= endChunk; chunkIdx++) {
			const versionTag = this.getFileVersionTag(fileEntry);
			const cacheKey = this.getChunkCacheKey(filepath, versionTag, chunkIdx);
			const chunk = this.chunkCache.get(cacheKey);

			if (!chunk) {
				console.error('[LazyFS] Chunk not in cache after fetch:', cacheKey);
				return null;
			}

			const chunkStart = chunkIdx * this.CHUNK_SIZE;

			// Calculate what part of this chunk we need
			const readStart = Math.max(0, offset - chunkStart);
			const readEnd = Math.min(chunk.length, offset + length - chunkStart);
			const readLength = readEnd - readStart;

			// Copy only the needed bytes
			result.set(chunk.subarray(readStart, readEnd), resultOffset);
			resultOffset += readLength;
		}

		return result;
	}
};

// Expose LazyFS on Module (will be merged when Module is created)
if (typeof Module === 'undefined') {
	Module = {};
}
// Merge existing configuration if it was defined in index.html before this script loaded
if (Module.LazyFS) {
	Object.assign(LazyFS, Module.LazyFS);
}
Module.LazyFS = LazyFS;
