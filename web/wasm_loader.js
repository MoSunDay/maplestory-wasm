(function (root, factory) {
    const api = Object.freeze(factory());
    if (typeof module === 'object' && module.exports) {
        module.exports = api;
    }
    if (root) {
        root.MapleWasmLoader = api;
    }
})(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    const DEFAULTS = Object.freeze({
        chunkSize: 1024 * 1024,
        concurrency: 3,
        timeoutMs: 15000,
        maxAttempts: 3,
        retryDelayMs: 400
    });

    function positiveInteger(value, fallback, name) {
        const resolved = value === undefined ? fallback : value;
        if (!Number.isSafeInteger(resolved) || resolved <= 0) {
            throw new TypeError(name + ' must be a positive integer');
        }
        return resolved;
    }

    function nonNegativeInteger(value, fallback, name) {
        const resolved = value === undefined ? fallback : value;
        if (!Number.isSafeInteger(resolved) || resolved < 0) {
            throw new TypeError(name + ' must be a non-negative integer');
        }
        return resolved;
    }

    function normalizeOptions(options) {
        const input = options || {};
        const fetchImpl = input.fetchImpl || (typeof fetch === 'function' ? fetch.bind(globalThis) : null);
        if (typeof fetchImpl !== 'function') {
            throw new TypeError('A fetch implementation is required');
        }
        return {
            fetchImpl,
            chunkSize: positiveInteger(input.chunkSize, DEFAULTS.chunkSize, 'chunkSize'),
            concurrency: positiveInteger(input.concurrency, DEFAULTS.concurrency, 'concurrency'),
            timeoutMs: positiveInteger(input.timeoutMs, DEFAULTS.timeoutMs, 'timeoutMs'),
            maxAttempts: positiveInteger(input.maxAttempts, DEFAULTS.maxAttempts, 'maxAttempts'),
            retryDelayMs: nonNegativeInteger(
                input.retryDelayMs,
                DEFAULTS.retryDelayMs,
                'retryDelayMs'
            ),
            onProgress: typeof input.onProgress === 'function' ? input.onProgress : function () {},
            onRetry: typeof input.onRetry === 'function' ? input.onRetry : function () {}
        };
    }

    function createChunkRanges(totalBytes, chunkSize) {
        const total = positiveInteger(totalBytes, undefined, 'totalBytes');
        const size = positiveInteger(chunkSize, DEFAULTS.chunkSize, 'chunkSize');
        const ranges = [];
        for (let start = 0; start < total; start += size) {
            ranges.push(Object.freeze({ start, end: Math.min(total - 1, start + size - 1) }));
        }
        return ranges;
    }

    function parseContentRange(value) {
        const match = typeof value === 'string'
            ? /^bytes (\d+)-(\d+)\/(\d+)$/.exec(value.trim())
            : null;
        if (!match) {
            throw new Error('Invalid Content-Range header: ' + String(value));
        }
        const parsed = {
            start: Number(match[1]),
            end: Number(match[2]),
            total: Number(match[3])
        };
        if (!Number.isSafeInteger(parsed.start)
            || !Number.isSafeInteger(parsed.end)
            || !Number.isSafeInteger(parsed.total)
            || parsed.start < 0
            || parsed.end < parsed.start
            || parsed.total <= parsed.end)
        {
            throw new Error('Invalid Content-Range values: ' + value);
        }
        return parsed;
    }

    function responseLength(response) {
        const raw = response.headers.get('Content-Length');
        const value = Number(raw);
        if (!raw || !Number.isSafeInteger(value) || value <= 0) {
            throw new Error('Missing or invalid Content-Length header');
        }
        return value;
    }

    function wait(milliseconds) {
        return milliseconds > 0
            ? new Promise(resolve => setTimeout(resolve, milliseconds))
            : Promise.resolve();
    }

    async function requestWithTimeout(fetchImpl, url, init, timeoutMs, consume) {
        const controller = new AbortController();
        const timer = setTimeout(function () {
            controller.abort();
        }, timeoutMs);
        try {
            const response = await fetchImpl(url, Object.assign({}, init, { signal: controller.signal }));
            // Keep the timeout armed while consuming the body. Fetch resolves
            // after headers, which is too early to detect a stalled transfer.
            return await consume(response);
        } catch (error) {
            if (controller.signal.aborted) {
                throw new Error('Request timed out after ' + timeoutMs + 'ms');
            }
            throw error;
        } finally {
            clearTimeout(timer);
        }
    }

    async function withRetries(operation, context, options) {
        let lastError = null;
        for (let attempt = 1; attempt <= options.maxAttempts; attempt++) {
            try {
                return await operation();
            } catch (error) {
                lastError = error instanceof Error ? error : new Error(String(error));
                if (attempt === options.maxAttempts) {
                    break;
                }
                options.onRetry(Object.assign({}, context, {
                    attempt,
                    nextAttempt: attempt + 1,
                    error: lastError
                }));
                await wait(options.retryDelayMs * attempt);
            }
        }
        throw new Error(
            context.label + ' failed after ' + options.maxAttempts + ' attempts: ' + lastError.message
        );
    }

    async function fetchTotalBytes(url, options) {
        return withRetries(async function () {
            return requestWithTimeout(options.fetchImpl, url, {
                method: 'HEAD',
                cache: 'no-store',
                credentials: 'same-origin'
            }, options.timeoutMs, async function (response) {
                if (!response.ok) {
                    throw new Error('HEAD returned HTTP ' + response.status);
                }
                return responseLength(response);
            });
        }, { label: 'WASM metadata', phase: 'head' }, options);
    }

    async function fetchChunk(url, range, totalBytes, options) {
        const expectedLength = range.end - range.start + 1;
        return withRetries(async function () {
            return requestWithTimeout(options.fetchImpl, url, {
                method: 'GET',
                cache: 'no-store',
                credentials: 'same-origin',
                headers: { Range: 'bytes=' + range.start + '-' + range.end }
            }, options.timeoutMs, async function (response) {
                if (response.status !== 206) {
                    throw new Error('Range request returned HTTP ' + response.status);
                }

                const contentRange = parseContentRange(response.headers.get('Content-Range'));
                if (contentRange.start !== range.start
                    || contentRange.end !== range.end
                    || contentRange.total !== totalBytes)
                {
                    throw new Error('Range response does not match the requested bytes');
                }
                if (responseLength(response) !== expectedLength) {
                    throw new Error('Range Content-Length does not match the requested bytes');
                }

                const bytes = new Uint8Array(await response.arrayBuffer());
                if (bytes.byteLength !== expectedLength) {
                    throw new Error('Range body length does not match the requested bytes');
                }
                return bytes;
            });
        }, {
            label: 'WASM bytes ' + range.start + '-' + range.end,
            phase: 'chunk',
            range
        }, options);
    }

    async function downloadWasm(url, inputOptions) {
        if (typeof url !== 'string' || !url.trim()) {
            throw new TypeError('A WASM URL is required');
        }
        const options = normalizeOptions(inputOptions);
        const totalBytes = await fetchTotalBytes(url, options);
        const ranges = createChunkRanges(totalBytes, options.chunkSize);
        const output = new Uint8Array(totalBytes);
        let nextRange = 0;
        let receivedBytes = 0;

        async function worker() {
            while (true) {
                const index = nextRange++;
                if (index >= ranges.length) {
                    return;
                }
                const range = ranges[index];
                const chunk = await fetchChunk(url, range, totalBytes, options);
                output.set(chunk, range.start);
                receivedBytes += chunk.byteLength;
                options.onProgress(Object.freeze({
                    receivedBytes,
                    totalBytes,
                    percent: Math.floor(receivedBytes * 100 / totalBytes)
                }));
            }
        }

        const workers = Array.from(
            { length: Math.min(options.concurrency, ranges.length) },
            function () { return worker(); }
        );
        await Promise.all(workers);
        return output;
    }

    return {
        DEFAULTS,
        createChunkRanges,
        parseContentRange,
        downloadWasm
    };
});
