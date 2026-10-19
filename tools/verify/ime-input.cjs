const assert = require('assert');
const ime = require('../../web/ime_input.js');

assert.strictEqual(ime.shouldSync(true, false, true), true);
assert.strictEqual(ime.shouldSync(true, true, true), false);
assert.strictEqual(ime.shouldSync(true, false, false), false);
assert.strictEqual(ime.shouldSync(false, false, true), false);

assert.strictEqual(ime.isImeOwnedKey(true, { isComposing: false, keyCode: 65 }), true);
assert.strictEqual(ime.isImeOwnedKey(false, { isComposing: true, keyCode: 65 }), true);
assert.strictEqual(ime.isImeOwnedKey(false, { isComposing: false, keyCode: 229 }), true);
assert.strictEqual(ime.isImeOwnedKey(false, { isComposing: false, keyCode: 65 }), false);

assert.strictEqual(ime.clampCaret('中文', -1), 0);
assert.strictEqual(ime.clampCaret('中文', 1), 1);
assert.strictEqual(ime.clampCaret('中文', 9), 2);
assert.strictEqual(ime.sameSnapshot(
    { value: '中文', caret: 2 }, { value: '中文', caret: 2 }), true);
assert.strictEqual(ime.sameSnapshot(
    { value: 'zhongwen', caret: 8 }, { value: '中文', caret: 2 }), false);
assert.deepStrictEqual(ime.selectionRange('1', 1, true), { start: 0, end: 1 });
assert.deepStrictEqual(ime.selectionRange('123', 2, false), { start: 2, end: 2 });
assert.deepStrictEqual(ime.selectionRange('', 4, true), { start: 0, end: 0 });

// The design-space field rectangle follows the current canvas resolution and
// its on-screen bounds, including a non-zero canvas offset.
assert.deepStrictEqual(
    ime.scaleField(
        { left: 100, top: 50, width: 640, height: 360 },
        200, 100, 400, 100, { width: 1280, height: 720 }),
    { left: 200, top: 100, width: 200, height: 50, fontSize: 40 });
assert.deepStrictEqual(
    ime.scaleField(
        { left: 0, top: 0, width: 1280, height: 720 },
        240, 60, 100, 40, { width: 1280, height: 720 }),
    { left: 240, top: 60, width: 100, height: 40, fontSize: 32 });
assert.deepStrictEqual(
    ime.scaleField(
        { left: 0, top: 0, width: 400, height: 300 },
        0, 0, 40, 20, { width: 800, height: 600 }),
    { left: 0, top: 0, width: 40, height: 24, fontSize: 12 });
assert.deepStrictEqual(
    ime.scaleField(
        { left: 0, top: 0, width: 800, height: 600 },
        100, 100, 200, 100),
    { left: 100, top: 100, width: 200, height: 100, fontSize: 80 });

console.log('IME input policy verification passed');
