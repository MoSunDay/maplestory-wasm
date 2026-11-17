//////////////////////////////////////////////////////////////////////////////
// Initialization for the character select screen.
//
// UICharSelect::initialize_ui() picks one of two art generations:
//   * 079MAX4 bitmap screen (CharSelect079 + frame079 + Back/login079)
//   * 2023 flat fallback used by UI.nx files without the injected nodes.
// Keeping both builders here leaves the update/input translation unit free
// of layout detail; draw code lives in UICharSelectDraw.cpp.
//////////////////////////////////////////////////////////////////////////////
#include "UICharSelect.h"

#include "../CharacterSelect/CharSelectModernPanels.h"
#include "../UiAssetVersion.h"
#include "../UILayout.h"
#include "UICharSelectNodes.h"

#include "../Components/AreaButton.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"

#include "../../Character/Job.h"
#include "../../Console.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace
    {
        // Map.nx stores the 38 backdrop canvases in name-sorted order, but
        // the original WZ draws them in this layer order. Numeric sorting is
        // close; the XML order is the exact one that produced the known
        // reference composite.
        constexpr const char* BACKDROP_LAYERS[] = {
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
            "16", "17", "18", "15", "12", "13", "14", "28",
            "19", "20", "21", "22", "23", "24", "25", "26", "27",
            "29", "30", "31", "32", "33", "34", "35", "36", "37"
        };

        const char* rank_move_icon(int8_t movement)
        {
            switch (movement)
            {
            case '+':
                return "up";
            case '-':
                return "down";
            default:
                return "same";
            }
        }

        nl::node job_icon_node(int32_t jobid)
        {
            if (jobid == 0)
            {
                return charselect_nodes::job_icon(0);
            }

            // 079 ships icons for the five pre-pirate branches only:
            // beginner, warrior, magician, bowman and thief.
            const int32_t branch = jobid / 100;
            if (branch >= 1 && branch <= 4)
            {
                return charselect_nodes::job_icon(static_cast<size_t>(branch));
            }

            return {};
        }
    }

    Text& UICharSelect::level_label()
    {
        static Text label{ Text::A11M, Text::CENTER, Text::WHITE };
        return label;
    }

    void UICharSelect::initialize_ui()
    {
        bitmap_ui = charselect_nodes::has_bitmap_ui();
        if (bitmap_ui)
        {
            initialize_bitmap_ui();
        }
        else
        {
            initialize_modern_ui();
        }
    }

    void UICharSelect::initialize_bitmap_ui()
    {
        namespace layout = charselect_079;

        nl::node charselect = charselect_nodes::root079();
        nl::node backdrop   = charselect_nodes::back079();

        for (const char* layer_name : BACKDROP_LAYERS)
        {
            nl::node layer = backdrop["back"][layer_name];
            if (layer)
            {
                sprites.emplace_back(layer, layout::BACKDROP_POS);
            }
        }

        if (charselect_nodes::frame079())
        {
            sprites.emplace_back(charselect_nodes::frame079(), layout::FRAME_POS);
        }
        else
        {
            Console::get().print("[UICharSelect] Missing Login.img/frame079.");
        }

        info_has_rank_rows = charselect_nodes::has_long_info();
        sprites.emplace_back(charselect_nodes::info_board(), layout::INFO_ANCHOR);

        // World banner board sits above the frame interior; it is only
        // present in UI.nx variants produced by the 079 injection step.
        if (charselect_nodes::select_world079())
        {
            sprites.emplace_back(
                charselect_nodes::select_world079(), layout::WORLD_BANNER_POS);
        }
        else
        {
            Console::get().print("[UICharSelect] Missing Login.img/selectWorld079.");
        }

        slotcountpos   = layout::COUNT_POS;
        slotcountlabel = { Text::A11M, Text::RIGHT, Text::WHITE };

        emptyslot    = charselect_nodes::empty_slot();
        charplatform = charselect_nodes::platform();
        nametag      = charselect_nodes::name_tag();

        buttons[BT_SELECTCHAR] = std::make_unique<MapleButton>(
            charselect["BtSelect"], layout::START_POS);
        buttons[BT_CREATECHAR] = std::make_unique<MapleButton>(
            charselect["BtNew"], layout::CREATE_POS);
        buttons[BT_DELETECHAR] = std::make_unique<MapleButton>(
            charselect["BtDelete"], layout::DELETE_POS);

        // pageL/pageR are plain two-frame pairs in 079; state 0 is the
        // resting arrow and state 1 the brighter hover arrow. Each state is
        // a numbered frame container, so resolve the inner bitmap.
        buttons[BT_PAGELEFT] = std::make_unique<TwoSpriteButton>(
            charselect_nodes::page_arrow(false, 0),
            charselect_nodes::page_arrow(false, 1),
            layout::PAGE_LEFT_POS);
        buttons[BT_PAGERIGHT] = std::make_unique<TwoSpriteButton>(
            charselect_nodes::page_arrow(true, 0),
            charselect_nodes::page_arrow(true, 1),
            layout::PAGE_RIGHT_POS);

        for (uint8_t i = 0; i < PAGESIZE; ++i)
        {
            buttons[BT_CHAR0 + i] = std::make_unique<AreaButton>(
                layout::slot_hotzone_pos(i),
                layout::slot_hotzone_dim()
            );
        }

        bitmapname = { Text::A12M, Text::CENTER, Text::WHITE };
        jobvalue   = { Text::A11M, Text::LEFT,   Text::BLACK };
        levelvalue = { Text::A11M, Text::RIGHT,  Text::BLACK };
        famevalue  = { Text::A11M, Text::RIGHT,  Text::BLACK };
        strvalue   = { Text::A11M, Text::RIGHT,  Text::BLACK };
        intvalue   = { Text::A11M, Text::RIGHT,  Text::BLACK };
        dexvalue   = { Text::A11M, Text::RIGHT,  Text::BLACK };
        lukvalue   = { Text::A11M, Text::RIGHT,  Text::BLACK };
        rankvalue  = { Text::A11M, Text::CENTER, Text::BLACK };
        jobrankvalue = { Text::A11M, Text::CENTER, Text::BLACK };

        nametags.reserve(characters.size());
        for (const auto& entry : characters)
        {
            nametags.emplace_back(nametag, Text::A13M, Text::WHITE, entry.stats.name);
        }

        update_counts();
        update_selection();
    }

    void UICharSelect::initialize_modern_ui()
    {
        namespace layout = charselect_modern;

        nl::node back       = nl::nx::map["Back"]["login.img"]["back"];
        nl::node title      = nl::nx::ui["Login.img"]["Title"];
        nl::node common     = nl::nx::ui["Login.img"]["Common"];
        nl::node charselect = charselect_nodes::root();

        // Backdrop and frame are shared login-chain art; the 2023 chrome
        // is drawn procedurally on top of them.
        if (back["11"])
        {
            sprites.emplace_back(back["11"], Point<int16_t>(360, 313));
        }
        else if (back["0"])
        {
            sprites.emplace_back(back["0"], Point<int16_t>(400, 300));
        }

        // Keep legacy world-select art as an optional overlay.
        if (title["worldsel"])
        {
            sprites.emplace_back(title["worldsel"]);
        }

        if (common["frame"])
        {
            sprites.emplace_back(common["frame"], Point<int16_t>(400, login_frame_y()));
        }
        else
        {
            Console::get().print("[UICharSelect] Missing Login.img/Common/frame for this UI.nx variant.");
        }

        // NX accent: the world banner sits under the flat top bar.
        sprites.emplace_back(common["selectWorld"], layout::WORLD_BANNER_POS);
        if (charselect["selectedWorld"])
        {
            sprites.emplace_back(charselect["selectedWorld"]["icon"]["15"], layout::WORLD_BANNER_POS);
            sprites.emplace_back(charselect["selectedWorld"]["name"]["15"], layout::WORLD_BANNER_POS);
            sprites.emplace_back(charselect["selectedWorld"]["ch"][channel_id], layout::WORLD_BANNER_POS);
        }
        else
        {
            Console::get().print("[UICharSelect] Missing Login.img/CharSelect/selectedWorld for this UI.nx variant.");
        }

        // NX accent: the slot count window. Without it the count label
        // falls back into the top bar.
        if (charselect["charSlot"])
        {
            nl::node charslot = charselect["charSlot"];
            Point<int16_t> charslotpos = charslot["pos"];
            sprites.emplace_back(charslot["0"], charslotpos);
            slotcountpos = charslotpos + Point<int16_t>(charslot["numberpos"]);
        }
        else
        {
            Console::get().print("[UICharSelect] Missing Login.img/CharSelect/charSlot for this UI.nx variant.");
            slotcountpos = layout::COUNT_FALLBACK_POS;
        }
        slotcountlabel = { Text::A11M, Text::RIGHT, Text::WHITE };

        // Use the 2023 nodes directly: root079() may be present without a
        // complete bitmap backdrop and must not leak into the fallback.
        emptyslot    = charselect["buyCharacter"];
        charplatform = charselect["character"]["0"];
        nametag      = charselect["nameTag"];

        buttons[BT_SELECTCHAR] = std::make_unique<MapleButton>(
            charselect["BtSelect"], layout::START_POS);
        buttons[BT_CREATECHAR] = std::make_unique<MapleButton>(
            charselect["BtNew"], layout::CREATE_POS);
        buttons[BT_DELETECHAR] = std::make_unique<MapleButton>(
            charselect["BtDelete"], layout::DELETE_POS);
        buttons[BT_PAGELEFT]   = std::make_unique<MapleButton>(
            charselect["pageL"], layout::PAGE_ARROW_LEFT_POS);
        buttons[BT_PAGERIGHT]  = std::make_unique<MapleButton>(
            charselect["pageR"], layout::PAGE_ARROW_RIGHT_POS);

        for (uint8_t i = 0; i < PAGESIZE; ++i)
        {
            buttons[BT_CHAR0 + i] = std::make_unique<AreaButton>(
                layout::slot_hotzone_pos(i),
                layout::slot_hotzone_dim()
            );
        }

        levelset = { charselect["lv"], Charset::CENTER };
        if (!charselect_nodes::has_level_charset())
        {
            Console::get().print("[UICharSelect] Missing Login.img/CharSelect/lv charset; falling back to a plain level label.");
        }

        namelabel = { Text::A18M, Text::CENTER };
        for (size_t i = 0; i < NUM_LABELS; ++i)
        {
            infolabels[i] = { Text::A11M, Text::RIGHT };
        }

        nametags.reserve(characters.size());
        for (const auto& entry : characters)
        {
            nametags.emplace_back(nametag, Text::A13M, Text::WHITE, entry.stats.name);
        }

        update_counts();
        update_selection();
    }

    void UICharSelect::update_bitmap_values()
    {
        if (!bitmap_ui ||
            selected_absolute >= charcount_absolute ||
            selected_absolute >= characters.size())
        {
            return;
        }

        const StatsEntry& stats = characters[selected_absolute].stats;
        const int32_t jobid = stats.stats[Maplestat::JOB];

        bitmapname.change_text(stats.name);
        jobvalue.change_text(Job(static_cast<uint16_t>(jobid)).get_name());
        levelvalue.change_text(std::to_string(stats.stats[Maplestat::LEVEL]));
        famevalue.change_text(std::to_string(stats.stats[Maplestat::FAME]));
        strvalue.change_text(std::to_string(stats.stats[Maplestat::STR]));
        intvalue.change_text(std::to_string(stats.stats[Maplestat::INT]));
        dexvalue.change_text(std::to_string(stats.stats[Maplestat::DEX]));
        lukvalue.change_text(std::to_string(stats.stats[Maplestat::LUK]));

        const auto rank_text = [](const std::pair<int32_t, int8_t>& rank) {
            return rank.first < 0 ? std::string("-") : std::to_string(rank.first);
        };
        rankvalue.change_text(rank_text(stats.rank));
        jobrankvalue.change_text(rank_text(stats.jobrank));

        jobicon = job_icon_node(jobid);

        nl::node icon = charselect_nodes::root079()["icon"];
        rankmoveicon    = icon[rank_move_icon(stats.rank.second)];
        jobrankmoveicon = icon[rank_move_icon(stats.jobrank.second)];
    }
}
