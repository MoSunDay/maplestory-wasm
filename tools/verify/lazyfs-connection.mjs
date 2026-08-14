import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const lazyfsSource = readFileSync(
  new URL('../../src/client/LazyFS/lazyfs.js', import.meta.url),
  'utf8',
);
const connectionSource = readFileSync(
  new URL('../../src/client/LazyFS/lazyfs_connection.js', import.meta.url),
  'utf8',
);

function createHarness() {
  const sockets = [];
  const alerts = [];
  const intervals = new Map();
  const listeners = new Map();
  let nextInterval = 1;

  class FakeWebSocket {
    constructor(url) {
      this.url = url;
      this.sent = [];
      this.readyState = 0;
      sockets.push(this);
    }

    open() {
      this.readyState = 1;
      this.onopen?.();
    }

    send(message) {
      if (this.readyState !== 1) {
        throw new Error('socket is not open');
      }
      this.sent.push(message);
    }

    message(value) {
      this.onmessage?.({ data: JSON.stringify(value) });
    }

    close() {
      if (this.readyState === 3) return;
      this.readyState = 3;
      this.onclose?.();
    }

    fail() {
      this.onerror?.(new Error('connection failed'));
      this.close();
    }
  }

  const window = {
    location: { protocol: 'http:', hostname: 'localhost' },
    addEventListener: (name, callback) => listeners.set(name, callback),
    MapleConnectionLost: {
      show: message => alerts.push(message),
    },
    MapleAssetLoading: {
      begin() {},
      end() {},
      fail() {},
    },
  };
  const context = vm.createContext({
    ArrayBuffer,
    clearInterval: id => intervals.delete(id),
    clearTimeout,
    console: {
      debug() {},
      error() {},
      log() {},
      warn() {},
    },
    Map,
    Module: {},
    performance,
    Promise,
    setInterval: callback => {
      const id = nextInterval++;
      intervals.set(id, callback);
      return id;
    },
    setTimeout,
    TextDecoder,
    Uint8Array,
    WebSocket: FakeWebSocket,
    window,
  });
  vm.runInContext(connectionSource, context, { filename: 'lazyfs_connection.js' });
  vm.runInContext(lazyfsSource, context, { filename: 'lazyfs.js' });
  const lazyfs = context.Module.LazyFS;
  lazyfs.RECONNECT_DELAYS_MS = [0, 0, 0];
  lazyfs.delay = () => Promise.resolve();
  return { alerts, intervals, lazyfs, listeners, sockets, window };
}

async function until(predicate) {
  for (let attempt = 0; attempt < 50; attempt++) {
    if (predicate()) return;
    await new Promise(resolve => setImmediate(resolve));
  }
  throw new Error('condition was not reached');
}

async function testSharedConnectionAndKeepAlive() {
  const harness = createHarness();
  assert.equal(harness.lazyfs.formatUrlHost('2001:db8::1'), '[2001:db8::1]');
  assert.equal(harness.lazyfs.formatUrlHost('[2001:db8::1]'), '[2001:db8::1]');
  assert.equal(harness.lazyfs.formatUrlHost('example.test'), 'example.test');
  const first = harness.lazyfs.connect();
  const second = harness.lazyfs.connect();
  assert.strictEqual(first, second, 'concurrent callers must share one connection promise');
  assert.equal(harness.sockets.length, 1);

  harness.sockets[0].open();
  await Promise.all([first, second]);
  harness.lazyfs.registerFile('Base.nx');
  assert.equal(harness.intervals.size, 1);
  [...harness.intervals.values()][0]();
  assert.deepEqual(JSON.parse(harness.sockets[0].sent.at(-1)), {
    type: 'get_size',
    file: 'Base.nx',
  });
}

async function testReconnectReplaysPendingRequest() {
  const harness = createHarness();
  const connection = harness.lazyfs.connect();
  harness.sockets[0].open();
  await connection;

  const size = harness.lazyfs.getFileSize('UI.nx');
  await until(() => harness.sockets[0].sent.length === 1);
  harness.sockets[0].close();
  await until(() => harness.sockets.length === 2);
  harness.sockets[1].open();
  await until(() => harness.sockets[1].sent.length === 1);
  assert.deepEqual(JSON.parse(harness.sockets[1].sent[0]), {
    type: 'get_size',
    file: 'UI.nx',
  });
  harness.sockets[1].message({ type: 'size', file: 'UI.nx', size: 123, version: 1 });
  assert.equal(await size, 123);
  assert.equal(harness.alerts.length, 0);
}

async function testTerminalAssetFailureDoesNotExitGame() {
	const harness = createHarness();
	const connection = harness.lazyfs.connect();
  for (let index = 0; index < 4; index++) {
    await until(() => harness.sockets.length === index + 1);
    harness.sockets[index].fail();
	}
	await assert.rejects(connection);
	assert.equal(harness.alerts.length, 0);
	await assert.rejects(harness.lazyfs.connect());
	assert.equal(harness.alerts.length, 0);
	assert.equal(harness.lazyfs.resetConnectionFailure(), true);
	const retry = harness.lazyfs.connect();
	await until(() => harness.sockets.length === 5);
	harness.sockets[4].open();
	await retry;
}

function configurePreload(harness, persistent) {
	const { lazyfs } = harness;
	lazyfs.files.set('String.nx', { size: 1, version: 1 });
	lazyfs.files.set('Item.nx', { size: 1, version: 1 });
	lazyfs.requestPersistentStorage = async () => persistent;
	lazyfs.characterMetadataEnd = async () => 1;
	return lazyfs;
}

async function testPersistentPreloadStates() {
	const complete = configurePreload(createHarness(), true);
	complete.prefetchFileRange = async () => {};
	await complete.startItemAssetPreload();
	assert.equal(complete.itemAssetPreloadState, 'complete');
	assert.equal(complete.itemAssetPreloadResult.persistent, true);

	const unavailable = configurePreload(createHarness(), false);
	let unavailablePrefetches = 0;
	unavailable.prefetchFileRange = async () => { unavailablePrefetches += 1; };
	await unavailable.startItemAssetPreload();
	assert.equal(unavailable.itemAssetPreloadState, 'unavailable');
	assert.equal(unavailable.itemAssetPreloadResult.persistent, false);
	assert.equal(unavailable.itemAssetPreloadResult.reason, 'persistent-storage-unavailable');
	assert.equal(unavailablePrefetches, 0);

	const failed = configurePreload(createHarness(), true);
	let injected = false;
	failed.prefetchFileRange = async () => {
		if (!injected) {
			injected = true;
			failed.trackCacheWrite(Promise.reject(new Error('quota exceeded')));
		}
	};
	await assert.rejects(failed.startItemAssetPreload(), /cache writes failed/);
	assert.equal(failed.itemAssetPreloadState, 'failed');
	assert.equal(failed.cacheWriteStats.failed, 1);
}

async function testUnloadSuppressesRecovery() {
  const harness = createHarness();
  const connection = harness.lazyfs.connect();
  harness.sockets[0].open();
  await connection;
  harness.listeners.get('beforeunload')();
  harness.sockets[0].close();
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(harness.sockets.length, 1);
  assert.equal(harness.alerts.length, 0);
}

async function testForegroundAssetFailureRetries() {
  const harness = createHarness();
  const attempts = [];
  const events = [];
  let retry;
  harness.window.MapleAssetLoading = {
    begin: key => events.push(`begin:${key}`),
    end: key => events.push(`end:${key}`),
    fail: (key, callback) => {
      events.push(`fail:${key}`);
      retry = callback;
    },
  };
  harness.lazyfs.isFileRangeResident = () => false;
  harness.lazyfs.prefetchFileRange = () => new Promise((resolve, reject) => {
    attempts.push({ resolve, reject });
  });
  harness.lazyfs.promotePrefetch = () => {};

  const result = harness.lazyfs.requestForegroundFileRange('Map.nx', 10, 20);
  await until(() => attempts.length === 1);
  attempts[0].reject(new Error('simulated asset failure'));
  await until(() => typeof retry === 'function');
  harness.lazyfs.terminalConnectionFailure = true;
  retry();
  await until(() => attempts.length === 2);
  assert.equal(harness.lazyfs.terminalConnectionFailure, false);
  attempts[1].resolve('ready');
  assert.equal(await result, 'ready');
  assert.deepEqual(events, [
    'begin:range:Map.nx:10:20',
    'fail:range:Map.nx:10:20',
    'begin:range:Map.nx:10:20',
    'end:range:Map.nx:10:20',
  ]);
}

async function testPriorityAssetDoesNotBlockGameplay() {
  const harness = createHarness();
  const events = [];
  const attempts = [];
  let promotions = 0;
  harness.window.MapleAssetLoading = {
    begin: key => events.push(`begin:${key}`),
    end: key => events.push(`end:${key}`),
    fail: key => events.push(`fail:${key}`),
  };
  harness.lazyfs.isFileRangeResident = () => false;
  harness.lazyfs.prefetchFileRange = () => new Promise((resolve, reject) => {
    attempts.push({ resolve, reject });
  });
  harness.lazyfs.promotePrefetch = () => { promotions += 1; };

  const result = harness.lazyfs.requestPriorityFileRange('Character.nx', 30, 40);
  await until(() => attempts.length === 1);
  assert.deepEqual(events, []);
  assert.equal(promotions, 1);
  attempts[0].resolve('ready');
  assert.equal(await result, 'ready');
  assert.deepEqual(events, []);
}

async function testPriorityAssetFailureEscalatesToRetry() {
  const harness = createHarness();
  const events = [];
  const attempts = [];
  let retry;
  harness.window.MapleAssetLoading = {
    begin: key => events.push(`begin:${key}`),
    end: key => events.push(`end:${key}`),
    fail: (key, callback) => {
      events.push(`fail:${key}`);
      retry = callback;
    },
  };
  harness.lazyfs.isFileRangeResident = () => false;
  harness.lazyfs.prefetchFileRange = () => new Promise((resolve, reject) => {
    attempts.push({ resolve, reject });
  });
  harness.lazyfs.promotePrefetch = () => {};

  const result = harness.lazyfs.requestPriorityFileRange('Character.nx', 50, 60);
  await until(() => attempts.length === 1);
  attempts[0].reject(new Error('priority request failed'));
  await until(() => attempts.length === 2);
  attempts[1].reject(new Error('foreground retry required'));
  await until(() => typeof retry === 'function');
  retry();
  await until(() => attempts.length === 3);
  attempts[2].resolve('ready');
  assert.equal(await result, 'ready');
  assert.deepEqual(events, [
    'begin:range:Character.nx:50:60',
    'fail:range:Character.nx:50:60',
    'begin:range:Character.nx:50:60',
    'end:range:Character.nx:50:60',
  ]);
}

await testSharedConnectionAndKeepAlive();
await testReconnectReplaysPendingRequest();
await testTerminalAssetFailureDoesNotExitGame();
await testUnloadSuppressesRecovery();
await testPersistentPreloadStates();
await testForegroundAssetFailureRetries();
await testPriorityAssetDoesNotBlockGameplay();
await testPriorityAssetFailureEscalatesToRetry();
console.log('LazyFS connection verification passed');
