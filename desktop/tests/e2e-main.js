// automated e2e entry; run: xvfb-run -a npx electron tests/e2e-main.js
// (as root additionally set ELECTRON_DISABLE_SANDBOX=1)
// overrides: E2E_SITE_ROOT, E2E_BIN_DIR
'use strict';

const { app, BrowserWindow, session } = require('electron');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const { hasNxAssets, resolveLayout } = require('../main/paths');
const { allocatePorts } = require('../main/ports');
const { buildClientConfig } = require('../main/settings');
const {
  activateConfigScheme,
  attachConfigRedirect,
  registerConfigScheme,
} = require('../main/config-inject');
const { buildServicePlan, startServices, stopAll } = require('../main/services');

let started = null;
let finished = false;
let mainWindow = null;

const shutdown = async (code) => {
  if (finished) return;
  finished = true;
  if (mainWindow && !mainWindow.isDestroyed()) mainWindow.destroy();
  if (started) {
    const batch = started;
    started = null;
    await stopAll(batch);
  }
  app.exit(code);
};

const report = async (ok, detail) => {
  console.log(ok ? 'E2E_OK' : 'E2E_FAIL');
  console.log(typeof detail === 'string' ? detail : JSON.stringify(detail));
  await shutdown(ok ? 0 : 1);
};

const logService = (name, text) => {
  const line = String(text).trimEnd();
  if (line) console.log(`[${name}] ${line}`);
};

const run = async () => {
  app.setPath('userData', fs.mkdtempSync(path.join(os.tmpdir(), 'msw-e2e-')));
  const layout = resolveLayout(app);
  layout.siteRoot = process.env.E2E_SITE_ROOT || layout.siteRoot;
  layout.binDir = process.env.E2E_BIN_DIR || layout.binDir;
  layout.hasLocalAssets = hasNxAssets(layout.siteRoot);
  console.log('layout:', JSON.stringify(layout));

  const ports = await allocatePorts();
  console.log('ports:', JSON.stringify(ports));

  const plan = buildServicePlan(layout, ports, { withLocalAssets: layout.hasLocalAssets });
  started = await startServices(plan, { onLog: logService });

  const runtime = {
    localProxyPort: ports.proxy,
    localAssetsPort: layout.hasLocalAssets ? ports.assets : null,
  };
  const config = buildClientConfig(
    { serverUrl: 'demo.example.com', assetsUrl: '' },
    runtime
  );
  console.log('config:', JSON.stringify(config));

  activateConfigScheme(() => config);
  attachConfigRedirect(session.defaultSession);

  mainWindow = new BrowserWindow({
    show: false,
    width: 1024,
    height: 768,
    webPreferences: { contextIsolation: true, nodeIntegration: false },
  });
  await mainWindow.loadURL(`http://127.0.0.1:${ports.web}/web/index.html`);

  const fetched = await mainWindow.webContents.executeJavaScript(
    "fetch('./config.json').then((r) => r.json())"
  );
  const checks = {
    ProxyIP: fetched.ProxyIP === '127.0.0.1',
    ProxyPort: fetched.ProxyPort === String(ports.proxy),
    MapleStoryServerIp: fetched.MapleStoryServerIp === 'demo.example.com',
    MapleStoryServerPort: fetched.MapleStoryServerPort === '8484',
  };
  if (Object.values(checks).some((pass) => !pass)) {
    await report(false, { stage: 'config-fetch', checks, fetched });
    return;
  }

  const assetPath = path.join(layout.siteRoot, 'build', 'JourneyClient.js');
  if (fs.existsSync(assetPath)) {
    const status = await mainWindow.webContents.executeJavaScript(
      "fetch('../build/JourneyClient.js', { method: 'HEAD' }).then((r) => r.status)"
    );
    if (status !== 200) {
      await report(false, { stage: 'asset-head', status });
      return;
    }
    checks.assetHead = true;
  }

  await report(true, { checks, config, ports });
};

registerConfigScheme();
app.whenReady().then(run).catch((err) => report(false, String((err && err.stack) || err)));
setTimeout(() => report(false, 'timeout after 120s'), 120000).unref();
process.on('uncaughtException', (err) => report(false, String((err && err.stack) || err)));
