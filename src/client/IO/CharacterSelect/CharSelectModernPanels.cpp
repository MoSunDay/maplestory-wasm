//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful, but      //
// WITHOUT ANY WARRANTY; without even the implied warranty of               //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           //
// GNU Affero General Public License for more details.                     //
//                                                                          //
// You should have received a copy of the GNU Affero General Public        //
// License along with this program.  If not, see                           //
// <http://www.gnu.org/licenses/>.                                          //
//////////////////////////////////////////////////////////////////////////////
// Flat panel rendering for the 2023 character select chrome. Colour values
// mirror the CashShop flat panels: near-black translucent fills, slate
// outlines and a single orange accent per panel.
#include "CharSelectModernPanels.h"
#include "CharSelectModernLayout.h"
#include "../Components/FlatDraw.h"
#include "../../Graphics/Text.h"
#include "../../Template/Point.h"

namespace jrc
{
    namespace charselect_panels
    {
        namespace layout = charselect_modern;

        namespace
        {
            constexpr float ACCENT_R = 0.96f;
            constexpr float ACCENT_G = 0.52f;
            constexpr float ACCENT_B = 0.14f;

            inline int16_t right_edge(Point<int16_t> pos, Point<int16_t> size)
            {
                return static_cast<int16_t>(pos.x() + size.x() - 1);
            }

            inline int16_t bottom_edge(Point<int16_t> pos, Point<int16_t> size)
            {
                return static_cast<int16_t>(pos.y() + size.y() - 1);
            }

            // Pill tint by button state: disabled dims, hover brightens,
            // pressed shifts warm and every active state keeps a hairline.
            void control_backing(
                Point<int16_t> origin,
                Point<int16_t> pos,
                Point<int16_t> size,
                Button::State state,
                bool accent_notch
            ) {
                switch (state)
                {
                case Button::DISABLED:
                    flatdraw::fill_rect(
                        origin, pos, size, 0.10f, 0.11f, 0.13f, 0.55f);
                    flatdraw::outline_rect(
                        origin, pos, size, 0.20f, 0.22f, 0.26f, 0.60f);
                    break;
                case Button::MOUSEOVER:
                    flatdraw::fill_rect(
                        origin, pos, size, 0.17f, 0.18f, 0.22f, 0.95f);
                    flatdraw::outline_rect(
                        origin, pos, size, ACCENT_R, ACCENT_G, ACCENT_B, 0.90f);
                    break;
                case Button::PRESSED:
                    flatdraw::fill_rect(
                        origin, pos, size, 0.22f, 0.16f, 0.11f, 0.95f);
                    flatdraw::outline_rect(
                        origin, pos, size, ACCENT_R, ACCENT_G, ACCENT_B, 0.90f);
                    break;
                default:
                    flatdraw::fill_rect(
                        origin, pos, size, 0.12f, 0.13f, 0.16f, 0.85f);
                    flatdraw::outline_rect(
                        origin, pos, size, 0.28f, 0.31f, 0.37f, 0.90f);
                    break;
                }

                if (accent_notch && state != Button::DISABLED)
                {
                    flatdraw::fill_rect(
                        origin,
                        pos,
                        Point<int16_t>(4, size.y()),
                        ACCENT_R, ACCENT_G, ACCENT_B, 0.85f
                    );
                }
            }

            // Captions are baked into the flat rows. The Text objects are
            // function local statics because Text caches its layout and
            // change_text() only costs a string compare per frame.
            enum CaptionKey
            {
                CAP_JOB,
                CAP_STR,
                CAP_DEX,
                CAP_INT,
                CAP_LUK
            };

            Text& caption(CaptionKey key)
            {
                static Text texts[5]{
                    Text{ Text::A11M, Text::LEFT, Text::LIGHTGREY },
                    Text{ Text::A11M, Text::LEFT, Text::LIGHTGREY },
                    Text{ Text::A11M, Text::LEFT, Text::LIGHTGREY },
                    Text{ Text::A11M, Text::LEFT, Text::LIGHTGREY },
                    Text{ Text::A11M, Text::LEFT, Text::LIGHTGREY }
                };
                static const char* labels[5]{
                    "职业", "力量", "敏捷", "智力", "运气"
                };

                texts[key].change_text(labels[key]);
                return texts[key];
            }

            void draw_caption(
                Point<int16_t> origin,
                Point<int16_t> pos,
                CaptionKey key
            ) {
                caption(key).draw(origin + pos);
            }

            void draw_data_cell(
                Point<int16_t> origin,
                Point<int16_t> pos,
                CaptionKey key
            ) {
                const Point<int16_t> size(
                    layout::CARD_CELL_HALF,
                    layout::CARD_STAT_ROW_SIZE.y()
                );
                flatdraw::fill_rect(origin, pos, size, 0.13f, 0.14f, 0.17f, 0.85f);
                flatdraw::outline_rect(
                    origin, pos, size, 0.24f, 0.26f, 0.31f, 0.80f);
                draw_caption(
                    origin,
                    pos + Point<int16_t>(
                        layout::CARD_CELL_INNER_X,
                        layout::CARD_ROW_TEXT_TOP_OFFSET),
                    key
                );
            }
        }

        void draw_chrome(Point<int16_t> origin)
        {
            // Top bar with the orange accent underline.
            flatdraw::fill_rect(
                origin, layout::TOPBAR_POS, layout::TOPBAR_SIZE,
                0.08f, 0.09f, 0.12f, 0.92f);
            flatdraw::hline(
                origin,
                layout::TOPBAR_POS.x(),
                right_edge(layout::TOPBAR_POS, layout::TOPBAR_SIZE),
                layout::TOPBAR_POS.y(),
                0.35f, 0.38f, 0.45f, 0.55f);
            flatdraw::hline(
                origin,
                layout::TOPBAR_POS.x(),
                right_edge(layout::TOPBAR_POS, layout::TOPBAR_SIZE),
                static_cast<int16_t>(
                    bottom_edge(layout::TOPBAR_POS, layout::TOPBAR_SIZE) -
                    layout::TOPBAR_ACCENT_HEIGHT + 1),
                ACCENT_R, ACCENT_G, ACCENT_B, 1.0f,
                layout::TOPBAR_ACCENT_HEIGHT);

            // Left stage band behind the roster row.
            flatdraw::fill_rect(
                origin, layout::STAGE_POS, layout::STAGE_SIZE,
                0.97f, 0.97f, 1.0f, 0.16f);
            flatdraw::outline_rect(
                origin, layout::STAGE_POS, layout::STAGE_SIZE,
                0.95f, 0.96f, 1.0f, 0.35f);

            // Right info card: dark panel, header notch, hairline divider.
            flatdraw::fill_rect(
                origin, layout::CARD_POS, layout::CARD_SIZE,
                0.07f, 0.08f, 0.10f, 0.92f);
            flatdraw::outline_rect(
                origin, layout::CARD_POS, layout::CARD_SIZE,
                0.30f, 0.33f, 0.40f, 1.0f);
            flatdraw::fill_rect(
                origin, layout::CARD_POS, Point<int16_t>(64, 2),
                ACCENT_R, ACCENT_G, ACCENT_B, 1.0f);
            flatdraw::hline(
                origin,
                static_cast<int16_t>(layout::CARD_POS.x() + 8),
                static_cast<int16_t>(
                    right_edge(layout::CARD_POS, layout::CARD_SIZE) - 8),
                layout::CARD_HEADER_BOTTOM,
                0.30f, 0.33f, 0.40f, 0.80f);

            // Row 1: job on a full-width pill.
            flatdraw::fill_rect(
                origin, layout::CARD_JOB_ROW_POS, layout::CARD_JOB_ROW_SIZE,
                0.13f, 0.14f, 0.17f, 0.85f);
            flatdraw::outline_rect(
                origin, layout::CARD_JOB_ROW_POS, layout::CARD_JOB_ROW_SIZE,
                0.24f, 0.26f, 0.31f, 0.80f);
            draw_caption(
                origin,
                layout::CARD_JOB_ROW_POS + Point<int16_t>(
                    layout::CARD_CELL_INNER_X,
                    layout::CARD_ROW_TEXT_TOP_OFFSET),
                CAP_JOB);

            // Rows 2-3: two stat cells per row (STR|INT then DEX|LUK in
            // UICharSelect::InfoLabel order).
            const int16_t cell_r_x = static_cast<int16_t>(
                layout::CARD_STAT_ROW_POS.x() + layout::CARD_CELL_HALF +
                layout::CARD_CELL_GAP);
            draw_data_cell(origin, layout::CARD_STAT_ROW_POS, CAP_STR);
            draw_data_cell(
                origin,
                Point<int16_t>(cell_r_x, layout::CARD_STAT_ROW_POS.y()),
                CAP_INT);
            draw_data_cell(origin, layout::CARD_STAT_ROW2_POS, CAP_DEX);
            draw_data_cell(
                origin,
                Point<int16_t>(cell_r_x, layout::CARD_STAT_ROW2_POS.y()),
                CAP_LUK);
        }

        void draw_start_backing(Point<int16_t> origin, Button::State state)
        {
            // BtSelect art is 140x52 at START_POS; the pill pads it by 6.
            control_backing(
                origin,
                layout::START_POS - Point<int16_t>(6, 6),
                Point<int16_t>(152, 64),
                state,
                true
            );
        }

        void draw_footer_backing(
            Point<int16_t> origin,
            Button::State create_state,
            Button::State delete_state
        ) {
            // Footer band with the accent underline on its top edge.
            flatdraw::fill_rect(
                origin, layout::FOOTER_POS, layout::FOOTER_SIZE,
                0.08f, 0.09f, 0.12f, 0.90f);
            flatdraw::outline_rect(
                origin, layout::FOOTER_POS, layout::FOOTER_SIZE,
                0.30f, 0.33f, 0.40f, 0.90f);
            flatdraw::hline(
                origin,
                layout::FOOTER_POS.x(),
                right_edge(layout::FOOTER_POS, layout::FOOTER_SIZE),
                layout::FOOTER_POS.y(),
                ACCENT_R, ACCENT_G, ACCENT_B, 1.0f, 2);

            // Pills pad the 110x37 button art by 5.
            control_backing(
                origin,
                layout::CREATE_POS - Point<int16_t>(5, 5),
                Point<int16_t>(120, 47),
                create_state,
                false
            );
            control_backing(
                origin,
                layout::DELETE_POS - Point<int16_t>(5, 5),
                Point<int16_t>(120, 47),
                delete_state,
                false
            );
        }

        void draw_slot_highlight(
            Point<int16_t> origin,
            uint8_t slot,
            bool selected,
            Button::State state
        ) {
            const Point<int16_t> pos =
                layout::slot_hotzone_pos(slot) -
                Point<int16_t>(
                    layout::SLOT_SELECT_MARGIN,
                    layout::SLOT_SELECT_MARGIN);
            const Point<int16_t> size = layout::slot_hotzone_dim() +
                Point<int16_t>(
                    layout::SLOT_SELECT_MARGIN * 2,
                    layout::SLOT_SELECT_MARGIN * 2);

            if (selected)
            {
                flatdraw::fill_rect(
                    origin, pos, size, ACCENT_R, ACCENT_G, ACCENT_B, 0.06f);
                flatdraw::outline_rect(
                    origin, pos, size, ACCENT_R, ACCENT_G, ACCENT_B, 0.90f, 2);
                return;
            }

            if (state == Button::MOUSEOVER)
            {
                flatdraw::fill_rect(
                    origin, pos, size, 1.0f, 1.0f, 1.0f, 0.10f);
                flatdraw::outline_rect(
                    origin, pos, size, 1.0f, 1.0f, 1.0f, 0.35f);
            }
        }
    }
}
