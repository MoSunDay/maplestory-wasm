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
					// wake-up, so this open() runs again. Only the read that actually
					// suspended may go through handleSleep; every other re-executed
					// open finds the size already cached and returns it synchronously
					// (never both, or the replay would stop at the wrong frame).
					var asyncifyState = (typeof Asyncify !== 'undefined') ? Asyncify.state : 0;
					var sizeKey = 'size#' + path;
					var replayingSuspendedSize =
						asyncifyState === 2 && Module.LazyFS.pendingSyncRead === sizeKey;
					if (asyncifyState === 2 && !replayingSuspendedSize && fileEntry.size === null)
					{
						// The original size fetch failed; report it and let the
						// caller retry in the Normal state instead of wedging the
						// rewinder in a suspension that can never be replayed.
						return 0;
					}
					if (fileEntry.size === null || replayingSuspendedSize)
					{
						var sizeWasRewinding = asyncifyState === 2;
						Module.LazyFS.pendingSyncRead = sizeKey;
						var sizeResult = Asyncify.handleSleep(function(wakeUp) {
							Module.LazyFS.getFileSize(path).then(function(size) {
								fileEntry.size = size;
								wakeUp(size);
							}).catch(function(e) {
								if (e === 'unwind' || (e && e.message === 'unwind')) return;
								console.error('[LazyFS] Error fetching file size for', path, ':', e);
								wakeUp(0);
							});
						});
						// A rewinding call runs its tail, so only clear our own tag.
						if (sizeWasRewinding && Module.LazyFS.pendingSyncRead === sizeKey) {
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
				var wasRewinding = asyncifyState === 2;
				Module.LazyFS.pendingSyncRead = pendingKey;
				var fetched = Asyncify.handleSleep(function(wakeUp) {
					Module.LazyFS.readFileData(path, offset, size)
						.then(function(data) {
							if (data) {
								if (data.length > size) {
									console.error('[LazyFS] OOB Write Risk! chunks:', data.length, 'vs req:', size);
									try { wakeUp(0); } catch(e) {}
									return;
								}
								// Check for valid pointer and heap
								if (bufferPtr < 0 || bufferPtr + data.length > HEAPU8.length) {
									console.error('[LazyFS] OOB FATAL: Ptr:', bufferPtr, 'Len:', data.length, 'Heap:', HEAPU8.length);
									try { wakeUp(0); } catch(e) {}
									return;
								}
								
								HEAPU8.set(data, bufferPtr);
								try { wakeUp(1); } catch(e) {}
							} else {
								console.error('[LazyFS] No data returned for', path);
								try { wakeUp(0); } catch(e) {}
							}
						}) // Use single .then() and no .catch() for the wakeUp path to act naturally
						.catch(function(e) {
							// If 'unwind', IGNORE IT completely.
							if (e === 'unwind' || (e && e.message === 'unwind')) return;
							
							console.error('[LazyFS] Error fetching data:', e);
							console.error(e.stack);
							try { wakeUp(0); } catch(e) {} 
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
