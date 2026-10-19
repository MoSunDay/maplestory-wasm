(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports) {
        module.exports = api;
    }
    if (root) {
        root.MapleCanvasResolution = api;
    }
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    const DESIGN_WIDTH = 800;
    const DESIGN_HEIGHT = 600;
    const PRESETS = Object.freeze([
        Object.freeze({ width: 800, height: 600 }),
        Object.freeze({ width: 1280, height: 720 })
    ]);

    function validSize(width, height) {
        return Number.isFinite(width) && Number.isFinite(height)
            && width > 0 && height > 0;
    }

    function isPreset(width, height) {
        return PRESETS.some(function (preset) {
            return preset.width === width && preset.height === height;
        });
    }

    function fitSize(viewW, viewH, gameW, gameH) {
        const designWidth = validSize(gameW, gameH) ? gameW : DESIGN_WIDTH;
        const designHeight = validSize(gameW, gameH) ? gameH : DESIGN_HEIGHT;
        const width = Number.isFinite(viewW) ? viewW : 0;
        const height = Number.isFinite(viewH) ? viewH : 0;

        if (width * designHeight <= height * designWidth) {
            const fittedWidth = Math.max(1, Math.floor(width));
            return {
                width: fittedWidth,
                height: Math.max(1, Math.floor(fittedWidth * designHeight / designWidth))
            };
        }

        const fittedHeight = Math.max(1, Math.floor(height));
        return {
            width: Math.max(1, Math.floor(fittedHeight * designWidth / designHeight)),
            height: fittedHeight
        };
    }

    function designOrigin(width, height) {
        const safeWidth = Number.isFinite(width) ? width : 0;
        const safeHeight = Number.isFinite(height) ? height : 0;
        return {
            x: Math.max(0, Math.floor((safeWidth - DESIGN_WIDTH) / 2)),
            y: Math.max(0, Math.floor((safeHeight - DESIGN_HEIGHT) / 2))
        };
    }

    function createController(options) {
        const settings = options || {};
        const getModule = typeof settings.getModule === 'function'
            ? settings.getModule
            : function () { return undefined; };
        const getCanvas = typeof settings.getCanvas === 'function'
            ? settings.getCanvas
            : function () { return undefined; };
        const getWindow = typeof settings.getWindow === 'function'
            ? settings.getWindow
            : function () { return undefined; };
        let resolution = { width: DESIGN_WIDTH, height: DESIGN_HEIGHT };

        function getResolution() {
            return { width: resolution.width, height: resolution.height };
        }

        function getDesignOrigin() {
            return designOrigin(resolution.width, resolution.height);
        }

        function viewportSize() {
            const win = getWindow();
            if (!win) {
                return { width: DESIGN_WIDTH, height: DESIGN_HEIGHT };
            }
            const viewport = win.visualViewport;
            if (viewport && validSize(viewport.width, viewport.height)) {
                return { width: viewport.width, height: viewport.height };
            }
            const documentElement = win.document && win.document.documentElement;
            const width = documentElement && documentElement.clientWidth
                ? documentElement.clientWidth
                : win.innerWidth;
            const height = documentElement && documentElement.clientHeight
                ? documentElement.clientHeight
                : win.innerHeight;
            if (validSize(width, height)) {
                return { width: width, height: height };
            }
            return { width: DESIGN_WIDTH, height: DESIGN_HEIGHT };
        }

        function applyScale() {
            const canvas = getCanvas();
            const viewport = viewportSize();
            const fitted = fitSize(
                viewport.width, viewport.height, resolution.width, resolution.height);

            if (canvas && canvas.style) {
                // The render surface keeps its selected resolution. Only the
                // presented size changes; horizontal centering stays in CSS.
                canvas.style.width = fitted.width + 'px';
                canvas.style.height = fitted.height + 'px';
                canvas.style.top = '0px';
            }
            return fitted;
        }

        function setResolution(width, height) {
            if (!isPreset(width, height)) {
                return false;
            }
            if (resolution.width === width && resolution.height === height) {
                return true;
            }

            resolution = { width: width, height: height };
            const module = getModule();
            if (module && typeof module.setCanvasSize === 'function') {
                module.setCanvasSize(width, height);
            } else {
                const canvas = getCanvas();
                if (canvas) {
                    canvas.width = width;
                    canvas.height = height;
                }
            }
            applyScale();
            return true;
        }

        return Object.freeze({
            getResolution,
            getDesignOrigin,
            setResolution,
            applyScale
        });
    }

    const installedControllers = new WeakMap();

    function install(options) {
        const settings = options || {};
        const win = settings.window || settings.root
            || (typeof settings.getWindow === 'function' ? settings.getWindow() : undefined)
            || (typeof globalThis !== 'undefined' ? globalThis : undefined);
        if (!win) {
            return createController(settings);
        }
        if (installedControllers.has(win)) {
            return installedControllers.get(win);
        }

        const controller = createController({
            getModule: settings.getModule || function () { return win.Module; },
            getCanvas: settings.getCanvas || function () {
                return win.document ? win.document.getElementById('canvas') : undefined;
            },
            getWindow: settings.getWindow || function () { return win; }
        });

        win.MapleWasmUI = controller;
        if (typeof win.addEventListener === 'function') {
            win.addEventListener('resize', controller.applyScale);
            if (win.document && typeof win.document.addEventListener === 'function') {
                win.document.addEventListener('fullscreenchange', controller.applyScale);
            }
        }
        controller.applyScale();
        installedControllers.set(win, controller);
        return controller;
    }

    return Object.freeze({
        DESIGN_WIDTH,
        DESIGN_HEIGHT,
        PRESETS,
        validSize,
        isPreset,
        fitSize,
        designOrigin,
        createController,
        install
    });
});
