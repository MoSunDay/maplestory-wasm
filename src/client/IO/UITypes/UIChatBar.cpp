#include "UIChatBar.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "StatusBar/StatusBarPreChaos.h"

#include "../../Audio/Audio.h"
#include "../../Net/Packets/MessagingPackets.h"

#include "UIParty.h"
#include "UIStatusMessenger.h"
#include "../Social/UIBuddy.h"

#include "nlnx/nx.hpp"

namespace jrc
{
    namespace
    {
        std::string trim(const std::string& value)
        {
            size_t first = value.find_first_not_of(' ');
            if (first == std::string::npos)
            {
                return "";
            }

            size_t last = value.find_last_not_of(' ');
            return value.substr(first, last - first + 1);
        }
    }

    UIChatbar::UIChatbar(Point<int16_t> pos)
    {
        position = pos;
        dimension = { 500, 60 };
        chatopen = true;
        chattarget = CHT_ALL;
        chatrows = 4;
        rowpos = 0;
        rowmax = -1;
        lastpos = 0;
        dragchattop = false;

        if (statusbar::prechaos())
        {
            nl::node base = nl::nx::ui["StatusBar.img"]["base"];

            chat_strip = base["chat"];
            chat_target_box = base["chatTarget"];
            chat_input_row = {
                statusbar::chat_log_width(), 17,
                Geometry::BLACK, 0.66f
            };
            chat_target_label = { Text::A11M, Text::CENTER, Text::BLACK };
            apply_chat_target_label();

            chatbox = {
                statusbar::chat_log_width(),
                1 + chatrows * CHATROWHEIGHT,
                Geometry::BLACK, 0.6f
            };
        }
        else
        {
            nl::node mainbar = nl::nx::ui["StatusBar2.img"]["mainBar"];

            buttons[BT_OPENCHAT] = std::make_unique<MapleButton>(mainbar["chatOpen"]);
            buttons[BT_CLOSECHAT] = std::make_unique<MapleButton>(mainbar["chatClose"]);
            buttons[BT_SCROLLUP] = std::make_unique<MapleButton>(mainbar["scrollUp"]);
            buttons[BT_SCROLLDOWN] = std::make_unique<MapleButton>(mainbar["scrollDown"]);
            buttons[BT_CHATTARGETS] = std::make_unique<MapleButton>(mainbar["chatTarget"]["base"]);

            chatspace[false] = mainbar["chatSpace"];
            chatspace[true] = mainbar["chatEnter"];
            chatenter = mainbar["chatSpace2"];
            chatcover = mainbar["chatCover"];

            chattargets[CHT_ALL] = mainbar["chatTarget"]["all"];
            chattargets[CHT_BUDDY] = mainbar["chatTarget"]["friend"];
            chattargets[CHT_GUILD] = mainbar["chatTarget"]["guild"];
            chattargets[CHT_ALLIANCE] = mainbar["chatTarget"]["association"];
            chattargets[CHT_PARTY] = mainbar["chatTarget"]["party"];
            chattargets[CHT_SQUAD] = mainbar["chatTarget"]["expedition"];

            nl::node chat = nl::nx::ui["StatusBar2.img"]["chat"];

            tapbar = chat["tapBar"];
            tapbartop = chat["tapBarOver"];

            chatbox = { 502, 1 + chatrows * CHATROWHEIGHT, Geometry::BLACK, 0.6f };
        }

        chatfield = {
            Text::A11M, Text::LEFT, Text::BLACK,
            statusbar::prechaos()
                ? statusbar::chat_field_rect()
                : Rectangle<int16_t>{ { -435, -58 }, { -40, -35 } },
            0
        };
        set_chat_open(chatopen);
        chatfield.set_enter_callback([&](std::string msg) {
            msg = trim(msg);
            if (msg.empty())
            {
                chatfield.set_state(Textfield::NORMAL);
                return;
            }

            if (!handle_social_command(msg))
            {
                send_chat_message(msg);
            }

            lastentered.push_back(msg);
            lastpos = lastentered.size();
            chatfield.set_state(Textfield::NORMAL);
        });
        chatfield.set_key_callback(KeyAction::UP, [&](){
            if (lastpos > 0)
            {
                lastpos--;
                chatfield.change_text(lastentered[lastpos]);
            }
        });
        chatfield.set_key_callback(KeyAction::DOWN, [&](){
            if (lastentered.size() > 0 && lastpos < lastentered.size() - 1)
            {
                lastpos++;
                chatfield.change_text(lastentered[lastpos]);
            }
        });
        chatfield.set_key_callback(KeyAction::RETURN, [&](){
            chatfield.set_state(Textfield::NORMAL);
        });

        slider = {
            11,
            Range<int16_t>(0, CHATROWHEIGHT * chatrows - 14),
            statusbar::prechaos() ? statusbar::chat_slider_x() : -22,
            chatrows, 1,
            [&](bool up) {
            int16_t next = up ?
                rowpos - 1 :
                rowpos + 1;
            if (next >= 0 && next <= rowmax)
                rowpos = next;
        } };
    }

    void UIChatbar::apply_chat_target_label()
    {
        static const char* TARGET_NAMES[UIChatbar::NUM_TARGETS] = {
            "All", "Buddy", "Guild", "Alliance", "Party", "Squad"
        };

        if (chattarget >= CHT_ALL && chattarget < NUM_TARGETS)
        {
            chat_target_label.change_text(TARGET_NAMES[chattarget]);
        }
    }

    void UIChatbar::set_chat_open(bool open)
    {
        chatopen = open;
        if (buttons.count(BT_OPENCHAT))
        {
            buttons[BT_OPENCHAT]->set_active(!open);
            buttons[BT_CLOSECHAT]->set_active(open);
            buttons[BT_CHATTARGETS]->set_active(open);
        }
        chatfield.set_state(open ? Textfield::NORMAL : Textfield::DISABLED);
    }

    void UIChatbar::focus_chatfield()
    {
        if (!chatopen)
        {
            set_chat_open(true);
        }

        chatfield.set_state(Textfield::FOCUSED);
    }

    void UIChatbar::draw(float inter) const
    {
        if (statusbar::prechaos())
        {
            UIElement::draw(inter);

            // Input row inside the bottom strip: dark box under a thin
            // divider (base/chat), selector box on the left.
            chat_strip.draw(position + statusbar::chat_strip_pos());
            chat_input_row.draw(position + Point<int16_t>(0, 54));
            chat_target_box.draw(position + statusbar::chat_target_pos());
            chat_target_label.draw(position + statusbar::chat_target_pos()
                + Point<int16_t>(40, 5));

            if (chatopen)
            {
                chatbox.draw({ 0, getchattop() + 2 });
                draw_chatrows();
                slider.draw({ position.x(), getchattop() + 5 });
            }
            else if (rowtexts.count(rowmax))
            {
                rowtexts.at(rowmax).draw({ 4, position.y() - CHATROWHEIGHT + 1 });
            }

            chatfield.draw(position);
            return;
        }

        chatspace[chatopen].draw(position);
        chatenter.draw(position);

        UIElement::draw(inter);

        if (chatopen)
        {
            tapbartop.draw({ position.x() - 576, getchattop() });
            chatbox.draw({ 0, getchattop() + 2 });

            draw_chatrows();

            slider.draw({ position.x(), getchattop() + 5 });

            if (chattarget >= CHT_ALL && chattarget < NUM_TARGETS)
            {
                chattargets[chattarget].draw(position + Point<int16_t>(0, 2));
            }
            chatcover.draw(position);
            chatfield.draw(position);
        }
        else if (rowtexts.count(rowmax))
        {
            rowtexts.at(rowmax).draw(position + Point<int16_t>(-500, -60));
        }
    }

    void UIChatbar::draw_chatrows() const
    {
        int16_t chatheight = CHATROWHEIGHT * chatrows;
        int16_t yshift = -chatheight;
        for (int16_t i = 0; i < chatrows; i++)
        {
            int16_t rowid = rowpos - i;
            if (!rowtexts.count(rowid))
                break;

            int16_t textheight = rowtexts.at(rowid).height() / CHATROWHEIGHT;
            while (textheight > 0)
            {
                yshift += CHATROWHEIGHT;
                textheight--;
            }
            rowtexts.at(rowid).draw({ 4, getchattop() - yshift - 1 });
        }
    }

    void UIChatbar::update()
    {
        UIElement::update();

        chatfield.update(position);
    }

    void UIChatbar::set_position(Point<int16_t> pos)
    {
        position = pos;
    }

    Button::State UIChatbar::button_pressed(uint16_t id)
    {
        switch (id)
        {
        case BT_OPENCHAT:
            set_chat_open(true);
            break;
        case BT_CLOSECHAT:
            set_chat_open(false);
            break;
        case BT_CHATTARGETS:
            cycle_chat_target();
            return Button::PRESSED;
        }
        return Button::NORMAL;
    }

    bool UIChatbar::is_in_range(Point<int16_t> cursorpos) const
    {
        if (statusbar::prechaos())
        {
            const Rectangle<int16_t> bounds(
                0,
                statusbar::chat_log_width(),
                static_cast<int16_t>(getchattop() - 6),
                static_cast<int16_t>(position.y() + statusbar::BAR_HEIGHT)
            );
            return bounds.contains(cursorpos);
        }

        Point<int16_t> absp(0, getchattop() - 16);
        Point<int16_t> dim(500, chatrows * CHATROWHEIGHT + CHATYOFFSET + 16);
        return Rectangle<int16_t>(absp, absp + dim)
            .contains(cursorpos);
    }

    bool UIChatbar::remove_cursor(bool clicked, Point<int16_t> cursorpos)
    {
        if (slider.remove_cursor(clicked))
            return true;

        return UIElement::remove_cursor(clicked, cursorpos);
    }

    UIElement::CursorResult UIChatbar::send_cursor(bool clicking, Point<int16_t> cursorpos)
    {
        if (statusbar::prechaos())
        {
            // The v83 chat target selector is a plain bitmap: hit-test it
            // here instead of routing through a MapleButton.
            const Point<int16_t> target_pos = position + statusbar::chat_target_pos();
            const Rectangle<int16_t> target_bounds(
                target_pos,
                target_pos + Point<int16_t>(81, 20)
            );
            if (target_bounds.contains(cursorpos))
            {
                if (clicking)
                {
                    Sound(Sound::BUTTONCLICK).play();
                    cycle_chat_target();
                    return { Cursor::CLICKING, true };
                }
                return { Cursor::CANCLICK, true };
            }
        }

        if (slider.isenabled())
        {
            auto cursoroffset = cursorpos - Point<int16_t>(position.x(), getchattop() + 5);
            Cursor::State sstate = slider.send_cursor(cursoroffset, clicking);
            if (sstate != Cursor::IDLE)
            {
                return { sstate, true };
            }
        }

        if (chatfield.get_state() == Textfield::NORMAL)
        {
            Cursor::State tstate = chatfield.send_cursor(cursorpos, clicking);
            if (tstate != Cursor::IDLE)
            {
                return { tstate, true };
            }
        }

        const int16_t drag_width = statusbar::prechaos()
            ? statusbar::chat_log_width()
            : 502;
        auto chattop = Rectangle<int16_t>(
            0,  drag_width,
            getchattop(),
            getchattop() + 6
            );
        bool contains = chattop.contains(cursorpos);
        if (dragchattop)
        {
            if (clicking)
            {
                int16_t ydelta = cursorpos.y() - getchattop();
                while (ydelta > 0 && chatrows > MINCHATROWS)
                {
                    chatrows--;
                    ydelta -= CHATROWHEIGHT;
                }
                while (ydelta < 0 && chatrows < MAXCHATROWS)
                {
                    chatrows++;
                    ydelta += CHATROWHEIGHT;
                }
                chatbox.setheight(1 + chatrows * CHATROWHEIGHT);
                slider.setrows(rowpos, chatrows, rowmax);
                slider.setvertical({ 0, CHATROWHEIGHT * chatrows - 14 });
                return { Cursor::CLICKING, true };
            }
            else
            {
                dragchattop = false;
            }
        }
        else if (contains)
        {
            if (clicking)
            {
                dragchattop = true;
                return { Cursor::CLICKING, true };
            }
            else
            {
                return { Cursor::CANCLICK, true };
            }
        }

        return UIElement::send_cursor(clicking, cursorpos);
    }

    void UIChatbar::send_line(const std::string& line, LineType type)
    {
        rowmax++;
        rowpos = rowmax;

        slider.setrows(rowpos, chatrows, rowmax);

        Text::Color color;
        switch (type)
        {
        case RED:
            color = Text::DARKRED;
            break;
        case BLUE:
            color = Text::MEDIUMBLUE;
            break;
        case YELLOW:
            color = Text::YELLOW;
            break;
        default:
            color = Text::WHITE;
            break;
        }

        rowtexts.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(rowmax),
            std::forward_as_tuple(Text::A12M, Text::LEFT, color, line, 480)
        );
    }

    void UIChatbar::set_chat_target(ChatTarget target)
    {
        if (target < CHT_ALL || target >= NUM_TARGETS)
        {
            return;
        }

        chattarget = target;
        apply_chat_target_label();
    }

    void UIChatbar::cycle_chat_target()
    {
        chattarget = static_cast<ChatTarget>((chattarget + 1) % NUM_TARGETS);
        apply_chat_target_label();
    }

    void UIChatbar::set_pending_party_invite(int32_t in_party_id, const std::string& inviter)
    {
        party_state = social::invite_to_party(party_state, in_party_id, inviter);

        if (auto messenger = UI::get().get_element<UIStatusMessenger>())
        {
            messenger->show_party_invite(in_party_id, inviter);
        }
    }

    void UIChatbar::clear_pending_party_invite()
    {
        party_state = social::clear_party_invite(party_state);

        if (auto party_window = UI::get().get_element<UIParty>())
        {
            party_window->clear_pending_party_invite();
        }

        if (auto messenger = UI::get().get_element<UIStatusMessenger>())
        {
            messenger->clear_party_invite();
        }
    }

    void UIChatbar::set_party_state(int32_t in_party_id, int32_t leader_id, const std::vector<PartyMember>& members)
    {
        const bool clears_invite = party_state.pending_id == in_party_id;
        party_state = social::replace_party(party_state, in_party_id, leader_id, members);
        if (clears_invite)
        {
            if (auto messenger = UI::get().get_element<UIStatusMessenger>())
            {
                messenger->clear_party_invite();
            }
        }
    }

    void UIChatbar::confirm_party_created(int32_t in_party_id)
    {
        party_state = social::confirm_party_created(party_state, in_party_id);
    }

    void UIChatbar::clear_party_state()
    {
        party_state = social::clear_party(party_state);
    }

    void UIChatbar::set_party_leader(int32_t leader_id)
    {
        party_state = social::update_party_leader(party_state, leader_id);
    }

    void UIChatbar::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
    {
        party_state = social::update_party_member_hp(party_state, cid, hp, max_hp);
    }

    int32_t UIChatbar::get_party_id() const
    {
        return party_state.id;
    }

    int32_t UIChatbar::get_party_leader_id() const
    {
        return party_state.leader_id;
    }

    int32_t UIChatbar::get_pending_party_invite_id() const
    {
        return party_state.pending_id;
    }

    const std::string& UIChatbar::get_pending_party_inviter() const
    {
        return party_state.pending_inviter;
    }

    const std::vector<UIChatbar::PartyMember>& UIChatbar::get_party_members() const
    {
        return party_state.members;
    }

    void UIChatbar::set_buddy_list(const std::vector<social::BuddyEntry>& entries)
    {
        buddy_state = social::replace_buddies(buddy_state, entries);
        if (auto buddy_window = UI::get().get_element<UIBuddy>())
        {
            buddy_window->set_state(buddy_state);
        }
    }

    void UIChatbar::set_pending_buddy_request(int32_t id, const std::string& name)
    {
        buddy_state = social::request_buddy(buddy_state, id, name);
        if (auto buddy_window = UI::get().get_element<UIBuddy>())
        {
            buddy_window->set_state(buddy_state);
        }
        if (auto messenger = UI::get().get_element<UIStatusMessenger>())
        {
            messenger->show_buddy_request(id, name);
        }
    }

    void UIChatbar::clear_pending_buddy_request()
    {
        buddy_state = social::clear_buddy_request(buddy_state);
        if (auto buddy_window = UI::get().get_element<UIBuddy>())
        {
            buddy_window->set_state(buddy_state);
        }
        if (auto messenger = UI::get().get_element<UIStatusMessenger>())
        {
            messenger->clear_buddy_request();
        }
    }

    void UIChatbar::remove_buddy(int32_t id)
    {
        buddy_state = social::remove_buddy(buddy_state, id);
        if (auto buddy_window = UI::get().get_element<UIBuddy>())
        {
            buddy_window->set_state(buddy_state);
        }
    }

    void UIChatbar::begin_buddy_add(const std::string& name)
    {
        buddy_state = social::begin_buddy_add(buddy_state, name);
    }

    void UIChatbar::update_buddy_channel(int32_t id, int32_t channel)
    {
        buddy_state = social::update_buddy_channel(buddy_state, id, channel);
        if (auto buddy_window = UI::get().get_element<UIBuddy>())
        {
            buddy_window->set_state(buddy_state);
        }
    }

    void UIChatbar::set_buddy_capacity(uint8_t capacity)
    {
        buddy_state = social::update_buddy_capacity(buddy_state, capacity);
        if (auto buddy_window = UI::get().get_element<UIBuddy>())
        {
            buddy_window->set_state(buddy_state);
        }
    }

    const social::BuddyState& UIChatbar::get_buddy_state() const
    {
        return buddy_state;
    }

    void UIChatbar::record_multi_chat(uint8_t type)
    {
        if (type == MultiChatPacket::BUDDY)
        {
            buddy_state.received_messages++;
        }
        else if (type == MultiChatPacket::PARTY)
        {
            party_state.received_messages++;
        }
    }

    uint32_t UIChatbar::get_party_message_count() const
    {
        return party_state.received_messages;
    }

    int16_t UIChatbar::getchattop() const
    {
        if (statusbar::prechaos())
        {
            // The v83 log sits flush on top of the status bar.
            return position.y() - chatrows * CHATROWHEIGHT
                - statusbar::chat_top_gap();
        }

        return position.y() - chatrows * CHATROWHEIGHT - CHATYOFFSET;
    }
}
