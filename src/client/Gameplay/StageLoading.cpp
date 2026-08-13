#include "Stage.h"

#include "Loading/MapAssetGate.h"

#include "../Audio/Audio.h"
#include "../Graphics/GraphicsGL.h"
#ifdef MS_PLATFORM_WASM
#include "../LazyFS/LazyFS.h"
#include <emscripten.h>
#endif

#include <chrono>
#include <utility>

namespace jrc
{
    namespace
    {
        uint64_t monotonic_millis()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
        }

        void show_map_loading(size_t completed, size_t total)
        {
#ifdef MS_PLATFORM_WASM
            EM_ASM({
                window.MapleAssetLoading?.begin('map', {
                    message: '正在加载当前地图素材...',
                    completed: $0,
                    total: $1
                });
            }, static_cast<double>(completed), static_cast<double>(total));
#else
            (void)completed;
            (void)total;
#endif
        }

        void hide_map_loading()
        {
#ifdef MS_PLATFORM_WASM
            EM_ASM({ window.MapleAssetLoading?.end('map'); });
#endif
        }

        void fail_map_loading()
        {
#ifdef MS_PLATFORM_WASM
            EM_ASM({
                window.MapleAssetLoading?.fail('map', function () {
                    Module.ccall('msmap_retry', null, [], []);
                });
            });
#endif
        }

        void reset_asset_connection()
        {
#ifdef MS_PLATFORM_WASM
            EM_ASM({ Module.LazyFS?.resetConnectionFailure?.(); });
#endif
        }
    }

    void Stage::begin_loading(std::function<void()> onready)
    {
        asset_generation = GraphicsGL::get().beginbitmapbatch();
        loading_ready = std::move(onready);
        state = LOADING;
        asset_gate = map_asset_gate::begin(
            monotonic_millis(),
            GraphicsGL::get().bitmapbatchrevision(asset_generation)
        );
        show_map_loading(0, 0);
    }

    void Stage::end_loading()
    {
        GraphicsGL::get().endbitmapbatch(asset_generation);
        hide_map_loading();
        asset_generation = 0;
        asset_gate = {};
        loading_ready = {};
    }

    void Stage::update_loading()
    {
        // Placement packets arrive while the server still treats the player as
        // transitioning. Instantiate them, but do not advance simulation.
        reactors.instantiate_spawns(physics);
        npcs.instantiate_spawns(physics);
        mobs.instantiate_spawns();
        chars.instantiate_spawns();
        drops.instantiate_spawns();

        const auto graphics = GraphicsGL::get().bitmapbatchprogress(asset_generation);
        const map_asset_gate::Progress progress = {
            graphics.resident,
            graphics.total,
            graphics.prepared,
            graphics.required
        };
        const auto step = map_asset_gate::advance(
            asset_gate,
            progress,
            GraphicsGL::get().bitmapbatchrevision(asset_generation),
            monotonic_millis()
        );
        asset_gate = step.state;
        if (step.progress_changed)
        {
            report_loading_progress();
        }
        if (step.ready)
        {
            finish_loading();
        }
        else if (step.stalled)
        {
            fail_map_loading();
        }
    }

    void Stage::finish_loading()
    {
        GraphicsGL::get().endbitmapbatch(asset_generation);
        asset_generation = 0;
        asset_gate = {};
        state = ACTIVE;
        Music(mapinfo.get_bgm()).play();
        hide_map_loading();
#ifdef MS_PLATFORM_WASM
        LazyFS::StartItemAssetPreload();
#endif
        if (loading_ready)
        {
            auto ready = std::move(loading_ready);
            loading_ready = {};
            ready();
        }
    }

    void Stage::report_loading_progress() const
    {
        const auto progress = GraphicsGL::get().bitmapbatchprogress(asset_generation);
        show_map_loading(
            progress.resident + progress.prepared,
            progress.total + progress.required
        );
    }

    bool Stage::is_loading() const
    {
        return state == LOADING;
    }

    void Stage::retry_loading()
    {
        if (state != LOADING)
        {
            return;
        }

        asset_gate = map_asset_gate::retry(asset_gate, monotonic_millis());
        reset_asset_connection();
        Music(mapinfo.get_bgm()).prepare();
        GraphicsGL::get().retrybitmapbatch(asset_generation);
        report_loading_progress();
    }
}
