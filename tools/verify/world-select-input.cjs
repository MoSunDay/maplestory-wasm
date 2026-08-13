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
