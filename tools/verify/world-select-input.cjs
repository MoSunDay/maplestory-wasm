const assert = require('assert');
const input = require('../../web/world_select_input.js');

assert.deepStrictEqual(
    input.toGamePoint({ left: 10, top: 20, width: 400, height: 300 }, 265, 150),
    { x: 510, y: 260 });
assert.strictEqual(input.contains(input.GO_BUTTON, { x: 510, y: 216 }), true);
assert.strictEqual(input.contains(input.CHANNEL_BUTTON, { x: 255, y: 260 }), true);
assert.strictEqual(input.contains(input.GO_BUTTON, { x: 255, y: 260 }), false);

let time = 1000;
let activations = 0;
const route = input.createRouter(function () {
    activations += 1;
    return true;
}, function () { return time; });

assert.strictEqual(route('click', { x: 255, y: 260 }), false);
time += 200;
assert.strictEqual(route('click', { x: 255, y: 260 }), true);
assert.strictEqual(route('click', { x: 510, y: 216 }), true);
assert.strictEqual(route('doubleclick', { x: 255, y: 260 }), true);
assert.strictEqual(activations, 3);

console.log('World-select browser input verification passed');

// Resolution-aware conversion: a 1280x720 render surface maps 1:1 while the
// design-space rectangles above stay anchored at 800x600 coordinates.
assert.deepStrictEqual(
    input.toGamePoint({ left: 10, top: 20, width: 1280, height: 720 }, 265, 150, {
        width: 1280,
        height: 720
    }),
    { x: 255, y: 130 });
assert.deepStrictEqual(
    input.toGamePoint({ left: 10, top: 20, width: 1280, height: 720 }, 265, 150),
    { x: 159.375, y: 108.33333333333333 });

// Resolution/origin accessors read the shared UI controller with fallbacks.
assert.deepStrictEqual(input.getResolution({}), { width: 800, height: 600 });
assert.deepStrictEqual(input.getDesignOrigin({}), { x: 0, y: 0 });
const uiRoot = {
    MapleWasmUI: {
        getResolution: function () { return { width: 1280, height: 720 }; },
        getDesignOrigin: function () { return { x: 240, y: 60 }; }
    }
};
assert.deepStrictEqual(input.getResolution(uiRoot), { width: 1280, height: 720 });
assert.deepStrictEqual(input.getDesignOrigin(uiRoot), { x: 240, y: 60 });

// A design point shifted by the canvas origin hits the same button.
const origin = { x: 240, y: 60 };
let shiftedActivations = 0;
const shiftedRoute = input.createRouter(function () {
    shiftedActivations += 1;
    return true;
}, function () { return 5000; });
assert.strictEqual(
    shiftedRoute('click', { x: 510 + origin.x, y: 216 + origin.y }, origin), true);
assert.strictEqual(
    shiftedRoute('doubleclick', { x: 255 + origin.x, y: 260 + origin.y }, origin), true);
assert.strictEqual(shiftedRoute('click', { x: 510, y: 216 }, origin), false);
assert.strictEqual(shiftedActivations, 2);

let shiftedClickTime = 100;
let shiftedClickActivations = 0;
const shiftedClickRoute = input.createRouter(function () {
    shiftedClickActivations += 1;
    return true;
}, function () { return shiftedClickTime; });
const shiftedChannel = { x: 255 + origin.x, y: 260 + origin.y };
assert.strictEqual(shiftedClickRoute('click', shiftedChannel, origin), false);
shiftedClickTime += 100;
assert.strictEqual(shiftedClickRoute('click', shiftedChannel, origin), true);
assert.strictEqual(shiftedClickActivations, 1);

// install resolves the current resolution and origin on every event.
const handlers = {};
const installCanvas = {
    dataset: {},
    addEventListener: function (type, handler) { handlers[type] = handler; },
    getBoundingClientRect: function () {
        return { left: 0, top: 0, width: 640, height: 360 };
    }
};
let entered = 0;
const installModule = {
    ccall: function (name) {
        if (name === 'msui_state') {
            return 2;
        }
        entered += 1;
        return 1;
    }
};
assert.strictEqual(input.install(installModule, installCanvas, uiRoot), true);
handlers.click({
    clientX: (510 + origin.x) / 2,
    clientY: (216 + origin.y) / 2
});
assert.strictEqual(entered, 1);

console.log('World-select resolution and origin verification passed');
