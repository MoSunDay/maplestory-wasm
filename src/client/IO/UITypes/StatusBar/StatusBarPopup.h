#pragma once

#include "../../Components/MapleButton.h"
#include "../../../Graphics/Texture.h"

#include <memory>
#include <vector>

namespace jrc
{
    /// Owns the native Menu/System popups attached to the status bar.
    class StatusBarPopup
    {
    public:
        enum class Panel
        {
            NONE,
            MENU,
            SYSTEM
        };

        enum class Action
        {
            NONE,
            STATS,
            EQUIPS,
            ITEMS,
            SKILLS,
            KEY_CONFIG,
            QUIT
        };

        struct CursorResult
        {
            Cursor::State state = Cursor::IDLE;
            Action action = Action::NONE;
        };

        StatusBarPopup();

        void draw(float alpha) const;
        void update_anchors(Rectangle<int16_t> menu_anchor,
                            Rectangle<int16_t> system_anchor);

        void toggle(Panel panel);
        void close();
        bool is_open() const;
        bool contains(Point<int16_t> cursor_position) const;

        CursorResult send_cursor(bool pressed, Point<int16_t> cursor_position);
        void remove_cursor();

    private:
        struct ButtonSpec
        {
            const char* node_name;
            Action action;
        };

        struct Entry
        {
            std::unique_ptr<MapleButton> button;
            Action action;
        };

        struct PanelData
        {
            Texture top;
            Texture middle;
            Texture bottom;
            std::vector<Entry> entries;
            Point<int16_t> position;
            int16_t width = 0;
            int16_t height = 0;
            int16_t middle_height = 0;
        };

        static void initialize_panel(PanelData& panel, nl::node source,
                                     const std::vector<ButtonSpec>& specs);
        static void position_panel(PanelData& panel, Rectangle<int16_t> anchor);
        static Rectangle<int16_t> panel_bounds(const PanelData& panel);

        PanelData& active_data();
        const PanelData& active_data() const;

        PanelData menu;
        PanelData system;
        Panel active_panel = Panel::NONE;
    };
}
