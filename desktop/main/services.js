'use strict';

const { spawn } = require('node:child_process');
const http = require('node:http');
const net = require('node:net');

const { serviceExecutable } = require('./paths');

const POLL_INTERVAL_MS = 250;
const STARTUP_TIMEOUT_MS = 20000;
const MAX_OUTPUT_CHARS = 8192;

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const buildServicePlan = (layout, ports, options) => {
  const withLocalAssets = Boolean(options && options.withLocalAssets);
  const plan = [
    {
      name: 'web-server',
      exe: serviceExecutable(layout.binDir, 'web-server'),
      args: ['--bind', '127.0.0.1', '--port', String(ports.web), '--directory', layout.siteRoot],
      ready: { kind: 'http', url: `http://127.0.0.1:${ports.web}/web/index.html` },
    },
    {
      name: 'ws-proxy',
      exe: serviceExecutable(layout.binDir, 'ws-proxy'),
      args: ['--bind', '127.0.0.1', '--ws-port', String(ports.proxy)],
      ready: { kind: 'tcp', host: '127.0.0.1', port: ports.proxy },
    },
  ];
  if (withLocalAssets) {
    plan.push({
      name: 'assets-server',
      exe: serviceExecutable(layout.binDir, 'assets-server'),
      args: ['--bind', '127.0.0.1', '--port', String(ports.assets), '--directory', layout.siteRoot],
      ready: { kind: 'tcp', host: '127.0.0.1', port: ports.assets },
    });
  }
  return plan;
};

const httpOk = (url) =>
  new Promise((resolve) => {
    const req = http.get(url, { timeout: 2000 }, (res) => {
      res.resume();
      resolve(res.statusCode === 200);
    });
    req.on('timeout', () => req.destroy());
    req.on('error', () => resolve(false));
  });

const tcpOk = (host, port) =>
  new Promise((resolve) => {
    const socket = net.connect({ host, port });
    const done = (result) => {
      socket.destroy();
      resolve(result);
    };
    socket.setTimeout(2000, () => done(false));
    socket.once('connect', () => done(true));
    socket.once('error', () => done(false));
  });

const waitFor = async (probe, label, timeoutMs) => {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    if (await probe()) return;
    if (Date.now() >= deadline) throw new Error(`${label}在 ${timeoutMs}ms 内未就绪`);
    await delay(POLL_INTERVAL_MS);
  }
};

const waitForHttpOk = (url, timeoutMs) => waitFor(() => httpOk(url), url, timeoutMs);

const waitForTcp = (host, port, timeoutMs) =>
  waitFor(() => tcpOk(host, port), `${host}:${port}`, timeoutMs);

const spawnService = (service, onLog) => {
  const proc = spawn(service.exe, service.args, {
    stdio: ['ignore', 'pipe', 'pipe'],
    env: process.env,
  });
  const item = { name: service.name, proc, exe: service.exe, output: '' };
  const capture = (chunk) => {
    const text = chunk.toString();
    item.output = (item.output + text).slice(-MAX_OUTPUT_CHARS);
    onLog(service.name, text);
  };
  proc.stdout.on('data', capture);
  proc.stderr.on('data', capture);
  return item;
};

const outputTail = (item) => {
  const tail = String(item.output || '').trim();
  return tail ? `\n${tail.slice(-400)}` : '';
};

const waitForService = (item, service) => {
  const exited = new Promise((resolve) => {
    item.proc.once('exit', (code, signal) => resolve({ code, signal }));
  });
  const ready =
    service.ready.kind === 'http'
      ? waitForHttpOk(service.ready.url, STARTUP_TIMEOUT_MS)
      : waitForTcp(service.ready.host, service.ready.port, STARTUP_TIMEOUT_MS);
  const prefix = `服务 ${service.name} 启动失败`;
  return Promise.race([
    ready.catch((err) => {
      throw new Error(`${prefix}: ${err.message}${outputTail(item)}`);
    }),
    exited.then((info) => {
      throw new Error(
        `${prefix}: 进程提前退出 (code=${info.code} signal=${info.signal})${outputTail(item)}`
      );
    }),
  ]);
};

const killProcess = (proc) =>
  new Promise((resolve) => {
    if (proc.exitCode !== null || proc.signalCode !== null || proc.killed) {
      resolve();
      return;
    }
    let settled = false;
    const finish = () => {
      if (!settled) {
        settled = true;
        resolve();
      }
    };
    proc.once('exit', finish);
    setTimeout(finish, 5000);
    if (process.platform === 'win32') {
      spawn('taskkill', ['/pid', String(proc.pid), '/T', '/F'], { stdio: 'ignore' });
      return;
    }
    proc.kill('SIGTERM');
    setTimeout(() => {
      try {
        proc.kill('SIGKILL');
      } catch {
        /* already gone */
      }
    }, 3000);
  });

const stoppedBatches = new WeakSet();

const stopAll = async (started) => {
  if (!started || !Array.isArray(started.items)) return;
  if (stoppedBatches.has(started)) return;
  stoppedBatches.add(started);
  await Promise.all(started.items.map((item) => killProcess(item.proc)));
};

const startServices = async (plan, options) => {
  const onLog = options && options.onLog ? options.onLog : () => {};
  const items = [];
  try {
    for (const service of plan) {
      const item = spawnService(service, onLog);
      items.push(item);
      await waitForService(item, service);
    }
  } catch (err) {
    await stopAll({ items });
    throw err;
  }
  const started = { items, stopAll: () => stopAll(started) };
  return started;
};

module.exports = {
  buildServicePlan,
  waitForHttpOk,
  waitForTcp,
  startServices,
  stopAll,
};
