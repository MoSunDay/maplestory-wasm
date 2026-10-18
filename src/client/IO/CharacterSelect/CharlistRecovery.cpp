#include "CharlistRecovery.h"

#include "../UI.h"
#include "../UITypes/UICharCreation.h"
#include "../UITypes/UIWorldSelect.h"

namespace jrc::charlist_recovery
{
    void release_source_ui()
    {
        UI::get().remove(UIElement::WORLDSELECT);
        UI::get().remove(UIElement::CHARCREATION);
    }

    bool restore_source_ui()
    {
        auto worldselect = UI::get().get_element<UIWorldSelect>();
        auto creation = UI::get().get_element<UICharcreation>();
        const SourceUi source = choose_source(
            static_cast<bool>(worldselect),
            static_cast<bool>(creation)
        );

        if (source == SourceUi::WORLD_SELECT)
        {
            worldselect->recover_charlist_request();
        }
        else if (source == SourceUi::CHARACTER_CREATION)
        {
            creation->makeactive();
        }

        UI::get().enable();
        return source != SourceUi::NONE;
    }
}
