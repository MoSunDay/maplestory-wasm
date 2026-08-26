#include "Stage.h"
#include "Input/ActionTrigger.h"

#include "../Audio/Audio.h"
#include "../IO/KeyAction.h"
#include "../Net/Packets/GameplayPackets.h"

#include <array>
#include <chrono>

namespace jrc
{
    namespace
    {
        constexpr uint32_t PICKUP_INTERVAL_MS = 100;
    }

    void Stage::handle_held_actions()
    {
        if (!playable)
            return;

        if (player.is_key_down(KeyAction::JUMP))
            playable->send_action(KeyAction::JUMP, true);

        if (player.is_key_down(KeyAction::PICKUP))
        {
            const uint64_t current_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
            if (current_time - last_pickup_time >= PICKUP_INTERVAL_MS)
            {
                last_pickup_time = current_time;
                check_drops();
            }
        }
    }

    void Stage::check_portals()
    {
        if (player.is_attacking())
            return;

        const Point<int16_t> player_position = player.get_position();
        const Portal::WarpInfo warp = portals.find_warp_at(player_position);
        if (warp.intramap)
        {
            const Point<int16_t> spawn = portals.get_portal_by_name(warp.toname);
            player.respawn(physics.get_y_below(spawn), mapinfo.is_underwater());
        }
        else if (warp.valid)
        {
            ChangeMapPacket(false, -1, warp.name, false).dispatch();
            if (warp.has_target_map())
                player.get_stats().set_mapid(warp.mapid);
            Sound(Sound::PORTAL).play();
        }
    }

    void Stage::check_seats()
    {
        if (!player.is_sitting() && !player.is_attacking())
            player.set_seat(mapinfo.findseat(player.get_position()));
    }

    void Stage::check_ladders(bool up)
    {
        if (!player.is_climbing() && !player.is_attacking())
            player.set_ladder(mapinfo.findladder(player.get_position(), up));
    }

    void Stage::handle_directional_context(KeyAction::Id action, bool down)
    {
        if (!down)
            return;

        switch (action)
        {
        case KeyAction::UP:
            check_ladders(true);
            check_portals();
            break;
        case KeyAction::DOWN:
            check_ladders(false);
            break;
        case KeyAction::LEFT:
        case KeyAction::RIGHT:
            if (player.is_key_down(KeyAction::UP))
                check_ladders(true);
            else if (player.is_key_down(KeyAction::DOWN))
                check_ladders(false);
            break;
        default:
            break;
        }
    }

    void Stage::update_directional_context()
    {
        if (player.is_key_down(KeyAction::UP))
            check_ladders(true);
        else if (player.is_key_down(KeyAction::DOWN))
            check_ladders(false);
    }

    void Stage::check_drops()
    {
        const MapDrops::Loot loot = drops.find_loot_at(player.get_position());
        if (loot.first)
            PickupItemPacket(loot.first, loot.second).dispatch();
    }

    bool Stage::is_intro_input_locked() const
    {
        return effect.blocks_player_input();
    }

    void Stage::release_actions()
    {
        static constexpr std::array<KeyAction::Id, 8> ACTIONS =
        {
            KeyAction::LEFT, KeyAction::RIGHT, KeyAction::UP, KeyAction::DOWN,
            KeyAction::JUMP, KeyAction::ATTACK, KeyAction::PICKUP, KeyAction::SIT
        };
        if (playable)
        {
            for (KeyAction::Id action : ACTIONS)
                if (player.is_key_down(action))
                    playable->send_action(action, false);
        }
        confused_direction_state = {};
    }

    void Stage::send_key(KeyType::Id type, int32_t action, bool down)
    {
        if (state != ACTIVE || !playable)
            return;

        if (down && is_intro_input_locked()
            && (type == KeyType::ACTION || type == KeyType::SKILL
                || type == KeyType::ITEM || type == KeyType::FACE))
            return;

        const bool incapacitated = player.has_buff(Buffstat::STUN)
            || player.has_buff(Buffstat::SEDUCE);
        if (down && incapacitated
            && (type == KeyType::ACTION || type == KeyType::SKILL))
            return;
        if (down && type == KeyType::SKILL && player.has_buff(Buffstat::SEAL))
            return;

        if (type == KeyType::ACTION)
        {
            const confused_input::Transition mapped = confused_input::transition(
                confused_direction_state, KeyAction::actionbyid(action), down,
                player.has_buff(Buffstat::CONFUSE));
            confused_direction_state = mapped.state;
            action = mapped.action;
        }
        if (down && type == KeyType::ACTION && action == KeyAction::JUMP
            && player.has_buff(Buffstat::WEAKEN))
            return;

        switch (type)
        {
        case KeyType::ACTION:
        {
            const KeyAction::Id key_action = KeyAction::actionbyid(action);
            const bool repeated = down && player.is_key_down(key_action);
            handle_directional_context(key_action, down);
            if (action_trigger::pressed_once(down, repeated))
            {
                switch (action)
                {
                case KeyAction::SIT: check_seats(); break;
                case KeyAction::ATTACK: combat.use_move(0); break;
                case KeyAction::PICKUP: check_drops(); break;
                case KeyAction::NPCCHAT:
                    npcs.talk_to_nearest(player.get_position());
                    break;
                default: break;
                }
            }
            if (!repeated
                || (key_action != KeyAction::ATTACK && key_action != KeyAction::JUMP))
                playable->send_action(key_action, down);
            break;
        }
        case KeyType::SKILL: combat.use_move(action); break;
        case KeyType::ITEM: player.use_item(action); break;
        case KeyType::FACE: player.set_expression(action); break;
        default: break;
        }
    }
}
