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
    font: capture(/src:\s*url\('([^']+)'\)/, 'font URL'),
    fontPreload: capture(/<link\s+rel="preload"\s+href="([^"]+)"/, 'font preload URL'),
    script: capture(/script\.src\s*=\s*'([^']+)'/, 'WASM client script URL'),
};

test('internal resources remain under a reverse-proxy path prefix', () => {
    const pageUrl = 'https://octo.bytedance.net/proxy/app/web/index.html';

    assert.deepEqual(resources, {
        config: './config.json',
        font: '../src/client/fonts/DroidSansFallback/DroidSansFallbackFull.ttf',
        fontPreload: '../src/client/fonts/DroidSansFallback/DroidSansFallbackFull.ttf',
        script: '../build/JourneyClient.js',
    });
    assert.deepEqual(
        Object.fromEntries(
            Object.entries(resources).map(([name, path]) => [name, new URL(path, pageUrl).href]),
        ),
        {
            config: 'https://octo.bytedance.net/proxy/app/web/config.json',
            font: 'https://octo.bytedance.net/proxy/app/src/client/fonts/DroidSansFallback/DroidSansFallbackFull.ttf',
            fontPreload: 'https://octo.bytedance.net/proxy/app/src/client/fonts/DroidSansFallback/DroidSansFallbackFull.ttf',
            script: 'https://octo.bytedance.net/proxy/app/build/JourneyClient.js',
        },
    );
});
