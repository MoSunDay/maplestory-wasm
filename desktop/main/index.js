'use strict';

const { app, BrowserWindow, Menu, ipcMain, session } = require('electron');
const path = require('node:path');

const { resolveLayout } = require('./paths');
const { DEFAULT_PORTS, allocatePorts } = require('./ports');
const {
  buildClientConfig,
  defaultSettings,
  loadSettings,
  normalizeSettings,
  parseAssetsUrl,
  saveSettings,
  validateSettings,
} = require('./settings');
const {
  activateConfigScheme,
  attachConfigRedirect,
  registerConfigScheme,
} = require('./config-inject');
const { buildServicePlan, startServices, stopAll } = require('./services');

const state = {
  layout: null,
  settings: defaultSettings(),
  runtime: { webPort: null, localProxyPort: null, localAssetsPort: null },
  started: null,
  busy: false,
  lastError: '',
  cleaningUp: false,
  gameWindow: null,
  settingsWindow: null,
};

const settingsFilePath = () => path.join(app.getPath('userData'), 'settings.json');

const getConfig = () => buildClientConfig(state.settings, state.runtime) || {};

const logService = (name, text) => {
  const line = String(text).trimEnd();
  if (line) console.log(`[${name}] ${line}`);
};

const gameUrl = () => `http://127.0.0.1:${state.runtime.webPort}/web/index.html`;

const createGameWindow = () => {
  const existing = state.gameWindow;
  if (existing && !existing.isDestroyed()) {
    existing.loadURL(gameUrl());
    return existing;
  }
  const win = new BrowserWindow({
    width: 1366,
    height: 900,
    minWidth: 1024,
    minHeight: 768,
    title: 'JourneyClient',
    webPreferences: { contextIsolation: true, nodeIntegration: false },
  });
  win.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
  win.on('closed', () => {
    state.gameWindow = null;
  });
  state.gameWindow = win;
  win.loadURL(gameUrl());
  return win;
};

const createSettingsWindow = (reason) => {
  const existing = state.settingsWindow;
  if (existing && !existing.isDestroyed()) {
    existing.focus();
    return existing;
  }
  const win = new BrowserWindow({
    width: 560,
    height: 480,
    resizable: false,
    maximizable: false,
    title: 'JourneyClient 设置',
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      preload: path.join(__dirname, '..', 'preload.js'),
    },
  });
  win.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
  win.on('closed', () => {
    state.settingsWindow = null;
  });
  state.settingsWindow = win;
  const file = path.join(__dirname, '..', 'settings-ui', 'index.html');
  if (reason) win.loadFile(file, { query: { reason } });
  else win.loadFile(file);
  return win;
};

const useLocalAssets = () => {
  const assets = parseAssetsUrl(state.settings.assetsUrl || '');
  return Boolean(state.layout && state.layout.hasLocalAssets) && !(assets.ok && assets.host);
};

const relaunchServices = async () => {
  if (state.started) {
    const previous = state.started;
    state.started = null;
    await stopAll(previous);
  }
  const ports = await allocatePorts(DEFAULT_PORTS);
  const withLocalAssets = useLocalAssets();
  const plan = buildServicePlan(state.layout, ports, { withLocalAssets });
  const started = await startServices(plan, { onLog: logService });
  state.started = started;
  state.runtime = {
    webPort: ports.web,
    localProxyPort: ports.proxy,
    localAssetsPort: withLocalAssets ? ports.assets : null,
  };
  state.lastError = '';
  createGameWindow();
};

const sendLaunchResult = (result) => {
  const win = state.settingsWindow;
  if (win && !win.isDestroyed() && !win.webContents.isDestroyed()) {
    win.webContents.send('services:launch-result', result);
    return true;
  }
  return false;
};

const launchServicesAndReport = async () => {
  state.busy = true;
  try {
    await relaunchServices();
    state.busy = false;
    sendLaunchResult({ ok: true });
  } catch (err) {
    state.busy = false;
    state.lastError = err.message;
    if (!sendLaunchResult({ ok: false, error: err.message })) {
      createSettingsWindow(err.message);
    }
  }
};

const buildMenu = () => {
  Menu.setApplicationMenu(
    Menu.buildFromTemplate([
      {
        label: '文件',
        submenu: [
          { label: '设置…', accelerator: 'CmdOrCtrl+,', click: () => createSettingsWindow() },
          { type: 'separator' },
          { role: 'quit', label: '退出' },
        ],
      },
      {
        label: '视图',
        submenu: [
          { role: 'reload', label: '重新加载' },
          { role: 'toggleDevTools', label: '开发者工具' },
        ],
      },
    ])
  );
};

const boot = async () => {
  buildMenu();
  state.layout = resolveLayout(app);
  state.settings = await loadSettings(settingsFilePath());
  attachConfigRedirect(session.defaultSession);
  activateConfigScheme(getConfig);
  const error = validateSettings(state.settings, { hasLocalAssets: state.layout.hasLocalAssets });
  if (error) {
    createSettingsWindow(state.settings.serverUrl && state.settings.serverUrl.trim() ? error : '');
    return;
  }
  await launchServicesAndReport();
};

ipcMain.handle('app:get-state', () => ({
  settings: state.settings,
  hasLocalAssets: Boolean(state.layout && state.layout.hasLocalAssets),
  appVersion: app.getVersion(),
  busy: state.busy,
  lastError: state.lastError,
}));

ipcMain.handle('settings:save', async (_event, raw) => {
  const candidate = normalizeSettings(raw || {});
  const error = validateSettings(candidate, {
    hasLocalAssets: Boolean(state.layout && state.layout.hasLocalAssets),
  });
  if (error) return { ok: false, error };
  state.settings = candidate;
  await saveSettings(settingsFilePath(), candidate);
  launchServicesAndReport();
  return { ok: true };
});

app.on('second-instance', () => {
  if (state.gameWindow && !state.gameWindow.isDestroyed()) state.gameWindow.focus();
  else if (state.settingsWindow && !state.settingsWindow.isDestroyed()) {
    state.settingsWindow.focus();
  }
});

app.on('window-all-closed', () => {
  app.quit();
});

app.on('will-quit', (event) => {
  if (!state.started || state.cleaningUp) return;
  state.cleaningUp = true;
  event.preventDefault();
  const started = state.started;
  state.started = null;
  stopAll(started).finally(() => app.quit());
});

app.on('web-contents-created', (_event, contents) => {
  contents.on('will-attach-webview', (event) => event.preventDefault());
});

const gotLock = app.requestSingleInstanceLock();

if (gotLock) {
  registerConfigScheme();
  app.whenReady().then(boot).catch((err) => {
    console.error(err);
    state.lastError = err.message;
    app.quit();
  });
} else {
  app.quit();
}
