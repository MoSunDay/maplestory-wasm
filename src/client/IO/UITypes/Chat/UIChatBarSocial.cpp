#include "../UIChatBar.h"

#include "../../../Net/Packets/GameplayPackets.h"
#include "../../../Net/Packets/MessagingPackets.h"

#include <algorithm>
#include <cctype>
#include <list>
#include <sstream>

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

        std::string lowercase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        bool parse_int32(const std::string& value, int32_t& result)
        {
            if (value.empty())
            {
                return false;
            }

            const size_t start = value[0] == '+' || value[0] == '-' ? 1 : 0;
            if (start == value.size())
            {
                return false;
            }
            for (size_t index = start; index < value.size(); ++index)
            {
                if (!std::isdigit(static_cast<unsigned char>(value[index])))
                {
                    return false;
                }
            }

            try
            {
                result = std::stoi(value);
            }
            catch (...)
            {
                return false;
            }
            return true;
        }
    }

    int32_t UIChatbar::resolve_party_member_id(const std::string& token) const
    {
        int32_t member_id = 0;
        if (parse_int32(token, member_id))
        {
            return member_id;
        }

        const std::string lowered = lowercase(token);
        for (const PartyMember& member : party_state.members)
        {
            if (lowercase(member.name) == lowered)
            {
                return member.id;
            }
        }
        return 0;
    }

    bool UIChatbar::handle_social_command(const std::string& message)
    {
        if (message.empty() || message[0] != '/')
        {
            return false;
        }

        std::istringstream whole(message);
        std::string command;
        whole >> command;
        command = lowercase(command);

        std::string argument_block;
        std::getline(whole, argument_block);
        argument_block = trim_spaces(argument_block);

        if (command == "/buddy" || command == "/b")
        {
            if (argument_block.empty())
            {
                send_line("[Buddy] Usage: /buddy <message>", YELLOW);
            }
            else
            {
                send_buddy_message(argument_block);
            }
            return true;
        }

        if (command == "/pt")
        {
            if (argument_block.empty())
            {
                send_line("[Party] Usage: /pt <message>", YELLOW);
            }
            else
            {
                send_party_message(argument_block);
            }
            return true;
        }

        if (command != "/party" && command != "/p")
        {
            return false;
        }
        if (argument_block.empty())
        {
            send_line("[Party] /party create | leave | invite <name> | join <partyId>", YELLOW);
            send_line("[Party] /party expel <name|id> | leader <name|id> | accept | deny | list", YELLOW);
            return true;
        }

        std::istringstream arguments(argument_block);
        std::string action;
        arguments >> action;
        action = lowercase(action);
        std::string argument;
        std::getline(arguments, argument);
        argument = trim_spaces(argument);

        if (action == "create")
        {
            CreatePartyPacket().dispatch();
            send_line("[Party] Sent create request.", YELLOW);
        }
        else if (action == "leave")
        {
            LeavePartyPacket().dispatch();
            send_line("[Party] Sent leave request.", YELLOW);
        }
        else if (action == "join")
        {
            int32_t party_id = 0;
            if (!parse_int32(argument, party_id))
            {
                send_line("[Party] Usage: /party join <partyId>", YELLOW);
            }
            else
            {
                JoinPartyPacket(party_id).dispatch();
                send_line("[Party] Sent join request.", YELLOW);
            }
        }
        else if (action == "invite")
        {
            if (argument.empty())
            {
                send_line("[Party] Usage: /party invite <name>", YELLOW);
            }
            else
            {
                InviteToPartyPacket(argument).dispatch();
                send_line("[Party] Sent invite request to " + argument + ".", YELLOW);
            }
        }
        else if (action == "expel" || action == "kick")
        {
            const int32_t member_id = resolve_party_member_id(argument);
            if (member_id <= 0)
            {
                send_line("[Party] Usage: /party expel <name|id>", YELLOW);
            }
            else
            {
                ExpelFromPartyPacket(member_id).dispatch();
                send_line("[Party] Sent expel request.", YELLOW);
            }
        }
        else if (action == "leader" || action == "lead")
        {
            const int32_t member_id = resolve_party_member_id(argument);
            if (member_id <= 0)
            {
                send_line("[Party] Usage: /party leader <name|id>", YELLOW);
            }
            else
            {
                ChangePartyLeaderPacket(member_id).dispatch();
                send_line("[Party] Sent leadership transfer request.", YELLOW);
            }
        }
        else if (action == "accept")
        {
            if (party_state.pending_id <= 0)
            {
                send_line("[Party] There is no pending invitation.", YELLOW);
            }
            else
            {
                JoinPartyPacket(party_state.pending_id).dispatch();
                send_line("[Party] Sent invitation accept request.", YELLOW);
                clear_pending_party_invite();
            }
        }
        else if (action == "deny")
        {
            if (party_state.pending_inviter.empty())
            {
                send_line("[Party] There is no pending invitation.", YELLOW);
            }
            else
            {
                const std::string inviter = party_state.pending_inviter;
                DenyPartyInvitePacket(inviter).dispatch();
                send_line("[Party] Declined invitation from " + inviter + ".", YELLOW);
                clear_pending_party_invite();
            }
        }
        else if (action == "list")
        {
            if (party_state.members.empty())
            {
                send_line("[Party] No cached party members.", YELLOW);
            }
            else
            {
                send_line("[Party] Current members:", YELLOW);
                for (const PartyMember& member : party_state.members)
                {
                    const std::string leader_tag = member.id == party_state.leader_id ? " (leader)" : "";
                    send_line(" - " + member.name + " Lv." + std::to_string(member.level) + leader_tag, WHITE);
                }
            }
        }
        else
        {
            send_line("[Party] Unknown command. Use /party for help.", YELLOW);
        }
        return true;
    }

    void UIChatbar::send_targeted_message(const std::string& target, const std::string& message)
    {
        send_line("[To " + target + "] " + message, WHITE);
    }

    void UIChatbar::send_buddy_message(const std::string& message)
    {
        std::list<int32_t> recipients;
        for (const social::BuddyEntry& buddy : buddy_state.entries)
        {
            recipients.push_back(buddy.id);
        }
        if (recipients.empty())
        {
            send_line("[Buddy] Add a buddy before using buddy chat.", YELLOW);
            return;
        }
        MultiChatPacket(MultiChatPacket::BUDDY, recipients, message).dispatch();
        send_targeted_message("Buddy", message);
    }

    void UIChatbar::send_party_message(const std::string& message)
    {
        if (party_state.id <= 0)
        {
            send_line("[Party] Join or create a party before using party chat.", YELLOW);
            return;
        }
        MultiChatPacket(MultiChatPacket::PARTY, {}, message).dispatch();
        send_targeted_message("Party", message);
    }

    void UIChatbar::send_chat_message(const std::string& message)
    {
        const std::list<int32_t> recipients;
        switch (chattarget)
        {
        case CHT_ALL:
            GeneralChatPacket(message, true).dispatch();
            break;
        case CHT_BUDDY:
            send_buddy_message(message);
            break;
        case CHT_GUILD:
            MultiChatPacket(MultiChatPacket::GUILD, recipients, message).dispatch();
            send_targeted_message("Guild", message);
            break;
        case CHT_ALLIANCE:
            MultiChatPacket(MultiChatPacket::ALLIANCE, recipients, message).dispatch();
            send_targeted_message("Alliance", message);
            break;
        case CHT_SQUAD:
        case CHT_PARTY:
        default:
            send_party_message(message);
            break;
        }
    }
}
