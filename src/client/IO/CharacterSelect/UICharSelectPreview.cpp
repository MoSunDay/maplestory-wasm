#include "../UITypes/UICharSelect.h"

#include "CharlistRecovery.h"
#include "../UI.h"
#include "../../Console.h"
#include "../../Net/Login/CharlistTransition.h"

namespace jrc
{
    void UICharSelect::load_next_preview()
    {
        const auto next = character_preview::next_index(
            preview_states,
            selected_absolute,
            page,
            PAGESIZE
        );
        if (!next)
        {
            complete_previews_if_ready();
            return;
        }

        const size_t index = *next;
        charlist_transition::set_character(index, 0);
        const character_preview::LoadResult result =
            character_preview::attempt_load([&] {
                charlooks[index].emplace(
                    characters[index].look,
                    Texture::LoadMode::DEFERRED,
                    [](int32_t equip_id) {
                        charlist_transition::set_equip(equip_id);
                    }
                );
                if (index == selected_absolute)
                {
                    charlooks[index]->set_stance(Stance::WALK1);
                }
            });

        if (result.state == character_preview::LoadState::READY)
        {
            resolve_preview(index, result.state);
        }
        else
        {
            fail_preview(index, result.error);
        }
    }

    void UICharSelect::resolve_preview(
        size_t index,
        character_preview::LoadState state
    ) {
        preview_states[index] = state;
        preview_ready = character_preview::resolved_count(preview_states);
        charlist_transition::set_preview_progress(
            preview_ready,
            preview_states.size()
        );
        complete_previews_if_ready();
    }

    void UICharSelect::complete_previews_if_ready()
    {
        if (!character_preview::should_announce_completion(
                preview_states,
                preview_completion_announced
            ))
        {
            return;
        }

        preview_completion_announced = true;
        charlist_transition::set_preview_progress(
            preview_ready,
            preview_states.size()
        );
        charlist_recovery::release_source_ui();
        charlist_transition::set_phase(charlist_transition::Phase::READY);
        UI::get().enable();
    }

    void UICharSelect::fail_preview(size_t index, const std::string& message)
    {
        const int32_t equip_id = charlist_transition::snapshot().equip_id;
        charlooks[index].reset();
        Console::get().print(
            "Warning: Character preview " + std::to_string(index) +
            " (cid " + std::to_string(characters[index].cid) +
            ", equip " + std::to_string(equip_id) +
            ") failed: " + message
        );
        resolve_preview(index, character_preview::LoadState::FAILED);
    }

    void UICharSelect::fail_ui(const std::exception& error)
    {
        fail_ui(error.what());
    }

    void UICharSelect::fail_ui(const std::string& message)
    {
        ui_failed = true;
        active = false;
        charlist_transition::set_phase(charlist_transition::Phase::FAILED_UI);
        Console::get().print(
            std::string("[UICharSelect] Character selection UI failed: ") +
            message
        );
        if (!charlist_recovery::restore_source_ui())
        {
            Console::get().print(
                "[UICharSelect] No source UI was available for recovery."
            );
        }
    }

    void UICharSelect::draw_preview(
        size_t index,
        Point<int16_t> position,
        float alpha
    ) const {
        if (index >= charlooks.size() || !charlooks[index] ||
            preview_draw_failed[index])
        {
            return;
        }

        try
        {
            charlooks[index]->draw(position, alpha);
        }
        catch (const std::exception& error)
        {
            preview_draw_failed[index] = true;
            Console::get().print(
                "Warning: Character preview draw " + std::to_string(index) +
                " (cid " + std::to_string(characters[index].cid) +
                ") failed: " + error.what()
            );
        }
        catch (...)
        {
            preview_draw_failed[index] = true;
            Console::get().print(
                "Warning: Character preview draw " + std::to_string(index) +
                " (cid " + std::to_string(characters[index].cid) +
                ") failed: unknown exception"
            );
        }
    }
}
