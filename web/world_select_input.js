(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports) {
        module.exports = api;
    }
    root.MapleWorldSelectInput = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    const GAME_WIDTH = 800;
    const GAME_HEIGHT = 600;
    const WORLD_SELECT_STATE = 2;
    const DOUBLE_CLICK_MS = 350;
    // These are the visible normal-state bitmap rectangles from UI.nx after
    // applying UIWorldSelect's channel-panel anchor.
    const GO_BUTTON = Object.freeze({ left: 445, top: 201, right: 576, bottom: 232 });
    const CHANNEL_BUTTON = Object.freeze({ left: 222, top: 246, right: 290, bottom: 273 });

    function toGamePoint(bounds, clientX, clientY) {
        if (!bounds || bounds.width <= 0 || bounds.height <= 0) {
            return null;
        }
        return {
            x: (clientX - bounds.left) * GAME_WIDTH / bounds.width,
            y: (clientY - bounds.top) * GAME_HEIGHT / bounds.height
        };
    }

    function contains(rectangle, point) {
        return point !== null
            && point.x >= rectangle.left && point.x <= rectangle.right
            && point.y >= rectangle.top && point.y <= rectangle.bottom;
    }

    function createRouter(activate, now) {
        let lastChannelClickAt = -Infinity;
        return function route(kind, point) {
            const time = now();
            if (kind === 'click' && contains(GO_BUTTON, point)) {
                return activate();
            }
            if (kind === 'doubleclick' && contains(CHANNEL_BUTTON, point)) {
                return activate();
            }
            if (kind !== 'click' || !contains(CHANNEL_BUTTON, point)) {
                return false;
            }

            const isSecondClick = time - lastChannelClickAt <= DOUBLE_CLICK_MS;
            lastChannelClickAt = time;
            return isSecondClick ? activate() : false;
        };
    }

    function install(module, canvas) {
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
            return toGamePoint(canvas.getBoundingClientRect(), event.clientX, event.clientY);
        };

        canvas.addEventListener('click', function (event) {
            route('click', point(event));
        });
        canvas.addEventListener('dblclick', function (event) {
            if (route('doubleclick', point(event))) {
                event.preventDefault();
            }
        });
        canvas.dataset.worldSelectInput = 'installed';
        return true;
    }

    return Object.freeze({
        GO_BUTTON,
        CHANNEL_BUTTON,
        toGamePoint,
        contains,
        createRouter,
        install
    });
});
