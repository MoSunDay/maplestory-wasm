#include "Gameplay/Loading/MapAssetGate.h"

#include <cassert>

int main()
{
    using namespace jrc::map_asset_gate;

    State state = begin(1000, 1);
    Progress empty;
    auto too_early = advance(state, empty, 1, 1249);
    assert(!too_early.ready);
    auto stable = advance(too_early.state, empty, 1, 1250);
    assert(stable.ready);

    Progress loading = { 1, 2, 0, 1 };
    state = begin(2000, 3);
    auto first_progress = advance(state, loading, 3, 2050);
    assert(first_progress.progress_changed);
    assert(!first_progress.ready);
    assert(first_progress.state.last_progress_ms == 2050);

    Progress resident = { 2, 2, 0, 1 };
    auto network_done = advance(first_progress.state, resident, 3, 2100);
    assert(network_done.progress_changed);
    assert(!network_done.ready);

    Progress prepared = { 2, 2, 1, 1 };
    auto gpu_done = advance(network_done.state, prepared, 3, 2200);
    assert(gpu_done.progress_changed);
    assert(!gpu_done.ready);

    auto ready = advance(gpu_done.state, prepared, 3, 2250);
    assert(ready.ready);

    auto new_asset = advance(ready.state, prepared, 4, 2251);
    assert(!new_asset.ready);
    assert(new_asset.state.stable_since_ms == 2251);

    state = begin(10000, 1);
    state.last_resident = loading.resident;
    state.last_total = loading.total;
    state.last_prepared = loading.prepared;
    state.last_required = loading.required;
    auto stalled = advance(state, loading, 1, 10000 + STALL_TIMEOUT_MS);
    assert(stalled.stalled);
    assert(stalled.state.failed);
    auto only_once = advance(stalled.state, loading, 1, 10001 + STALL_TIMEOUT_MS);
    assert(!only_once.stalled);

    auto retried = retry(stalled.state, 80000);
    assert(!retried.failed);
    assert(retried.last_progress_ms == 80000);
}
