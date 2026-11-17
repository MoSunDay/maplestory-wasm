//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../UIElement.h"
#include "../CharacterSelect/CharSelect079Layout.h"
#include "../CharacterSelect/CharSelectModernLayout.h"

#include "../Components/Charset.h"
#include "../Components/Nametag.h"
#include "../CharacterSelect/PreviewLoadPolicy.h"

#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Sprite.h"
#include "../../Graphics/Texture.h"
#include "../../Net/Login.h"

#include <exception>
#include <optional>

namespace jrc
{
    // The character selection screen.
    class UICharSelect : public UIElement
    {
    public:
        static constexpr Type TYPE = CHARSELECT;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        UICharSelect(std::vector<CharEntry> characters,
                     uint8_t slots,
                     uint8_t channel_id,
                     int8_t pic);

        void draw(float alpha) const override;
        void update() override;
        void update_screen(int16_t width, int16_t height) override;
        Button::State button_pressed(uint16_t id) override;
        void doubleclick(Point<int16_t> pos) override;

        void add_character(CharEntry&& character);
        void remove_char(int32_t cid);
        bool handle_pic_failure(int8_t message, int32_t cid = -1);
        void clear_pending_pic_request();

        const CharEntry& get_character(int32_t cid);

        // True when the screen is driven by the injected 079MAX4 bitmap
        // art. Browser layout checks read this through ImeBridge to pick
        // between the bitmap and the flat contract.
        bool uses_bitmap_ui() const { return bitmap_ui; }

    private:
        enum class PicAction
        {
            NONE,
            REGISTER,
            SELECT,
            DELETE
        };

        void dispatch_pic_action(PicAction action, int32_t cid, const std::string& pic, bool auto_submit);
        void prompt_pic_entry(PicAction action, int32_t cid);
        void send_selection();
        void send_deletion();
        void initialize_ui();
        void initialize_bitmap_ui();
        void initialize_modern_ui();
        static Text& level_label();
        void draw_bitmap(float alpha) const;
        void draw_modern(float alpha) const;
        void update_bitmap_values();
        void load_next_preview();
        void resolve_preview(size_t index, character_preview::LoadState state);
        void complete_previews_if_ready();
        void fail_preview(size_t index, const std::string& message);
        void fail_ui(const std::exception& error);
        void fail_ui(const std::string& message);
        void draw_preview(size_t index, Point<int16_t> position, float alpha) const;
        void update_selection();
        void update_counts();
        std::string get_label_string(size_t label) const;

        enum Buttons
        {
            BT_CREATECHAR,
            BT_DELETECHAR,
            BT_SELECTCHAR,
            BT_PAGELEFT,
            BT_PAGERIGHT,
            BT_CHAR0
        };

        static constexpr uint8_t PAGESIZE = charselect_modern::PAGESIZE;

        Sprite emptyslot;
        Sprite charplatform;
        Charset levelset;
        nl::node nametag;
        Text slotcountlabel;

        // 079 bitmap-screen extras. The card captions are baked into
        // charInfo1, so these only carry the character-specific values.
        bool bitmap_ui;
        bool info_has_rank_rows;
        Texture jobicon;
        Texture rankmoveicon;
        Texture jobrankmoveicon;
        Text bitmapname;
        Text jobvalue;
        Text levelvalue;
        Text famevalue;
        Text strvalue;
        Text intvalue;
        Text dexvalue;
        Text lukvalue;
        Text rankvalue;
        Text jobrankvalue;

        Point<int16_t> slotcountpos;

        std::vector<CharEntry> characters;
        std::vector<std::optional<CharLook>> charlooks;
        std::vector<character_preview::LoadState> preview_states;
        mutable std::vector<bool> preview_draw_failed;
        std::vector<Nametag> nametags;
        int8_t require_pic;
        uint8_t channel_id;
        PicAction pending_pic_action;
        int32_t pending_pic_cid;
        size_t preview_ready;
        bool preview_completion_announced;
        bool ui_initialized;
        bool ui_failed;

        uint8_t charcount_absolute;
        uint8_t charcount_relative;
        uint8_t slots_absolute;
        uint8_t slots_relative;
        uint8_t selected_absolute;
        uint8_t selected_relative;
        uint8_t page;

        struct OutlinedText
        {
            Text inner;
            Text l;
            Text r;
            Text t;
            Text b;

            OutlinedText(Text::Font font,
                         Text::Alignment alignment,
                         Text::Color inner_color = Text::WHITE,
                         Text::Color outline_color = Text::DARKGREY)
            {
                inner = Text(font, alignment, inner_color);
                l     = Text(font, alignment, outline_color);
                r     = Text(font, alignment, outline_color);
                t     = Text(font, alignment, outline_color);
                b     = Text(font, alignment, outline_color);
            }

            OutlinedText() = default;

            void draw(Point<int16_t> parentpos) const
            {
                l.draw(parentpos + Point<int16_t>(-1,  0));
                r.draw(parentpos + Point<int16_t>( 1,  0));
                t.draw(parentpos + Point<int16_t>( 0, -1));
                b.draw(parentpos + Point<int16_t>( 0,  1));
                inner.draw(parentpos);
            }

            void change_text(const std::string& text)
            {
                inner.change_text(text);
                l.change_text(text);
                r.change_text(text);
                t.change_text(text);
                b.change_text(text);
            }
        };
        OutlinedText namelabel;

        static const size_t NUM_LABELS = 5;
        enum InfoLabel
        {
            JOB, STR, INT, DEX, LUK
        };
        OutlinedText infolabels[NUM_LABELS];
    };
}
