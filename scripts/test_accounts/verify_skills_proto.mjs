// Protocol bot: logs in one test character, enters the game via the ws-proxy
// and fires every attack skill from attack_skills.json at a spawned mob,
// verifying that the server settles the damage (SHOW_MONSTER_HP 0xFA).
//
// Attack packets mirror src/client/Net/Packets/AttackAndSkillPackets.h
// AttackPacket byte-for-byte; mob oid offsets mirror MapObjectHandlers.cpp.
// Wire/crypto stack (copied verbatim from verify_login.mjs) lives in
// ./maple_wire.mjs — split out to respect the 400-line file limit.
import fs from 'fs';
import net from 'net';
import { writeStr, openSession } from './maple_wire.mjs';

const WS_URL       = process.env.WS_URL || 'ws://127.0.0.1:8090';
const LOGIN_TARGET = process.env.TARGET || '127.0.0.1:8484';
const USER         = process.env.WIRE_USER || 'test1'; // accounts.name is VARCHAR(13)
const PASS         = process.env.PASS || USER;
const WORLD        = parseInt(process.env.WIRE_WORLD || '4', 10);
const SUFFIX       = process.env.CHAR_SUFFIX || '';
const ATTACK_JSON  = process.env.ATTACK_JSON || new URL('./attack_skills.json', import.meta.url).pathname;
const RESULTS_JSON = process.env.RESULTS_JSON || `/tmp/skill_proto_${USER}_${SUFFIX || 'none'}.json`;

// Opcodes: client Net/OutPacket.h, Net/PacketSwitch.cpp, docs/ms-network-protocol.md
const OP = {
  CLOSE_ATTACK: 0x2C, RANGED_ATTACK: 0x2D, MAGIC_ATTACK: 0x2E,
  PING: 0x11, PONG: 0x18, STAT_CHANGED: 0x1F, KILL_MOB: 0xED,
  SPAWN_MOB: 0xEC, SPAWN_MOB_C: 0xEE, SHOW_MONSTER_HP: 0xFA,
};
const ATTACK_OPCODE = { melee: OP.CLOSE_ATTACK, ranged: OP.RANGED_ATTACK, magic: OP.MAGIC_ATTACK };
const STANCE = { melee: 2, ranged: 2, magic: 27 };
const SPEED_BYTE = 44;

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------
const sleep = (ms) => new Promise((res) => setTimeout(res, ms));
const opcodeOf = (p) => p[0] | (p[1] << 8);
const i32 = (p, o) => (p.length >= o + 4 ? p.readInt32LE(o) : null);
const u16 = (p, o) => (p.length >= o + 2 ? p.readUInt16LE(o) : null);

// Mirrors AttackPacket (AttackAndSkillPackets.h lines 30-80):
//   skip(1); byte (mobcount<<4 | hitcount); int skill; [int charge if >0];
//   skip(8); byte display; byte toleft; byte stance; skip(1); byte speed;
//   ranged ? { skip(1); byte toleft; skip(7); } : skip(4);
//   per target: int oid; skip(14); per hit int encode(damage, false); skip(4)
//   (skill 5221004 omits the trailing skip(4)).
// NOTE: magic takes the same skip(4) tail as melee — NO extra magic bytes.
function buildAttack({ kind, skillid, hitcount, charge, oid, damage }) {
  const parts = [];
  parts.push(Buffer.from([0x00]));                              // skip(1)
  parts.push(Buffer.from([(1 << 4) | (hitcount & 0xF)]));       // mobcount=1
  const sk = Buffer.alloc(4); sk.writeInt32LE(skillid, 0); parts.push(sk);
  if (charge) { const ch = Buffer.alloc(4); ch.writeInt32LE(1000, 0); parts.push(ch); }
  parts.push(Buffer.alloc(8));                                  // skip(8)
  parts.push(Buffer.from([0x00, 0x00, STANCE[kind] ?? 2]));     // display, toleft, stance
  parts.push(Buffer.from([0x00]));                              // skip(1)
  parts.push(Buffer.from([SPEED_BYTE]));
  if (kind === 'ranged') {
    parts.push(Buffer.from([0x00, 0x00]));                      // skip(1) + rangedirection
    parts.push(Buffer.alloc(7));
  } else {
    parts.push(Buffer.alloc(4));
  }
  const o = Buffer.alloc(4); o.writeInt32LE(oid, 0); parts.push(o);
  parts.push(Buffer.alloc(14));                                 // skip(14)
  for (let h = 0; h < hitcount; h++) {
    const d = Buffer.alloc(4); d.writeInt32LE(damage, 0); parts.push(d);
  }
  if (skillid !== 5221004) parts.push(Buffer.alloc(4));
  return { opcode: ATTACK_OPCODE[kind] ?? OP.CLOSE_ATTACK, payload: Buffer.concat(parts) };
}

// MapObjectHandlers.cpp: SPAWN_MOB(0xEC) = int oid @2; SPAWN_MOB_C(0xEE) =
// byte mode @2 then int oid @3; KILL_MOB(0xED) = int oid @2;
// SHOW_MOB_HP(0xFA) = int oid @2 + byte hppercent @6.
const oidOfSpawn = (p) => (opcodeOf(p) === OP.SPAWN_MOB_C ? i32(p, 3) : i32(p, 2));
const oidOfKill = (p) => i32(p, 2);
const oidOfHp = (p) => i32(p, 2);

// CHARLIST (0x0B): status @2, count @3, entries of stride 181:
// int32 id then fixed 13-byte NUL-padded name (MaplePacketCreator.addCharEntry).
function parseCharlist(body) {
  const count = body[3] || 0;
  const chars = [];
  for (let i = 0; i < count; i++) {
    const base = 4 + i * 181;
    if (base + 17 > body.length) break;
    chars.push({
      id: body.readInt32LE(base),
      name: body.toString('utf8', base + 4, base + 17).split('\0')[0],
    });
  }
  return { count, chars };
}

// Fallback if the stride drifts: locate the padded 13-byte name field, id at -4.
function findCharByName(body, name) {
  const want = Buffer.concat([Buffer.from(name, 'utf8'), Buffer.alloc(13, 0)]).subarray(0, 13);
  const idx = body.indexOf(want);
  return idx >= 4 ? body.readInt32LE(idx - 4) : null;
}

// SERVER_IP (0x0C): u16 0, ipv4 4B, u16 port, int32 cid (after 2-byte opcode).
const parseServerIp = (body) => ({
  host: `${body[4]}.${body[5]}.${body[6]}.${body[7]}`,
  port: u16(body, 8),
  charId: i32(body, 10),
});

const pushOid = (oids, oid) => {
  if (oid !== null && oid !== undefined && !oids.includes(oid)) oids.push(oid);
};

export {
  buildAttack, parseCharlist, findCharByName, parseServerIp,
  oidOfSpawn, oidOfKill, oidOfHp, OP, opcodeOf,
};

// One received channel packet → state side effects (PONG on PING etc.).
function handleServerPacket(sess, state, p) {
  const op = opcodeOf(p);
  if (op === OP.PING) sess.sendPacket(OP.PONG, Buffer.alloc(0));
  else if (op === OP.SPAWN_MOB || op === OP.SPAWN_MOB_C) pushOid(state.oids, oidOfSpawn(p));
  else if (op === OP.KILL_MOB) state.dead.add(oidOfKill(p));
  return op;
}

// Receive until `deadline` (or close); per-packet timeouts are expected ticks.
async function pump(sess, state, deadline, slice = 1000) {
  while (!state.closed && Date.now() < deadline) {
    try { handleServerPacket(sess, state, await sess.recvPacket(slice)); }
    catch (e) { if (state.closed) break; }
  }
}

// Non-blocking drain of already-buffered packets.
function drain(sess, state) {
  for (;;) {
    if (sess.pending.length < 4) return;
    const hm = sess.pending[0] | (sess.pending[1] << 8) | (sess.pending[2] << 16) | (sess.pending[3] << 24);
    const len = ((hm >> 16) ^ (hm & 0xFFFF)) & 0xFFFF;
    if (sess.pending.length < 4 + len) return;
    try { handleServerPacket(sess, state, sess.recvPacket(50)); } catch (e) { return; }
  }
}

const pickTarget = (state) => state.oids.find((o) => !state.dead.has(o)) ?? null;

// Fire one attack and classify the server's settlement response:
// 0xFA(oid) → settled; 0xED(oid) → settled (mob died); 0x1F alone → rejected.
async function fireSkill(sess, state, skill, kind, target) {
  const { opcode, payload } = buildAttack({
    kind, skillid: skill.id, hitcount: skill.attackCount,
    charge: !!skill.charge, oid: target, damage: 2,
  });
  sess.sendPacket(opcode, payload);
  const deadline = Date.now() + 2000;
  const notes = [];
  let statChanged = false;
  while (!state.closed && Date.now() < deadline) {
    let p;
    try { p = await sess.recvPacket(250); }
    catch (e) { if (state.closed) break; continue; }
    const op = handleServerPacket(sess, state, p);
    if (op === OP.SHOW_MONSTER_HP && oidOfHp(p) === target) {
      return { ok: 'settled', notes: [...notes, `0xFA hp=${p.length > 6 ? p[6] : -1}%`].join(',') };
    }
    if (op === OP.KILL_MOB && oidOfKill(p) === target) {
      return { ok: 'settled', notes: [...notes, '0xED target died'].join(',') };
    }
    if (op === OP.STAT_CHANGED) { statChanged = true; notes.push('0x1F'); }
  }
  if (state.closed) return { ok: 'disconnected', notes: 'session closed' };
  return statChanged
    ? { ok: 'rejected', notes: notes.join(',') || 'enableActions only' }
    : { ok: 'timeout', notes: notes.join(',') || 'no response' };
}

// ---------------------------------------------------------------------------
// Flow
// ---------------------------------------------------------------------------
// Full login + channel-enter cycle. Returns the channel session plus a
// kick-aware state object; state.closed set means the cycle must be retried.
async function loginAndEnter(wantName, MACS, HWID) {
  const { sock: lsock, sess: login } = await openSession(LOGIN_TARGET, WS_URL);
  login.sendPacket(0x01, Buffer.concat([writeStr(USER), writeStr(PASS), Buffer.alloc(10)]));
  let r1 = await login.recvPacket();
  let status = r1.readInt32LE(2);
  if (status === 23) {
    login.sendPacket(0x07, Buffer.from([1]));
    r1 = await login.recvPacket();
    status = r1.readInt32LE(2);
  }
  if (status !== 0) throw new Error(`LOGIN FAILED status=${status}`);
  console.log('LOGIN OK');
  login.sendPacket(0x0B, Buffer.alloc(0));
  let r2 = await login.recvPacket();
  while (opcodeOf(r2) !== 0x1B) r2 = await login.recvPacket();
  login.sendPacket(0x05, Buffer.from([0x00, WORLD, 0x00]));
  const r3 = await login.recvPacket();

  const list = parseCharlist(r3);
  const entry = list.chars.find((c) => c.name === wantName);
  const charId = entry ? entry.id : findCharByName(r3, wantName);
  if (charId === null) {
    throw new Error(`char ${wantName} not found (${list.count} entries: ${list.chars.map((c) => c.name).join(',')})`);
  }
  console.log(`CHAR ${wantName} id=${charId}`);

  const cid = Buffer.alloc(4); cid.writeInt32LE(charId, 0);
  login.sendPacket(0x13, Buffer.concat([cid, writeStr(MACS), writeStr(HWID)]));
  let rip = await login.recvPacket();
  while (opcodeOf(rip) !== 0x0C) rip = await login.recvPacket();
  const chan = parseServerIp(rip);
  console.log(`SERVER_IP ${chan.host}:${chan.port} cid=${chan.charId}`);
  login.sendPacket(0x0C, Buffer.alloc(0));
  login._closed = () => {}; // our own close() must not trigger the fatal handler
  lsock instanceof net.Socket ? lsock.destroy() : lsock.close();

  const { sock: csock, sess: game } = await openSession(`${chan.host}:${chan.port}`, WS_URL);
  const state = { oids: [], dead: new Set(), closed: false };
  game._closed = () => { state.closed = true; }; // intercept kick: mark, don't exit
  game.sendPacket(0x14, cid);
  await sleep(700); // PLAYER_LOGGEDIN answer (SET_FIELD burst) must arrive
  drain(game, state);
  return { game, csock, state, charId };
}

async function main() {
  const report = { user: USER, suffix: SUFFIX, charId: null, oids: [], skipped: [], results: [] };
  const writeResults = () => fs.writeFileSync(RESULTS_JSON, JSON.stringify(report, null, 2) + '\n');

  if (!SUFFIX) { console.error('CHAR_SUFFIX env required (e.g. Hero)'); process.exit(4); }
  const manifest = JSON.parse(fs.readFileSync(ATTACK_JSON, 'utf8'));
  const job = Object.values(manifest).find((j) => j.suffix === SUFFIX);
  if (!job) {
    console.error(`no manifest entry with suffix ${SUFFIX}; have:`,
      Object.values(manifest).map((j) => j.suffix).join(','));
    writeResults(); process.exit(4);
  }
  console.log(`job ${job.kind} "${SUFFIX}" — ${job.skills.length} manifest skills`);

  // 1-4) Login → charlist → SELECT_CHAR → SERVER_IP → channel + PLAYER_LOGGEDIN.
  // Retried as a whole: the server's charid-in-transition token is per remote
  // IP (shared by every ws-proxy client) and consumed by the first
  // PLAYER_LOGGEDIN, so a dropped channel attempt needs a fresh login cycle.
  const MACS = '68-5D-43-F8-B8-6C, 7A-79-19-8B-31-3F';
  const HWID = '685D43F8B86C_B86C7A79';
  const wantName = USER + SUFFIX;
  let game = null, csock = null, state = null, charId = null;
  for (let attempt = 1; attempt <= 3; attempt++) {
    const enter = await loginAndEnter(wantName, MACS, HWID);
    ({ game, csock, state, charId } = enter);
    report.charId = charId;
    if (!state.closed) break;
    console.log(`channel closed before/just after PLAYER_LOGGEDIN (attempt ${attempt}/3), redoing login`);
    await sleep(1000);
  }
  if (state.closed) {
    report.results = job.skills.map((s) => ({ id: s.id, name: s.name, kind: job.kind, ok: 'disconnected', notes: 'channel dropped at login' }));
    writeResults(); process.exit(5);
  }
  console.log('PLAYER_LOGGEDIN accepted; waiting for spawns (~8s)');
  const spawnDeadline = Date.now() + 8000;
  while (Date.now() < spawnDeadline && !state.closed) {
    await pump(game, state, Date.now() + 1000, 1000);
    if (state.oids.length >= 1) break;
  }
  report.oids = state.oids;
  console.log(`mobs seen: ${state.oids.join(',') || 'NONE'}`);

  // 5) Fire every attack skill once.
  let failed = 0;
  for (const skill of job.skills) {
    if (skill.special) {
      report.skipped.push({ id: skill.id, name: skill.name, reason: `special:${skill.special}` });
      continue;
    }
    if (!skill.attackCount || skill.attackCount < 1) {
      report.skipped.push({ id: skill.id, name: skill.name, reason: 'attackCount<1 (buff?)' });
      continue;
    }
    if (state.closed) {
      report.results.push({ id: skill.id, name: skill.name, kind: job.kind, ok: 'disconnected', notes: 'closed before attack' });
      console.log(`[DISCONNECTED] ${skill.id} ${skill.name}`);
      failed++; continue;
    }
    drain(game, state);
    let res = null, usedTarget = null, tries = 0;
    while (tries < 2) {
      let target = pickTarget(state);
      if (target === null) {
        await pump(game, state, Date.now() + 10000, 1000); // wait for respawn
        target = pickTarget(state);
      }
      if (target === null) {
        res = { ok: 'timeout', notes: 'no living mob' };
        break;
      }
      res = await fireSkill(game, state, skill, job.kind, target);
      usedTarget = target;
      tries++;
      if (res.ok !== 'timeout') break;
      // A silent server usually means the target oid went stale (died without
      // a broadcast we saw): suspect it and retry once on the next oid.
      state.dead.add(target);
      drain(game, state);
    }
    report.results.push({ id: skill.id, name: skill.name, kind: job.kind, ok: res.ok, target: usedTarget, notes: res.notes });
    console.log(`[${res.ok.toUpperCase()}] ${skill.id} ${skill.name} (${job.kind}) → mob ${usedTarget} ${res.notes}`);
    if (res.ok !== 'settled') failed++;
    writeResults();
    await sleep(150);
  }

  // 6) Clean disconnect (PLAYER_DC 0x0C) + report.
  writeResults();
  console.log(`results: ${report.results.length} fired, ${report.skipped.length} skipped, ${failed} not settled → ${RESULTS_JSON}`);
  try { game.sendPacket(0x0C, Buffer.alloc(0)); } catch (e) { /* already closed */ }
  await sleep(250);
  csock instanceof net.Socket ? csock.destroy() : csock.close();
  process.exit(failed === 0 ? 0 : 5);
}

if (process.argv[1] && new URL(`file://${process.argv[1]}`).pathname === new URL(import.meta.url).pathname) {
  main().catch((e) => { console.error('SKILL BOT ERROR', e); process.exit(1); });
}
