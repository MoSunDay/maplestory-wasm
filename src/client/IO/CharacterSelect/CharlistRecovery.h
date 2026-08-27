#pragma once

namespace jrc::charlist_recovery
{
    enum class SourceUi
    {
        NONE,
        WORLD_SELECT,
        CHARACTER_CREATION
    };

    constexpr SourceUi choose_source(bool has_world_select,
        bool has_character_creation)
    {
        if (has_world_select)
        {
            return SourceUi::WORLD_SELECT;
        }
        if (has_character_creation)
        {
            return SourceUi::CHARACTER_CREATION;
        }
        return SourceUi::NONE;
    }

    void release_source_ui();
    bool restore_source_ui();
}
