#include "Chair.h"

#include "nlnx/nx.hpp"
#include "nlnx/node.hpp"

#include <string>

namespace jrc
{
    void Chair::set(int32_t id)
    {
        if (id <= 0)
        {
            clear();
            return;
        }

        std::string group = "0" + std::to_string(id / 10000) + ".img";
        std::string item = "0" + std::to_string(id);
        nl::node effect = nl::nx::item["Install"][group][item]["effect"];
        animation = effect;
        z = static_cast<int32_t>(effect["z"].get_integer(-1));
        item_id = id;
    }

    void Chair::clear()
    {
        animation = {};
        item_id = 0;
        z = -1;
    }

    void Chair::update()
    {
        if (item_id > 0)
            animation.update();
    }

    void Chair::draw(const DrawArgument& args, float alpha, bool below) const
    {
        if (item_id > 0 && (z < 0) == below)
            animation.draw(args, alpha);
    }

    int32_t Chair::get_item_id() const
    {
        return item_id;
    }
}
