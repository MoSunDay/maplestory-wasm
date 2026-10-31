#pragma once

#include <LazyFS/LazyFileBackend.h>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace LazyFS
{

	/**
	 * LazyFileLoader - Manages on-demand chunk loading for a file
	 *
	 * This class maintains an LRU cache of loaded chunks and provides access to
	 * file data via offsets. It replaces the memory-mapped file strategy on WASM.
	 */
	class LazyFileLoader
	{
	  public:
		LazyFileLoader(const std::string& path);
		~LazyFileLoader();

		LazyFileLoader(const LazyFileLoader&) = delete;
		LazyFileLoader& operator=(const LazyFileLoader&) = delete;

		/**
		 * Open the file.
		 * @return true if successful
		 */
		bool open();

		/**
		 * Get the total file size.
		 */
		size_t size() const;

		/**
		 * Get a pointer to data at the specified offset.
		 * This ensures the relevant chunk is loaded in memory.
		 * Note: single-chunk pointers live until LRU eviction; cross-chunk
		 * ranges are pinned in stitch_cache_ and stay valid indefinitely.
		 *
		 * @param offset Byte offset in the file
		 * @return Pointer to data, or nullptr if failed
		 */
		template <typename T> const T* get_data_at(size_t offset)
		{
			return static_cast<const T*>(get_contiguous_data(offset, sizeof(T)));
		}

		/**
		 * Get a pointer to contiguous data of a specific size.
		 * Handles cross-chunk stitching if necessary.
		 */
		const void* get_contiguous_data(size_t offset, size_t size);

		// Start a browser-side fetch without suspending the WASM main loop.
		void prefetch_contiguous_data(size_t offset, size_t size) const;

		// Promote a missing range without blocking normal gameplay.
		void prioritize_contiguous_data(size_t offset, size_t size) const;

		// Promote a missing range to a user-visible foreground asset request.
		void request_contiguous_data(size_t offset, size_t size) const;

		// Return whether every required chunk is already in browser memory.
		bool is_contiguous_data_ready(size_t offset, size_t size) const;

		/**
		 * Static helper to load file (creates a Loader instance)
		 * @param path Path to file
		 * @param out_size Output for file size
		 * @return opaque pointer to LazyFileLoader instance
		 */
		static void* load_file(const std::string& path, size_t& out_size);

		/**
		 * Static helper to free file (deletes Loader instance)
		 */
		static void free_file(void* handle);

	  private:
		// Maximum chunks to keep in memory (160 * 2MB = ~320MB per file)
		static constexpr size_t CACHE_SIZE = 160;
		// Hard ceiling for pinned cross-chunk stitch buffers. Eviction only
		// kicks in far above any realistic working set; see stitch_cache_.
		static constexpr size_t STITCH_CACHE_CAP_BYTES = 384u * 1024u * 1024u;
		static constexpr size_t CHUNK_SIZE = LazyFileBackend::CHUNK_SIZE;

		struct Chunk
		{
			size_t index;
			std::vector<uint8_t> data;
			uint64_t last_access;
		};

		std::string path_;
		std::unique_ptr<LazyFileBackend> backend_;
		std::vector<std::unique_ptr<Chunk>> cache_;
		// Direct chunk-index lookup table (one slot per chunk in the file);
		// turns cache hits into O(1) instead of a linear scan over cache_.
		std::vector<Chunk*> chunk_slots_;
		uint64_t access_counter_ = 0;

		// Stable per-range buffers for cross-chunk reads. The previous single
		// shared scratch buffer was overwritten by every subsequent spanning
		// read while callers (node tables, parsed audio lengths) still held
		// pointers into it, corrupting the nx tree and trapping wasm with
		// out-of-bounds accesses. Each distinct range is stitched once and
		// pinned; FIFO eviction guards against runaway growth.
		std::map<std::pair<size_t, size_t>,
		         std::unique_ptr<std::vector<uint8_t>>> stitch_cache_;
		std::deque<std::pair<size_t, size_t>> stitch_cache_order_;
		size_t stitch_cache_bytes_ = 0;

		// const void* get_chunk_data(size_t offset, size_t size); // Replaced by get_contiguous_data implementation
		Chunk* get_or_load_chunk(size_t chunk_index);
	};

} // namespace LazyFS
