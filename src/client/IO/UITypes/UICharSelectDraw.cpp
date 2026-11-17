//////////////////////////////////////////////////////////////////////////////
// Drawing for the character select screen.
//
// Two art generations share this window: the 079MAX4 bitmap screen
// (CharSelect079 + frame079 + Back/login079) and the older 2023 flat
// fallback. Geometry lives in CharSelect079Layout.h / CharSelectModernLayout.h;
// this translation unit only turns the selected mode into draw calls.
//////////////////////////////////////////////////////////////////////////////
#include "UICharSelect.h"

#include "../CharacterSelect/CharSelectModernPanels.h"
#include "../UILayout.h"
#include "UICharSelectNodes.h"

namespace jrc
{
    void UICharSelect::draw(float alpha) const
    {
        if (!ui_initialized)
        {
            UIElement::draw(alpha);
            return;
        }

        if (bitmap_ui)
        {
            draw_bitmap(alpha);
        }
        else
        {
            draw_modern(alpha);
        }
    }

    void UICharSelect::draw_bitmap(float alpha) const
    {
        namespace layout = charselect_079;

        // Backdrop layers, frame and the 079 info card all live in the
        // sprite list; they were queued in that order during init.
        draw_sprites(alpha);
        draw_buttons(alpha);

        slotcountlabel.draw(layout::COUNT_POS + position);

        for (uint8_t i = 0; i < charcount_relative; ++i)
        {
            const Point<int16_t> charpos = layout::char_slot_pos(i) + position;
            const uint8_t index = i + page * PAGESIZE;
            charplatform.draw(charpos, alpha);
            draw_preview(index, charpos, alpha);
            nametags[index].draw(charpos);
        }

        for (uint8_t i = charcount_relative; i < slots_relative; ++i)
        {
            const Point<int16_t> slotpos = layout::char_slot_pos(i) + position;
            charplatform.draw(slotpos, alpha);
            emptyslot.draw(slotpos, alpha);
        }

        if (selected_absolute >= charcount_absolute ||
            selected_absolute >= characters.size())
        {
            return;
        }

        bitmapname.draw(layout::INFO_NAME_POS + position);

        // The job row has a baked 10px-tall label bitmap in 079; the text
        // fallback only runs when that node is missing.
        if (jobicon.is_valid())
        {
            jobicon.draw(layout::INFO_JOB_ICON_POS + position);
        }
        else
        {
            jobvalue.draw(layout::INFO_JOB_POS + position);
        }

        levelvalue.draw(layout::INFO_LEVEL_POS + position);
        famevalue.draw(layout::INFO_FAME_POS + position);
        strvalue.draw(layout::INFO_STR_POS + position);
        intvalue.draw(layout::INFO_INT_POS + position);
        dexvalue.draw(layout::INFO_DEX_POS + position);
        lukvalue.draw(layout::INFO_LUK_POS + position);

        if (info_has_rank_rows)
        {
            // The rank values are centred in their light value bands; the
            // movement arrow is placed from the measured text width.
            const Point<int16_t> rank_pos = layout::INFO_RANK_POS + position;
            rankvalue.draw(rank_pos);
            if (rankmoveicon.is_valid())
            {
                const Point<int16_t> icon = layout::rank_icon_pos(
                    rank_pos, rankvalue.width(), rankmoveicon.get_dimensions().y());
                rankmoveicon.draw(DrawArgument(icon, alpha));
            }

            const Point<int16_t> jobrank_pos =
                layout::INFO_JOBRANK_POS + position;
            jobrankvalue.draw(jobrank_pos);
            if (jobrankmoveicon.is_valid())
            {
                const Point<int16_t> icon = layout::rank_icon_pos(
                    jobrank_pos,
                    jobrankvalue.width(),
                    jobrankmoveicon.get_dimensions().y());
                jobrankmoveicon.draw(DrawArgument(icon, alpha));
            }
        }
    }

    void UICharSelect::draw_modern(float alpha) const
    {
        namespace layout = charselect_modern;

        draw_sprites(alpha);
        charselect_panels::draw_chrome(position);
        charselect_panels::draw_start_backing(
            position, buttons.at(BT_SELECTCHAR)->get_state());
        charselect_panels::draw_footer_backing(
            position,
            buttons.at(BT_CREATECHAR)->get_state(),
            buttons.at(BT_DELETECHAR)->get_state()
        );
        draw_buttons(alpha);

        slotcountlabel.draw(slotcountpos + position);

        for (uint8_t i = 0; i < charcount_relative; ++i)
        {
            const Point<int16_t> charpos = layout::char_slot_pos(i) + position;
            const uint8_t index = i + page * PAGESIZE;
            charselect_panels::draw_slot_highlight(
                position, i, i == selected_relative,
                buttons.at(static_cast<uint16_t>(BT_CHAR0 + i))->get_state()
            );
            charplatform.draw(charpos + layout::PLATFORM_OFFSET, alpha);
            draw_preview(index, charpos, alpha);
            nametags[index].draw(charpos + layout::NAMETAG_OFFSET);
        }

        for (uint8_t i = charcount_relative; i < slots_relative; ++i)
        {
            const Point<int16_t> slotpos = layout::char_slot_pos(i) + position;
            charplatform.draw(slotpos + layout::PLATFORM_OFFSET, alpha);
            emptyslot.draw(slotpos, alpha);
        }

        if (selected_absolute >= charcount_absolute ||
            selected_absolute >= characters.size())
        {
            return;
        }

        const StatsEntry& stats = characters[selected_absolute].stats;

        if (charselect_nodes::has_level_charset())
        {
            const std::string levelstr =
                std::to_string(stats.stats[Maplestat::LEVEL]);
            const int16_t lvx = levelset.draw(
                levelstr, layout::CARD_LV_POS + position);
            levelset.draw(
                'l',
                layout::CARD_LV_POS +
                    Point<int16_t>(-8 - lvx / 2, 0) + position);
        }
        else
        {
            Text& lvlabel = level_label();
            lvlabel.change_text(
                "Lv. " + std::to_string(stats.stats[Maplestat::LEVEL]));
            lvlabel.draw(layout::CARD_LV_POS + position);
        }

        namelabel.draw(layout::CARD_NAME_POS + position);

        for (size_t i = 0; i < NUM_LABELS; ++i)
        {
            infolabels[i].draw(layout::card_label_pos(i) + position);
        }
    }
}
