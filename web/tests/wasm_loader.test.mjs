import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import test from 'node:test';

const require = createRequire(import.meta.url);
const { createChunkRanges, downloadWasm, parseContentRange } = require('../wasm_loader.js');

function headResponse(total) {
    return new Response(null, {
        status: 200,
        headers: { 'Content-Length': String(total), 'Accept-Ranges': 'bytes' },
    });
}

function rangeResponse(source, start, end, overrides = {}) {
    const body = source.slice(start, end + 1);
    return new Response(body, {
        status: overrides.status ?? 206,
        headers: {
            'Content-Range': overrides.contentRange ?? `bytes ${start}-${end}/${source.length}`,
            'Content-Length': overrides.contentLength ?? String(body.length),
        },
    });
}

function requestedRange(init) {
    const match = /^bytes=(\d+)-(\d+)$/.exec(init.headers.Range);
    assert.ok(match, `invalid Range header: ${init.headers.Range}`);
    return { start: Number(match[1]), end: Number(match[2]) };
}

test('range helpers preserve exact byte boundaries', () => {
    assert.deepEqual(createChunkRanges(10, 4), [
        { start: 0, end: 3 },
        { start: 4, end: 7 },
        { start: 8, end: 9 },
    ]);
    assert.deepEqual(parseContentRange('bytes 4-7/10'), { start: 4, end: 7, total: 10 });
    assert.throws(() => parseContentRange('bytes 7-4/10'), /Invalid Content-Range/);
});

test('downloadWasm assembles concurrent chunks and reports completed bytes', async () => {
    const source = Uint8Array.from({ length: 23 }, (_, index) => index + 1);
    let active = 0;
    let maxActive = 0;
    const progress = [];
    const fetchImpl = async (_url, init) => {
        if (init.method === 'HEAD') return headResponse(source.length);
        const { start, end } = requestedRange(init);
        active++;
        maxActive = Math.max(maxActive, active);
        await new Promise(resolve => setTimeout(resolve, 5));
        active--;
        return rangeResponse(source, start, end);
    };

    const result = await downloadWasm('/build/client.wasm', {
        fetchImpl,
        chunkSize: 5,
        concurrency: 3,
        retryDelayMs: 0,
        onProgress: value => progress.push(value),
    });

    assert.deepEqual(result, source);
    assert.equal(maxActive, 3);
    assert.equal(progress.at(-1).receivedBytes, source.length);
    assert.equal(progress.at(-1).percent, 100);
});

test('timed-out chunks are aborted and retried', async () => {
    const source = Uint8Array.from([0, 97, 115, 109, 1, 0, 0, 0]);
    let rangeAttempts = 0;
    const retries = [];
    const fetchImpl = async (_url, init) => {
        if (init.method === 'HEAD') return headResponse(source.length);
        rangeAttempts++;
        if (rangeAttempts === 1) {
            return new Promise((_resolve, reject) => {
                init.signal.addEventListener('abort', () => reject(new Error('aborted')), { once: true });
            });
        }
        const { start, end } = requestedRange(init);
        return rangeResponse(source, start, end);
    };

    const result = await downloadWasm('/build/client.wasm', {
        fetchImpl,
        chunkSize: source.length,
        concurrency: 1,
        timeoutMs: 5,
        retryDelayMs: 0,
        onRetry: retry => retries.push(retry),
    });

    assert.deepEqual(result, source);
    assert.equal(rangeAttempts, 2);
    assert.equal(retries.length, 1);
    assert.match(retries[0].error.message, /timed out/);
});

test('timeout remains active while the response body is being consumed', async () => {
    const source = Uint8Array.from([0, 97, 115, 109]);
    let rangeAttempts = 0;
    const fetchImpl = async (_url, init) => {
        if (init.method === 'HEAD') return headResponse(source.length);
        rangeAttempts++;
        if (rangeAttempts === 1) {
            const { start, end } = requestedRange(init);
            return {
                status: 206,
                headers: new Headers({
                    'Content-Range': `bytes ${start}-${end}/${source.length}`,
                    'Content-Length': String(source.length),
                }),
                arrayBuffer: () => new Promise((_resolve, reject) => {
                    init.signal.addEventListener('abort', () => reject(new Error('aborted')), { once: true });
                }),
            };
        }
        const { start, end } = requestedRange(init);
        return rangeResponse(source, start, end);
    };

    const result = await downloadWasm('/build/client.wasm', {
        fetchImpl,
        chunkSize: source.length,
        timeoutMs: 5,
        retryDelayMs: 0,
    });
    assert.deepEqual(result, source);
    assert.equal(rangeAttempts, 2);
});

test('retry exhaustion fails explicitly instead of falling back to a full GET', async () => {
    const methods = [];
    const fetchImpl = async (_url, init) => {
        methods.push(init.method);
        if (init.method === 'HEAD') return headResponse(8);
        return new Response('unavailable', { status: 503 });
    };

    await assert.rejects(
        downloadWasm('/build/client.wasm', {
            fetchImpl,
            chunkSize: 8,
            concurrency: 1,
            maxAttempts: 2,
            retryDelayMs: 0,
        }),
        /failed after 2 attempts: Range request returned HTTP 503/,
    );
    assert.deepEqual(methods, ['HEAD', 'GET', 'GET']);
});

test('mismatched Content-Range and body lengths are rejected', async t => {
    const source = Uint8Array.from({ length: 8 }, (_, index) => index);

    await t.test('wrong range', async () => {
        const fetchImpl = async (_url, init) => {
            if (init.method === 'HEAD') return headResponse(source.length);
            const { start, end } = requestedRange(init);
            return rangeResponse(source, start, end, { contentRange: 'bytes 1-7/8' });
        };
        await assert.rejects(downloadWasm('/client.wasm', {
            fetchImpl, chunkSize: 8, maxAttempts: 1,
        }), /does not match the requested bytes/);
    });

    await t.test('wrong content length', async () => {
        const fetchImpl = async (_url, init) => {
            if (init.method === 'HEAD') return headResponse(source.length);
            const { start, end } = requestedRange(init);
            return rangeResponse(source, start, end, { contentLength: '7' });
        };
        await assert.rejects(downloadWasm('/client.wasm', {
            fetchImpl, chunkSize: 8, maxAttempts: 1,
        }), /Content-Length does not match/);
    });
});
