import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import test from 'node:test';

const require = createRequire(import.meta.url);
const resolution = require('../canvas_resolution.js');

function fakeCanvas(width = 800, height = 600) {
    return { width, height, style: {} };
}

function fakeViewport(size = { width: 1280, height: 720 }) {
    const listeners = {};
    return {
        width: size.width,
        height: size.height,
        offsetLeft: Number.isFinite(size.offsetLeft) ? size.offsetLeft : 0,
        offsetTop: Number.isFinite(size.offsetTop) ? size.offsetTop : 0,
        listeners,
        addEventListener(type, handler) { listeners[type] = handler; }
    };
}

function fakeWindow(canvas, viewport = { width: 1280, height: 720 }) {
    const listeners = {};
    return {
        Module: undefined,
        visualViewport: fakeViewport(viewport),
        innerWidth: viewport.width,
        innerHeight: viewport.height,
        document: {
            documentElement: { clientWidth: viewport.width, clientHeight: viewport.height },
            getElementById: () => canvas,
            addEventListener: (type, handler) => { listeners[`doc:${type}`] = handler; }
        },
        listeners,
        addEventListener(type, handler) { listeners[type] = handler; }
    };
}

test('exports fixed presets and validates sizes', () => {
    assert.equal(resolution.DESIGN_WIDTH, 800);
    assert.equal(resolution.DESIGN_HEIGHT, 600);
    assert.deepEqual(
        resolution.PRESETS.map(preset => [preset.width, preset.height]),
        [[800, 600], [1280, 720]]);
    assert.equal(resolution.isPreset(800, 600), true);
    assert.equal(resolution.isPreset(1280, 720), true);
    assert.equal(resolution.isPreset(1024, 768), false);
    assert.equal(resolution.isPreset(800.5, 600), false);
    assert.equal(resolution.validSize(800, 600), true);
    assert.equal(resolution.validSize(0, 600), false);
    assert.equal(resolution.validSize(800, -1), false);
    assert.equal(resolution.validSize(Number.NaN, 600), false);
});

test('fitSize preserves the 4:3 design aspect and floors dimensions', () => {
    assert.deepEqual(resolution.fitSize(800, 600), { width: 800, height: 600 });
    assert.deepEqual(resolution.fitSize(1280, 720), { width: 960, height: 720 });
    assert.deepEqual(resolution.fitSize(1920, 1080), { width: 1440, height: 1080 });
    assert.deepEqual(resolution.fitSize(1000, 1000), { width: 1000, height: 750 });
    assert.deepEqual(resolution.fitSize(640, 480), { width: 640, height: 480 });

    const wide = resolution.fitSize(1280, 720);
    assert.equal(wide.width / wide.height, 4 / 3);
    const tall = resolution.fitSize(900, 1200);
    assert.equal(tall.width / tall.height, 4 / 3);
    assert.deepEqual(resolution.fitSize(900, 1200), { width: 900, height: 675 });
});

test('fitSize keeps at least one pixel for tiny viewports', () => {
    assert.deepEqual(resolution.fitSize(0, 0), { width: 1, height: 1 });
    assert.deepEqual(resolution.fitSize(0.5, 0.5), { width: 1, height: 1 });
    assert.deepEqual(resolution.fitSize(-5, -5), { width: 1, height: 1 });
});

test('designOrigin centers only canvas space larger than the design', () => {
    assert.deepEqual(resolution.designOrigin(800, 600), { x: 0, y: 0 });
    assert.deepEqual(resolution.designOrigin(1280, 720), { x: 240, y: 60 });
    assert.deepEqual(resolution.designOrigin(960, 720), { x: 80, y: 60 });
    assert.deepEqual(resolution.designOrigin(1920, 1080), { x: 560, y: 240 });
    assert.deepEqual(resolution.designOrigin(400, 300), { x: 0, y: 0 });
});

test('controller applies CSS scale without mutating the render surface', () => {
    const canvas = fakeCanvas();
    const win = fakeWindow(canvas);
    const calls = [];
    const module = {
        setCanvasSize(width, height) { calls.push({ width, height }); }
    };
    const controller = resolution.createController({
        getModule: () => module,
        getCanvas: () => canvas,
        getWindow: () => win
    });

    assert.deepEqual(controller.getResolution(), { width: 800, height: 600 });
    assert.deepEqual(controller.getDesignOrigin(), { x: 0, y: 0 });
    assert.deepEqual(controller.applyScale(), { width: 960, height: 720 });
    assert.equal(canvas.style.width, '960px');
    assert.equal(canvas.style.height, '720px');
    assert.equal(canvas.style.left, '640px');
    assert.equal(canvas.style.top, '0px');
    assert.equal(canvas.width, 800);
    assert.equal(canvas.height, 600);

    assert.equal(controller.setResolution(1280, 720), true);
    assert.deepEqual(calls, [{ width: 1280, height: 720 }]);
    assert.deepEqual(controller.getResolution(), { width: 1280, height: 720 });
    assert.deepEqual(controller.getDesignOrigin(), { x: 240, y: 60 });
    // Module.setCanvasSize is preferred, so direct canvas mutation is avoided.
    assert.equal(canvas.width, 800);
    assert.equal(canvas.height, 600);

    assert.equal(controller.setResolution(1280, 720), true);
    assert.equal(calls.length, 1);
    assert.equal(controller.setResolution(1024, 768), false);
    assert.equal(controller.setResolution(0, 600), false);
    assert.equal(controller.setResolution(-1280, -720), false);
    assert.deepEqual(controller.getResolution(), { width: 1280, height: 720 });
});

test('controller falls back to canvas dimensions and documentElement viewport', () => {
    const canvas = fakeCanvas();
    const win = fakeWindow(canvas, { width: 800, height: 600 });
    win.visualViewport = null;
    const controller = resolution.createController({
        getModule: () => undefined,
        getCanvas: () => canvas,
        getWindow: () => win
    });

    assert.equal(controller.setResolution(1280, 720), true);
    assert.equal(canvas.width, 1280);
    assert.equal(canvas.height, 720);
    assert.deepEqual(controller.applyScale(), { width: 800, height: 450 });
    assert.equal(canvas.style.width, '800px');
    assert.equal(canvas.style.height, '450px');
});

test('controller centers the canvas inside visualViewport offsets', () => {
    const canvas = fakeCanvas();
    const win = fakeWindow(canvas, {
        width: 1280, height: 720, offsetLeft: 20, offsetTop: 30
    });
    const controller = resolution.createController({
        getModule: () => undefined,
        getCanvas: () => canvas,
        getWindow: () => win
    });

    assert.deepEqual(controller.applyScale(), { width: 960, height: 720 });
    assert.equal(canvas.style.width, '960px');
    assert.equal(canvas.style.height, '720px');
    // translateX(-50%) stays in CSS, so left is the visual-viewport center.
    assert.equal(canvas.style.left, '660px');
    assert.equal(canvas.style.top, '30px');
});

test('applyScale repositions the installed IME bridge when present', () => {
    const canvas = fakeCanvas();
    const win = fakeWindow(canvas);
    let repositions = 0;
    win.MapleWasmIME = {
        reposition() { repositions += 1; }
    };
    const controller = resolution.createController({
        getModule: () => undefined,
        getCanvas: () => canvas,
        getWindow: () => win
    });

    assert.deepEqual(controller.applyScale(), { width: 960, height: 720 });
    assert.equal(repositions, 1);
    assert.equal(canvas.style.left, '640px');

    win.MapleWasmIME = {
        reposition() { throw new Error('ime unavailable'); }
    };
    assert.deepEqual(controller.applyScale(), { width: 960, height: 720 });
    assert.equal(repositions, 1);

    win.MapleWasmIME = undefined;
    assert.doesNotThrow(() => controller.applyScale());
});

test('install exposes a frozen controller and hooks viewport changes', () => {
    const canvas = fakeCanvas();
    const win = fakeWindow(canvas);
    const controller = resolution.install({
        window: win,
        getModule: () => undefined,
        getCanvas: () => canvas,
        getWindow: () => win
    });

    assert.equal(win.MapleWasmUI, controller);
    assert.equal(Object.isFrozen(controller), true);
    assert.equal(typeof win.listeners.resize, 'function');
    assert.equal(typeof win.listeners['doc:fullscreenchange'], 'function');
    assert.equal(win.visualViewport.listeners.resize, controller.applyScale);
    assert.equal(win.visualViewport.listeners.scroll, controller.applyScale);
    assert.equal(canvas.style.width, '960px');
    assert.equal(canvas.width, 800);
    assert.equal(resolution.install({ window: win }), controller);
});

test('install tolerates a visualViewport without addEventListener', () => {
    const canvas = fakeCanvas();
    const win = fakeWindow(canvas);
    delete win.visualViewport.addEventListener;
    const controller = resolution.install({
        window: win,
        getModule: () => undefined,
        getCanvas: () => canvas,
        getWindow: () => win
    });

    assert.equal(win.MapleWasmUI, controller);
    assert.equal(canvas.style.width, '960px');
});
