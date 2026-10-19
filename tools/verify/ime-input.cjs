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

// createBridge keeps the focused field rectangle and can replay it after the
// canvas presentation changes (pinch zoom, resolution switch, and so on).
function fakeDom(initialRect) {
    const document = {
        activeElement: null,
        getElementById(id) {
            if (id === 'canvas') return canvas;
            if (id === 'ime-input') return textarea;
            return null;
        },
        addEventListener() {}
    };
    const canvas = {
        rect: initialRect,
        getBoundingClientRect() { return canvas.rect; },
        focus() { document.activeElement = canvas; }
    };
    const textarea = {
        style: {},
        value: '',
        selectionStart: 0,
        selectionEnd: 0,
        setSelectionRange(start, end) {
            textarea.selectionStart = start;
            textarea.selectionEnd = end;
        },
        focus() { document.activeElement = textarea; },
        addEventListener() {}
    };
    return { document, canvas, textarea };
}

const dom = fakeDom({ left: 0, top: 0, width: 800, height: 600 });
assert.strictEqual(ime.createBridge({ getElementById: () => null }, () => null,
    callback => callback()), null);
const bridge = ime.createBridge(dom.document, () => null, callback => callback());
assert.ok(bridge);
assert.strictEqual(bridge.reposition(), false);

bridge.onFocus(200, 100, 400, 100, 'hi', 2);
assert.strictEqual(bridge.active, true);
assert.strictEqual(dom.textarea.style.left, '200px');
assert.strictEqual(dom.textarea.style.top, '100px');
assert.strictEqual(dom.textarea.style.width, '400px');
assert.strictEqual(dom.textarea.style.height, '100px');

dom.canvas.rect = { left: 40, top: 20, width: 1280, height: 720 };
assert.strictEqual(bridge.reposition(), true);
assert.strictEqual(dom.textarea.style.left, '360px');
assert.strictEqual(dom.textarea.style.top, '140px');
assert.strictEqual(dom.textarea.style.width, '640px');
assert.strictEqual(dom.textarea.style.height, '120px');

assert.strictEqual(bridge.reposition(240, 60, 100, 40), true);
assert.strictEqual(dom.textarea.style.left, '424px');
assert.strictEqual(dom.textarea.style.top, '92px');
assert.strictEqual(dom.textarea.style.width, '160px');
assert.strictEqual(dom.textarea.style.height, '48px');

bridge.onBlur();
assert.strictEqual(bridge.active, false);
assert.strictEqual(bridge.reposition(), false);
assert.strictEqual(dom.textarea.style.left, '-9999px');

console.log('IME input policy verification passed');
