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

console.log('IME input policy verification passed');
