/**
 * LazyFS boot warmup & item asset preload pipeline.
 *
 * Split out of lazyfs.js (which reached its iteration line cap). Injected via
 * --pre-js AFTER lazyfs.js (order is fixed in src/client/CMakeLists.txt:
 * lazyfs_connection.js -> lazyfs.js -> lazyfs_warmup.js -> lazyfs_sync_cache.js).
 *
 * Installs onto the existing Module.LazyFS object:
 * - scheduleBootWarmup / startItemAssetPreload entry points,
 * - their itemAssetPreload* state fields,
 * - the helpers used only by them (awaitCacheWrites, requestPersistentStorage,
 *   characterMetadataEnd).
 *
 * Pure functional augmentation via Object.assign: no classes, no hidden
 * state. The attached functions keep plain object-literal `this` semantics, so
 * this.files, this.prefetchFileRange(...), this.request(...) resolve against
 * the very same LazyFS object as before the split - runtime behavior is
 * unchanged.
 */
(function (global) {
	'use strict';

	function install(LazyFS) {
		if (!LazyFS || LazyFS.startItemAssetPreload) {
			return;
		}

		Object.assign(LazyFS, {
			itemAssetPreloadPromise: null,
			itemAssetPreloadState: 'idle',

			awaitCacheWrites: async function () {
				while (this.pendingCacheWrites.size > 0) {
					await Promise.all(Array.from(this.pendingCacheWrites));
				}
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
				// Background chunk pipeline only: readFileData is skipped while
				// Asyncify is parked, which is nearly always the case at boot.
				const fileEntry = this.files.get('Character.nx');
				if (!fileEntry || !fileEntry.size) throw new Error('Character.nx is not registered');
				await this.prefetchFileRange('Character.nx', 0, 40);
				const chunk = this.chunkCache.get(this.getChunkCacheKey('Character.nx', this.getFileVersionTag(fileEntry), 0));
				if (!chunk || chunk.length < 40) throw new Error('Character.nx header is unavailable');
				const view = new DataView(chunk.buffer, chunk.byteOffset, chunk.byteLength);
				const bitmapCount = view.getUint32(28, true);
				const bitmapOffset = view.getUint32(32, true) + view.getUint32(36, true) * 0x100000000;
				return bitmapOffset + bitmapCount * 8;
			},

			// Boot warmup: once every preload file has a size (before the login flow's
			// first lazy reads), start the warmup once; never gated on a persist() grant.
			scheduleBootWarmup: function () {
				if (this.bootWarmupScheduled) return;
				const hasSize = name => { const e = this.files.get(name); return !!e && typeof e.size === 'number' && e.size > 0; };
				if (hasSize('String.nx') && hasSize('Item.nx') && hasSize('Character.nx')) {
					this.bootWarmupScheduled = true;
					this.startItemAssetPreload();
				}
			},

			/**
			 * Start once at boot (scheduleBootWarmup) and again after entering the
			 * game; the second call reuses the in-flight promise. Item and String are
			 * retained in full; Character metadata is retained up front, and normal
			 * LazyFS reads retain every encountered equipment image chunk.
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
						// Best-effort: cold-cache first visits have no persist() grant yet.
						const persistent = await this.requestPersistentStorage();
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
		});
	}

	const target = global.Module && global.Module.LazyFS ? global.Module.LazyFS : null;
	if (target) {
		install(target);
	} else {
		const queue = global.__lazyfsWarmupQueue || (global.__lazyfsWarmupQueue = []);
		queue.push(install);
		global.__lazyfsInstallWarmup = install;
	}
})(typeof window !== 'undefined' ? window : globalThis);
