'use strict';

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('desktopBridge', {
  getState: () => ipcRenderer.invoke('app:get-state'),
  save: (payload) => ipcRenderer.invoke('settings:save', payload),
  onLaunchResult: (callback) => {
    ipcRenderer.on('services:launch-result', (_event, result) => callback(result));
  },
});
