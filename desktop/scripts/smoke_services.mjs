#!/usr/bin/env node
// Smoke test for packaged desktop services: web-server (HTTP + COOP/COEP),
// ws-proxy (TCP) and assets-server (TCP, only when local .nx assets exist).
// Node >= 18, zero dependencies.

import { spawn } from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
import net from 'node:net';
import path from 'node:path';

const HOST = '127.0.0.1';
const TAIL_MAX = 20;
const POLL_MS = 250;

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const parseArgs = (argv) => {
  const out = { binDir: '', site: '', webPort: 0 };
  for (let i = 0; i < argv.length; i += 1) {
    const key = argv[i];
    const value = argv[i + 1];
    if (key === '--bin-dir' && value) { out.binDir = value; i += 1; }
    else if (key === '--site' && value) { out.site = value; i += 1; }
    else if (key === '--web-port' && value) { out.webPort = Number(value); i += 1; }
    else if (key === '--help' || key === '-h') { out.help = true; }
    else { throw new Error(`unknown or missing value for argument: ${key}`); }
  }
  if (out.help || !out.binDir || !out.site) {
    throw new Error('usage: node smoke_services.mjs --bin-dir <dir> --site <mswebRoot> [--web-port N]');
  }
  return out;
};

// Grab a free port by binding to :0, reading the assigned port, then closing.
const freePort = () => new Promise((resolve, reject) => {
  const server = net.createServer();
  server.on('error', reject);
  server.listen(0, HOST, () => {
    const { port } = server.address();
    server.close(() => resolve(port));
  });
});

const makeTail = () => {
  const lines = [];
  return {
    push: (chunk) => {
      for (const line of String(chunk).split(/\r?\n/)) {
        if (!line.trim()) continue;
        lines.push(line);
        if (lines.length > TAIL_MAX) lines.shift();
      }
    },
    text: () => lines.join('\n'),
  };
};

const startService = (binDir, name, args) => {
  const exe = name + (process.platform === 'win32' ? '.exe' : '');
  const file = path.join(binDir, exe);
  if (!fs.existsSync(file)) throw new Error(`missing service binary: ${file}`);
  const tail = makeTail();
  const child = spawn(file, args, { stdio: ['ignore', 'ignore', 'pipe'] });
  const exited = new Promise((resolve) => child.once('exit', () => resolve()));
  child.stderr.on('data', tail.push);
  child.once('error', (err) => tail.push(`spawn error: ${err.message}`));
  return { name, child, tail, exited, failed: null };
};

const tryConnect = (port) => new Promise((resolve) => {
  const sock = net.connect({ host: HOST, port });
  sock.once('connect', () => { sock.destroy(); resolve(true); });
  sock.once('error', () => { sock.destroy(); resolve(false); });
});

const httpFetch = (url, timeoutMs) => new Promise((resolve, reject) => {
  const req = http.get(url, { timeout: timeoutMs }, (res) => {
    res.resume();
    resolve({ status: res.statusCode, headers: res.headers });
  });
  req.once('timeout', () => req.destroy(new Error('http timeout')));
  req.once('error', reject);
});

const checkWeb = async (port) => {
  const res = await httpFetch(`http://${HOST}:${port}/web/index.html`, 3000);
  const coop = res.headers['cross-origin-opener-policy'];
  const coep = res.headers['cross-origin-embedder-policy'];
  if (res.status !== 200) return { ok: false, error: `HTTP status ${res.status}` };
  if (String(coop).toLowerCase() !== 'same-origin') {
    return { ok: false, error: `Cross-Origin-Opener-Policy=${coop ?? '(missing)'}` };
  }
  if (String(coep).toLowerCase() !== 'require-corp') {
    return { ok: false, error: `Cross-Origin-Embedder-Policy=${coep ?? '(missing)'}` };
  }
  return { ok: true, value: { status: res.status, coop, coep } };};

const waitFor = async (label, check, services, timeoutMs) => {
  const deadline = Date.now() + timeoutMs;
  let lastReason = 'no response yet';
  for (;;) {
    const dead = services.filter((s) => s.child.exitCode !== null);
    if (dead.length) {
      throw new Error(`${label}: process exited early (${dead.map((s) => s.name).join(', ')})`);
    }
    try {
      const result = await check();
      if (result.ok) return result.value;
      lastReason = result.error || 'check failed';
    } catch (err) {
      lastReason = err.message;
    }
    if (Date.now() + POLL_MS >= deadline) {
      throw new Error(`${label}: not ready within ${timeoutMs}ms (last: ${lastReason})`);
    }
    await sleep(POLL_MS);
  }
};

const hasLocalNx = (site) => {
  try {
    return fs.readdirSync(path.join(site, 'assets')).some((f) => f.toLowerCase().endsWith('.nx'));
  } catch {
    return false;
  }
};const taskkill = (pid) => new Promise((resolve) => {
  const killer = spawn('taskkill', ['/pid', String(pid), '/T', '/F'], { stdio: 'ignore' });
  killer.once('close', resolve);
  killer.once('error', resolve);
});

const stopService = async (service) => {
  const { child, exited } = service;
  if (child.exitCode !== null) return;
  if (process.platform === 'win32') await taskkill(child.pid);
  else child.kill('SIGKILL');
  await Promise.race([exited, sleep(3000)]);
};

const dumpServices = (services) => {
  for (const s of services) {
    const state = s.child.exitCode === null ? 'running' : `exit ${s.child.exitCode}`;
    console.error(`--- ${s.name} (pid ${s.child.pid}, ${state}) stderr tail ---`);
    console.error(s.tail.text() || '(no stderr output)');
  }
};

const main = async () => {
  const args = parseArgs(process.argv.slice(2));
  const binDir = path.resolve(args.binDir);
  const site = path.resolve(args.site);
  const webPort = args.webPort > 0 ? args.webPort : await freePort();
  const proxyPort = await freePort();
  const assetsPort = await freePort();
  const services = [];
  try {
    const web = startService(binDir, 'web-server', ['--bind', HOST, '--port', String(webPort), '--directory', site]);
    services.push(web);
    const webCheck = await waitFor('web-server GET /web/index.html', () => checkWeb(webPort), [web], 15000);

    const proxy = startService(binDir, 'ws-proxy', ['--bind', HOST, '--ws-port', String(proxyPort)]);
    services.push(proxy);
    await waitFor('ws-proxy tcp connect',
      async () => ({ ok: await tryConnect(proxyPort), error: 'connect refused' }), [proxy], 10000);

    let assetsState = 'skipped';
    let assetsUsed = null;
    if (hasLocalNx(site)) {
      const assets = startService(binDir, 'assets-server',
        ['--bind', HOST, '--port', String(assetsPort), '--directory', site]);
      services.push(assets);
      await waitFor('assets-server tcp connect',
        async () => ({ ok: await tryConnect(assetsPort), error: 'connect refused' }), [assets], 10000);
      assetsState = 'ok';
      assetsUsed = assetsPort;
    } else {
      console.log('SKIP assets-server (no local .nx)');
    }

    const summary = {
      webPort,
      proxyPort,
      assetsPort: assetsUsed,
      index: webCheck,
      wsProxy: 'ok',
      assets: assetsState,
    };
    console.log(`SMOKE_OK ${JSON.stringify(summary)}`);
  } catch (err) {
    console.error(`SMOKE_FAIL: ${err.message}`);
    dumpServices(services);
    process.exitCode = 1;
  } finally {
    for (const service of services) await stopService(service);
  }
};

main().catch((err) => { console.error(err); process.exit(1); });
