#include "IO/UITypes/CharacterCreation/Flow.h"
#include "IO/UITypes/CharacterCreation/NamePolicy.h"

#include <cassert>

using namespace jrc::CharacterCreation;

namespace
{
    constexpr uint32_t TIMEOUT = 8000;

    void verify_name_policy()
    {
        assert(is_locally_valid_name("Hero123"));
        assert(is_locally_valid_name("\xE8\xA7\x92\xE8\x89\xB2\xE5\x90\x8D"));
        assert(!is_locally_valid_name("ab"));
        assert(!is_locally_valid_name("bad id"));
        assert(!is_locally_valid_name("admin123"));
        assert(!is_locally_valid_name("\xF0\x9F\x98\x80"));
    }

    void verify_success_path()
    {
        Flow flow = checking_name("Hero123");
        assert(name_response_action(flow, "Other", false) == NameResponseAction::IGNORE);
        assert(name_response_action(flow, "Hero123", false) ==
            NameResponseAction::ENTER_CUSTOMIZATION);

        flow = checking_creation_name("Hero123");
        assert(shows_customization(flow.phase));
        assert(name_response_action(flow, "Hero123", false) ==
            NameResponseAction::DISPATCH_CREATION);
    }

    void verify_recoverable_failures()
    {
        Flow flow = checking_creation_name("Hero123");
        assert(name_response_action(flow, "Hero123", true) ==
            NameResponseAction::REJECT_NAME);

        flow = creating("Hero123");
        assert(name_response_action(flow, "Hero123", true) ==
            NameResponseAction::REJECT_NAME);
        assert(name_response_action(flow, "Hero123", false) ==
            NameResponseAction::RESTORE_CUSTOMIZATION);

        flow = advance(flow, TIMEOUT);
        assert(timeout_action(flow, TIMEOUT) == TimeoutAction::RECHECK_CREATED_NAME);

        flow = recovering_creation("Hero123");
        flow = advance(flow, TIMEOUT);
        assert(timeout_action(flow, TIMEOUT) == TimeoutAction::RESTORE_CUSTOMIZATION);
    }
}

int main()
{
    verify_name_policy();
    verify_success_path();
    verify_recoverable_failures();
}
