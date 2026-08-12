import { spawn } from 'child_process';
import fs from 'fs';
import os from 'os';
import path from 'path';

const sleep = (milliseconds) => new Promise(resolve => setTimeout(resolve, milliseconds));

export class BrowserDriver {
  constructor(options) {
    this.binary = options.binary;
    this.debugPort = options.debugPort;
    this.artifactDir = fs.mkdtempSync(path.join(os.tmpdir(), 'maplestory-webui-'));
    this.nextId = 0;
    this.pending = new Map();
    this.logs = [];
    this.errors = [];
  }

  async start() {
    const extraArgs = (process.env.E2E_CHROME_ARGS || '')
      .split(/\s+/)
      .filter(Boolean);
    const browserArgs = [
      '--no-sandbox', '--enable-webgl', '--ignore-gpu-blocklist',
      '--use-gl=swiftshader', '--enable-unsafe-swiftshader', '--mute-audio',
      '--no-proxy-server', '--proxy-bypass-list=*',
      `--remote-debugging-port=${this.debugPort}`, '--window-size=820,640',
      ...extraArgs, 'about:blank'
    ];
    if (process.env.E2E_HEADED !== '1') browserArgs.unshift('--headless=new');
    this.process = spawn(this.binary, browserArgs, { stdio: 'ignore' });

    for (let attempt = 0; attempt < 60; attempt += 1) {
      await sleep(250);
      try {
        const targets = await (await fetch(`http://127.0.0.1:${this.debugPort}/json/list`)).json();
        const page = targets.find(target => target.type === 'page');
        if (!page) continue;
        this.socket = new WebSocket(page.webSocketDebuggerUrl);
        break;
      } catch (error) {
        // Chromium is still starting.
      }
    }
    if (!this.socket) throw new Error('Chromium DevTools endpoint did not start');

    this.socket.onmessage = event => this.onMessage(JSON.parse(event.data));
    await new Promise(resolve => { this.socket.onopen = resolve; });
    await this.send('Runtime.enable');
    await this.send('Page.enable');
    await this.send('Network.enable');
  }

  onMessage(message) {
    if (message.id && this.pending.has(message.id)) {
      const callback = this.pending.get(message.id);
      this.pending.delete(message.id);
      message.error ? callback.reject(new Error(JSON.stringify(message.error))) : callback.resolve(message.result);
      return;
    }
    if (message.method === 'Runtime.consoleAPICalled') {
      this.logs.push(message.params.args.map(argument => argument.value ?? argument.description ?? '').join(' '));
    }
    if (message.method === 'Runtime.exceptionThrown') {
      this.errors.push(JSON.stringify(message.params.exceptionDetails));
    }
    if (message.method === 'Network.loadingFailed') {
      this.errors.push(`Network load failed: ${message.params.errorText} ${message.params.blockedReason || ''}`.trim());
    }
  }

  send(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = ++this.nextId;
      this.pending.set(id, { resolve, reject });
      this.socket.send(JSON.stringify({ id, method, params }));
    });
  }

  async evaluate(expression) {
    const result = await this.send('Runtime.evaluate', { expression, returnByValue: true });
    return result.result.value;
  }

  async navigate(url) {
    const result = await this.send('Page.navigate', { url });
    if (result.errorText) throw new Error(`Navigation failed: ${result.errorText}`);
    for (let attempt = 0; attempt < 120; attempt += 1) {
      // A provisional navigation may already expose the destination URL while
      // the old about:blank document still reports readyState=complete.
      if (await this.evaluate("document.readyState === 'complete' && document.documentElement.outerHTML.length > 100")) return;
      await sleep(250);
    }
    throw new Error(`Page did not finish loading: ${url}`);
  }

  async setNetworkLatency(milliseconds) {
    await this.send('Network.emulateNetworkConditions', {
      offline: false,
      latency: milliseconds,
      downloadThroughput: -1,
      uploadThroughput: -1
    });
  }

  async click(gameX, gameY) {
    await this.evaluate(`(() => {
      const canvas = document.getElementById('canvas');
      const bounds = canvas.getBoundingClientRect();
      const x = bounds.left + ${gameX} * bounds.width / 800;
      const y = bounds.top + ${gameY} * bounds.height / 600;
      const emit = (type, buttons) => canvas.dispatchEvent(new MouseEvent(type, {
        bubbles: true, cancelable: true, clientX: x, clientY: y,
        button: 0, buttons, view: window
      }));
      emit('mousemove', 0); emit('mousedown', 1); emit('mouseup', 0);
    })()`);
  }

  async doubleClick(gameX, gameY) {
    await this.click(gameX, gameY);
    await this.click(gameX, gameY);
  }

  async key(key, code, virtualKeyCode, text = '') {
    await this.send('Input.dispatchKeyEvent', {
      type: 'keyDown', key, code, windowsVirtualKeyCode: virtualKeyCode,
      nativeVirtualKeyCode: virtualKeyCode, text
    });
    await this.send('Input.dispatchKeyEvent', {
      type: 'keyUp', key, code, windowsVirtualKeyCode: virtualKeyCode,
      nativeVirtualKeyCode: virtualKeyCode
    });
  }

  async typeAscii(value) {
    for (const character of value) {
      const upper = character.toUpperCase();
      await this.key(character, `Key${upper}`, upper.charCodeAt(0), character);
    }
  }

  async compose(value) {
    const encoded = JSON.stringify(value);
    await this.evaluate(`(() => {
      const input = document.getElementById('ime-input');
      input.dispatchEvent(new CompositionEvent('compositionstart', { bubbles: true }));
      input.value = ${encoded};
      input.dispatchEvent(new InputEvent('input', { bubbles: true, data: ${encoded}, isComposing: true }));
      input.dispatchEvent(new CompositionEvent('compositionend', { bubbles: true, data: ${encoded} }));
    })()`);
  }

  async screenshot(name) {
    const result = await this.send('Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(path.join(this.artifactDir, `${name}.png`), Buffer.from(result.data, 'base64'));
  }

  async stop() {
    try { this.socket?.close(); } catch (error) {}
    try { this.process?.kill('SIGKILL'); } catch (error) {}
  }
}

export { sleep };
