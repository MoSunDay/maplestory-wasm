(function (root, factory) {
    const api = factory(root);
    if (typeof module === 'object' && module.exports) {
        module.exports = api;
    }
    if (root) {
        root.MapleWorldSelectInput = api;
    }
})(typeof globalThis !== 'undefined' ? globalThis : this, function (root) {
    'use strict';

    const GAME_WIDTH = 800;
    const GAME_HEIGHT = 600;
    const WORLD_SELECT_STATE = 2;
    const DOUBLE_CLICK_MS = 350;
    // These are the visible normal-state bitmap rectangles from UI.nx after
    // applying UIWorldSelect's channel-panel anchor.
    const GO_BUTTON = Object.freeze({ left: 445, top: 201, right: 576, bottom: 232 });
    const CHANNEL_BUTTON = Object.freeze({ left: 222, top: 246, right: 290, bottom: 273 });

    function positiveSize(width, height) {
        return Number.isFinite(width) && Number.isFinite(height)
            && width > 0 && height > 0;
    }

    function resolutionOrDefault(resolution) {
        return positiveSize(resolution && resolution.width, resolution && resolution.height)
            ? { width: resolution.width, height: resolution.height }
            : { width: GAME_WIDTH, height: GAME_HEIGHT };
    }

    function getResolution(uiRoot) {
        const target = uiRoot || root;
        const ui = target && target.MapleWasmUI;
        if (ui && typeof ui.getResolution === 'function') {
            try {
                const resolution = ui.getResolution();
                if (positiveSize(resolution && resolution.width, resolution && resolution.height)) {
                    return { width: resolution.width, height: resolution.height };
                }
            } catch (error) {
                // Fall through to the design resolution.
            }
        }
        return { width: GAME_WIDTH, height: GAME_HEIGHT };
    }

    function getDesignOrigin(uiRoot) {
        const target = uiRoot || root;
        const ui = target && target.MapleWasmUI;
        if (ui && typeof ui.getDesignOrigin === 'function') {
            try {
                const origin = ui.getDesignOrigin();
                if (origin && Number.isFinite(origin.x) && Number.isFinite(origin.y)) {
                    return { x: origin.x, y: origin.y };
                }
            } catch (error) {
                // Fall through to the top-left game origin.
            }
        }
        return { x: 0, y: 0 };
    }

    function toGamePoint(bounds, clientX, clientY, resolution) {
        if (!bounds || bounds.width <= 0 || bounds.height <= 0) {
            return null;
        }
        const size = resolutionOrDefault(resolution);
        return {
            x: (clientX - bounds.left) * size.width / bounds.width,
            y: (clientY - bounds.top) * size.height / bounds.height
        };
    }

    function contains(rectangle, point) {
        return point !== null
            && point.x >= rectangle.left && point.x <= rectangle.right
            && point.y >= rectangle.top && point.y <= rectangle.bottom;
    }

    function designPoint(point, origin) {
        if (!point) {
            return null;
        }
        const shift = origin && Number.isFinite(origin.x) && Number.isFinite(origin.y)
            ? origin
            : { x: 0, y: 0 };
        return { x: point.x - shift.x, y: point.y - shift.y };
    }

    function createRouter(activate, now) {
        let lastChannelClickAt = -Infinity;
        return function route(kind, point, origin) {
            const time = now();
            const local = designPoint(point, origin);
            if (kind === 'click' && contains(GO_BUTTON, local)) {
                return activate();
            }
            if (kind === 'doubleclick' && contains(CHANNEL_BUTTON, local)) {
                return activate();
            }
            if (kind !== 'click' || !contains(CHANNEL_BUTTON, local)) {
                return false;
            }

            const isSecondClick = time - lastChannelClickAt <= DOUBLE_CLICK_MS;
            lastChannelClickAt = time;
            return isSecondClick ? activate() : false;
        };
    }

    function install(module, canvas, uiRoot) {
        if (!module || !canvas || canvas.dataset.worldSelectInput === 'installed') {
            return false;
        }

        const activate = function () {
            if (module.ccall('msui_state', 'number', [], []) !== WORLD_SELECT_STATE) {
                return false;
            }
            return module.ccall('msworldselect_enter', 'number', [], []) === 1;
        };
        const route = createRouter(activate, function () { return performance.now(); });
        const point = function (event) {
            return toGamePoint(
                canvas.getBoundingClientRect(),
                event.clientX,
                event.clientY,
                getResolution(uiRoot));
        };
        const origin = function () {
            return getDesignOrigin(uiRoot);
        };

        canvas.addEventListener('click', function (event) {
            route('click', point(event), origin());
        });
        canvas.addEventListener('dblclick', function (event) {
            if (route('doubleclick', point(event), origin())) {
                event.preventDefault();
            }
        });
        canvas.dataset.worldSelectInput = 'installed';
        return true;
    }

    return Object.freeze({
        GO_BUTTON,
        CHANNEL_BUTTON,
        getResolution,
        getDesignOrigin,
        toGamePoint,
        contains,
        createRouter,
        install
    });
});
