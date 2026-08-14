// Protocol-level E2E: login (auto-register) with a Chinese username and create a
// Chinese-named character on the linked server. Supports raw TCP (8484) or the
// ws-proxy (first WS message = "host:port").
import crypto from 'crypto';
import net from 'net';
import fs from 'fs';

const WS_URL = process.env.WS_URL || 'ws://127.0.0.1:8090';
const TARGET  = process.env.TARGET  || '127.0.0.1:8484';
const USER    = process.env.WIRE_USER || ('中测' + Date.now().toString().slice(-5)); // accounts.name is VARCHAR(13)
const PASS    = process.env.PASS    || 'test123';
const CHARNAME= process.env.CHARNAME|| '炎龙战士';
const VERSION = 83;

const MAPLE_KEY = Buffer.from([
  0x13,0,0,0, 0x08,0,0,0, 0x06,0,0,0, 0xB4,0,0,0,
  0x1B,0,0,0, 0x0F,0,0,0, 0x33,0,0,0, 0x52,0,0,0]);
const MB = JSON.parse(fs.readFileSync(new URL('./maplebytes.json', import.meta.url)));

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
  for (let i = 0; i < 16; i++) miv[i] = iv[i % 4];
  for (let x = 0; x < b.length; x++) {
    if (x % 16 === 0) aesEncryptBlock(miv).copy(miv);
    b[x] ^= miv[x % 16];
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

async function main() {
  const useTCP = !!process.env.DIRECT_TCP;
  const [host, port] = TARGET.split(':');
  const sock = useTCP ? net.connect(Number(port), host) : new WebSocket(WS_URL);
  if (!useTCP) {
    await new Promise((res, rej) => { sock.onopen = res; sock.onerror = rej; });
    sock.send(TARGET);
  }
  const hello = await firstFrame(sock);
  console.log('HELLO bytes:', hello.length, hello.toString('hex'));
  const sess = new MapleSession(sock);
  sess.attach();
  for (let i = 0; i < 4; i++) sess.sendiv[i] = hello[i + 7];
  for (let i = 0; i < 4; i++) sess.recviv[i] = hello[i + 11];

  // 1. LOGIN
  sess.sendPacket(0x01, Buffer.concat([writeStr(USER), writeStr(PASS), Buffer.alloc(10)]));
  let r1 = await sess.recvPacket();
  let status = r1.readInt32LE(2);
  console.log(`LOGIN_STATUS opcode=${r1[0]}|${r1[1]} status=${status}`);
  if (status === 23) {
    // TOS not accepted yet -> send ACCEPT_TOS, expect auth success next
    sess.sendPacket(0x07, Buffer.from([1])); // ACCEPT_TOS requires byte 1
    r1 = await sess.recvPacket();
    status = r1.readInt32LE(2);
    console.log(`after ACCEPT_TOS: opcode=${r1[0]}|${r1[1]} status=${status}`);
  }
  if (status !== 0) { console.log('LOGIN FAILED'); process.exit(2); }
  console.log('LOGIN OK (auto-register accepted Chinese username)');

  // 3. SERVERLIST_REQUEST (server replies with SERVERLIST + end marker + world info)
  sess.sendPacket(0x0B, Buffer.alloc(0));
  let r2 = await sess.recvPacket();
  console.log(`SERVERLIST opcode=${r2[0]}|${r2[1]} bytes=${r2.length}`);
  // Server sends: SERVERLIST, end marker (0x0A len 3), LAST_CONNECTED_WORLD (0x1A),
  // RECOMMENDED_WORLD_MESSAGE (0x1B). Stop after the last one.
  while (r2[0] !== 0x1B) {
    r2 = await sess.recvPacket();
    console.log(`world-info packet opcode=${r2[0]}|${r2[1]} bytes=${r2.length}`);
  }

  // 4. CHARLIST_REQUEST world=0 channel=0
  sess.sendPacket(0x05, Buffer.from([0x00, 0x00, 0x00]));
  const r3 = await sess.recvPacket();
  console.log(`CHARLIST opcode=${r3[0]}|${r3[1]} (byte2=${r3[2]} byte3=${r3[3]} byte4=${r3[4]})`);

  // 5. CHECK_CHAR_NAME with Chinese name
  sess.sendPacket(0x15, writeStr(CHARNAME));
  const r4 = await sess.recvPacket();
  let pos = 2;
  const nlen = r4.readInt16LE(pos); pos += 2;
  const echoed = r4.subarray(pos, pos + nlen).toString('utf8'); pos += nlen;
  const available = r4[pos];
  console.log(`CHAR_NAME_RESPONSE opcode=${r4[0]}|${r4[1]} name="${echoed}" available=${available}`);
  if (echoed !== CHARNAME) { console.log('NAME ECHO MISMATCH'); process.exit(3); }
  if (available !== 0) { console.log('NAME NOT AVAILABLE'); }

  // 6. CREATE_CHAR
  const nameBuf = writeStr(CHARNAME);
  const cc = Buffer.alloc(nameBuf.length + 40); // name + 9 ints + 1 byte
  nameBuf.copy(cc, 0);
  let o = nameBuf.length;
  cc.writeInt32LE(1, o); o += 4;      // job 1 = Adventurer (Beginner)
  cc.writeInt32LE(20000, o); o += 4;  // face
  cc.writeInt32LE(30030, o); o += 4;  // hair
  cc.writeInt32LE(0, o); o += 4;      // haircolor
  cc.writeInt32LE(0, o); o += 4;      // skincolor
  cc.writeInt32LE(1040002, o); o += 4; // top
  cc.writeInt32LE(1060002, o); o += 4; // bottom
  cc.writeInt32LE(1072001, o); o += 4; // shoes
  cc.writeInt32LE(1302000, o); o += 4; // weapon
  cc[o] = 0;                            // gender male
  sess.sendPacket(0x16, cc.subarray(0, o + 1));
  const r5 = await sess.recvPacket();
  console.log(`CREATE_CHAR response opcode=${r5[0]}`);
  if (r5[0] !== 0x0E) { console.log('UNEXPECTED CREATE RESPONSE'); process.exit(4); }
  const found = r5.includes(Buffer.from(CHARNAME, 'utf8'));
  console.log(`new char entry contains name echo: ${found}`);
  if (!found) {
    console.log('E2E FAIL');
    process.exit(5);
  }

  const closed = sess.waitForClose();
  sess.sendPacket(0x0C, Buffer.alloc(0));
  const transport = await closed;
  console.log(`PLAYER_DC closed ${transport} session`);
  console.log('E2E PASS: Chinese login + Chinese character creation + logout');
}

main().catch(e => { console.error('E2E ERROR:', e.message); process.exit(1); });
