#include "UIBuddy.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../UITypes/CharacterCreation/NamePolicy.h"
#include "../UITypes/UIStatusBar.h"
#include "../../Net/Packets/SocialPackets.h"

#include "nlnx/nx.hpp"

#include <algorithm>

namespace jrc
{
    namespace
    {
        std::string trim_spaces(const std::string& value)
        {
            const size_t first = value.find_first_not_of(' ');
            if (first == std::string::npos)
            {
                return "";
            }
            return value.substr(first, value.find_last_not_of(' ') - first + 1);
        }

        void send_buddy_status(const std::string& message)
        {
            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                statusbar->send_chatline(message, UIChatbar::YELLOW);
            }
        }
    }

    UIBuddy::UIBuddy()
        : UIDragElement<PosBUDDY>(Point<int16_t>(300, 22)),
          background(300, 246, Geometry::BLACK, 0.80f),
          header(300, 22, Geometry::WHITE, 0.12f),
          divider(260, 1, Geometry::WHITE, 0.25f),
          title(Text::A12B, Text::LEFT, Text::WHITE, "Buddy"),
          summary(Text::A11M, Text::LEFT, Text::WHITE, ""),
          pending(Text::A11M, Text::LEFT, Text::YELLOW, ""),
          add_label(Text::A11M, Text::LEFT, Text::WHITE, "Add:")
    {
        dimension = Point<int16_t>(300, 246);
        nl::node basic = nl::nx::ui["Basic.img"];
        buttons[BT_CLOSE] = std::make_unique<MapleButton>(basic["BtClose"], Point<int16_t>(276, 4));
        buttons[BT_ADD] = std::make_unique<MapleButton>(basic["BtOK4"], Point<int16_t>(240, 171));
        buttons[BT_DELETE] = std::make_unique<MapleButton>(basic["BtCancel4"], Point<int16_t>(20, 210));
        buttons[BT_ACCEPT] = std::make_unique<MapleButton>(basic["BtOK4"], Point<int16_t>(160, 210));
        buttons[BT_DECLINE] = std::make_unique<MapleButton>(basic["BtCancel4"], Point<int16_t>(230, 210));

        button_labels[BT_ADD] = Text(Text::A11M, Text::CENTER, Text::WHITE, "Add");
        button_labels[BT_DELETE] = Text(Text::A11M, Text::CENTER, Text::WHITE, "Delete");
        button_labels[BT_ACCEPT] = Text(Text::A11M, Text::CENTER, Text::WHITE, "Accept");
        button_labels[BT_DECLINE] = Text(Text::A11M, Text::CENTER, Text::WHITE, "Decline");

        add_field = Textfield(Text::A11M, Text::LEFT, Text::WHITE,
                              Rectangle<int16_t>(48, 207, 160, 191), 12);
        add_field.set_state(Textfield::NORMAL);
        add_field.set_enter_callback([this](std::string) { add_buddy(); });

        for (Text& row : rows)
        {
            row = Text(Text::A11M, Text::LEFT, Text::LIGHTGREY, "");
        }

        if (auto statusbar = UI::get().get_element<UIStatusbar>())
        {
            set_state(statusbar->get_buddy_state());
        }
        sync_text();
    }

    void UIBuddy::draw(float alpha) const
    {
        background.draw(position);
        header.draw(position);
        divider.draw(position + Point<int16_t>(20, 24));
        divider.draw(position + Point<int16_t>(20, 160));
        title.draw(position + Point<int16_t>(12, 6));
        summary.draw(position + Point<int16_t>(20, 29));
        for (size_t index = 0; index < VISIBLE_ROWS; ++index)
        {
            rows[index].draw(position + Point<int16_t>(20, ROW_TOP + static_cast<int16_t>(index * ROW_HEIGHT)));
        }
        pending.draw(position + Point<int16_t>(20, 151));
        add_label.draw(position + Point<int16_t>(20, 177));
        add_field.draw(position);
        UIElement::draw_buttons(alpha);
        draw_button_labels();
    }

    void UIBuddy::update()
    {
        UIElement::update();
        add_field.update(position);
    }

    UIElement::CursorResult UIBuddy::send_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (dragged)
        {
            return UIDragElement::send_cursor(clicked, cursorpos);
        }

        if (clicked)
        {
            const Point<int16_t> relative = cursorpos - position;
            if (relative.x() >= 16 && relative.x() <= 282 && relative.y() >= ROW_TOP &&
                relative.y() < ROW_TOP + static_cast<int16_t>(VISIBLE_ROWS * ROW_HEIGHT))
            {
                const size_t visible = static_cast<size_t>((relative.y() - ROW_TOP) / ROW_HEIGHT);
                const size_t index = first_row + visible;
                if (index < state.entries.size())
                {
                    selected_id = state.entries[index].id;
                    sync_text();
                    return { Cursor::CLICKING, true };
                }
            }
        }

        if (add_field.get_state() != Textfield::DISABLED)
        {
            const Cursor::State field_state = add_field.send_cursor(cursorpos, clicked);
            if (field_state != Cursor::IDLE)
            {
                return { field_state, true };
            }
        }
        return UIDragElement::send_cursor(clicked, cursorpos);
    }

    void UIBuddy::send_scroll(double yoffset)
    {
        const size_t max_first = state.entries.size() > VISIBLE_ROWS ? state.entries.size() - VISIBLE_ROWS : 0;
        if (yoffset > 0 && first_row > 0)
        {
            first_row--;
        }
        else if (yoffset < 0 && first_row < max_first)
        {
            first_row++;
        }
        sync_text();
    }

    void UIBuddy::send_key(int32_t keycode, bool pressed, bool escape)
    {
        if (!pressed)
        {
            return;
        }
        if (escape)
        {
            deactivate();
        }
        else if (keycode == KeyAction::RETURN && !add_field.get_text().empty())
        {
            add_buddy();
        }
    }

    void UIBuddy::set_state(const social::BuddyState& next)
    {
        state = next;
        const auto selected = std::find_if(state.entries.begin(), state.entries.end(), [this](const auto& entry) {
            return entry.id == selected_id;
        });
        if (selected == state.entries.end())
        {
            selected_id = -1;
        }
        const size_t max_first = state.entries.size() > VISIBLE_ROWS ? state.entries.size() - VISIBLE_ROWS : 0;
        first_row = std::min(first_row, max_first);
        sync_text();
    }

    Button::State UIBuddy::button_pressed(uint16_t buttonid)
    {
        if (buttonid == BT_CLOSE)
        {
            deactivate();
            return Button::NORMAL;
        }
        if (buttonid == BT_ADD)
        {
            add_buddy();
        }
        else if (buttonid == BT_DELETE && selected_id > 0)
        {
            const int32_t deleted_id = selected_id;
            DeleteBuddyPacket(deleted_id).dispatch();
            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                statusbar->remove_buddy(deleted_id);
            }
            send_buddy_status("[Buddy] Sent delete request.");
        }
        else if (buttonid == BT_ACCEPT && state.pending.id > 0)
        {
            AcceptBuddyPacket(state.pending.id).dispatch();
            send_buddy_status("[Buddy] Accepted " + state.pending.name + ".");
            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                statusbar->clear_pending_buddy_request();
            }
        }
        else if (buttonid == BT_DECLINE && state.pending.id > 0)
        {
            DeleteBuddyPacket(state.pending.id).dispatch();
            send_buddy_status("[Buddy] Declined " + state.pending.name + ".");
            if (auto statusbar = UI::get().get_element<UIStatusbar>())
            {
                statusbar->clear_pending_buddy_request();
            }
        }
        return Button::PRESSED;
    }

    void UIBuddy::add_buddy()
    {
        const std::string name = trim_spaces(add_field.get_text());
        if (!CharacterCreation::is_locally_valid_name(name))
        {
            send_buddy_status("[Buddy] Enter a valid 3-12 byte character name.");
            return;
        }
        if (auto statusbar = UI::get().get_element<UIStatusbar>())
        {
            statusbar->begin_buddy_add(name);
        }
        AddBuddyPacket(name).dispatch();
        send_buddy_status("[Buddy] Sent request to " + name + ".");
        add_field.change_text("");
        add_field.set_state(Textfield::NORMAL);
    }

    void UIBuddy::sync_text()
    {
        const size_t online = static_cast<size_t>(std::count_if(state.entries.begin(), state.entries.end(), [](const auto& entry) {
            return entry.channel >= 0;
        }));
        std::string capacity = state.capacity > 0 ? "/" + std::to_string(state.capacity) : "";
        summary.change_text("Buddies: " + std::to_string(state.entries.size()) + capacity + "  Online: " + std::to_string(online));
        pending.change_text(state.pending.id > 0 ? "Request: " + state.pending.name : "Request: none");

        for (size_t visible = 0; visible < VISIBLE_ROWS; ++visible)
        {
            const size_t index = first_row + visible;
            if (index >= state.entries.size())
            {
                rows[visible].change_text(visible == 0 && state.entries.empty() ? "No buddies yet." : "");
                rows[visible].change_color(Text::LIGHTGREY);
                continue;
            }
            const auto& entry = state.entries[index];
            const std::string marker = entry.id == selected_id ? "> " : "  ";
            const std::string status = entry.channel >= 0 ? "Ch." + std::to_string(entry.channel + 1) : "Offline";
            rows[visible].change_color(entry.channel >= 0 ? Text::WHITE : Text::LIGHTGREY);
            rows[visible].change_text(marker + entry.name + "  " + status);
        }
        sync_buttons();
    }

    void UIBuddy::sync_buttons()
    {
        buttons[BT_DELETE]->set_active(selected_id > 0);
        buttons[BT_ACCEPT]->set_active(state.pending.id > 0);
        buttons[BT_DECLINE]->set_active(state.pending.id > 0);
    }

    void UIBuddy::draw_button_labels() const
    {
        for (const auto& label : button_labels)
        {
            auto button = buttons.find(label.first);
            if (button == buttons.end() || !button->second || !button->second->is_active())
            {
                continue;
            }
            const Rectangle<int16_t> bounds = button->second->bounds(position);
            label.second.draw(Point<int16_t>(static_cast<int16_t>((bounds.l() + bounds.r()) / 2),
                                             static_cast<int16_t>((bounds.t() + bounds.b()) / 2 + 2)));
        }
    }
}
