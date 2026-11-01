#include "ImeBridge.h"

#include "Components/Textfield.h"
#include "CashShop/UICashShop.h"
#include "UI.h"
#include "../Constants.h"
#include "UITypes/UICharCreation.h"
#include "UITypes/UILoginNotice.h"
#include "UITypes/UIWorldSelect.h"
#include "UITypes/UIStatusBar.h"
#include "UITypes/Quest/UIQuestLog.h"
#include "../Gameplay/Stage.h"
#include "../Character/Inventory/InventoryType.h"
#include "../Net/Login/CharlistTransition.h"

#ifdef MS_PLATFORM_WASM

#include <emscripten.h>

#include <GLFW/glfw3.h>

namespace jrc
{
    namespace
    {
        bool active = false;
        const Textfield* focusedfield = nullptr;

        // The JS side forwards DOM key codes; convert the small whitelist to
        // GLFW codes so UI::send_key sees its usual key space.
        int32_t dom_to_glfw(int32_t dom_keycode)
        {
            switch (dom_keycode)
            {
            case 13: return GLFW_KEY_ENTER;
            case 9:  return GLFW_KEY_TAB;
            case 27: return GLFW_KEY_ESCAPE;
            case 38: return GLFW_KEY_UP;
            case 40: return GLFW_KEY_DOWN;
            default: return 0;
            }
        }
    }

    bool ImeBridge::is_active()
    {
        return active;
    }

    void ImeBridge::focus_field(Textfield* field)
    {
        if (field->is_crypted())
        {
            // Password fields stay on the plain keyboard path: release the
            // DOM textarea so keystrokes cannot land in the stale IME buffer
            // of the previously focused field (which would silently rewrite
            // that field's text, e.g. doubling the login username).
            blur_field();
            return;
        }

        active = true;
        focusedfield = field;

        Rectangle<int16_t> bounds = field->get_bounds();
        // The scene is rendered shifted down by VIEWYOFFSET; report the
        // on-screen position so the DOM caret and IME candidates track the
        // drawn glyphs instead of the unshifted field bounds.
        std::string text = field->get_text();
        int caret = static_cast<int>(field->caret_utf16());

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onFocus)
            {
                window.MapleWasmIME.onFocus($0, $1, $2, $3, UTF8ToString($4), $5);
            }
        },
            bounds.getlt().x(),
            static_cast<int16_t>(bounds.getlt().y() + Constants::VIEWYOFFSET),
            bounds.width(), bounds.height(),
            text.c_str(), caret);
    }

    void ImeBridge::blur_field()
    {
        if (!active)
        {
            return;
        }

        active = false;
        focusedfield = nullptr;

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onBlur)
            {
                window.MapleWasmIME.onBlur();
            }
        });
    }

    void ImeBridge::sync_field(const Textfield* field)
    {
        if (!active || field != focusedfield)
        {
            return;
        }

        std::string text = field->get_text();
        int caret = static_cast<int>(field->caret_utf16());

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onText)
            {
                window.MapleWasmIME.onText(UTF8ToString($0), $1);
            }
        }, text.c_str(), caret);
    }

    void ImeBridge::reposition_field(const Textfield* field)
    {
        if (!active || field != focusedfield)
        {
            return;
        }

        Rectangle<int16_t> bounds = field->get_bounds();
        // Same scene offset as focus_field: report where the field is drawn.
        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.reposition)
            {
                window.MapleWasmIME.reposition($0, $1, $2, $3);
            }
        },
            bounds.getlt().x(),
            static_cast<int16_t>(bounds.getlt().y() + Constants::VIEWYOFFSET),
            bounds.width(), bounds.height());
    }

    void ImeBridge::select_all(const Textfield* field)
    {
        if (!active || field != focusedfield)
        {
            return;
        }

        EM_ASM({
            if (window.MapleWasmIME && window.MapleWasmIME.onSelectAll)
            {
                window.MapleWasmIME.onSelectAll();
            }
        });
    }

    void ImeBridge::apply_text(const std::string& text, size_t caret_utf16)
    {
        if (active)
        {
            UI::get().ime_input(text, caret_utf16);
        }
    }

    void ImeBridge::apply_key(int32_t dom_keycode, bool pressed)
    {
        if (!active)
        {
            return;
        }

        int32_t key = dom_to_glfw(dom_keycode);
        if (key != 0)
        {
            UI::get().send_key(key, pressed);
        }
    }
}

extern "C"
{
    // Read-only state probe used by the browser acceptance flow. Keeping the
    // state in C++ avoids treating a timed click sequence as proof of success.
    EMSCRIPTEN_KEEPALIVE int msui_state()
    {
        jrc::UI& ui = jrc::UI::get();
        if (ui.is_element_active(jrc::UIElement::CASHSHOP)) return 6;
        if (ui.is_element_active(jrc::UIElement::STATUSBAR)) return 5;
        if (ui.is_element_active(jrc::UIElement::CHARCREATION)) return 4;
        if (ui.is_element_active(jrc::UIElement::CHARSELECT)) return 3;
        if (ui.is_element_active(jrc::UIElement::WORLDSELECT)) return 2;
        if (ui.is_element_active(jrc::UIElement::LOGIN)) return 1;
        return 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_enabled()
    {
        return jrc::UI::get().is_enabled() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int mscharlist_phase()
    {
        return static_cast<int>(jrc::charlist_transition::snapshot().phase);
    }

    EMSCRIPTEN_KEEPALIVE int mscharlist_character_index()
    {
        return jrc::charlist_transition::snapshot().character_index;
    }

    EMSCRIPTEN_KEEPALIVE int mscharlist_equip_id()
    {
        return jrc::charlist_transition::snapshot().equip_id;
    }

    EMSCRIPTEN_KEEPALIVE int mscharlist_remaining_bytes()
    {
        return jrc::charlist_transition::snapshot().remaining_bytes;
    }

    EMSCRIPTEN_KEEPALIVE int mscharlist_preview_ready()
    {
        return jrc::charlist_transition::snapshot().preview_ready;
    }

    EMSCRIPTEN_KEEPALIVE int msime_field_focused()
    {
        return jrc::UI::get().has_focused_textfield() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int mscharlist_preview_total()
    {
        return jrc::charlist_transition::snapshot().preview_total;
    }

    // A modal notice does not replace its parent UI state, so expose it
    // separately to let browser acceptance prove that an error was visible.
    EMSCRIPTEN_KEEPALIVE int msui_login_notice_active()
    {
        return jrc::UI::get().is_element_active(jrc::UILoginNotice::TYPE) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_login_notice_message()
    {
        auto notice = jrc::UI::get().get_element<jrc::UILoginNotice>();
        return notice ? static_cast<int>(notice->get_message()) : -1;
    }

    EMSCRIPTEN_KEEPALIVE int msui_character_creation_customizing()
    {
        auto creation = jrc::UI::get().get_element<jrc::UICharcreation>();
        return creation && creation->is_customizing() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_notice_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::NOTICE) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_questlog_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::QUESTLOG) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_questlog_forfeit_armed()
    {
        auto window = jrc::UI::get().get_element<jrc::UIQuestLog>();
        return window && window->is_active() && window->forfeit_armed()
            ? 1
            : 0;
    }

    EMSCRIPTEN_KEEPALIVE int msui_progressbanner_active()
    {
        return jrc::UI::get().is_element_active(jrc::UIElement::PROGRESSBANNER) ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_locker_count()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->locker_count() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_inventory_count()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->inventory_count() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_nx_credit()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->nx_credit() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_selected_right_row()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->selected_right_row() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_pending()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop && shop->is_pending() ? 1 : 0;
    }

    // Settlement probes: the UI keeps purchases locked until every balance
    // push arrives, so expose the individual gates to tell a slow balance
    // push apart from a failed one or a lingering pending operation.
    EMSCRIPTEN_KEEPALIVE int mscashshop_balances_ready()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? (!shop->balances_ready() ? 0 : 1) : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_balance_failed()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? (shop->balance_failed() ? 1 : 0) : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_can_mutate()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? (shop->can_mutate() ? 1 : 0) : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_require_reenter()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? (shop->require_reenter() ? 1 : 0) : -1;
    }

    // PendingKind: 0=none 1=buy 2=take 3=put 4=exit.
    EMSCRIPTEN_KEEPALIVE int mscashshop_pending_op()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? static_cast<int>(shop->pending_kind()) : -1;
    }

    // Cash ids are stored as 64-bit, but the JS bridge only carries 32-bit
    // numbers; the truncated id is enough to match one row in a single run.
    EMSCRIPTEN_KEEPALIVE int mscashshop_selected_right_cash_id()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? static_cast<int>(shop->selected_right_cash_id()) : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_filtered_count()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->filtered_count() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_equip_sub()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->equip_sub_index() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_equip_menu_open()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop && shop->is_equip_menu_open() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_highlighted_item()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->highlighted_item_id() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_selected_left()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->selected_left_index() : -2;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_tryon_count()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->tryon_count() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_preview_cap()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->preview_cap_id() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_preview_weapon()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->preview_weapon_id() : -1;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_buy_x()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->button_center(jrc::BT_BUY).x() : -32768;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_buy_y()
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->button_center(jrc::BT_BUY).y() : -32768;
    }

    // Current slot maximum of one inventory tab (1=EQUIP..4=ETC).
    EMSCRIPTEN_KEEPALIVE int mscashshop_slotmax(int type)
    {
        const auto slotmax = jrc::Stage::get().get_player().get_inventory()
            .get_slotmax(static_cast<jrc::InventoryType::Id>(type));
        return slotmax;
    }

    // Centre of the i-th (0..3) slot-expansion pill.
    EMSCRIPTEN_KEEPALIVE int mscashshop_slotbuy_x(int index)
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->button_center(static_cast<uint16_t>(
            jrc::BT_SLOTBUY_FIRST + index)).x() : -32768;
    }

    EMSCRIPTEN_KEEPALIVE int mscashshop_slotbuy_y(int index)
    {
        auto shop = jrc::UI::get().get_element<jrc::UICashShop>();
        return shop ? shop->button_center(static_cast<uint16_t>(
            jrc::BT_SLOTBUY_FIRST + index)).y() : -32768;
    }

    EMSCRIPTEN_KEEPALIVE int msplayer_frame()
    {
        return jrc::Stage::get().get_player().get_look().get_frame();
    }


    // Post-chaos chat bar toggle state + open-button center for browser
    // acceptance: Enter key events can be silently dropped by a busy
    // renderer, so tests click the button instead.
    EMSCRIPTEN_KEEPALIVE int mschatbar_open()
    {
        auto bar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return bar && bar->chat_open() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE int mschatbar_openbutton_x()
    {
        auto bar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return bar ? bar->chat_open_button().x() : -32768;
    }

    EMSCRIPTEN_KEEPALIVE int mschatbar_openbutton_y()
    {
        auto bar = jrc::UI::get().get_element<jrc::UIStatusbar>();
        return bar ? bar->chat_open_button().y() : -32768;
    }

    // Browser click compatibility entry point. UIWorldSelect makes this
    // idempotent with the normal GLFW mouse-down route.
    EMSCRIPTEN_KEEPALIVE int msworldselect_enter()
    {
        auto worldselect = jrc::UI::get().get_element<jrc::UIWorldSelect>();
        return worldselect && worldselect->enter_selected_channel() ? 1 : 0;
    }

    // Replace the focused field's text with the textarea content; the caret is
    // a UTF-16 offset, matching the browser's selectionStart.
    EMSCRIPTEN_KEEPALIVE void msime_input(const char* full_text, int caret_utf16)
    {
        jrc::ImeBridge::apply_text(
            full_text != nullptr ? full_text : "",
            caret_utf16 > 0 ? static_cast<size_t>(caret_utf16) : 0);
    }

    // Forward a whitelisted control key using its DOM key code.
    EMSCRIPTEN_KEEPALIVE void msime_key(int dom_keycode, int pressed)
    {
        jrc::ImeBridge::apply_key(dom_keycode, pressed != 0);
    }
}

#else

namespace jrc
{
    bool ImeBridge::is_active()
    {
        return false;
    }

    void ImeBridge::focus_field(Textfield*) {}

    void ImeBridge::blur_field() {}

    void ImeBridge::sync_field(const Textfield*) {}

    void ImeBridge::reposition_field(const Textfield*) {}

    void ImeBridge::select_all(const Textfield*) {}

    void ImeBridge::apply_text(const std::string&, size_t) {}

    void ImeBridge::apply_key(int32_t, bool) {}
}

#endif
