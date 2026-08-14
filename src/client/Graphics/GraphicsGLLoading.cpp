#include "GraphicsGL.h"

#include <chrono>

namespace jrc
{
    void GraphicsGL::addtobitmapbatch(const nl::bitmap& bmp, BitmapLoadClass load_class)
    {
        const size_t id = bmp.id();
        if (bitmap_batch.generation == 0 || id == 0)
        {
            return;
        }

        bool changed = bitmap_batch.bitmaps.emplace(id, bmp).second;
        if (bitmap_loading::is_map_required(load_class))
        {
            changed = bitmap_batch.required_bitmap_ids.emplace(id).second || changed;
        }
        if (changed)
        {
            bitmap_batch.revision++;
        }
    }

    uint64_t GraphicsGL::beginbitmapbatch()
    {
        bitmap_batch = {};
        bitmap_batch.generation = ++next_bitmap_batch_generation;
        return bitmap_batch.generation;
    }

    GraphicsGL::BitmapBatchProgress GraphicsGL::bitmapbatchprogress(uint64_t generation) const
    {
        BitmapBatchProgress progress;
        if (generation == 0 || generation != bitmap_batch.generation)
        {
            return progress;
        }

        progress.total = bitmap_batch.bitmaps.size();
        progress.required = bitmap_batch.required_bitmap_ids.size();
        for (const auto& entry : bitmap_batch.bitmaps)
        {
            if (entry.second.data_ready())
            {
                progress.resident++;
            }
            if (bitmap_batch.required_bitmap_ids.count(entry.first) > 0 && hasbitmap(entry.second))
            {
                progress.prepared++;
            }
        }
        return progress;
    }

    uint64_t GraphicsGL::bitmapbatchrevision(uint64_t generation) const
    {
        return generation == bitmap_batch.generation ? bitmap_batch.revision : 0;
    }

    void GraphicsGL::retrybitmapbatch(uint64_t generation)
    {
        if (generation != bitmap_batch.generation)
        {
            return;
        }

        for (const auto& entry : bitmap_batch.bitmaps)
        {
            const bool required = bitmap_batch.required_bitmap_ids.count(entry.first) > 0;
            if (!entry.second.data_ready())
            {
                entry.second.prefetch();
                if (required)
                {
                    entry.second.request();
                }
            }
            else if (required && !hasbitmap(entry.second))
            {
                queuebitmap(entry.second, BitmapLoadClass::BLOCKING_VISIBLE);
            }
        }
    }

    void GraphicsGL::endbitmapbatch(uint64_t generation)
    {
        if (generation == bitmap_batch.generation)
        {
            bitmap_batch = {};
        }
    }

    bool GraphicsGL::hasblockingbitmaps() const
    {
#ifdef MS_PLATFORM_WASM
        return !pending_blocking_bitmap_ids.empty();
#else
        return false;
#endif
    }

    void GraphicsGL::preparebitmaps(uint32_t budget_ms)
    {
#ifdef MS_PLATFORM_WASM
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);
        auto prepare_queue = [&](std::deque<nl::bitmap>& queue, size_t candidates) {
            bool prepared = false;
            while (candidates-- > 0 && !queue.empty())
            {
                if (std::chrono::steady_clock::now() >= deadline)
                {
                    break;
                }

                nl::bitmap bmp = queue.front();
                queue.pop_front();
                if (hasbitmap(bmp))
                {
                    pending_bitmap_ids.erase(bmp.id());
                    pending_blocking_bitmap_ids.erase(bmp.id());
                }
                else if (bmp.data_ready())
                {
                    addbitmap(bmp);
                    pending_bitmap_ids.erase(bmp.id());
                    pending_blocking_bitmap_ids.erase(bmp.id());
                    prepared = true;
                }
                else
                {
                    queue.push_back(bmp);
                }
            }
            return prepared;
        };

        prepare_queue(pending_priority_bitmaps, pending_priority_bitmaps.size());
        if (pending_priority_bitmaps.empty())
        {
            prepare_queue(pending_bitmaps, pending_bitmaps.size());
        }
#else
        (void)budget_ms;
#endif
    }
}
