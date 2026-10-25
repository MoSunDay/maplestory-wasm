#include <cstddef>
// Geometry acceptance for the post-Chaos pet panel socket grid.
//
// Ground truth is the shipped UIWindow2.img/Equip/pet/backgrnd3 bitmap:
// 98x108 at origin (-11,-27), painted at window + (184,0).  Its three
// slot columns/rows are separated by 1px transparent slits, so the
// sockets reported by UIEquipInventoryNodes.h must land on the top-left
// corner of the matching cell.

#include "IO/UITypes/UIEquipInventoryNodes.h"

#include "nlnx/nx.hpp"
#include "nlnx/node.hpp"
#include "nlnx/bitmap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace
{
    int failures = 0;

    void expect(bool condition, const std::string& label)
    {
        std::cout << (condition ? "PASS " : "FAIL ") << label << '\n';
        if (!condition)
            ++failures;
    }

    std::vector<int> runs(const std::vector<int>& counts)
    {
        std::vector<int> result;
        int start = -1;
        for (size_t index = 0; index <= counts.size(); ++index)
        {
            const bool filled = index < counts.size() && counts[index] > 0;
            if (filled && start < 0)
                start = static_cast<int>(index);
            if (!filled && start >= 0)
            {
                result.push_back(start);
                start = -1;
            }
        }
        return result;
    }

    using Cells = std::array<std::array<jrc::Point<int16_t>, 3>, 3>;

    Cells grid_cells(jrc::Point<int16_t> panel_origin)
    {
        const nl::node art = nl::nx::ui["UIWindow2.img"]["Equip"]["pet"]["backgrnd3"];
        const nl::bitmap bitmap = art.get_bitmap();
        const auto* pixels = static_cast<const uint8_t*>(bitmap.data());
        std::vector<int> columns(bitmap.width(), 0);
        std::vector<int> rows(bitmap.height(), 0);
        for (uint32_t y = 0; y < bitmap.height(); ++y)
            for (uint32_t x = 0; x < bitmap.width(); ++x)
                if (pixels[(y * bitmap.width() + x) * 4 + 3] > 0)
                {
                    ++columns[x];
                    ++rows[y];
                }

        const auto origin = art["origin"].get_vector();
        const auto colruns = runs(columns);
        const auto rowruns = runs(rows);
        const jrc::Point<int16_t> grid_origin(
            panel_origin.x() + static_cast<int16_t>(-origin.first + colruns.at(0)),
            panel_origin.y() + static_cast<int16_t>(-origin.second + rowruns.at(0)));

        Cells cells{};
        static constexpr int16_t X[3] = { 0, 33, 66 };
        static constexpr int16_t Y[3] = { 0, 43, 76 };
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                cells[row][column] = grid_origin
                    + jrc::Point<int16_t>(X[column], Y[row]);
        return cells;
    }

    bool inside(jrc::Point<int16_t> point, jrc::Point<int16_t> top_left,
        int16_t width, int16_t height)
    {
        return point.x() >= top_left.x() && point.x() < top_left.x() + width
            && point.y() >= top_left.y() && point.y() < top_left.y() + height;
    }

    struct IconFootprint
    {
        int16_t left;
        int16_t top;
        int16_t width;
        int16_t height;
    };

    IconFootprint footprint(int32_t item_id, jrc::Point<int16_t> socket)
    {
        const nl::node icon = nl::nx::character["PetEquip"][
            std::string("0") + std::to_string(item_id) + ".img"]["info"]["icon"];
        const nl::bitmap bitmap = icon.get_bitmap();
        const auto origin = icon["origin"].get_vector();
        // draw_pet_item() anchors at socket + (0,32); Texture::draw places
        // the bitmap top-left at anchor - origin.
        return {
            static_cast<int16_t>(socket.x() - origin.first),
            static_cast<int16_t>(socket.y() + 32 - origin.second),
            static_cast<int16_t>(bitmap.width()),
            static_cast<int16_t>(bitmap.height())
        };
    }
}

int main(int argc, char** argv)
{
    using namespace jrc;

    const char* assets = argc > 1 ? argv[1] : "assets";
    if (chdir(assets) != 0)
    {
        std::cerr << "FAIL cannot enter assets directory " << assets << '\n';
        return 2;
    }
    nl::nx::load_all();

    expect(equipinv_nodes::post_chaos(), "post-Chaos UI.nx generation detected");

    const Point<int16_t> window(250, 150);
    expect(equipinv_nodes::pet_panel_origin() == Point<int16_t>(184, 0),
        "pet panel origin is window + (184,0)");

    // Sockets are window-relative, the bitmap anchor is panel-relative.
    const Cells cells = grid_cells(equipinv_nodes::pet_panel_origin());
    Cells abs{};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            abs[row][column] = cells[row][column] + window;
    std::cout << "INFO grid";
    for (const auto& row : cells)
        for (const auto& cell : row)
            std::cout << " (" << cell.x() << "," << cell.y() << ")";
    std::cout << '\n';

    const bool hp_on_cell = equipinv_nodes::pet_hp_pos() == cells[0][0];
    const bool mp_on_cell = equipinv_nodes::pet_mp_pos() == cells[0][1];
    expect(hp_on_cell, "auto HP socket is cell (0,0) top-left");
    expect(mp_on_cell, "auto MP socket is cell (1,0) top-left");

    const auto abilities = equipinv_nodes::pet_ability_positions();
    bool abilities_ok = true;
    for (size_t index = 0; index < abilities.size(); ++index)
        abilities_ok &= abilities[index] == cells[1 + index / 3][index % 3];
    expect(abilities_ok, "five ability sockets are cells (0,1)-(2,1) then (0,2)-(1,2)");

    const auto panel_bounds = equipinv_nodes::pet_panel_bounds(Point<int16_t>(0, 0));
    bool grid_in_panel = true;
    for (const auto& row : cells)
        for (const auto& cell : row)
            grid_in_panel &= panel_bounds.contains(cell)
                && panel_bounds.contains(cell + Point<int16_t>(31, 31));
    expect(grid_in_panel, "all nine grid cells lie inside the pet panel bounds");

    // Hitboxes: 32px squares anchored at the socket corner.
    const auto hp_bounds = equipinv_nodes::pet_hp_bounds(window);
    const auto mp_bounds = equipinv_nodes::pet_mp_bounds(window);
    expect(hp_bounds.l() == abs[0][0].x() && hp_bounds.t() == abs[0][0].y()
        && hp_bounds.r() == abs[0][0].x() + 32
        && hp_bounds.b() == abs[0][0].y() + 32,
        "auto HP hitbox is the 32px cell (0,0)");
    expect(mp_bounds.l() == abs[0][1].x() && mp_bounds.t() == abs[0][1].y()
        && mp_bounds.r() == abs[0][1].x() + 32
        && mp_bounds.b() == abs[0][1].y() + 32,
        "auto MP hitbox is the 32px cell (1,0)");
    expect(!hp_bounds.overlaps(mp_bounds), "HP and MP hitboxes are disjoint");

    const Rectangle<int16_t> hp_cell(abs[0][0], abs[0][0] + Point<int16_t>(32, 32));
    const Rectangle<int16_t> mp_cell(abs[0][1], abs[0][1] + Point<int16_t>(32, 32));
    expect(hp_cell.contains(Point<int16_t>(hp_bounds.l(), hp_bounds.t()))
        && hp_cell.contains(Point<int16_t>(hp_bounds.r(), hp_bounds.b())),
        "auto HP hitbox stays inside cell (0,0)");
    expect(mp_cell.contains(Point<int16_t>(mp_bounds.l(), mp_bounds.t()))
        && mp_cell.contains(Point<int16_t>(mp_bounds.r(), mp_bounds.b())),
        "auto MP hitbox stays inside cell (1,0)");

    const std::array<int32_t, 5> ABILITY_ITEMS = {
        1812000, 1812001, 1812002, 1812003, 1812004
    };
    bool footprints_ok = true;
    for (size_t index = 0; index < ABILITY_ITEMS.size(); ++index)
    {
        const IconFootprint icon = footprint(ABILITY_ITEMS[index], abilities[index]);
        const Point<int16_t> cell = cells[1 + index / 3][index % 3];
        const int16_t left = std::max<int16_t>(icon.left, cell.x());
        const int16_t top = std::max<int16_t>(icon.top, cell.y());
        const int16_t right = std::min<int16_t>(
            static_cast<int16_t>(icon.left + icon.width),
            static_cast<int16_t>(cell.x() + 32));
        const int16_t bottom = std::min<int16_t>(
            static_cast<int16_t>(icon.top + icon.height),
            static_cast<int16_t>(cell.y() + 32));
        const bool bleed = icon.left >= cell.x() - 1
            && icon.top >= cell.y() - 1
            && icon.left + icon.width <= cell.x() + 33
            && icon.top + icon.height <= cell.y() + 33;
        std::cout << "INFO icon " << ABILITY_ITEMS[index]
            << " footprint (" << icon.left << "," << icon.top << ") "
            << icon.width << "x" << icon.height << " cell ("
            << cell.x() << "," << cell.y() << ")"
            << " overlap " << (right - left) << "x" << (bottom - top) << '\n';
        footprints_ok &= bleed && right - left >= 28 && bottom - top >= 28;
    }
    expect(footprints_ok, "ability item icons stay inside their cell (+/-1px art bleed)");

    // Dragging: the nine cell centres must map to distinct targets.
    const auto target_at = [&](Point<int16_t> point) {
        return equipinv_nodes::pet_hp_bounds(window).contains(point) ? 1
             : equipinv_nodes::pet_mp_bounds(window).contains(point) ? 2
             : 0;
    };
    const int hp_target = target_at(abs[0][0] + Point<int16_t>(16, 16));
    const int mp_target = target_at(abs[0][1] + Point<int16_t>(16, 16));
    bool empty_targets = true;
    for (const auto& cell : { abs[0][2], abs[1][0], abs[1][1], abs[1][2],
             abs[2][0], abs[2][1], abs[2][2] })
        empty_targets &= target_at(cell + Point<int16_t>(16, 16)) == 0;
    expect(hp_target == 1 && mp_target == 2,
        "cell centres resolve to HP / MP drops");
    expect(empty_targets, "the other seven cell centres are not potion targets");

    expect(inside(Point<int16_t>(hp_bounds.l(), hp_bounds.t()), abs[0][0], 33, 33)
        && inside(Point<int16_t>(hp_bounds.r(), hp_bounds.b()), abs[0][0], 33, 33),
        "potion hitbox corners stay on the 32px cell plus its 1px slit");

    std::cout << (failures == 0 ? "OK pet equip layout" : "FAILED") << '\n';
    return failures == 0 ? 0 : 1;
}
