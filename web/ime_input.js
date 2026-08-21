(function (root, factory) {
    const api = factory(root);
    if (typeof module === 'object' && module.exports) {
        module.exports = api;
    }
    root.MapleImeInput = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function (root) {
    'use strict';

    const GAME_WIDTH = 800;
    const GAME_HEIGHT = 600;
    const FORWARD_KEYS = Object.freeze({ 13: true, 9: true, 27: true, 38: true, 40: true });

    function shouldSync(active, composing, focused) {
        return active && !composing && focused;
    }

    function isImeOwnedKey(composing, event) {
        return composing || event.isComposing === true || event.keyCode === 229;
    }

    function clampCaret(value, caretUtf16) {
        const caret = typeof caretUtf16 === 'number' ? caretUtf16 : value.length;
        return Math.max(0, Math.min(caret, value.length));
    }

    function sameSnapshot(left, right) {
        return left.value === right.value && left.caret === right.caret;
    }

    function createBridge(document, moduleProvider, schedule) {
        const textarea = document.getElementById('ime-input');
        if (!textarea) {
            return null;
        }

        let active = false;
        let composing = false;
        let settlingComposition = false;
        let lastCompositionEnd = 0;
        let lastSynced = { value: '', caret: 0 };

        function moduleReady() {
            const module = moduleProvider();
            return module && typeof module.ccall === 'function';
        }

        function currentSnapshot() {
            return {
                value: textarea.value,
                caret: clampCaret(textarea.value, textarea.selectionStart)
            };
        }

        function syncToGame() {
            const compositionBlocked = composing || settlingComposition;
            if (!shouldSync(active, compositionBlocked, document.activeElement === textarea)
                || !moduleReady()) {
                return false;
            }
            const snapshot = currentSnapshot();
            if (sameSnapshot(lastSynced, snapshot)) {
                return false;
            }
            lastSynced = snapshot;
            moduleProvider().ccall(
                'msime_input', null, ['string', 'number'], [snapshot.value, snapshot.caret]);
            return true;
        }

        function placeTextarea(x, y, width, height) {
            const canvas = document.getElementById('canvas');
            if (!canvas) {
                return;
            }
            const rect = canvas.getBoundingClientRect();
            const scaleX = rect.width / GAME_WIDTH;
            const scaleY = rect.height / GAME_HEIGHT;
            textarea.style.left = (rect.left + x * scaleX) + 'px';
            textarea.style.top = (rect.top + y * scaleY) + 'px';
            textarea.style.width = Math.max(40, width * scaleX) + 'px';
            textarea.style.height = Math.max(24, height * scaleY) + 'px';
            textarea.style.fontSize = Math.max(12, height * scaleY * 0.8) + 'px';
        }

        const bridge = {
            get active() {
                return active;
            },
            get composing() {
                return composing;
            },
            onFocus: function (x, y, width, height, text, caretUtf16) {
                const value = text || '';
                const caret = clampCaret(value, caretUtf16);
                placeTextarea(x, y, width, height);
                active = true;
                composing = false;
                settlingComposition = false;
                textarea.value = value;
                textarea.setSelectionRange(caret, caret);
                lastSynced = { value, caret };
                textarea.focus({ preventScroll: true });
            },
            onBlur: function () {
                active = false;
                composing = false;
                settlingComposition = false;
                textarea.value = '';
                lastSynced = { value: '', caret: 0 };
                textarea.style.left = '-9999px';
                textarea.style.top = '-9999px';
                const canvas = document.getElementById('canvas');
                if (canvas) {
                    canvas.focus({ preventScroll: true });
                }
            },
            onText: function (text, caretUtf16) {
                if (!active || composing || settlingComposition) {
                    return;
                }
                const value = text || '';
                const caret = clampCaret(value, caretUtf16);
                const snapshot = { value, caret };
                lastSynced = snapshot;
                if (sameSnapshot(currentSnapshot(), snapshot)
                    && textarea.selectionEnd === caret) {
                    return;
                }
                textarea.value = value;
                textarea.setSelectionRange(caret, caret);
            }
        };

        textarea.addEventListener('compositionstart', function () {
            composing = true;
            settlingComposition = false;
        });

        textarea.addEventListener('compositionend', function () {
            composing = false;
            settlingComposition = true;
            lastCompositionEnd = Date.now();
            // Browsers differ on whether the final non-composing input event
            // runs before or after compositionend. Wait one task as a fallback
            // so a stale pinyin preedit value can never be committed here.
            schedule(function () {
                if (!settlingComposition) {
                    return;
                }
                settlingComposition = false;
                syncToGame();
            }, 0);
        });

        textarea.addEventListener('input', function (event) {
            if (composing || event.isComposing) {
                return;
            }
            if (settlingComposition) {
                settlingComposition = false;
            }
            syncToGame();
        });

        document.addEventListener('selectionchange', function () {
            syncToGame();
        });

        textarea.addEventListener('keydown', function (event) {
            if (!active) {
                return;
            }
            if (isImeOwnedKey(composing, event)) {
                // Keep browser IME default handling, but do not let its
                // physical pinyin keys bubble into GLFW's window handler.
                event.stopPropagation();
                return;
            }

            if (event.keyCode === 8) {
                event.preventDefault();
                event.stopPropagation();
                const start = textarea.selectionStart;
                const end = textarea.selectionEnd;
                if (start === end) {
                    if (start === 0) {
                        return;
                    }
                    let previous = start - 1;
                    const trail = textarea.value.charCodeAt(previous);
                    if (trail >= 0xDC00 && trail <= 0xDFFF && previous > 0) {
                        const lead = textarea.value.charCodeAt(previous - 1);
                        if (lead >= 0xD800 && lead <= 0xDBFF) {
                            previous--;
                        }
                    }
                    textarea.value = textarea.value.slice(0, previous) + textarea.value.slice(start);
                    textarea.setSelectionRange(previous, previous);
                } else {
                    textarea.value = textarea.value.slice(0, start) + textarea.value.slice(end);
                    textarea.setSelectionRange(start, start);
                }
                syncToGame();
                return;
            }

            if (FORWARD_KEYS[event.keyCode]) {
                if (event.keyCode === 13 && Date.now() - lastCompositionEnd < 100) {
                    event.preventDefault();
                    event.stopPropagation();
                    return;
                }
                event.preventDefault();
                event.stopPropagation();
                if (moduleReady()) {
                    moduleProvider().ccall(
                        'msime_key', null, ['number', 'number'], [event.keyCode, 1]);
                }
            }
        }, true);

        textarea.addEventListener('blur', function () {
            if (active) {
                schedule(function () {
                    if (active) {
                        textarea.focus({ preventScroll: true });
                    }
                }, 0);
            }
        });

        return bridge;
    }

    function install() {
        const bridge = createBridge(
            root.document,
            function () { return root.Module; },
            function (callback, delay) { return root.setTimeout(callback, delay); });
        if (!bridge) {
            return false;
        }
        root.MapleWasmIME = bridge;
        return true;
    }

    return Object.freeze({ shouldSync, isImeOwnedKey, clampCaret, sameSnapshot, createBridge, install });
});
