#include <LazyFS/LazyFileLoader.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>
#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#endif

namespace LazyFS
{

	LazyFileLoader::LazyFileLoader(const std::string& path) : path_(path)
	{
		cache_.reserve(CACHE_SIZE);
	}

	LazyFileLoader::~LazyFileLoader() = default;

	bool LazyFileLoader::open()
	{
		backend_ = std::make_unique<LazyFileBackend>();
		if (!backend_->open(path_))
		{
			return false;
		}

		const size_t file_size = backend_->size();
		chunk_slots_.assign(file_size / CHUNK_SIZE + 1, nullptr);
		return true;
	}

	size_t LazyFileLoader::size() const
	{
		return backend_ ? backend_->size() : 0;
	}

	void* LazyFileLoader::load_file(const std::string& path, size_t& out_size)
	{
		auto loader = new LazyFileLoader(path);
		if (!loader->open())
		{
			delete loader;
			out_size = 0;
			return nullptr;
		}
		out_size = loader->size();
		return static_cast<void*>(loader);
	}

	void LazyFileLoader::free_file(void* handle)
	{
		if (handle)
		{
			delete static_cast<LazyFileLoader*>(handle);
		}
	}

	const void* LazyFileLoader::get_contiguous_data(size_t offset, size_t size)
	{
		if (!backend_ || !backend_->is_open())
			return nullptr;

		size_t chunk_index = offset / CHUNK_SIZE;
		size_t chunk_offset = offset % CHUNK_SIZE;

		// Fast path: Data fits entirely within a single chunk. Serving an
		// already-resident chunk never suspends, so it stays safe for
		// re-entrant readers (they only mutate last_access).
		if (chunk_offset + size <= CHUNK_SIZE)
		{
			Chunk* chunk = get_or_load_chunk(chunk_index);
			if (!chunk)
				return nullptr;

			// Verify chunk data is large enough (EOF handling)
			if (chunk->data.size() < chunk_offset + size)
			{
				// If we request data past EOF inside a chunk, we could return nullptr or short read
				// But get_data_at implies we expect valid data.
				// For now, allow reading partial if it fits in chunk, but strict check is safer.
				if (chunk->data.size() <= chunk_offset)
					return nullptr;
			}
			return chunk->data.data() + chunk_offset;
		}

		// Slow path: Data spans across chunks. Stitch into a stable per-range
		// buffer so the returned pointer stays valid across later reads: the
		// node/audio/bitmap callers retain these pointers (or lengths parsed
		// from them), and the previous shared scratch buffer was overwritten
		// mid-use by any other spanning read, corrupting the nx tree.
		// A cached stitch is pure lookup; safe for re-entrant readers.
		const auto range = std::make_pair(offset, size);
		auto found = stitch_cache_.find(range);
		if (found != stitch_cache_.end())
		{
			return found->second->data();
		}


		// Refuse ranges beyond EOF instead of caching truncated data.
		if (size > backend_->size() || offset > backend_->size() - size)
		{
			std::cerr << "[LazyFS] Rejecting out-of-bounds range " << offset
			          << "+" << size << " (file " << backend_->size() << ")" << std::endl;
			return nullptr;
		}

		auto stitched = std::make_unique<std::vector<uint8_t>>(size);
		size_t bytes_copied = 0;
		size_t current_offset = offset;
		bool complete = true;

		while (bytes_copied < size)
		{
			size_t current_chunk_idx = current_offset / CHUNK_SIZE;
			size_t current_chunk_off = current_offset % CHUNK_SIZE;
			size_t bytes_left_in_chunk = CHUNK_SIZE - current_chunk_off;
			size_t to_copy = std::min(size - bytes_copied, bytes_left_in_chunk);

			Chunk* chunk = get_or_load_chunk(current_chunk_idx);
			if (!chunk)
				return nullptr;

			// Handle EOF within chunk
			if (chunk->data.size() < current_chunk_off + to_copy)
			{
				if (current_chunk_off >= chunk->data.size())
				{
					complete = false; // EOF reached before the range was filled
					break;
				}
				to_copy = chunk->data.size() - current_chunk_off;
			}

			std::memcpy(stitched->data() + bytes_copied, chunk->data.data() + current_chunk_off, to_copy);
			bytes_copied += to_copy;
			current_offset += to_copy;

			if (to_copy == 0)
			{
				complete = false; // Should not happen unless logic error or true EOF
				break;
			}
		}

		if (!complete)
		{
			std::cerr << "[LazyFS] Short stitch for range " << offset << "+"
			          << size << " (" << bytes_copied << " bytes); not caching" << std::endl;
			return nullptr;
		}

		const void* result = stitched->data();
		stitch_cache_bytes_ += size;
		stitch_cache_[range] = std::move(stitched);
		stitch_cache_order_.push_back(range);

		// FIFO eviction once the pinned total exceeds the cap. Keep at least
		// the freshly inserted range so a cache-cap run never returns nullptr
		// for the read in flight.
		while (stitch_cache_bytes_ > STITCH_CACHE_CAP_BYTES &&
		       stitch_cache_order_.size() > 1)
		{
			const auto oldest = stitch_cache_order_.front();
			stitch_cache_order_.pop_front();
			auto it = stitch_cache_.find(oldest);
			if (it != stitch_cache_.end())
			{
				stitch_cache_bytes_ -= it->second->size();
				stitch_cache_.erase(it);
				std::cerr << "[LazyFS] Stitch cache cap reached; evicted range "
				          << oldest.first << "+" << oldest.second << std::endl;
			}
		}

		return result;
	}

	void LazyFileLoader::prefetch_contiguous_data(size_t offset, size_t size) const
	{
#ifdef MS_PLATFORM_WASM
		EM_ASM({
			const path = UTF8ToString($0);
			if (Module.LazyFS && Module.LazyFS.prefetchFileRange) {
				Module.LazyFS.prefetchFileRange(path, $1, $2).catch(error => {
					console.error('[LazyFS] Bitmap prefetch failed for', path, error);
				});
			}
		}, path_.c_str(), static_cast<double>(offset), static_cast<double>(size));
#else
		(void)offset;
		(void)size;
#endif
	}

	void LazyFileLoader::prioritize_contiguous_data(size_t offset, size_t size) const
	{
#ifdef MS_PLATFORM_WASM
		EM_ASM({
			const path = UTF8ToString($0);
			if (Module.LazyFS && Module.LazyFS.requestPriorityFileRange) {
				Module.LazyFS.requestPriorityFileRange(path, $1, $2).catch(error => {
					console.error('[LazyFS] Priority bitmap request failed for', path, error);
				});
			}
		}, path_.c_str(), static_cast<double>(offset), static_cast<double>(size));
#else
		(void)offset;
		(void)size;
#endif
	}

	void LazyFileLoader::request_contiguous_data(size_t offset, size_t size) const
	{
#ifdef MS_PLATFORM_WASM
		EM_ASM({
			const path = UTF8ToString($0);
			if (Module.LazyFS && Module.LazyFS.requestForegroundFileRange) {
				Module.LazyFS.requestForegroundFileRange(path, $1, $2).catch(error => {
					console.error('[LazyFS] Foreground bitmap request failed for', path, error);
				});
			}
		}, path_.c_str(), static_cast<double>(offset), static_cast<double>(size));
#else
		(void)offset;
		(void)size;
#endif
	}

	bool LazyFileLoader::is_contiguous_data_ready(size_t offset, size_t size) const
	{
#ifdef MS_PLATFORM_WASM
		return EM_ASM_INT({
			const path = UTF8ToString($0);
			return Module.LazyFS && Module.LazyFS.isFileRangeResident &&
				Module.LazyFS.isFileRangeResident(path, $1, $2) ? 1 : 0;
		}, path_.c_str(), static_cast<double>(offset), static_cast<double>(size)) == 1;
#else
		(void)offset;
		(void)size;
		return true;
#endif
	}

	LazyFileLoader::Chunk* LazyFileLoader::get_or_load_chunk(size_t chunk_index)
	{
		access_counter_++;

		// Out-of-range chunk means an invalid offset for this file.
		if (chunk_index >= chunk_slots_.size())
		{
			return nullptr;
		}

		// O(1) cache hit path
		if (Chunk* cached = chunk_slots_[chunk_index])
		{
			cached->last_access = access_counter_;
			return cached;
		}

		// Not in cache: load it. Concurrency safety comes from the main tick
		// guard in Journey.cpp: at most one wasm stack is ever runnable, so
		// this suspension can never interleave with another loader call.
		auto new_chunk = std::make_unique<Chunk>();
		new_chunk->index = chunk_index;
		new_chunk->last_access = access_counter_;

		// Determine size to read
		size_t file_sum_size = backend_->size();
		size_t start_pos = chunk_index * CHUNK_SIZE;
		size_t end_pos = std::min(start_pos + CHUNK_SIZE, file_sum_size);
		size_t bytes_to_read = end_pos - start_pos;

		new_chunk->data.resize(bytes_to_read);

		if (!backend_->seek(start_pos))
		{
			std::cerr << "[LazyFS] Seek failed for chunk " << chunk_index << std::endl;
			return nullptr;
		}

		size_t read = backend_->read(new_chunk->data.data(), bytes_to_read);
		if (read != bytes_to_read)
		{
			std::cerr << "[LazyFS] Read failed for chunk " << chunk_index << std::endl;
			return nullptr;
		}

		// Add to cache (evict if needed)
		if (cache_.size() >= CACHE_SIZE)
		{
			auto lru_it = cache_.begin();
			for (auto it = cache_.begin(); it != cache_.end(); ++it)
			{
				if ((*it)->last_access < (*lru_it)->last_access)
				{
					lru_it = it;
				}
			}

			chunk_slots_[(*lru_it)->index] = nullptr;
			cache_.erase(lru_it);
		}

		Chunk* loaded = new_chunk.get();
		cache_.push_back(std::move(new_chunk));
		chunk_slots_[chunk_index] = loaded;
		return loaded;
	}

} // namespace LazyFS
