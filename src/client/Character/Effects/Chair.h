#pragma once

#include "../../Graphics/Animation.h"

namespace jrc
{
    /// Owns the Item.nx animation for an item chair without changing gameplay state.
    class Chair
    {
    public:
        /// Load a chair animation by item ID; zero clears the current chair.
        void set(int32_t item_id);
        void clear();
        void update();
        /// Draw on the side of the character selected by the NX z value.
        void draw(const DrawArgument& args, float alpha, bool below) const;
        int32_t get_item_id() const;

    private:
        Animation animation;
        int32_t item_id = 0;
        int32_t z = -1;
    };
}
