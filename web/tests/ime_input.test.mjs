import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import test from 'node:test';

const require = createRequire(import.meta.url);
const ime = require('../ime_input.js');

function fakeTextarea() {
    const listeners = {};
    return {
        value: '',
        selectionStart: 0,
        selectionEnd: 0,
        style: {},
        listeners,
        addEventListener(type, handler) { listeners[type] = handler; },
        setSelectionRange(start, end) {
            this.selectionStart = start;
            this.selectionEnd = end;
        },
        focus() {},
    };
}

function fakeCanvas() {
    return {
        getBoundingClientRect() {
            return { left: 0, top: 0, width: 800, height: 600 };
        }
    };
}

function fakeDocument(textarea, canvas) {
    return {
        activeElement: textarea,
        getElementById(id) {
            if (id === 'ime-input') return textarea;
            if (id === 'canvas') return canvas;
            return null;
        },
        addEventListener() {},
    };
}

function createFixture() {
    const textarea = fakeTextarea();
    const calls = [];
    const bridge = ime.createBridge(
        fakeDocument(textarea, fakeCanvas()),
        () => ({ ccall: (...args) => calls.push(args) }),
        () => {},
    );
    return { textarea, bridge, calls };
}

test('bridge installs a beforeinput listener on the ime textarea', () => {
    const { textarea, bridge } = createFixture();
    assert.ok(bridge);
    assert.equal(typeof textarea.listeners.beforeinput, 'function');
});

test('line break insertions are cancelled so they cannot reach the game field', () => {
    const { textarea } = createFixture();

    for (const inputType of ['insertLineBreak', 'insertParagraph']) {
        let prevented = false;
        textarea.listeners.beforeinput({
            inputType,
            preventDefault() { prevented = true; },
        });
        assert.equal(prevented, true, `${inputType} must be cancelled`);
    }
});

test('regular text insertions still pass through', () => {
    const { textarea } = createFixture();

    let prevented = false;
    textarea.listeners.beforeinput({
        inputType: 'insertText',
        data: 'a',
        preventDefault() { prevented = true; },
    });
    assert.equal(prevented, false);
});

test('focusing from an in-flight Enter keydown syncs no text', () => {
    // Reproduces the bug: Enter focuses the chat field from C++ while the
    // keydown is still dispatching; the browser default action then tries to
    // insert a line break into the just-focused textarea.
    const { textarea, bridge, calls } = createFixture();

    bridge.onFocus(0, 0, 80, 20, '', 0);
    assert.equal(textarea.value, '');

    textarea.listeners.beforeinput({
        inputType: 'insertLineBreak',
        preventDefault() {},
    });

    assert.equal(textarea.value, '');
    assert.equal(calls.length, 0);
});
