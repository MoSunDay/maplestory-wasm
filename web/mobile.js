(function () {
    const GAME_WIDTH = 800;
    const GAME_HEIGHT = 600;
    const DEFAULT_STORAGE_KEY = 'maplewasm.mobileControls.v1';
    const MIN_BUTTON_SIZE = 0.06;
    const MAX_BUTTON_SIZE = 0.35;
    const NEW_BUTTON_SIZE = 0.12;

    const KEY_DEFS = [];

    function addKey(code, label, shortLabel, glfw) {
        KEY_DEFS.push({ code, label, shortLabel, glfw });
    }

    for (let digit = 0; digit <= 9; digit += 1) {
        addKey(`Digit${digit}`, `${digit}`, `${digit}`, 48 + digit);
    }

    for (let code = 65; code <= 90; code += 1) {
        const letter = String.fromCharCode(code);
        addKey(`Key${letter}`, letter, letter, code);
    }

    addKey('ArrowLeft', 'Left Arrow', 'Left', 263);
    addKey('ArrowRight', 'Right Arrow', 'Right', 262);
    addKey('ArrowUp', 'Up Arrow', 'Up', 265);
    addKey('ArrowDown', 'Down Arrow', 'Down', 264);
    addKey('Space', 'Space', 'Space', 32);
    addKey('Enter', 'Enter', 'Enter', 257);
    addKey('Tab', 'Tab', 'Tab', 258);
    addKey('Escape', 'Escape', 'Esc', 256);
    addKey('Backspace', 'Backspace', 'Bksp', 259);
    addKey('Insert', 'Insert', 'Ins', 260);
    addKey('Delete', 'Delete', 'Del', 261);
    addKey('Home', 'Home', 'Home', 268);
    addKey('End', 'End', 'End', 269);
    addKey('PageUp', 'Page Up', 'PgUp', 266);
    addKey('PageDown', 'Page Down', 'PgDn', 267);
    addKey('LeftShift', 'Left Shift', 'Shift', 340);
    addKey('LeftControl', 'Left Control', 'Ctrl', 341);
    addKey('LeftAlt', 'Left Alt', 'Alt', 342);
    addKey('RightShift', 'Right Shift', 'RShift', 344);
    addKey('RightControl', 'Right Control', 'RCtrl', 345);
    addKey('RightAlt', 'Right Alt', 'RAlt', 346);
    addKey('Backquote', 'Backquote', '`', 96);
    addKey('Minus', 'Minus', '-', 45);
    addKey('Equal', 'Equal', '=', 61);
    addKey('BracketLeft', 'Left Bracket', '[', 91);
    addKey('BracketRight', 'Right Bracket', ']', 93);
    addKey('Backslash', 'Backslash', '\\', 92);
    addKey('Semicolon', 'Semicolon', ';', 59);
    addKey('Quote', 'Quote', '\'', 39);
    addKey('Comma', 'Comma', ',', 44);
    addKey('Period', 'Period', '.', 46);
    addKey('Slash', 'Slash', '/', 47);

    for (let number = 1; number <= 12; number += 1) {
        addKey(`F${number}`, `F${number}`, `F${number}`, 289 + number);
    }

    const KEY_MAP = new Map(KEY_DEFS.map((definition) => [definition.code, definition]));

    let generatedButtonId = 1;

    const state = {
        config: {
            enabled: false,
            debugDesktop: false,
            storageKey: DEFAULT_STORAGE_KEY
        },
        defaultButtons: [],
        buttons: [],
        active: false,
        runtimeReady: false,
        runtime: {
            sendKey: null,
            syncText: null,
            submitText: null,
            blurText: null
        },
        controlsRoot: null,
        controlsLayer: null,
        editorToggle: null,
        editorPanel: null,
        editorStatus: null,
        editorLabelInput: null,
        editorKeySelect: null,
        editorDeleteButton: null,
        nativeInput: null,
        selectedButtonId: null,
        editorOpen: false,
        textField: null,
        textInputVisible: false,
        suppressInputSync: false,
        suppressBlurCallback: false,
        pointerBindings: new Map(),
        keyRefCounts: new Map(),
        pressedButtons: new Set(),
        buttonElements: new Map(),
        editorInteraction: null
    };

    function clamp(value, min, max) {
        return Math.min(max, Math.max(min, value));
    }

    function getCanvas() {
        return document.getElementById('canvas');
    }

    function getCanvasRect() {
        const canvas = getCanvas();
        if (!canvas) {
            return null;
        }

        const rect = canvas.getBoundingClientRect();
        if (rect.width <= 0 || rect.height <= 0) {
            return null;
        }

        return rect;
    }

    function nextButtonId() {
        return `mobile-btn-${generatedButtonId++}`;
    }

    function shortLabelForKey(code) {
        const definition = KEY_MAP.get(code);
        return definition ? definition.shortLabel : code;
    }

    function sanitizeButton(rawButton) {
        const definition = KEY_MAP.get(rawButton && rawButton.key) || KEY_MAP.get('LeftControl');
        const width = clamp(Number(rawButton && rawButton.w) || NEW_BUTTON_SIZE, MIN_BUTTON_SIZE, MAX_BUTTON_SIZE);
        const height = clamp(Number(rawButton && rawButton.h) || NEW_BUTTON_SIZE, MIN_BUTTON_SIZE, MAX_BUTTON_SIZE);
        const x = clamp(Number(rawButton && rawButton.x) || 0, 0, 1 - width);
        const y = clamp(Number(rawButton && rawButton.y) || 0, 0, 1 - height);
        const label = typeof (rawButton && rawButton.label) === 'string' && rawButton.label.trim() ?
            rawButton.label.trim().slice(0, 12) :
            shortLabelForKey(definition.code);

        return {
            id: typeof (rawButton && rawButton.id) === 'string' && rawButton.id ? rawButton.id : nextButtonId(),
            label,
            key: definition.code,
            x,
            y,
            w: width,
            h: height
        };
    }

    function sanitizeButtons(rawButtons) {
        if (!Array.isArray(rawButtons)) {
            return [];
        }

        const seen = new Set();
        return rawButtons.map(sanitizeButton).map((button) => {
            if (seen.has(button.id)) {
                button.id = nextButtonId();
            }
            seen.add(button.id);
            return button;
        });
    }

    function cloneButtons(buttons) {
        return buttons.map((button) => ({ ...button }));
    }

    function detectMobileEnvironment() {
        if (state.config.debugDesktop) {
            return true;
        }

        if (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) {
            return true;
        }

        return navigator.maxTouchPoints > 0;
    }

    function storageKey() {
        return typeof state.config.storageKey === 'string' && state.config.storageKey ?
            state.config.storageKey :
            DEFAULT_STORAGE_KEY;
    }

    function saveButtons() {
        try {
            localStorage.setItem(storageKey(), JSON.stringify(state.buttons));
        } catch (error) {
            console.warn('[mobile] Failed to save touch controls:', error);
        }
    }

    function loadButtons() {
        let storedButtons = null;
        try {
            const storedValue = localStorage.getItem(storageKey());
            if (storedValue) {
                storedButtons = sanitizeButtons(JSON.parse(storedValue));
            }
        } catch (error) {
            console.warn('[mobile] Failed to load saved touch controls:', error);
        }

        if (storedButtons && storedButtons.length > 0) {
            return storedButtons;
        }

        return cloneButtons(state.defaultButtons);
    }

    function getSelectedButton() {
        return state.buttons.find((button) => button.id === state.selectedButtonId) || null;
    }

    function getButtonById(buttonId) {
        return state.buttons.find((button) => button.id === buttonId) || null;
    }

    function installStyles() {
        if (document.getElementById('maple-mobile-style')) {
            return;
        }

        const style = document.createElement('style');
        style.id = 'maple-mobile-style';
        style.textContent = `
            html, body {
                overscroll-behavior: none;
            }

            #container,
            #canvas {
                touch-action: none;
            }

            #maple-mobile-root {
                position: fixed;
                inset: 0;
                z-index: 20;
                pointer-events: none;
            }

            #maple-mobile-controls {
                position: absolute;
                inset: 0;
            }

            .maple-mobile-control {
                position: fixed;
                display: inline-flex;
                align-items: center;
                justify-content: center;
                border: 1px solid rgba(255, 255, 255, 0.24);
                border-radius: 18px;
                background: linear-gradient(180deg, rgba(35, 46, 69, 0.78), rgba(18, 24, 40, 0.88));
                color: #f7f8fb;
                box-shadow: 0 14px 28px rgba(0, 0, 0, 0.28);
                font: 600 14px/1.1 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
                letter-spacing: 0.02em;
                pointer-events: auto;
                user-select: none;
                touch-action: none;
                backdrop-filter: blur(6px);
                -webkit-user-select: none;
            }

            .maple-mobile-control.pressed {
                transform: scale(0.96);
                background: linear-gradient(180deg, rgba(66, 92, 131, 0.9), rgba(32, 47, 72, 0.92));
            }

            .maple-mobile-control.edit-mode {
                background: linear-gradient(180deg, rgba(83, 58, 28, 0.82), rgba(52, 36, 18, 0.9));
                border-color: rgba(255, 206, 124, 0.42);
            }

            .maple-mobile-control.selected {
                outline: 2px solid rgba(255, 214, 135, 0.95);
                outline-offset: 2px;
            }

            .maple-mobile-control-label {
                pointer-events: none;
                padding: 0 8px;
                text-shadow: 0 1px 1px rgba(0, 0, 0, 0.65);
            }

            .maple-mobile-control-handle {
                position: absolute;
                right: 4px;
                bottom: 4px;
                width: 12px;
                height: 12px;
                border-radius: 999px;
                background: rgba(255, 214, 135, 0.95);
                border: 1px solid rgba(58, 39, 16, 0.95);
                display: none;
                pointer-events: auto;
            }

            #maple-mobile-root.editor-open .maple-mobile-control-handle {
                display: block;
            }

            #maple-mobile-editor-toggle,
            #maple-mobile-editor-panel button,
            #maple-native-input {
                pointer-events: auto;
            }

            #maple-mobile-editor-toggle {
                position: fixed;
                top: 14px;
                right: 14px;
                min-width: 64px;
                border: 0;
                border-radius: 999px;
                background: rgba(12, 18, 29, 0.82);
                color: #f7f8fb;
                padding: 10px 16px;
                font: 600 13px/1 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
                box-shadow: 0 12px 26px rgba(0, 0, 0, 0.28);
            }

            #maple-mobile-editor-panel {
                position: fixed;
                top: 64px;
                right: 14px;
                width: min(280px, calc(100vw - 28px));
                display: none;
                background: rgba(10, 14, 22, 0.94);
                color: #f7f8fb;
                border: 1px solid rgba(255, 255, 255, 0.12);
                border-radius: 18px;
                padding: 14px;
                box-shadow: 0 18px 42px rgba(0, 0, 0, 0.38);
                font: 500 13px/1.35 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
            }

            #maple-mobile-editor-panel.open {
                display: block;
            }

            .maple-mobile-editor-title {
                font-weight: 700;
                margin-bottom: 6px;
            }

            .maple-mobile-editor-row {
                display: flex;
                gap: 8px;
                margin-top: 10px;
            }

            .maple-mobile-editor-row button,
            .maple-mobile-editor-field input,
            .maple-mobile-editor-field select {
                width: 100%;
                border-radius: 12px;
                border: 1px solid rgba(255, 255, 255, 0.14);
                background: rgba(255, 255, 255, 0.08);
                color: #f7f8fb;
                padding: 10px 12px;
                font: inherit;
            }

            .maple-mobile-editor-field {
                margin-top: 10px;
            }

            .maple-mobile-editor-field label {
                display: block;
                margin-bottom: 6px;
                color: rgba(255, 255, 255, 0.72);
            }

            .maple-mobile-editor-help {
                margin: 10px 0 0;
                color: rgba(255, 255, 255, 0.72);
            }

            #maple-native-input {
                position: fixed;
                display: none;
                margin: 0;
                padding: 0;
                border: 0;
                outline: none;
                background: transparent;
                color: transparent;
                caret-color: transparent;
                opacity: 0.01;
                font-size: 16px;
                touch-action: manipulation;
            }
        `;

        document.head.appendChild(style);
    }

    function ensureElements() {
        if (state.controlsRoot) {
            return;
        }

        installStyles();

        const root = document.createElement('div');
        root.id = 'maple-mobile-root';

        const controlsLayer = document.createElement('div');
        controlsLayer.id = 'maple-mobile-controls';
        root.appendChild(controlsLayer);

        const editorToggle = document.createElement('button');
        editorToggle.id = 'maple-mobile-editor-toggle';
        editorToggle.type = 'button';
        editorToggle.textContent = 'Edit';
        editorToggle.addEventListener('click', () => {
            state.editorOpen = !state.editorOpen;
            if (state.editorOpen && !state.selectedButtonId && state.buttons.length > 0) {
                state.selectedButtonId = state.buttons[0].id;
            }
            state.editorInteraction = null;
            releaseAllActiveKeys();
            updateEditorPanel();
            refreshLayouts();
        });
        root.appendChild(editorToggle);

        const editorPanel = document.createElement('div');
        editorPanel.id = 'maple-mobile-editor-panel';
        editorPanel.innerHTML = `
            <div class="maple-mobile-editor-title">Touch Controls</div>
            <div id="maple-mobile-editor-status"></div>
            <div class="maple-mobile-editor-row">
                <button type="button" id="maple-mobile-add-button">Add Button</button>
                <button type="button" id="maple-mobile-reset-buttons">Reset</button>
            </div>
            <div class="maple-mobile-editor-field">
                <label for="maple-mobile-button-label">Label</label>
                <input id="maple-mobile-button-label" maxlength="12" />
            </div>
            <div class="maple-mobile-editor-field">
                <label for="maple-mobile-button-key">Key</label>
                <select id="maple-mobile-button-key"></select>
            </div>
            <div class="maple-mobile-editor-row">
                <button type="button" id="maple-mobile-delete-button">Delete</button>
            </div>
            <p class="maple-mobile-editor-help">Tap a button to select it. Drag to move it. Drag the corner dot to resize it.</p>
        `;
        root.appendChild(editorPanel);

        const nativeInput = document.createElement('input');
        nativeInput.id = 'maple-native-input';
        nativeInput.type = 'text';
        nativeInput.autocomplete = 'off';
        nativeInput.autocorrect = 'off';
        nativeInput.autocapitalize = 'off';
        nativeInput.spellcheck = false;
        nativeInput.enterKeyHint = 'done';
        nativeInput.addEventListener('input', syncNativeInputToWasm);
        nativeInput.addEventListener('select', syncNativeInputToWasm);
        nativeInput.addEventListener('click', syncNativeInputToWasm);
        nativeInput.addEventListener('keyup', syncNativeInputToWasm);
        nativeInput.addEventListener('keydown', (event) => {
            if (event.key === 'Enter') {
                event.preventDefault();
                if (state.runtimeReady && state.runtime.submitText) {
                    state.runtime.submitText();
                }
            }
        });
        nativeInput.addEventListener('blur', () => {
            if (!state.textInputVisible || state.suppressBlurCallback) {
                return;
            }

            if (state.runtimeReady && state.runtime.blurText) {
                state.runtime.blurText();
            }
        });
        root.appendChild(nativeInput);

        document.body.appendChild(root);

        state.controlsRoot = root;
        state.controlsLayer = controlsLayer;
        state.editorToggle = editorToggle;
        state.editorPanel = editorPanel;
        state.editorStatus = editorPanel.querySelector('#maple-mobile-editor-status');
        state.editorLabelInput = editorPanel.querySelector('#maple-mobile-button-label');
        state.editorKeySelect = editorPanel.querySelector('#maple-mobile-button-key');
        state.editorDeleteButton = editorPanel.querySelector('#maple-mobile-delete-button');
        state.nativeInput = nativeInput;

        KEY_DEFS.forEach((definition) => {
            const option = document.createElement('option');
            option.value = definition.code;
            option.textContent = definition.label;
            state.editorKeySelect.appendChild(option);
        });

        editorPanel.querySelector('#maple-mobile-add-button').addEventListener('click', () => {
            const button = sanitizeButton({
                id: nextButtonId(),
                label: 'Ctrl',
                key: 'LeftControl',
                x: 0.44,
                y: 0.44,
                w: NEW_BUTTON_SIZE,
                h: NEW_BUTTON_SIZE
            });

            state.buttons.push(button);
            state.selectedButtonId = button.id;
            saveButtons();
            syncButtonElements();
            updateEditorPanel();
        });

        editorPanel.querySelector('#maple-mobile-reset-buttons').addEventListener('click', () => {
            state.buttons = cloneButtons(state.defaultButtons);
            state.selectedButtonId = state.buttons.length > 0 ? state.buttons[0].id : null;
            saveButtons();
            syncButtonElements();
            updateEditorPanel();
        });

        state.editorDeleteButton.addEventListener('click', () => {
            if (!state.selectedButtonId) {
                return;
            }

            state.buttons = state.buttons.filter((button) => button.id !== state.selectedButtonId);
            state.selectedButtonId = state.buttons.length > 0 ? state.buttons[0].id : null;
            saveButtons();
            syncButtonElements();
            updateEditorPanel();
        });

        state.editorLabelInput.addEventListener('input', () => {
            const selectedButton = getSelectedButton();
            if (!selectedButton) {
                return;
            }

            selectedButton.label = state.editorLabelInput.value.trim().slice(0, 12) || shortLabelForKey(selectedButton.key);
            saveButtons();
            syncButtonElements();
        });

        state.editorKeySelect.addEventListener('change', () => {
            const selectedButton = getSelectedButton();
            if (!selectedButton) {
                return;
            }

            selectedButton.key = state.editorKeySelect.value;
            if (!selectedButton.label) {
                selectedButton.label = shortLabelForKey(selectedButton.key);
            }
            saveButtons();
            syncButtonElements();
            updateEditorPanel();
        });

        document.addEventListener('selectionchange', () => {
            if (document.activeElement === state.nativeInput) {
                syncNativeInputToWasm();
            }
        });

        window.addEventListener('blur', releaseAllActiveKeys);
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                releaseAllActiveKeys();
            }
        });

        if (window.visualViewport) {
            window.visualViewport.addEventListener('resize', refreshLayouts);
            window.visualViewport.addEventListener('scroll', refreshLayouts);
        }

        window.addEventListener('pointermove', handleEditorPointerMove, true);
        window.addEventListener('pointerup', stopEditorInteraction, true);
        window.addEventListener('pointercancel', stopEditorInteraction, true);
    }

    function updateEditorPanel() {
        ensureElements();

        state.controlsRoot.classList.toggle('editor-open', state.editorOpen);
        state.editorPanel.classList.toggle('open', state.editorOpen);
        state.editorToggle.textContent = state.editorOpen ? 'Done' : 'Edit';

        const selectedButton = getSelectedButton();
        const hasSelection = !!selectedButton;

        state.editorStatus.textContent = hasSelection ?
            `Selected: ${selectedButton.label} (${selectedButton.key})` :
            'Select a button to edit it.';
        state.editorLabelInput.disabled = !hasSelection;
        state.editorKeySelect.disabled = !hasSelection;
        state.editorDeleteButton.disabled = !hasSelection;

        state.editorLabelInput.value = hasSelection ? selectedButton.label : '';
        state.editorKeySelect.value = hasSelection ? selectedButton.key : KEY_DEFS[0].code;
    }

    function syncButtonElements() {
        ensureElements();

        const expectedIds = new Set(state.buttons.map((button) => button.id));
        for (const [buttonId, element] of state.buttonElements.entries()) {
            if (!expectedIds.has(buttonId)) {
                element.remove();
                state.buttonElements.delete(buttonId);
            }
        }

        state.buttons.forEach((button) => {
            let element = state.buttonElements.get(button.id);
            if (!element) {
                element = document.createElement('button');
                element.type = 'button';
                element.className = 'maple-mobile-control';
                element.dataset.buttonId = button.id;

                const label = document.createElement('span');
                label.className = 'maple-mobile-control-label';
                element.appendChild(label);

                const handle = document.createElement('span');
                handle.className = 'maple-mobile-control-handle';
                element.appendChild(handle);

                element.addEventListener('pointerdown', handleControlPointerDown);
                element.addEventListener('pointerup', handleControlPointerEnd);
                element.addEventListener('pointercancel', handleControlPointerEnd);
                element.addEventListener('lostpointercapture', handleControlPointerEnd);

                state.controlsLayer.appendChild(element);
                state.buttonElements.set(button.id, element);
            }

            element.querySelector('.maple-mobile-control-label').textContent = button.label;
        });

        refreshLayouts();
    }

    function refreshLayouts() {
        if (!state.controlsRoot) {
            return;
        }

        state.controlsRoot.style.display = state.active ? 'block' : 'none';
        if (!state.active) {
            return;
        }

        const rect = getCanvasRect();
        for (const button of state.buttons) {
            const element = state.buttonElements.get(button.id);
            if (!element || !rect) {
                continue;
            }

            element.classList.toggle('pressed', state.pressedButtons.has(button.id));
            element.classList.toggle('edit-mode', state.editorOpen);
            element.classList.toggle('selected', state.editorOpen && state.selectedButtonId === button.id);

            element.style.left = `${rect.left + button.x * rect.width}px`;
            element.style.top = `${rect.top + button.y * rect.height}px`;
            element.style.width = `${button.w * rect.width}px`;
            element.style.height = `${button.h * rect.height}px`;
        }

        if (state.textInputVisible && state.textField && rect) {
            const inputLeft = rect.left + (state.textField.x / GAME_WIDTH) * rect.width;
            const inputTop = rect.top + (state.textField.y / GAME_HEIGHT) * rect.height;
            const inputWidth = (state.textField.width / GAME_WIDTH) * rect.width;
            const inputHeight = (state.textField.height / GAME_HEIGHT) * rect.height;

            state.nativeInput.style.display = 'block';
            state.nativeInput.style.left = `${inputLeft}px`;
            state.nativeInput.style.top = `${inputTop}px`;
            state.nativeInput.style.width = `${Math.max(1, inputWidth)}px`;
            state.nativeInput.style.height = `${Math.max(1, inputHeight)}px`;
        } else {
            state.nativeInput.style.display = 'none';
        }
    }

    function pressMappedKey(glfwKey) {
        if (!state.runtimeReady || !state.runtime.sendKey) {
            return;
        }

        const currentCount = state.keyRefCounts.get(glfwKey) || 0;
        if (currentCount === 0) {
            state.runtime.sendKey(glfwKey, 1);
        }
        state.keyRefCounts.set(glfwKey, currentCount + 1);
    }

    function releaseMappedKey(glfwKey) {
        if (!state.runtimeReady || !state.runtime.sendKey) {
            return;
        }

        const currentCount = state.keyRefCounts.get(glfwKey) || 0;
        if (currentCount <= 1) {
            state.keyRefCounts.delete(glfwKey);
            state.runtime.sendKey(glfwKey, 0);
            return;
        }

        state.keyRefCounts.set(glfwKey, currentCount - 1);
    }

    function releaseAllActiveKeys() {
        for (const binding of state.pointerBindings.values()) {
            releaseMappedKey(binding.glfwKey);
        }
        state.pointerBindings.clear();
        state.pressedButtons.clear();
        refreshLayouts();
    }

    function handleControlPointerDown(event) {
        const buttonId = event.currentTarget.dataset.buttonId;
        const button = getButtonById(buttonId);
        if (!button) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();

        state.selectedButtonId = button.id;

        if (state.editorOpen) {
            const isResizeHandle = event.target.classList.contains('maple-mobile-control-handle');
            state.editorInteraction = {
                pointerId: event.pointerId,
                buttonId: button.id,
                mode: isResizeHandle ? 'resize' : 'move',
                startClientX: event.clientX,
                startClientY: event.clientY,
                startButton: { ...button }
            };
            updateEditorPanel();
            refreshLayouts();
            return;
        }

        if (state.textInputVisible) {
            return;
        }

        if (Array.from(state.pointerBindings.values()).some((binding) => binding.buttonId === button.id)) {
            return;
        }

        const definition = KEY_MAP.get(button.key);
        if (!definition) {
            return;
        }

        event.currentTarget.setPointerCapture(event.pointerId);
        state.pointerBindings.set(event.pointerId, {
            buttonId: button.id,
            glfwKey: definition.glfw
        });
        state.pressedButtons.add(button.id);
        pressMappedKey(definition.glfw);
        refreshLayouts();
    }

    function handleControlPointerEnd(event) {
        const binding = state.pointerBindings.get(event.pointerId);
        if (!binding) {
            return;
        }

        event.preventDefault();
        event.stopPropagation();

        state.pointerBindings.delete(event.pointerId);
        state.pressedButtons.delete(binding.buttonId);
        releaseMappedKey(binding.glfwKey);
        refreshLayouts();
    }

    function handleEditorPointerMove(event) {
        if (!state.editorOpen || !state.editorInteraction || state.editorInteraction.pointerId !== event.pointerId) {
            return;
        }

        const rect = getCanvasRect();
        if (!rect) {
            return;
        }

        const button = getButtonById(state.editorInteraction.buttonId);
        if (!button) {
            return;
        }

        const deltaX = (event.clientX - state.editorInteraction.startClientX) / rect.width;
        const deltaY = (event.clientY - state.editorInteraction.startClientY) / rect.height;

        if (state.editorInteraction.mode === 'resize') {
            button.w = clamp(state.editorInteraction.startButton.w + deltaX, MIN_BUTTON_SIZE, 1 - state.editorInteraction.startButton.x);
            button.h = clamp(state.editorInteraction.startButton.h + deltaY, MIN_BUTTON_SIZE, 1 - state.editorInteraction.startButton.y);
        } else {
            button.x = clamp(state.editorInteraction.startButton.x + deltaX, 0, 1 - button.w);
            button.y = clamp(state.editorInteraction.startButton.y + deltaY, 0, 1 - button.h);
        }

        saveButtons();
        refreshLayouts();
    }

    function stopEditorInteraction(event) {
        if (!state.editorInteraction) {
            return;
        }

        if (event && state.editorInteraction.pointerId !== event.pointerId) {
            return;
        }

        state.editorInteraction = null;
    }

    function syncNativeInputToWasm() {
        if (!state.textInputVisible || state.suppressInputSync || !state.runtimeReady || !state.runtime.syncText) {
            return;
        }

        const caret = state.nativeInput.selectionStart == null ? state.nativeInput.value.length : state.nativeInput.selectionStart;
        state.runtime.syncText(state.nativeInput.value, caret);
    }

    function focusNativeInput() {
        if (document.activeElement === state.nativeInput) {
            return;
        }

        try {
            state.nativeInput.focus({ preventScroll: true });
        } catch (error) {
            state.nativeInput.focus();
        }
    }

    function blurNativeInputSilently() {
        state.suppressBlurCallback = true;
        state.nativeInput.blur();
        window.setTimeout(() => {
            state.suppressBlurCallback = false;
        }, 0);
    }

    function applyNativeTextFieldState(nextState) {
        if (!state.active) {
            return;
        }

        ensureElements();
        releaseAllActiveKeys();

        const caret = Math.max(0, Number(nextState.caret) || 0);
        state.textField = {
            x: Number(nextState.x) || 0,
            y: Number(nextState.y) || 0,
            width: Number(nextState.width) || 0,
            height: Number(nextState.height) || 0,
            text: typeof nextState.text === 'string' ? nextState.text : '',
            caret,
            limit: Math.max(0, Number(nextState.limit) || 0),
            masked: !!nextState.masked
        };
        state.textInputVisible = true;

        state.suppressInputSync = true;
        state.nativeInput.type = state.textField.masked ? 'password' : 'text';
        if (state.textField.limit > 0) {
            state.nativeInput.maxLength = state.textField.limit;
        } else {
            state.nativeInput.removeAttribute('maxlength');
        }
        if (state.nativeInput.value !== state.textField.text) {
            state.nativeInput.value = state.textField.text;
        }
        if (state.nativeInput.selectionStart !== caret || state.nativeInput.selectionEnd !== caret) {
            state.nativeInput.setSelectionRange(caret, caret);
        }
        state.suppressInputSync = false;

        refreshLayouts();
        focusNativeInput();
    }

    function clearNativeTextField() {
        if (!state.textInputVisible) {
            return;
        }

        state.textField = null;
        state.textInputVisible = false;
        releaseAllActiveKeys();
        blurNativeInputSilently();
        state.nativeInput.value = '';
        state.nativeInput.style.display = 'none';
        refreshLayouts();

        if (window.MapleWasmUI && window.MapleWasmUI.applyScale) {
            window.setTimeout(() => {
                window.MapleWasmUI.applyScale({ force: true });
            }, 0);
        }
    }

    function bindRuntime(module) {
        state.runtime.sendKey = module.cwrap('wasmSendKey', null, ['number', 'number']);
        state.runtime.syncText = module.cwrap('wasmSyncFocusedText', null, ['string', 'number']);
        state.runtime.submitText = module.cwrap('wasmSubmitFocusedText', null, []);
        state.runtime.blurText = module.cwrap('wasmBlurFocusedText', null, []);
        state.runtimeReady = true;
    }

    function configure(config) {
        state.config.enabled = config && config.enabled !== undefined ? !!config.enabled : false;
        state.config.debugDesktop = !!(config && config.debugDesktop);
        state.config.storageKey =
            typeof (config && config.storageKey) === 'string' && config.storageKey ?
                config.storageKey :
                DEFAULT_STORAGE_KEY;
        state.defaultButtons = sanitizeButtons(config && config.defaults);
        state.buttons = loadButtons();
        state.active = state.config.enabled && detectMobileEnvironment();
        if (!state.active) {
            state.editorOpen = false;
            clearNativeTextField();
        }
        if (!state.selectedButtonId && state.buttons.length > 0) {
            state.selectedButtonId = state.buttons[0].id;
        }
        ensureElements();
        syncButtonElements();
        updateEditorPanel();
        refreshLayouts();
    }

    window.MapleWasmMobile = {
        bindRuntime,
        clearTextField: clearNativeTextField,
        configure,
        refreshLayouts,
        setTextFieldState: applyNativeTextFieldState,
        shouldFreezeCanvasFit() {
            return state.active && state.textInputVisible;
        }
    };
})();
