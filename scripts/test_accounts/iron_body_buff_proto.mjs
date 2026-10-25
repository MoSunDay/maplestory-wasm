// Iron Body (圣甲术, 1001003) buff settlement probe — wire-level E2E.
//
// Logs in a warrior test char (created by create_accounts.py, all job-line
// skills granted), casts Iron Body via SPECIAL_MOVE (0x5B) and asserts the
// server answers with GIVE_BUFF (0x20) carrying the WDEF statup in the second
// buff mask (0x2_0000_0000), per docs/ms-network-protocol.md §0x0020:
//   firstMask long, secondMask long, then per active stat (LSB first,
//   first-mask stats before second-mask stats):
//   value short, buffSource int, duration int(ms).
//
// Usage (same env as verify_skills_proto.mjs):
//   WIRE_USER=test1 PASS=test1 WIRE_WORLD=4 CHAR_SUFFIX=Hero node iron_body_buff_proto.mjs
// Exit codes: 0 = buff settled, 1 = no GIVE_BUFF (regression), 2 = harness error.
import net from 'net';
import { writeStr, openSession } from './maple_wire.mjs';

const LOGIN_TARGET = process.env.TARGET || '127.0.0.1:8484';
const WS_URL = process.env.WS_URL || 'ws://127.0.0.1:8090';
const USER = process.env.WIRE_USER || 'test1';
const PASS = process.env.PASS || USER;
const WORLD = parseInt(process.env.WIRE_WORLD || '4', 10);
const SUFFIX = process.env.CHAR_SUFFIX || 'Hero';
const SKILL_ID = parseInt(process.env.SKILL_ID || '1001003', 10);
const SKILL_LEVEL = parseInt(process.env.SKILL_LEVEL || '20', 10);
const WAIT_MS = parseInt(process.env.WAIT_MS || '3000', 10);

const OP = { PING: 0x11, PONG: 0x18, GIVE_BUFF: 0x20, SPECIAL_MOVE: 0x5B, STAT_CHANGED: 0x1F };
const WDEF_MASK = 0x200000000n;
const MACS = '68-5D-43-F8-B8-6C, 7A-79-19-8B-31-3F';
const HWID = '685D43F8B86C_B86C7A79';

const sleep = (ms) => new Promise((res) => setTimeout(res, ms));
const opcodeOf = (p) => p[0] | (p[1] << 8);
const fail = (msg, code = 1) => { console.error(msg); process.exit(code); };

function parseGiveBuff(p) {
  if (p.length < 28) return null;
  const firstMask = p.readBigUInt64LE(2);
  const secondMask = p.readBigUInt64LE(10);
  const value = p.readInt16LE(18);
  const source = p.readInt32LE(20);
  const duration = p.readInt32LE(24);
  return { firstMask, secondMask, value, source, duration };
}

function buildUseSkill(skillid, level) {
  const t = Buffer.alloc(4); t.writeInt32LE(Math.floor(Date.now() / 1000) & 0x7fffffff, 0);
  const id = Buffer.alloc(4); id.writeInt32LE(skillid, 0);
  return { opcode: OP.SPECIAL_MOVE, payload: Buffer.concat([t, id, Buffer.from([level & 0xFF])]) };
}

async function drainUntil(sess, deadline, want) {
  const seen = [];
  while (Date.now() < deadline) {
    let p;
    try { p = await sess.recvPacket(250); } catch (e) { continue; }
    const op = opcodeOf(p);
    if (op === OP.PING) { sess.sendPacket(OP.PONG, Buffer.alloc(0)); continue; }
    seen.push(op);
    if (op === want) return { packet: p, ops: seen };
  }
  return { packet: null, ops: seen };
}

async function main() {
  const wantName = USER + SUFFIX;
  const { sock: lsock, sess: login } = await openSession(LOGIN_TARGET, WS_URL);
  login.sendPacket(0x01, Buffer.concat([writeStr(USER), writeStr(PASS), Buffer.alloc(10)]));
  let r1 = await login.recvPacket();
  let status = r1.readInt32LE(2);
  if (status === 23) { login.sendPacket(0x07, Buffer.from([1])); r1 = await login.recvPacket(); status = r1.readInt32LE(2); }
  if (status !== 0) fail(`LOGIN FAILED status=${status}`, 2);
  console.log('LOGIN OK');

  login.sendPacket(0x0B, Buffer.alloc(0));
  let r2 = await login.recvPacket();
  while (opcodeOf(r2) !== 0x1B) r2 = await login.recvPacket();
  login.sendPacket(0x05, Buffer.from([0x00, WORLD, 0x00]));
  const r3 = await login.recvPacket();

  const want = Buffer.concat([Buffer.from(wantName, 'utf8'), Buffer.alloc(13, 0)]).subarray(0, 13);
  const idx = r3.indexOf(want);
  const charId = idx >= 4 ? r3.readInt32LE(idx - 4) : null;
  if (charId === null) fail(`char ${wantName} not found on world ${WORLD}`, 2);
  console.log(`CHAR ${wantName} id=${charId}`);

  const cid = Buffer.alloc(4); cid.writeInt32LE(charId, 0);
  login.sendPacket(0x13, Buffer.concat([cid, writeStr(MACS), writeStr(HWID)]));
  let rip = await login.recvPacket();
  while (opcodeOf(rip) !== 0x0C) rip = await login.recvPacket();
  const chan = { host: `${rip[4]}.${rip[5]}.${rip[6]}.${rip[7]}`, port: rip.readUInt16LE(8) };
  login._closed = () => {};
  lsock instanceof net.Socket ? lsock.destroy() : lsock.close();

  const { sess: game } = await openSession(`${chan.host}:${chan.port}`, WS_URL);
  let kicked = false;
  game._closed = () => { kicked = true; };
  game.sendPacket(0x14, cid);
  await sleep(700); // SET_FIELD burst

  const { opcode, payload } = buildUseSkill(SKILL_ID, SKILL_LEVEL);
  game.sendPacket(opcode, payload);
  console.log(`CAST ${SKILL_ID} level=${SKILL_LEVEL}`);

  const res = await drainUntil(game, Date.now() + WAIT_MS, OP.GIVE_BUFF);
  try { game.sendPacket(0x0C, Buffer.alloc(0)); } catch (e) { /* closing */ }

  if (kicked) fail('FAIL: session kicked after cast', 2);
  if (!res.packet) {
    console.error(`FAIL: no GIVE_BUFF within ${WAIT_MS}ms (ops seen: ${res.ops.map((o) => '0x' + o.toString(16)).join(',') || 'none'})`);
    process.exit(1);
  }
  const buff = parseGiveBuff(res.packet);
  console.log(`GIVE_BUFF first=0x${buff.firstMask.toString(16)} second=0x${buff.secondMask.toString(16)} value=${buff.value} source=${buff.source} duration=${buff.duration}ms`);
  const ok = (buff.secondMask & WDEF_MASK) === WDEF_MASK
    && buff.firstMask === 0n
    && buff.source === SKILL_ID
    && buff.value > 0 && buff.value <= 40
    && buff.duration > 0;
  if (!ok) fail('FAIL: GIVE_BUFF shape mismatch (expected WDEF-only buff from Iron Body)', 1);
  console.log(`PASS: Iron Body settled — WDEF +${buff.value} for ${Math.round(buff.duration / 1000)}s`);
  process.exit(0);
}

main().catch((e) => fail(`HARNESS ERROR: ${e.message}`, 2));
