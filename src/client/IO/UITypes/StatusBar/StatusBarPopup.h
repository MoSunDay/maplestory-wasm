#pragma once

#include "../../Components/MapleButton.h"
#include "StatusBarPreChaos.h"
#include "../../../Graphics/Geometry.h"
#include "../../../Graphics/Texture.h"
#include "../../../Graphics/Text.h"

#include <memory>
#include <optional>
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
            PET_OPTIONS,
            LOGOUT
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
        void open(Panel panel, Action selected_action);
        void close();
        bool is_open() const;
        Action selected_action() const;
        bool contains(Point<int16_t> cursor_position) const;
        std::optional<Point<int16_t>> action_center(Action action) const;

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

        // v83 has no button textures for the popup: the frame is assembled
        // from UIWindow.img/ContextMenu and the rows are drawn as text.
        struct ItemSpec
        {
            const char* label;
            Action action;
        };

        struct TextItem
        {
            Text label;
            Action action;
        };

        struct PanelData
        {
            Texture top;
            Texture middle;
            Texture bottom;
            std::vector<Entry> entries;
            std::vector<TextItem> items;
            ColorBox hover_row;
            int16_t hover = -1;
            Point<int16_t> position;
            int16_t width = 0;
            int16_t height = 0;
            int16_t middle_height = 0;
        };

        static void initialize_panel(PanelData& panel, nl::node source,
                                     const std::vector<ButtonSpec>& specs);
        static void initialize_panel_prechaos(PanelData& panel,
                                              const std::vector<ItemSpec>& specs);
        static void position_panel(PanelData& panel, Rectangle<int16_t> anchor);
        static Rectangle<int16_t> panel_bounds(const PanelData& panel);
        static Rectangle<int16_t> item_bounds(const PanelData& panel, size_t index);

        void draw_prechaos(const PanelData& panel) const;
        void select_action(PanelData& panel, Action action);

        PanelData& active_data();
        const PanelData& active_data() const;

        PanelData menu;
        PanelData system;
        Panel active_panel = Panel::NONE;
        Action selection = Action::NONE;
    };
}
