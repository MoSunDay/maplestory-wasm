#include "MobileInput.h"

#include "UI.h"
#include "Components/Textfield.h"

#ifdef MS_PLATFORM_WASM
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <string>

namespace jrc
{
#ifdef MS_PLATFORM_WASM
    namespace
    {
        EM_JS(void, wasm_mobile_set_textfield_state,
              (int x, int y, int width, int height, const char* text, int caret, int limit, int masked), {
            if (!window.MapleWasmMobile || !window.MapleWasmMobile.setTextFieldState) {
                return;
            }

            window.MapleWasmMobile.setTextFieldState({
                x: x,
                y: y,
                width: width,
                height: height,
                text: UTF8ToString(text),
                caret: caret,
                limit: limit,
                masked: !!masked
            });
        });

        EM_JS(void, wasm_mobile_clear_textfield_state, (), {
            if (window.MapleWasmMobile && window.MapleWasmMobile.clearTextField) {
                window.MapleWasmMobile.clearTextField();
            }
        });
    }
#endif

    namespace mobile
    {
        void sync_textfield(const Textfield& textfield)
        {
#ifdef MS_PLATFORM_WASM
            if (textfield.get_state() != Textfield::FOCUSED)
            {
                clear_textfield();
                return;
            }

            Rectangle<int16_t> bounds = textfield.get_bounds();
            wasm_mobile_set_textfield_state(
                bounds.l(),
                bounds.t(),
                bounds.width(),
                bounds.height(),
                textfield.get_text().c_str(),
                static_cast<int>(textfield.get_markerpos()),
                static_cast<int>(textfield.get_limit()),
                textfield.is_masked() ? 1 : 0
            );
#else
            (void)textfield;
#endif
        }

        void clear_textfield()
        {
#ifdef MS_PLATFORM_WASM
            wasm_mobile_clear_textfield_state();
#endif
        }
    }

#ifdef MS_PLATFORM_WASM
    extern "C"
    {
        EMSCRIPTEN_KEEPALIVE void wasmSendKey(int key, int pressed)
        {
            UI::get().send_key(key, pressed != 0);
        }

        EMSCRIPTEN_KEEPALIVE void wasmSyncFocusedText(const char* text, int caret)
        {
            std::string value = text ? text : "";
            UI::get().sync_focused_textfield(
                value,
                static_cast<size_t>(std::max(caret, 0))
            );
        }

        EMSCRIPTEN_KEEPALIVE void wasmSubmitFocusedText()
        {
            UI::get().submit_focused_textfield();
        }

        EMSCRIPTEN_KEEPALIVE void wasmBlurFocusedText()
        {
            UI::get().blur_focused_textfield_from_js();
        }
    }
#endif
}
