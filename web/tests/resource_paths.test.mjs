import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const html = readFileSync(new URL('../index.html', import.meta.url), 'utf8');

function capture(pattern, label) {
    const match = html.match(pattern);
    assert.ok(match, `Could not find ${label} in web/index.html`);
    return match[1];
}

const resources = {
    config: capture(/fetch\('([^']+)'\)/, 'configuration URL'),
    loader: capture(/<script\s+src="([^"]*wasm_loader\.js)"/, 'WASM loader URL'),
    wasm: capture(/const WASM_CLIENT_URL\s*=\s*'([^']+)'/, 'WASM binary URL'),
    script: capture(/const WASM_SCRIPT_URL\s*=\s*'([^']+)'/, 'WASM client script URL'),
};

test('internal resources remain under a reverse-proxy path prefix', () => {
    const pageUrl = 'https://octo.bytedance.net/proxy/app/web/index.html';

    assert.deepEqual(resources, {
        config: './config.json',
        loader: './wasm_loader.js',
        wasm: '../build/JourneyClient.wasm',
        script: '../build/JourneyClient.js',
    });
    assert.deepEqual(
        Object.fromEntries(
            Object.entries(resources).map(([name, path]) => [name, new URL(path, pageUrl).href]),
        ),
        {
            config: 'https://octo.bytedance.net/proxy/app/web/config.json',
            loader: 'https://octo.bytedance.net/proxy/app/web/wasm_loader.js',
            wasm: 'https://octo.bytedance.net/proxy/app/build/JourneyClient.wasm',
            script: 'https://octo.bytedance.net/proxy/app/build/JourneyClient.js',
        },
    );
});

test('startup page does not download a second copy of the embedded CJK font', () => {
    assert.doesNotMatch(html, /DroidSansFallbackFull\.ttf/);
    assert.doesNotMatch(html, /rel="preload"[^>]+as="font"/);
});
