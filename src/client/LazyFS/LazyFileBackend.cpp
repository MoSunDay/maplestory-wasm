#include <LazyFS/LazyFileBackend.h>
#include <cstring>
#include <iostream>

namespace LazyFS
{

	bool LazyFileBackend::open(const std::string& path)
	{
		if (is_open_)
		{
			close();
		}

		path_ = path;
		current_offset_ = 0;

		// Get file size from JavaScript
		file_size_ = (size_t)EM_ASM_DOUBLE(
			{
				var path = UTF8ToString($0);
				if (typeof Module.LazyFS !== 'undefined' && Module.LazyFS.files.has(path))
				{
					var fileEntry = Module.LazyFS.files.get(path);
					// Asyncify replays the whole wasm stack from the bottom frame on
					// wake-up, so this open() runs again. In Rewinding state
					// handleSleep never re-runs its closure: it only stops the
					// rewind and hands back the recorded wakeUp value, so the
					// frame that actually suspended MUST re-enter handleSleep to
					// stop the replay exactly here. Every other re-executed open
					// finds the size already cached and returns that snapshot
					// synchronously - never handleSleep (a stray frame would stop
					// the rewind at the wrong place) and never 0 (0 trips the C++
					// failure branch, nl::file::open throws while the rewind is
					// still in progress and the single rewind buffer stays
					// wedged in state 2).
					var asyncifyState = (typeof Asyncify !== 'undefined') ? Asyncify.state : 0;
					var sizeKey = 'size#' + path;
					var replayingSuspendedSize =
						asyncifyState === 2 && Module.LazyFS.pendingSyncRead === sizeKey;
					// Same wakeUp hardening as fetch_data below: a wakeUp that
					// throws (lost/stale rewind buffer) must not escape as an
					// unhandled rejection - it gets the one-shot reload bailout.
					var makeWakeUpOrReload = function(wakeUp) {
						return function(value) {
							try { wakeUp(value); }
							catch (e) {
								if (e === 'unwind' || (e && e.message === 'unwind')) return;
								console.error('[LazyFS] wakeUp failed (Asyncify state ' +
									Asyncify.state + '), reloading page:', e);
								var reloadedBefore = false;
								try {
									reloadedBefore = sessionStorage.getItem('lazyfsWakeUpReloaded') === '1';
								} catch (ignored) {}
								if (!reloadedBefore) {
									try { sessionStorage.setItem('lazyfsWakeUpReloaded', '1'); } catch (ignored) {}
									location.reload();
								}
							}
						};
					};
					var fetchSizeOnce = function() {
						return Asyncify.handleSleep(function(wakeUp) {
							var wakeUpOrReload = makeWakeUpOrReload(wakeUp);
							Module.LazyFS.getFileSize(path).then(function(size) {
								fileEntry.size = size;
								wakeUpOrReload(size);
							}).catch(function(e) {
								if (e === 'unwind' || (e && e.message === 'unwind')) return;
								console.error('[LazyFS] Error fetching file size for', path, ':', e);
								wakeUpOrReload(0);
							});
						});
					};
					if (asyncifyState === 2)
					{
						// Replayed frame that is not the tagged suspension: the
						// original pass already completed this open, so the
						// cached snapshot exists and must be returned synchronously.
						if (!replayingSuspendedSize && fileEntry.size !== null)
						{
							return fileEntry.size;
						}
						// This is - or may be - the frame that suspended. The
						// pendingSyncRead tag is one shared slot that an
						// interleaved suspension (ws_recv poll, another read)
						// can overwrite between our unwind and wakeUp, and a
						// rejected size fetch leaves size uncached, so a lost
						// tag must not degrade into a bare `return 0`: that
						// would throw nl::file::open from inside the replay.
						// Going through handleSleep is the only move that both
						// stops the rewind and recovers the recorded value; if
						// the fetch was rejected, the rewind has already
						// stopped when the recorded 0 surfaces, so the caller
						// retries in Normal state instead of wedging.
						var sizeResult = fetchSizeOnce();
						// A rewinding call runs its tail, so only clear our own tag.
						if (replayingSuspendedSize && Module.LazyFS.pendingSyncRead === sizeKey) {
							Module.LazyFS.pendingSyncRead = null;
						}
						// If a snapshot exists it always wins over the recorded
						// value (they are equal on the happy path anyway).
						return fileEntry.size !== null ? fileEntry.size : sizeResult;
					}
					if (fileEntry.size === null)
					{
						Module.LazyFS.pendingSyncRead = sizeKey;
						var sizeResult = fetchSizeOnce();
						if (Module.LazyFS.pendingSyncRead === sizeKey) {
							Module.LazyFS.pendingSyncRead = null;
						}
						return sizeResult;
					}
					return fileEntry.size;
				}
				else
				{
					// console.error('[LazyFS] File not registered:', path);
					return 0.0;
				}
			},
			path.c_str()
		);

		if (file_size_ == 0)
		{
			std::cerr << "[LazyFS] Failed to open file: " << path << std::endl;
			return false;
		}

		is_open_ = true;
		// std::cout << "[LazyFS] Opened file: " << path << " (size: " << file_size_ << " bytes)" << std::endl;
		return true;
	}

	void LazyFileBackend::close()
	{
		if (is_open_)
		{
			// std::cout << "[LazyFS] Closed file: " << path_ << std::endl;
			is_open_ = false;
			path_.clear();
			file_size_ = 0;
			current_offset_ = 0;
		}
	}

	size_t LazyFileBackend::read(void* buffer, size_t size)
	{
		if (!is_open_)
		{
			std::cerr << "[LazyFS] Cannot read from closed file" << std::endl;
			return 0;
		}

		// Don't read past end of file
		size_t bytes_available = file_size_ - current_offset_;
		size_t bytes_to_read = std::min(size, bytes_available);

		if (bytes_to_read == 0)
		{
			return 0; // EOF
		}

		// Fetch data using JavaScript
		if (!fetch_data(current_offset_, bytes_to_read, buffer))
		{
			std::cerr << "[LazyFS] Failed to fetch data" << std::endl;
			return 0;
		}

		current_offset_ += bytes_to_read;
		return bytes_to_read;
	}

	bool LazyFileBackend::seek(size_t offset)
	{
		if (!is_open_)
		{
			std::cerr << "[LazyFS] Cannot seek in closed file" << std::endl;
			return false;
		}

		if (offset > file_size_)
		{
			std::cerr << "[LazyFS] Seek offset beyond end of file" << std::endl;
			return false;
		}

		current_offset_ = offset;
		return true;
	}

	bool LazyFileBackend::fetch_data(size_t offset, size_t size, void* out_buffer)
	{
		// Call JavaScript to fetch data using Asyncify
		int result = EM_ASM_INT(
			{
				var path = UTF8ToString($0);
				var offset = $1;
				var size = $2;
				var bufferPtr = $3;

				if (typeof Module.LazyFS === 'undefined')
				{
					// console.error('[LazyFS] LazyFS module not loaded');
					return 0;
				}

				// Sync fast-path: a fully resident range is copied into the wasm
				// heap without suspending, so re-entrant reads can never collide
				// inside Asyncify's single rewind buffer.
				//
				// A woken handleSleep replays the wasm stack from the bottom frame
				// and reaches this block again (Asyncify state 2 = Rewinding):
				// - The read that actually suspended (tagged in
				//   pendingSyncRead) must replay through handleSleep so the rewind
				//   stops here with the recorded wakeUp value. Serving it from the
				//   fast path instead (its chunks are resident by then) would run
				//   past the recorded resume point and crash later dynCall
				//   dispatches with "null function".
				// - Any other re-executed read finished earlier in this tick, so its
				//   chunks are resident (chunkCache is append-only) and it MUST be
				//   answered synchronously: suspending again in Rewinding state
				//   restarts the replay from the bottom frame forever (busy-wedge).
				var asyncifyState = (typeof Asyncify !== 'undefined') ? Asyncify.state : 0;
				var pendingKey = path + '#' + offset + '#' + size;
				var replayingSuspendedRead =
					asyncifyState === 2 && Module.LazyFS.pendingSyncRead === pendingKey;
				if (asyncifyState === 2 && !replayingSuspendedRead)
				{
					var replayed = Module.LazyFS.readCachedSync
						? Module.LazyFS.readCachedSync(path, offset, size) : null;
					if (replayed) { HEAPU8.set(replayed, bufferPtr); return 1; }
					// No resident data means the original read failed too; report
					// the same failure so the caller retries in Normal state.
					Module.LazyFS.replayFailures = (Module.LazyFS.replayFailures || 0) + 1;
					return 0;
				}
				var hit = asyncifyState === 2 ? null
					: (Module.LazyFS.readCachedSync
						? Module.LazyFS.readCachedSync(path, offset, size) : null);
				if (hit) { HEAPU8.set(hit, bufferPtr); return 1; }

				// Use Asyncify.handleSleep directly to avoid Promise wrapper issues.
				// Tag this read as the live suspension point; a rewinding replay
				// clears the tag once it has resumed at the recorded frame.
				// A failed wakeUp wedges Asyncify's single rewind buffer (state
				// stuck; later RAF/input callbacks are then silently dropped and
				// the UI freezes without any error): surface the failure and do a
				// one-shot explicit reload instead of freezing silently.
				// NOTE: must live inside the handleSleep callback so it closes
				// over that invocation's wakeUp.
				var makeWakeUpOrReload = function(wakeUp) {
					return function(value) {
						try { wakeUp(value); }
						catch (e) {
							// 'unwind' is emscripten_set_main_loop's normal
							// stack-escape sentinel surfacing through the
							// synchronous doRewind replay, not a wake failure.
							if (e === 'unwind' || (e && e.message === 'unwind')) return;
							console.error('[LazyFS] wakeUp failed (Asyncify state ' +
								Asyncify.state + '), reloading page:', e);
							var reloadedBefore = false;
							try {
								reloadedBefore = sessionStorage.getItem('lazyfsWakeUpReloaded') === '1';
							} catch (ignored) {}
							if (!reloadedBefore) {
								try { sessionStorage.setItem('lazyfsWakeUpReloaded', '1'); } catch (ignored) {}
								location.reload();
							}
						}
					};
				};
				var wasRewinding = asyncifyState === 2;
				Module.LazyFS.pendingSyncRead = pendingKey;
				var fetched = Asyncify.handleSleep(function(wakeUp) {
					var wakeUpOrReload = makeWakeUpOrReload(wakeUp);
					Module.LazyFS.readFileData(path, offset, size)
						.then(function(data) {
							if (data) {
								if (data.length > size) {
									console.error('[LazyFS] OOB Write Risk! chunks:', data.length, 'vs req:', size);
									wakeUpOrReload(0);
									return;
								}
								// Check for valid pointer and heap
								if (bufferPtr < 0 || bufferPtr + data.length > HEAPU8.length) {
									console.error('[LazyFS] OOB FATAL: Ptr:', bufferPtr, 'Len:', data.length, 'Heap:', HEAPU8.length);
									wakeUpOrReload(0);
									return;
								}
								
								HEAPU8.set(data, bufferPtr);
								wakeUpOrReload(1);
							} else {
								console.error('[LazyFS] No data returned for', path);
								wakeUpOrReload(0);
							}
						}) // Use single .then() and no .catch() for the wakeUp path to act naturally
						.catch(function(e) {
							// If 'unwind', IGNORE IT completely.
							if (e === 'unwind' || (e && e.message === 'unwind')) return;
							
							console.error('[LazyFS] Error fetching data:', e);
							console.error(e.stack);
							wakeUpOrReload(0);
						});
				});
				if (wasRewinding && Module.LazyFS.pendingSyncRead === pendingKey) {
					Module.LazyFS.pendingSyncRead = null;
				}
				return fetched;
			},
			path_.c_str(),
			(double)offset,
			(int)size,
			out_buffer
		);

		return result == 1;
	}

	// LazyFile implementation

	LazyFile::LazyFile(const std::string& path) : backend_(std::make_unique<LazyFileBackend>())
	{
		backend_->open(path);
	}

	LazyFile::~LazyFile()
	{
		if (backend_)
		{
			backend_->close();
		}
	}

	LazyFile::LazyFile(LazyFile&& other) noexcept : backend_(std::move(other.backend_))
	{
	}

	LazyFile& LazyFile::operator=(LazyFile&& other) noexcept
	{
		if (this != &other)
		{
			backend_ = std::move(other.backend_);
		}
		return *this;
	}

	bool LazyFile::open(const std::string& path)
	{
		if (!backend_)
		{
			backend_ = std::make_unique<LazyFileBackend>();
		}
		return backend_->open(path);
	}

	void LazyFile::close()
	{
		if (backend_)
		{
			backend_->close();
		}
	}

	size_t LazyFile::read(void* buffer, size_t size)
	{
		return backend_ ? backend_->read(buffer, size) : 0;
	}

	bool LazyFile::seek(size_t offset)
	{
		return backend_ ? backend_->seek(offset) : false;
	}

	size_t LazyFile::tell() const
	{
		return backend_ ? backend_->tell() : 0;
	}

	size_t LazyFile::size() const
	{
		return backend_ ? backend_->size() : 0;
	}

	bool LazyFile::is_open() const
	{
		return backend_ && backend_->is_open();
	}

	const std::string& LazyFile::path() const
	{
		static const std::string empty;
		return backend_ ? backend_->path() : empty;
	}

} // namespace LazyFS
