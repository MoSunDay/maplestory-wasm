#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace jrc::CharacterCreation
{
    enum class Phase
    {
        EDITING,
        CHECKING_NAME,
        CUSTOMIZING,
        CHECKING_CREATION_NAME,
        CREATING,
        RECOVERING_CREATION
    };

    enum class NameResponseAction
    {
        IGNORE,
        REJECT_NAME,
        ENTER_CUSTOMIZATION,
        DISPATCH_CREATION,
        RESTORE_CUSTOMIZATION
    };

    enum class TimeoutAction
    {
        NONE,
        RESTORE_NAME_ENTRY,
        RESTORE_CUSTOMIZATION,
        RECHECK_CREATED_NAME
    };

    struct Flow
    {
        Phase phase = Phase::EDITING;
        std::string pending_name;
        uint32_t elapsed = 0;
    };

    inline Flow editing()
    {
        return {};
    }

    inline Flow customizing()
    {
        return { Phase::CUSTOMIZING, {}, 0 };
    }

    inline Flow checking_name(std::string name)
    {
        return { Phase::CHECKING_NAME, std::move(name), 0 };
    }

    inline Flow checking_creation_name(std::string name)
    {
        return { Phase::CHECKING_CREATION_NAME, std::move(name), 0 };
    }

    inline Flow creating(std::string name)
    {
        return { Phase::CREATING, std::move(name), 0 };
    }

    inline Flow recovering_creation(std::string name)
    {
        return { Phase::RECOVERING_CREATION, std::move(name), 0 };
    }

    inline Flow advance(Flow flow, uint32_t elapsed)
    {
        flow.elapsed += elapsed;
        return flow;
    }

    inline bool timed_out(const Flow& flow, uint32_t timeout)
    {
        return flow.elapsed >= timeout;
    }

    inline bool accepts_name_response(const Flow& flow, const std::string& name)
    {
        switch (flow.phase)
        {
        case Phase::CHECKING_NAME:
        case Phase::CHECKING_CREATION_NAME:
        case Phase::CREATING:
        case Phase::RECOVERING_CREATION:
            return name == flow.pending_name;
        default:
            return false;
        }
    }

    inline NameResponseAction name_response_action(
        const Flow& flow,
        const std::string& name,
        bool name_used
    )
    {
        if (!accepts_name_response(flow, name))
        {
            return NameResponseAction::IGNORE;
        }
        if (name_used)
        {
            return NameResponseAction::REJECT_NAME;
        }

        switch (flow.phase)
        {
        case Phase::CHECKING_NAME:
            return NameResponseAction::ENTER_CUSTOMIZATION;
        case Phase::CHECKING_CREATION_NAME:
            return NameResponseAction::DISPATCH_CREATION;
        case Phase::CREATING:
        case Phase::RECOVERING_CREATION:
            return NameResponseAction::RESTORE_CUSTOMIZATION;
        default:
            return NameResponseAction::IGNORE;
        }
    }

    inline TimeoutAction timeout_action(const Flow& flow, uint32_t timeout)
    {
        if (!timed_out(flow, timeout))
        {
            return TimeoutAction::NONE;
        }

        switch (flow.phase)
        {
        case Phase::CHECKING_NAME:
            return TimeoutAction::RESTORE_NAME_ENTRY;
        case Phase::CHECKING_CREATION_NAME:
        case Phase::RECOVERING_CREATION:
            return TimeoutAction::RESTORE_CUSTOMIZATION;
        case Phase::CREATING:
            return TimeoutAction::RECHECK_CREATED_NAME;
        default:
            return TimeoutAction::NONE;
        }
    }

    inline bool shows_customization(Phase phase)
    {
        return phase == Phase::CUSTOMIZING ||
            phase == Phase::CHECKING_CREATION_NAME ||
            phase == Phase::CREATING ||
            phase == Phase::RECOVERING_CREATION;
    }
}
