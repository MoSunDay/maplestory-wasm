#pragma once

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <cstdint>

namespace jrc
{
    // Login.img/Common/frame differs between UI.nx generations: the
    // post-Chaos frame the upstream coordinates were tuned against is
    // centered 10px lower than the pre-Chaos frame. Detect the generation
    // from the loaded asset set (same marker NxFiles::init uses) and return
    // the frame center Y that matches the frame actually shipped.
    inline int16_t login_frame_y()
    {
        const bool post_chaos = static_cast<bool>(
            nl::nx::ui["Login.img"]["WorldSelect"]["BtChannel"]);
        return post_chaos ? 300 : 290;
    }
}
