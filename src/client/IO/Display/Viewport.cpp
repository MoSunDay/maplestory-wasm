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
#include "Viewport.h"

#include "../../Constants.h"
#include "../../Graphics/GraphicsGL.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef __EMSCRIPTEN__
#include <GLFW/glfw3.h>
#endif

namespace jrc
{
    namespace viewport
    {
        namespace
        {
            void notify_browser_canvas(int16_t width, int16_t height)
            {
#ifdef __EMSCRIPTEN__
                EM_ASM({
                    const nextWidth = $0;
                    const nextHeight = $1;
                    const ui = typeof window !== "undefined"
                        ? window.MapleWasmUI
                        : null;

                    const applied = ui && typeof ui.setResolution === "function"
                        ? ui.setResolution(nextWidth, nextHeight) !== false
                        : false;
                    if (applied) {
                        return;
                    }

                    let canvas = typeof Module !== "undefined" && Module.canvas
                        ? Module.canvas
                        : null;
                    if (!canvas && typeof document !== "undefined") {
                        canvas = document.getElementById("canvas");
                    }
                    if (canvas) {
                        canvas.width = nextWidth;
                        canvas.height = nextHeight;
                    }
                }, width, height);
#else
                (void)width;
                (void)height;
#endif
            }

            display::Resolution supported_or_default(
                display::Resolution resolution
            )
            {
                return display::valid(resolution)
                    ? resolution
                    : display::default_resolution();
            }

#ifndef __EMSCRIPTEN__
            // Resize the native GLFW window to match the requested viewport.
            // In fullscreen the monitor owns the window size, so the actual
            // framebuffer size is returned instead: the logical viewport must
            // follow what is really rendered, not the requested preset.
            display::Resolution resize_native_window(
                display::Resolution resolution
            )
            {
                GLFWwindow* window = glfwGetCurrentContext();
                if (window == nullptr)
                {
                    return resolution;
                }

                GLFWmonitor* monitor = glfwGetWindowMonitor(window);
                if (monitor != nullptr)
                {
                    glfwSetWindowMonitor(
                        window,
                        monitor,
                        0,
                        0,
                        resolution.width,
                        resolution.height,
                        GLFW_DONT_CARE
                    );

                    int width = 0;
                    int height = 0;
                    glfwGetFramebufferSize(window, &width, &height);
                    if (width > 0 && height > 0)
                    {
                        return {
                            static_cast<int16_t>(width),
                            static_cast<int16_t>(height)
                        };
                    }

                    return resolution;
                }

                glfwSetWindowSize(window, resolution.width, resolution.height);
                return resolution;
            }
#endif
        }

        void apply_boot(display::Resolution resolution)
        {
            resolution = supported_or_default(resolution);

            Constants::set_viewsize(resolution.width, resolution.height);
            notify_browser_canvas(resolution.width, resolution.height);
        }

        void apply_runtime(display::Resolution resolution)
        {
            resolution = supported_or_default(resolution);

#ifndef __EMSCRIPTEN__
            // The WASM build sizes its canvas through notify_browser_canvas;
            // the native build must resize the GLFW window first so that the
            // constants and GL viewport follow the size actually rendered.
            resolution = resize_native_window(resolution);
#endif

            Constants::set_viewsize(resolution.width, resolution.height);
            notify_browser_canvas(resolution.width, resolution.height);

            glViewport(0, 0, resolution.width, resolution.height);
            GraphicsGL::get().set_screensize(
                resolution.width,
                resolution.height
            );
        }
    }
}
