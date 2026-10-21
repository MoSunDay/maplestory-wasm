import test from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const settings = require('../main/settings.js');

test('parseServerUrl bare host', () => {
  const r = settings.parseServerUrl('ms.example.com');
  assert.deepEqual(r, {
    ok: true,
    kind: 'tcp',
    proxyHost: null,
    proxyPort: null,
    targetHost: 'ms.example.com',
    targetPort: 8484,
    secure: false,
  });
});

test('parseServerUrl host:port', () => {
  const r = settings.parseServerUrl('ms.example.com:7575');
  assert.equal(r.ok, true);
  assert.equal(r.kind, 'tcp');
  assert.equal(r.targetHost, 'ms.example.com');
  assert.equal(r.targetPort, 7575);
  assert.equal(r.proxyHost, null);
});

test('parseServerUrl ws:// with port', () => {
  const r = settings.parseServerUrl('ws://proxy.example.com:9001');
  assert.equal(r.ok, true);
  assert.equal(r.kind, 'ws');
  assert.equal(r.proxyHost, 'proxy.example.com');
  assert.equal(r.proxyPort, 9001);
  assert.equal(r.targetHost, 'proxy.example.com');
  assert.equal(r.targetPort, 8484);
  assert.equal(r.secure, false);
});

test('parseServerUrl ws:// without port defaults to 8080', () => {
  const r = settings.parseServerUrl('ws://proxy.example.com');
  assert.equal(r.ok, true);
  assert.equal(r.kind, 'ws');
  assert.equal(r.proxyPort, 8080);
  assert.equal(r.targetPort, 8484);
});

test('parseServerUrl wss:// is secure', () => {
  const r = settings.parseServerUrl('wss://proxy.example.com:443');
  assert.equal(r.ok, true);
  assert.equal(r.secure, true);
  assert.equal(r.proxyPort, 443);
});

test('parseServerUrl uppercase WS:// prefix', () => {
  const r = settings.parseServerUrl('WS://Proxy.Example.COM');
  assert.equal(r.ok, true);
  assert.equal(r.kind, 'ws');
  assert.equal(r.proxyHost, 'proxy.example.com');
  assert.equal(r.proxyPort, 8080);
});

test('parseServerUrl ipv6 bracketed', () => {
  const r = settings.parseServerUrl('[::1]');
  assert.equal(r.ok, true);
  assert.equal(r.kind, 'tcp');
  assert.equal(r.targetHost, '::1');
  assert.equal(r.targetPort, 8484);
});

test('parseServerUrl ipv6 bracketed with port', () => {
  const r = settings.parseServerUrl('[::1]:7575');
  assert.equal(r.ok, true);
  assert.equal(r.targetHost, '::1');
  assert.equal(r.targetPort, 7575);
});

test('parseServerUrl errors', () => {
  assert.equal(settings.parseServerUrl('').ok, false);
  assert.equal(settings.parseServerUrl('   ').ok, false);
  const scheme = settings.parseServerUrl('http://ms.example.com');
  assert.equal(scheme.ok, false);
  assert.match(scheme.error, /ws:\/\//);
  const path = settings.parseServerUrl('ms.example.com/channel');
  assert.equal(path.ok, false);
  assert.equal(settings.parseServerUrl('ms.example.com:0').ok, false);
  assert.equal(settings.parseServerUrl('ms.example.com:99999').ok, false);
  assert.equal(settings.parseServerUrl('ms.example.com:8484 x').ok, false);
  assert.equal(settings.parseServerUrl('ftp://ms.example.com').ok, false);
  assert.equal(settings.parseServerUrl('ms.example.com?a=b').ok, false);
  assert.equal(settings.parseServerUrl('ms.example.com#top').ok, false);
});

test('parseAssetsUrl empty means unset', () => {
  const r = settings.parseAssetsUrl('   ');
  assert.deepEqual(r, { ok: true, host: '', port: 0, secure: false });
});

test('parseAssetsUrl ws default port 8765', () => {
  const r = settings.parseAssetsUrl('ws://assets.example.com');
  assert.deepEqual(r, { ok: true, host: 'assets.example.com', port: 8765, secure: false });
});

test('parseAssetsUrl wss default port 443', () => {
  const r = settings.parseAssetsUrl('wss://assets.example.com');
  assert.deepEqual(r, { ok: true, host: 'assets.example.com', port: 443, secure: true });
});

test('parseAssetsUrl explicit port', () => {
  const r = settings.parseAssetsUrl('ws://assets.example.com:9000');
  assert.deepEqual(r, { ok: true, host: 'assets.example.com', port: 9000, secure: false });
});

test('parseAssetsUrl rejects other schemes and bare hosts', () => {
  const r = settings.parseAssetsUrl('http://assets.example.com:8765');
  assert.equal(r.ok, false);
  assert.match(r.error, /ws:\/\//);
  assert.equal(settings.parseAssetsUrl('assets.example.com').ok, false);
});

test('buildClientConfig tcp kind with local assets uses loopback proxy and local assets', () => {
  const cfg = settings.buildClientConfig(
    { serverUrl: 'ms.example.com', assetsUrl: '' },
    { localProxyPort: 51000, localAssetsPort: 51001 }
  );
  assert.equal(cfg.ProxyIP, '127.0.0.1');
  assert.equal(cfg.ProxyPort, '51000');
  assert.equal(cfg.MapleStoryServerIp, 'ms.example.com');
  assert.equal(cfg.MapleStoryServerPort, '8484');
  assert.equal(cfg.AssetsServerIP, '127.0.0.1');
  assert.equal(cfg.AssetsServerPort, '51001');
  assert.equal(cfg.AssetsServerProtocol, 'ws');
  assert.ok(Object.values(cfg).every((value) => typeof value === 'string'));
});

test('buildClientConfig ws remote proxy with remote assets', () => {
  const cfg = settings.buildClientConfig(
    { serverUrl: 'ws://proxy.example.com:9001', assetsUrl: 'wss://assets.example.com' },
    { localProxyPort: 51000, localAssetsPort: null }
  );
  assert.equal(cfg.ProxyIP, 'proxy.example.com');
  assert.equal(cfg.ProxyPort, '9001');
  assert.equal(cfg.MapleStoryServerIp, 'proxy.example.com');
  assert.equal(cfg.MapleStoryServerPort, '8484');
  assert.equal(cfg.AssetsServerIP, 'assets.example.com');
  assert.equal(cfg.AssetsServerPort, '443');
  assert.equal(cfg.AssetsServerProtocol, 'wss');
});

test('buildClientConfig omits assets keys when none available', () => {
  const cfg = settings.buildClientConfig(
    { serverUrl: 'ms.example.com:7575', assetsUrl: '' },
    { localProxyPort: 51000, localAssetsPort: null }
  );
  assert.ok(!('AssetsServerIP' in cfg));
  assert.ok(!('AssetsServerPort' in cfg));
  assert.ok(!('AssetsServerProtocol' in cfg));
  assert.equal(cfg.MapleStoryServerPort, '7575');
});

test('buildClientConfig invalid server yields empty object', () => {
  const cfg = settings.buildClientConfig({ serverUrl: 'http://x', assetsUrl: '' }, {});
  assert.deepEqual(cfg, {});
});

test('validateSettings returns null when valid', () => {
  assert.equal(
    settings.validateSettings(
      { serverUrl: 'ms.example.com', assetsUrl: '' },
      { hasLocalAssets: true }
    ),
    null
  );
  assert.equal(
    settings.validateSettings(
      { serverUrl: 'ws://p.example.com', assetsUrl: 'ws://a.example.com' },
      { hasLocalAssets: false }
    ),
    null
  );
});

test('validateSettings requires server url', () => {
  const err = settings.validateSettings({ serverUrl: '', assetsUrl: '' }, { hasLocalAssets: true });
  assert.equal(err, '请输入服务器地址');
});

test('validateSettings requires assets url without local assets', () => {
  const err = settings.validateSettings(
    { serverUrl: 'ms.example.com', assetsUrl: '' },
    { hasLocalAssets: false }
  );
  assert.equal(err, '未检测到本地游戏资源，请填写资源服务器 URL');
});

test('validateSettings surfaces bad assets url', () => {
  const err = settings.validateSettings(
    { serverUrl: 'ms.example.com', assetsUrl: 'http://a.example.com' },
    { hasLocalAssets: true }
  );
  assert.equal(err, '资源服务器仅支持 ws:// 或 wss:// 地址');
});

test('normalizeSettings picks known keys and coerces null', () => {
  assert.deepEqual(
    settings.normalizeSettings({ serverUrl: 'a', assetsUrl: null, extra: 1 }),
    { serverUrl: 'a', assetsUrl: '' }
  );
  assert.deepEqual(settings.normalizeSettings(undefined), { serverUrl: '', assetsUrl: '' });
});

test('loadSettings falls back to defaults on missing or corrupt file', async () => {
  const missing = await settings.loadSettings('/nonexistent-dir/settings.json');
  assert.deepEqual(missing, { serverUrl: '', assetsUrl: '' });
});
