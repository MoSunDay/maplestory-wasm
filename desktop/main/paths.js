'use strict';

const fs = require('node:fs');
const path = require('node:path');

const PLATFORM_NAMES = { win32: 'windows', darwin: 'darwin', linux: 'linux' };

const platformTag = () => {
  const osName = PLATFORM_NAMES[process.platform] || process.platform;
  return `${osName}-${process.arch}`;
};

const devSiteRoot = () => path.resolve(__dirname, '..', '..');

const hasNxAssets = (siteRoot) => {
  try {
    const dir = path.join(siteRoot, 'assets');
    if (!fs.statSync(dir).isDirectory()) return false;
    return fs.readdirSync(dir).some((name) => name.toLowerCase().endsWith('.nx'));
  } catch {
    return false;
  }
};

// process.resourcesPath is used as fallback: app.resourcesPath is undefined in
// some Electron builds while both should point at <app>/resources.
const resourcesPath = (app) => app.resourcesPath || process.resourcesPath;

const resolveLayout = (app) => {
  const isPackaged = Boolean(app.isPackaged);
  const base = isPackaged
    ? {
        siteRoot: path.join(resourcesPath(app), 'msweb'),
        binDir: path.join(resourcesPath(app), 'bin', platformTag()),
      }
    : {
        siteRoot: devSiteRoot(),
        binDir: path.join(devSiteRoot(), 'target', 'release'),
      };
  return { ...base, isPackaged, hasLocalAssets: hasNxAssets(base.siteRoot) };
};

const serviceExecutable = (binDir, name) =>
  path.join(binDir, name + (process.platform === 'win32' ? '.exe' : ''));

module.exports = { platformTag, resolveLayout, serviceExecutable, hasNxAssets };
