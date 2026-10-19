// Reusable MapleStory v83 wire stack, copied verbatim from verify_login.mjs
// (crypto: mapleencrypt/mapledecrypt/aesEncryptBlock/updateiv/aesofb,
// MapleSession, toBuf/writeStr/firstFrame). The only change: recvPacket()
// accepts a read timeout so callers can poll with short windows.
// WS transport: connect to WS_URL, first WS message (text) = "host:port".
import crypto from 'crypto';
import net from 'net';
import fs from 'fs';

const VERSION = 83;

const MAPLE_KEY = Buffer.from([
  0x13,0,0,0, 0x08,0,0,0, 0x06,0,0,0, 0xB4,0,0,0,
  0x1B,0,0,0, 0x0F,0,0,0, 0x33,0,0,0, 0x52,0,0,0]);
const MB = JSON.parse(fs.readFileSync(new URL('../maplebytes.json', import.meta.url)));

const rollleft  = (d, c) => { const m = (d & 0xFF) << (c % 8); return ((m & 0xFF) | (m >> 8)) & 0xFF; };
const rollright = (d, c) => { const m = ((d & 0xFF) << 8) >> (c % 8); return ((m & 0xFF) | (m >> 8)) & 0xFF; };

function mapleencrypt(b) {
  const n = b.length;
  for (let j = 0; j < 3; j++) {
    let remember = 0, datalen = n & 0xFF;
    for (let i = 0; i < n; i++) {
      let cur = (rollleft(b[i], 3) + datalen) ^ remember; remember = cur & 0xFF;
      cur = rollright(cur & 0xFF, datalen & 0xFF);
      b[i] = (((~cur) & 0xFF) + 0x48) & 0xFF; datalen = (datalen - 1) & 0xFF;
    }
    remember = 0; datalen = n & 0xFF;
    for (let i = n; i-- > 0;) {
      let cur = (rollleft(b[i], 4) + datalen) ^ remember; remember = cur & 0xFF;
      b[i] = rollright((cur ^ 0x13) & 0xFF, 3); datalen = (datalen - 1) & 0xFF;
    }
  }
}

function mapledecrypt(b) {
  const n = b.length;
  for (let j = 0; j < 3; j++) {
    let remember = 0, datalen = n & 0xFF;
    for (let i = n; i-- > 0;) {
      let cur = (rollleft(b[i], 3) ^ 0x13) & 0xFF;
      b[i] = rollright((((cur ^ remember) - datalen) & 0xFF), 4);
      remember = cur; datalen = (datalen - 1) & 0xFF;
    }
    remember = 0; datalen = n & 0xFF;
    for (let i = 0; i < n; i++) {
      let cur = (~(b[i] - 0x48)) & 0xFF;
      cur = rollleft(cur, datalen & 0xFF);
      b[i] = rollright((((cur ^ remember) - datalen) & 0xFF), 3);
      remember = cur; datalen = (datalen - 1) & 0xFF;
    }
  }
}

function aesEncryptBlock(block) {
  const c = crypto.createCipheriv('aes-256-ecb', MAPLE_KEY, null);
  c.setAutoPadding(false);
  return Buffer.concat([c.update(block), c.final()]);
}

function updateiv(iv) {
  const m = [0xF2, 0x53, 0x50, 0xC6];
  for (let i = 0; i < 4; i++) {
    const ivbyte = iv[i];
    m[0] = (m[0] + MB[m[1] & 0xFF] - ivbyte) & 0xFF;
    m[1] = (m[1] - (m[2] ^ (MB[ivbyte & 0xFF] & 0xFF))) & 0xFF;
    m[2] = (m[2] ^ (MB[m[3] & 0xFF] + ivbyte)) & 0xFF;
    m[3] = (m[3] + (MB[ivbyte & 0xFF] & 0xFF) - (m[0] & 0xFF)) & 0xFF;
    let mask = m[0] | (m[1] << 8) | (m[2] << 16) | (m[3] << 24);
    mask = ((mask >>> 0x1D) | (mask << 3)) >>> 0;
    for (let j = 0; j < 4; j++) m[j] = (mask >>> (8 * j)) & 0xFF;
  }
  for (let i = 0; i < 4; i++) iv[i] = m[i];
}

function aesofb(b, iv) {
  const miv = Buffer.alloc(16);
  let remaining = b.length;
  let llength = 0x5b0;
  let start = 0;
  while (remaining > 0) {
    for (let i = 0; i < 16; i++) miv[i] = iv[i % 4];
    if (remaining < llength) llength = remaining;
    for (let x = start; x < start + llength; x++) {
      if ((x - start) % 16 === 0) aesEncryptBlock(miv).copy(miv);
      b[x] ^= miv[(x - start) % 16];
    }
    start += llength;
    remaining -= llength;
    llength = 0x5b4;
  }
  updateiv(iv);
}

class MapleSession {
  constructor(sock) {
    this.sock = sock;
    this.sendiv = Buffer.alloc(4);
    this.recviv = Buffer.alloc(4);
    this.queue = [];
    this.waiters = [];
    this.pending = Buffer.alloc(0);
    this.dataWaiters = [];
    this.attached = false;
    this.expectedClose = null;
  }
  attach() {
    if (this.attached) return;
    this.attached = true;
    if (this.sock instanceof net.Socket) {
      this.sock.on('data', (chunk) => { this._feed(chunk); });
      this.sock.on('close', () => { this._closed('TCP'); });
      this.sock.on('error', (e) => { console.error('TCP error:', e.message); });
    } else {
      this.sock.onmessage = (ev) => { toBuf(ev.data).then((d) => this._feed(d)); };
      this.sock.onclose = () => { this._closed('WS'); };
    }
  }
  _closed(transport) {
    if (this.expectedClose) {
      this.expectedClose(transport);
      this.expectedClose = null;
      return;
    }
    console.error(`${transport} closed by server`);
    process.exit(9);
  }
  waitForClose(timeout = 10000) {
    return new Promise((res, rej) => {
      const timer = setTimeout(() => {
        this.expectedClose = null;
        rej(new Error('logout close timeout'));
      }, timeout);
      this.expectedClose = (transport) => {
        clearTimeout(timer);
        res(transport);
      };
    });
  }
  _feed(data) {
    this.pending = Buffer.concat([this.pending, data]);
    this._pump();
    const waiters = this.dataWaiters;
    this.dataWaiters = [];
    for (const w of waiters) w();
  }
  _pump() {
    if (this.byteWaiter && this.pending.length >= this.byteWaiter.n) {
      const { n, res } = this.byteWaiter;
      this.byteWaiter = null;
      const chunk = this.pending.subarray(0, n);
      this.pending = this.pending.subarray(n);
      res(chunk);
      this._pump();
    }
  }
  readBytes(n, timeout = 10000) {
    if (this.pending.length >= n) {
      const chunk = this.pending.subarray(0, n);
      this.pending = this.pending.subarray(n);
      return Promise.resolve(chunk);
    }
    return new Promise((res, rej) => {
      const t = setTimeout(() => { this.byteWaiter = null; rej(new Error('recv timeout')); }, timeout);
      this.byteWaiter = { n, res: (d) => { clearTimeout(t); res(d); } };
    });
  }
  sendRaw(buf) { if (this.sock instanceof net.Socket) this.sock.write(buf); else this.sock.send(buf); }
  sendPacket(opcode, payload) {
    const body = Buffer.concat([Buffer.from([opcode & 0xFF, (opcode >> 8) & 0xFF]), payload]);
    // Header must be derived from the IV BEFORE encryption mutates it.
    const len = body.length;
    const a = ((this.sendiv[3] << 8) | this.sendiv[2]) ^ VERSION;
    const b = a ^ len;
    const header = Buffer.from([a & 0xFF, (a >> 8) & 0xFF, b & 0xFF, (b >> 8) & 0xFF]);
    mapleencrypt(body);
    aesofb(body, this.sendiv);
    this.sendRaw(Buffer.concat([header, body]));
    return len;
  }
  // Resolve(true) on the next _feed, resolve(false) on timeout. Never consumes.
  waitForMore(timeout) {
    return new Promise((res) => {
      const fire = () => { clearTimeout(t); res(true); };
      const t = setTimeout(() => {
        this.dataWaiters = this.dataWaiters.filter((w) => w !== fire);
        res(false);
      }, timeout);
      this.dataWaiters.push(fire);
    });
  }

  // Atomic frame read: peeks the length header, waits until the WHOLE frame is
  // buffered and only then consumes + decrypts it. A timeout rejects without
  // consuming anything, so a short read timeout can never desync the stream
  // (the old header-then-body variant ate the 4-byte header on a mid-frame
  // timeout and permanently misframed every later packet).
  async recvPacket(timeout = 10000) {
    for (;;) {
      if (this.pending.length >= 4) {
        const hm = this.pending[0] | (this.pending[1] << 8) | (this.pending[2] << 16) | (this.pending[3] << 24);
        const len = ((hm >> 16) ^ (hm & 0xFFFF)) & 0xFFFF;
        if (this.pending.length >= 4 + len) {
          const body = this.pending.subarray(4, 4 + len);
          this.pending = this.pending.subarray(4 + len);
          aesofb(body, this.recviv);
          mapledecrypt(body);
          return body;
        }
      }
      if (!(await this.waitForMore(timeout))) throw new Error('recv timeout');
    }
  }
}

async function toBuf(d) {
  if (Buffer.isBuffer(d)) return d;
  if (d instanceof ArrayBuffer) return Buffer.from(d);
  if (d && d.arrayBuffer) return Buffer.from(await d.arrayBuffer());
  return Buffer.from(String(d));
}

const writeStr = (s) => {
  const b = Buffer.from(s, 'utf8');
  const out = Buffer.alloc(2 + b.length);
  out.writeInt16LE(b.length, 0);
  b.copy(out, 2);
  return out;
};

async function firstFrame(sock, timeout = 10000) {
  if (sock instanceof net.Socket) {
    return await new Promise((res, rej) => {
      const t = setTimeout(() => rej(new Error('no handshake')), timeout);
      sock.once('data', (d) => { clearTimeout(t); res(Buffer.from(d)); });
    });
  }
  return await new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error('no handshake')), timeout);
    sock.onmessage = async (ev) => { clearTimeout(t); res(await toBuf(ev.data)); };
  });
}

// Opens a session to `hostPort` via ws-proxy (WS_URL; first WS message is the
// text "host:port") or raw TCP when DIRECT_TCP is set. Seeds the IVs from the
// server hello: sendiv = hello[7..11], recviv = hello[11..15].
async function openSession(hostPort, wsUrl) {
  const useTCP = !!process.env.DIRECT_TCP;
  const [host, port] = hostPort.split(':');
  const sock = useTCP ? net.connect(Number(port), host) : new WebSocket(wsUrl);
  if (!useTCP) {
    await new Promise((res, rej) => { sock.onopen = res; sock.onerror = rej; });
    sock.send(hostPort);
  }
  const hello = await firstFrame(sock);
  const sess = new MapleSession(sock);
  sess.attach();
  for (let i = 0; i < 4; i++) sess.sendiv[i] = hello[i + 7];
  for (let i = 0; i < 4; i++) sess.recviv[i] = hello[i + 11];
  return { sock, sess };
}

export { MapleSession, toBuf, writeStr, firstFrame, openSession };
