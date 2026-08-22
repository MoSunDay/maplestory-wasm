#include "../UI.h"
#include "UIBuddy.h"
#include "../UITypes/UIStatusBar.h"
#include "../../Gameplay/Stage.h"

#include <algorithm>

namespace
{
    jrc::Optional<jrc::UIStatusbar> statusbar()
    {
        return jrc::UI::get().get_element<jrc::UIStatusbar>();
    }
}

extern "C"
{
    int msplayer_id()
    {
        return jrc::Stage::get().get_player().get_oid();
    }

    int msplayer_level()
    {
        return jrc::Stage::get().get_player().get_level();
    }

    int msbuddy_count()
    {
        auto bar = statusbar();
        return bar ? static_cast<int>(bar->get_buddy_state().entries.size()) : 0;
    }

    int msbuddy_window_active()
    {
        auto buddy = jrc::UI::get().get_element<jrc::UIBuddy>();
        return buddy && buddy->is_active() ? 1 : 0;
    }

    int msbuddy_window_x()
    {
        auto buddy = jrc::UI::get().get_element<jrc::UIBuddy>();
        return buddy ? buddy->origin().x() : -1;
    }

    int msbuddy_window_y()
    {
        auto buddy = jrc::UI::get().get_element<jrc::UIBuddy>();
        return buddy ? buddy->origin().y() : -1;
    }

    int msbuddy_online_count()
    {
        auto bar = statusbar();
        if (!bar) return 0;
        return static_cast<int>(std::count_if(
            bar->get_buddy_state().entries.begin(), bar->get_buddy_state().entries.end(),
            [](const auto& entry) { return entry.channel >= 0; }
        ));
    }

    int msbuddy_pending_id()
    {
        auto bar = statusbar();
        return bar ? bar->get_buddy_state().pending.id : -1;
    }

    int msbuddy_contains(int id)
    {
        auto bar = statusbar();
        if (!bar) return 0;
        return std::any_of(bar->get_buddy_state().entries.begin(), bar->get_buddy_state().entries.end(),
                           [id](const auto& entry) { return entry.id == id; }) ? 1 : 0;
    }

    int msbuddy_message_count()
    {
        auto bar = statusbar();
        return bar ? static_cast<int>(bar->get_buddy_state().received_messages) : 0;
    }

    int msparty_id()
    {
        auto bar = statusbar();
        return bar ? bar->get_party_id() : -1;
    }

    int msparty_leader_id()
    {
        auto bar = statusbar();
        return bar ? bar->get_party_leader_id() : -1;
    }

    int msparty_member_count()
    {
        auto bar = statusbar();
        return bar ? static_cast<int>(bar->get_party_members().size()) : 0;
    }

    int msparty_pending_id()
    {
        auto bar = statusbar();
        return bar ? bar->get_pending_party_invite_id() : -1;
    }

    int msparty_has_member(int id)
    {
        auto bar = statusbar();
        if (!bar) return 0;
        return std::any_of(bar->get_party_members().begin(), bar->get_party_members().end(),
                           [id](const auto& member) { return member.id == id; }) ? 1 : 0;
    }

    int msparty_member_hp(int id)
    {
        auto bar = statusbar();
        if (!bar) return -1;
        const auto member = std::find_if(bar->get_party_members().begin(), bar->get_party_members().end(),
                                         [id](const auto& value) { return value.id == id; });
        return member == bar->get_party_members().end() ? -1 : member->hp;
    }

    int msparty_message_count()
    {
        auto bar = statusbar();
        return bar ? static_cast<int>(bar->get_party_message_count()) : 0;
    }
}
