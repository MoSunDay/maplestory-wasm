//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "Configuration.h"
#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#endif
#include "Constants.h"
#include "Error.h"
#include "Timer.h"

#include "Audio/Audio.h"
#include "Character/Char.h"
#include "Gameplay/Combat/DamageNumber.h"
#include "Gameplay/Stage.h"
#include "Graphics/GraphicsGL.h"
#include "IO/UI.h"
#include "IO/Window.h"
#include "Net/Session.h"
#include "Util/NxFiles.h"

#include <iostream>


namespace jrc
{
#ifdef MS_PLATFORM_WASM
    namespace
    {
        void set_loading_phase(const char* phase, const char* message, const char* detail)
        {
            EM_ASM({
                if (window.MapleWasmLoading)
                {
                    window.MapleWasmLoading.setPhase(
                        UTF8ToString($0), UTF8ToString($1), UTF8ToString($2));
                }
            }, phase, message, detail);
        }

        void finish_loading_screen()
        {
            EM_ASM({
                if (window.MapleWasmLoading)
                {
                    window.MapleWasmLoading.finish();
                }
            });
        }

        void show_loading_error(const char* message)
        {
            EM_ASM({
                if (window.MapleWasmLoading)
                {
                    window.MapleWasmLoading.fail(UTF8ToString($0));
                }
            }, message);
        }

        bool browser_connection_lost()
        {
            return EM_ASM_INT({
                return window.MapleConnectionLost &&
                    window.MapleConnectionLost.isLost() ? 1 : 0;
            }) != 0;
        }

        void show_connection_lost()
        {
            EM_ASM({
                if (window.MapleConnectionLost)
                {
                    window.MapleConnectionLost.show(
                        '连接已断开，游戏已经退出，请刷新页面重试。');
                }
            });
        }
    }
#endif

    Error init()
    {
#ifdef MS_PLATFORM_WASM
        auto loadConfigString = [](const char* key, auto& setting) {
            char* val = (char*)EM_ASM_INT({
                var k = UTF8ToString($0);
                if (typeof Module !== 'undefined' && Module.LazyFS && Module.LazyFS[k] !== undefined && Module.LazyFS[k] !== null) {
                    var str = Module.LazyFS[k].toString();
                    var lengthBytes = lengthBytesUTF8(str) + 1;
                    var stringOnWasmHeap = _malloc(lengthBytes);
                    stringToUTF8(str, stringOnWasmHeap, lengthBytes);
                    return stringOnWasmHeap;
                }
                return 0;
            }, key);
            
            if (val) {
                setting.save(std::string(val));
                free(val);
            }
        };

        loadConfigString("MapleStoryServerIp", Setting<MapleStoryServerIp>::get());
        loadConfigString("MapleStoryServerPort", Setting<MapleStoryServerPort>::get());
        loadConfigString("ProxyIP", Setting<ProxyIP>::get());
        loadConfigString("ProxyPort", Setting<ProxyPort>::get());
        loadConfigString("AssetsServerProtocol", Setting<AssetsServerProtocol>::get());
#endif

        if (Error error = Session::get().init())
        {
            return error;
        }

#ifdef MS_PLATFORM_WASM
        set_loading_phase(
            "assets",
            "正在加载游戏素材…",
            "首次进入可能需要更长时间，请不要关闭页面");
#endif
        if (Error error = NxFiles::init())
        {
            return error;
        }

#ifdef MS_PLATFORM_WASM
        set_loading_phase(
            "initializing",
            "正在初始化游戏界面…",
            "素材加载完成，即将进入游戏");
#endif
        if (Error error = Window::get().init())
        {
            return error;
        }

        if (Error error = Sound::init())
        {
            return error;
        }

        if (Error error = Music::init())
        {
            return error;
        }

        Char::init();
        DamageNumber::init();
        MapPortals::init();
        Stage::get().init();
        UI::get().init();

        return Error::NONE;
    }

    void update()
    {
        Window::get().check_events();
        Window::get().update();
#ifdef MS_PLATFORM_WASM
        const bool asset_blocking = GraphicsGL::get().hasblockingbitmaps() || EM_ASM_INT({
            return window.MapleAssetLoading?.isBlocking?.() ? 1 : 0;
        }) != 0;
        if (asset_blocking)
        {
            Stage::get().release_actions();
        }
        if (Stage::get().is_loading() || !asset_blocking)
        {
            Stage::get().update();
        }
        if (!asset_blocking)
        {
            UI::get().update();
        }
#else
        Stage::get().update();
        UI::get().update();
#endif
        Session::get().read();
    }

    void draw(float alpha)
    {
#ifdef MS_PLATFORM_WASM
        const bool graphics_blocking = GraphicsGL::get().hasblockingbitmaps();
        EM_ASM({
            if ($0) {
                if (!window.MapleAssetLoading?.has?.('gpu')) {
                    window.MapleAssetLoading?.begin('gpu', { message: '正在准备可见素材...' });
                }
            } else {
                window.MapleAssetLoading?.end('gpu');
            }
        }, graphics_blocking ? 1 : 0);
        const bool asset_loading = EM_ASM_INT({
            return window.MapleAssetLoading?.isBlocking?.() ? 1 : 0;
        }) != 0;
        GraphicsGL::get().preparebitmaps(asset_loading ? 8 : 2);
#endif
        Window::get().begin();
        Stage::get().draw(alpha);
        UI::get().draw(alpha);
        Window::get().end();
    }

    bool running()
    {
        return Session::get().is_connected()
            && UI::get().not_quitted()
            && Window::get().not_closed();
    }

#ifdef MS_PLATFORM_WASM
    extern "C" EMSCRIPTEN_KEEPALIVE void msmap_retry()
    {
        Stage::get().retry_loading();
    }

    extern "C" EMSCRIPTEN_KEEPALIVE int msstage_loading()
    {
        return Stage::get().is_loading() ? 1 : 0;
    }

    extern "C" EMSCRIPTEN_KEEPALIVE int msasset_blocking()
    {
        return GraphicsGL::get().hasblockingbitmaps() || EM_ASM_INT({
            return window.MapleAssetLoading?.isBlocking?.() ? 1 : 0;
        }) != 0;
    }

    static int64_t timestep = Constants::TIMESTEP * 1000;
    static int64_t accumulator = timestep;
    static int64_t period = 0;
    static int32_t samples = 0;

    void main_tick()
    {
        const bool connection_lost = browser_connection_lost();
        if (connection_lost || !running())
        {
            // The main loop is stopping permanently. Always release the startup
            // overlay first; otherwise a disconnect/quit that arrives before the
            // first rendered frame leaves the spinner stuck on screen forever.
            finish_loading_screen();
            const bool unexpected_session_close = !Session::get().is_connected()
                && UI::get().not_quitted()
                && Window::get().not_closed();
            if (!connection_lost && unexpected_session_close)
            {
                show_connection_lost();
            }
            emscripten_cancel_main_loop();
            Sound::close();
            return;
        }

        int64_t elapsed = Timer::get().stop();

        for (accumulator += elapsed; accumulator >= timestep; accumulator -= timestep)
            update();

        float alpha = static_cast<float>(accumulator) / timestep;
        draw(alpha);

        // Keep the DOM loading layer above the black canvas until the client
        // has actually completed its first rendered frame.
        static bool first_frame_presented = false;
        if (!first_frame_presented)
        {
            finish_loading_screen();
            first_frame_presented = true;
        }

        bool show_fps = true; // Hardcoded or from config
        if (show_fps)
        {
            if (samples < 100)
            {
                period += elapsed;
                samples++;
            }
            else if (period)
            {
                period = 0;
                samples = 0;
            }
        }
    }
#endif

    void loop()
    {
        Timer::get().start();

#ifdef MS_PLATFORM_WASM
        accumulator = timestep;
        emscripten_set_main_loop(main_tick, 0, 1);
#else
        int64_t timestep    = Constants::TIMESTEP * 1000;
        int64_t accumulator = timestep;

        int64_t period  = 0;
        int32_t samples = 0;

        while (running())
        {
            int64_t elapsed = Timer::get().stop();

            // Update game with constant timestep as many times as possible.
            for (accumulator += elapsed; accumulator >= timestep; accumulator -= timestep)
            {
                update();
            }

            // Draw the game. Interpolate to account for remaining time.
            float alpha = static_cast<float>(accumulator) / timestep;
            draw(alpha);

            if (samples < 100)
            {
                period += elapsed;
                samples++;
            }
            else if (period)
            {
                //int64_t fps = (samples * 1000000) / period;
                //std::cout << "FPS: " << fps << std::endl;

                period  = 0;
                samples = 0;
            }
        }

        Sound::close();
#endif
    }

    void start()
    {
        // Initialize and check for errors.
        if (Error error = init())
        {
            const char* message   = error.get_message();
            const char* args      = error.get_args();
            const bool  can_retry = error.can_retry();

            std::cout << "Error: " << message << args << std::endl;

#ifdef MS_PLATFORM_WASM
            const std::string loading_error = std::string(message) + args;
            show_loading_error(loading_error.c_str());
#endif
            std::string command;
            std::cin >> command;

            if (can_retry && command == "retry")
            {
                start();
            }
        }
        else
        {
            loop();
        }
    }
}

int main()
{
    jrc::start();
    return 0;
}
