'use strict';

const fs = require('node:fs');
const path = require('node:path');

const DEFAULT_LOGIN_PORT = 8484;
const DEFAULT_REMOTE_PROXY_PORT = 8080;
const DEFAULT_ASSETS_WS_PORT = 8765;
const DEFAULT_ASSETS_WSS_PORT = 443;

const EXPLICIT_SCHEME_RE = /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//;
const PORT_RANGE_ERROR = '端口必须是 1-65535 的数字';

const fail = (error) => ({ ok: false, error });

const explicitWsPort = (input) => {
  const match = input.match(
    /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\/(?:[^@/?#]*@)?(?:\[[^\]]*\]|[^:/?#]*)(?::(\d+))?/
  );
  return match && match[1] ? Number(match[1]) : null;
};

const parsePortText = (text) => {
  if (!/^\d+$/.test(text)) return null;
  const port = Number(text);
  return port >= 1 && port <= 65535 ? port : null;
};

const parseWsInput = (input, defaults) => {
  let url;
  try {
    url = new URL(input);
  } catch {
    return fail(defaults.invalidError);
  }
  if (!url.hostname) return fail(defaults.invalidError);
  const secure = url.protocol === 'wss:';
  const explicit = explicitWsPort(input);
  if (explicit !== null && (explicit < 1 || explicit > 65535)) {
    return fail(PORT_RANGE_ERROR);
  }
  return {
    ok: true,
    host: url.hostname,
    port: explicit !== null ? explicit : secure ? defaults.wssPort : defaults.wsPort,
    secure,
  };
};

const parseServerUrl = (raw) => {
  const input = String(raw == null ? '' : raw).trim();
  if (!input) return fail('请输入服务器地址');
  const lower = input.toLowerCase();
  if (lower.startsWith('ws://') || lower.startsWith('wss://')) {
    const parsed = parseWsInput(input, {
      invalidError: 'ws:// 服务器地址无效',
      wsPort: DEFAULT_REMOTE_PROXY_PORT,
      wssPort: DEFAULT_REMOTE_PROXY_PORT,
    });
    if (!parsed.ok) return parsed;
    return {
      ok: true,
      kind: 'ws',
      proxyHost: parsed.host,
      proxyPort: parsed.port,
      targetHost: parsed.host,
      targetPort: DEFAULT_LOGIN_PORT,
      secure: parsed.secure,
    };
  }
  if (EXPLICIT_SCHEME_RE.test(input)) {
    return fail('仅支持 ws:// 或 wss:// 前缀，或直接输入服务器地址');
  }
  return parseBareServerUrl(input);
};

const parseBareServerUrl = (input) => {
  if (/[/?#]/.test(input) || /\s/.test(input)) {
    return fail('服务器地址不能包含路径、参数或空格');
  }
  let host = input;
  let port = DEFAULT_LOGIN_PORT;
  if (input.startsWith('[')) {
    const close = input.indexOf(']');
    if (close === -1) return fail('IPv6 地址缺少右方括号');
    host = input.slice(1, close);
    const rest = input.slice(close + 1);
    if (rest) {
      if (!rest.startsWith(':')) return fail('IPv6 地址仅支持 [地址]:端口 格式');
      const parsed = parsePortText(rest.slice(1));
      if (parsed === null) return fail(PORT_RANGE_ERROR);
      port = parsed;
    }
  } else if (input.includes(':')) {
    const idx = input.indexOf(':');
    if (input.indexOf(':', idx + 1) !== -1) {
      return fail('IPv6 地址请使用方括号，例如 [::1]:7575');
    }
    host = input.slice(0, idx);
    const parsed = parsePortText(input.slice(idx + 1));
    if (parsed === null) return fail(PORT_RANGE_ERROR);
    port = parsed;
  }
  if (!host) return fail('请输入服务器地址');
  return {
    ok: true,
    kind: 'tcp',
    proxyHost: null,
    proxyPort: null,
    targetHost: host,
    targetPort: port,
    secure: false,
  };
};

const parseAssetsUrl = (raw) => {
  const input = String(raw == null ? '' : raw).trim();
  if (!input) return { ok: true, host: '', port: 0, secure: false };
  const lower = input.toLowerCase();
  if (!lower.startsWith('ws://') && !lower.startsWith('wss://')) {
    return fail('资源服务器仅支持 ws:// 或 wss:// 地址');
  }
  return parseWsInput(input, {
    invalidError: '资源服务器地址无效',
    wsPort: DEFAULT_ASSETS_WS_PORT,
    wssPort: DEFAULT_ASSETS_WSS_PORT,
  });
};

const buildClientConfig = (settings, runtime) => {
  const serverUrl = settings && settings.serverUrl ? String(settings.serverUrl) : '';
  const server = parseServerUrl(serverUrl);
  if (!server.ok) return {};
  const assetsUrl = settings && settings.assetsUrl ? String(settings.assetsUrl) : '';
  const assets = parseAssetsUrl(assetsUrl);
  const localProxyPort = runtime ? runtime.localProxyPort : null;
  const localAssetsPort = runtime ? runtime.localAssetsPort : null;
  const config = {
    MapleStoryServerIp: server.targetHost,
    MapleStoryServerPort: String(server.targetPort),
    ProxyIP: server.kind === 'tcp' ? '127.0.0.1' : server.proxyHost,
    ProxyPort:
      server.kind === 'tcp'
        ? localProxyPort == null
          ? null
          : String(localProxyPort)
        : String(server.proxyPort),
  };
  if (assets.ok && assets.host) {
    config.AssetsServerIP = assets.host;
    config.AssetsServerPort = String(assets.port);
    config.AssetsServerProtocol = assets.secure ? 'wss' : 'ws';
  } else if (localAssetsPort) {
    config.AssetsServerIP = '127.0.0.1';
    config.AssetsServerPort = String(localAssetsPort);
    config.AssetsServerProtocol = 'ws';
  }
  return config;
};

const validateSettings = (settings, options) => {
  const hasLocalAssets = Boolean(options && options.hasLocalAssets);
  const serverUrl = settings && settings.serverUrl ? String(settings.serverUrl).trim() : '';
  const server = parseServerUrl(serverUrl);
  if (!server.ok) return server.error;
  const assetsUrl = settings && settings.assetsUrl ? String(settings.assetsUrl).trim() : '';
  if (assetsUrl) {
    const assets = parseAssetsUrl(assetsUrl);
    if (!assets.ok) return assets.error;
    return null;
  }
  if (!hasLocalAssets) return '未检测到本地游戏资源，请填写资源服务器 URL';
  return null;
};

const defaultSettings = () => ({ serverUrl: '', assetsUrl: '' });

const normalizeSettings = (raw) => ({
  serverUrl: raw && raw.serverUrl != null ? String(raw.serverUrl) : '',
  assetsUrl: raw && raw.assetsUrl != null ? String(raw.assetsUrl) : '',
});

const loadSettings = async (filePath) => {
  try {
    const text = await fs.promises.readFile(filePath, 'utf8');
    return normalizeSettings(JSON.parse(text));
  } catch {
    return defaultSettings();
  }
};

const saveSettings = async (filePath, settings) => {
  const payload = normalizeSettings(settings);
  await fs.promises.mkdir(path.dirname(filePath), { recursive: true });
  await fs.promises.writeFile(filePath, JSON.stringify(payload, null, 2) + '\n', 'utf8');
};

module.exports = {
  DEFAULT_LOGIN_PORT,
  DEFAULT_REMOTE_PROXY_PORT,
  parseServerUrl,
  parseAssetsUrl,
  buildClientConfig,
  validateSettings,
  defaultSettings,
  normalizeSettings,
  loadSettings,
  saveSettings,
};
