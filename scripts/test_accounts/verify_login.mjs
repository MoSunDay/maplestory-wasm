// Protocol-level E2E: login (auto-register) with a Chinese username and create a
// Chinese-named character on the linked server. Supports raw TCP (8484) or the
// ws-proxy (first WS message = "host:port").
import crypto from 'crypto';
import net from 'net';
import fs from 'fs';

const WS_URL = process.env.WS_URL || 'ws://127.0.0.1:8090';
const TARGET  = process.env.TARGET  || '127.0.0.1:8484';
const USER    = process.env.WIRE_USER || 'test1'; // accounts.name is VARCHAR(13)
const PASS    = process.env.PASS    || USER;
const CHARNAME= process.env.CHARNAME|| '炎龙战士';
const VERSION = 83;
const WORLD   = parseInt(process.env.WIRE_WORLD || '4', 10); // server worlds: 0 Scania .. 4 Reboot

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
    this.attached = false;
    this.expectedClose = null;
  }
  attach() {
    if (this.attached) return;
    this.attached = true;
    if (this.sock instanceof net.Socket) {
      this.sock.on('data', (chunk) => { this._feed(chunk); });
      this.sock.on('close', () => this._closed('TCP'));
      this.sock.on('error', (e) => { console.error('TCP error:', e.message); });
    } else {
      this.sock.onmessage = (ev) => { toBuf(ev.data).then((d) => this._feed(d)); };
      this.sock.onclose = () => this._closed('WS');
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
  async recvPacket() {
    const header = await this.readBytes(4);
    const hm = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
    let len = ((hm >> 16) ^ (hm & 0xFFFF)) & 0xFFFF;
    if (process.env.DEBUG_PKTS) console.error(`hdr=${header.toString('hex')} len=${len} pending=${this.pending.length}`);
    let body = await this.readBytes(len);
    aesofb(body, this.recviv);
    mapledecrypt(body);
    return body;
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


// Verify pre-seeded test accounts: login, serverlist, charlist, then check
// that every expected character name is present in the character list.
async function main() {
  const useTCP = !!process.env.DIRECT_TCP;
  const [host, port] = TARGET.split(':');
  const sock = useTCP ? net.connect(Number(port), host) : new WebSocket(WS_URL);
  if (!useTCP) {
    await new Promise((res, rej) => { sock.onopen = res; sock.onerror = rej; });
    sock.send(TARGET);
  }
  const hello = await firstFrame(sock);
  const sess = new MapleSession(sock);
  sess.attach();
  for (let i = 0; i < 4; i++) sess.sendiv[i] = hello[i + 7];
  for (let i = 0; i < 4; i++) sess.recviv[i] = hello[i + 11];

  sess.sendPacket(0x01, Buffer.concat([writeStr(USER), writeStr(PASS), Buffer.alloc(10)]));
  let r1 = await sess.recvPacket();
  let status = r1.readInt32LE(2);
  console.log(`LOGIN_STATUS opcode=${r1[0]}|${r1[1]} status=${status}`);
  if (status === 23) {
    sess.sendPacket(0x07, Buffer.from([1]));
    r1 = await sess.recvPacket();
    status = r1.readInt32LE(2);
    console.log(`after ACCEPT_TOS status=${status}`);
  }
  if (status !== 0) { console.log('LOGIN FAILED'); process.exit(2); }
  console.log('LOGIN OK');

  sess.sendPacket(0x0B, Buffer.alloc(0));
  let r2 = await sess.recvPacket();
  while (r2[0] !== 0x1B) r2 = await sess.recvPacket();
  sess.sendPacket(0x05, Buffer.from([0x00, WORLD, 0x00]));
  const r3 = await sess.recvPacket();
  console.log(`CHARLIST opcode=${r3[0]}|${r3[1]} bytes=${r3.length} status=${r3[2]} count=${r3[3]}`);
  const ALL_SUFFIXES = ['Hero','Pala','DK','FPA','ILA','Bish','BM','MM','NL','Shad','Bucc','CS'];
  const n = parseInt(process.env.CHARCOUNT || '4', 10);
  const acct = parseInt((USER.match(/[0-9]+$/) || ['1'])[0], 10);
  const expected = process.env.EXPECT_CHARS
    ? process.env.EXPECT_CHARS.split(',')
    : ALL_SUFFIXES.slice((acct - 1) * n, acct * n).map(sfx => USER + sfx);
  let missing = 0;
  for (const name of expected) {
    const found = r3.includes(Buffer.from(name, 'utf8'));
    if (!found) { missing++; console.log('MISSING CHAR IN LIST:', name); }
  }
  const count = r3[3];
  if (count !== n) console.log(`CHARLIST COUNT MISMATCH: got ${count}, expected ${n} (${USER})`);
  console.log(missing === 0 && count === n
    ? `ALL ${n} CHARACTERS VISIBLE (${USER})`
    : `${missing} characters missing`);
  const closed = sess.waitForClose();
  sess.sendPacket(0x0C, Buffer.alloc(0));
  await closed;
  process.exit(missing === 0 && count === n ? 0 : 6);
}

main().catch((e) => { console.error('E2E ERROR', e); process.exit(1); });
