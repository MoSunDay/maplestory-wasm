'use strict';

const net = require('node:net');

const DEFAULT_PORTS = { web: 8000, proxy: 8080, assets: 8765 };

const bindProbe = (port) =>
  new Promise((resolve) => {
    const server = net.createServer();
    server.once('error', () => resolve(null));
    server.listen(port, '127.0.0.1', () => {
      const bound = server.address().port;
      server.close(() => resolve(bound));
    });
  });

const acquirePort = async (preferred) => {
  const direct = await bindProbe(preferred);
  if (direct !== null) return direct;
  const ephemeral = await bindProbe(0);
  if (ephemeral === null) throw new Error('无法分配本地端口');
  return ephemeral;
};

const allocatePorts = async (preferences = DEFAULT_PORTS) => {
  const web = await acquirePort(preferences.web);
  const taken = new Set([web]);
  const unique = async (preferred) => {
    for (let attempt = 0; attempt < 8; attempt += 1) {
      const port = await acquirePort(preferred);
      if (!taken.has(port)) {
        taken.add(port);
        return port;
      }
    }
    throw new Error('无法分配互不冲突的本地端口');
  };
  const proxy = await unique(preferences.proxy);
  const assets = await unique(preferences.assets);
  return { web, proxy, assets };
};

module.exports = { DEFAULT_PORTS, acquirePort, allocatePorts };
