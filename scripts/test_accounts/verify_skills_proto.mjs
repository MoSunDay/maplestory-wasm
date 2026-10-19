// Protocol bot: logs in one test character, enters the game via the ws-proxy
// and fires every attack skill from attack_skills.json at a spawned mob,
// verifying that the server settles the damage (SHOW_MONSTER_HP 0xFA).
// Attack packets mirror src/client/Net/Packets/AttackAndSkillPackets.h
// byte-for-byte; mob oid offsets mirror MapObjectHandlers.cpp; wire/crypto
// stack lives in ./maple_wire.mjs.
// Skills flagged serverBuff in the manifest (SkillFactory.isBuff switch,
// parsed by attack_skills.py) are skipped: "skipped: server-buff".
// Two exotic attacks are supported: Meso Explosion (4211006, kind "meso")
// uses its own meso-id layout (buildMesoAttack); Snipe (3221007) fires a
// normal ranged packet — the server's SNIPE branch overrides the damage.
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
  SPECIAL_MOVE: 0x5B, GIVE_BUFF: 0x20,
};
const ATTACK_OPCODE = { melee: OP.CLOSE_ATTACK, ranged: OP.RANGED_ATTACK, magic: OP.MAGIC_ATTACK };
const STANCE = { melee: 2, ranged: 2, magic: 27 };
const SPEED_BYTE = 44;

// GameConstants.isFinisherSkill (server): finishers silently drop without
// combo orbs — CloseRangeDamageHandler "numFinisherOrbs == 0 → return".
const isFinisher = (id) => (id > 1111002 && id < 1111007) || id === 11111002 || id === 11111003;
const COMBO_SKILL = 1111002; // Crusader.COMBO — buff that grants orbs per hit
const COMBO_LEVEL = parseInt(process.env.COMBO_LEVEL || '30', 10);
// RangedAttackHandler drops the attack unless a projectile is in USE OR the
// Soul Arrow buff is active (bow/crossbow chosen by skill-id prefix 31/32).
const SOUL_ARROW = { 31: 3101004, 32: 3201004 };
const SOUL_LEVEL = parseInt(process.env.SOUL_LEVEL || '20', 10);

// ---------------------------- pure helpers ---------------------------------
const sleep = (ms) => new Promise((res) => setTimeout(res, ms));
const opcodeOf = (p) => p[0] | (p[1] << 8);
const i32 = (p, o) => (p.length >= o + 4 ? p.readInt32LE(o) : null);
const u16 = (p, o) => (p.length >= o + 2 ? p.readUInt16LE(o) : null);

// Mirrors AttackPacket (AttackAndSkillPackets.h lines 30-80):
//   skip(1); byte (mobcount<<4 | hitcount); int skill; [int charge if >0];
//   skip(8); byte display; byte toleft; byte stance; skip(1); byte speed;
//   ranged ? { skip(1); byte toleft; skip(7); } : skip(4);   (magic = skip(4), no extra bytes)
//   per target: int oid; skip(14); per hit int encode(damage, false); skip(4)
//   (skill 5221004 omits the trailing skip(4)). Hold skills (HURRICANE,
//   PIERCING_ARROW, RAPID_FIRE, WindArcher hurricane) append skip(4) after the
//   ranged tail.
const HOLD_RANGED = new Set([3121004, 3221001, 5221004, 13121004]);

// Skills whose manifest entry carries serverBuff:true (attack_skills.py
// parses SkillFactory's isBuff switch at manifest build time): an "attack"
// packet for them never reaches damage settlement, and charges
// additionally poison later attacks (verified live: Blast/Rush settle
// again on a fresh session).
const isServerBuff = (skill) => skill.serverBuff === true;

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
    if (HOLD_RANGED.has(skillid)) parts.push(Buffer.alloc(4));  // hurricane/pierce/rapidfire
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

// Meso Explosion (4211006, kind "meso") has its own parseDamage branch in
// AbstractDealDamageHandler (MESO_EXPLOSION, ~lines 589-624). It diverges
// from the melee layout right after the stance byte:
//   skip(1); byte (numAttacked<<4 | numDamage); int skill; NO charge int
//     (4211006 is not in the server's charge-skill list);
//   skip(8); byte display; byte direction; byte stance; skip(6);
//   numAttacked monster entries: int oid; skip(12) (2 shorter than melee!);
//     byte bullets; bullets × int damage; skip(4);
//   one trailing meso entry: int dummyOid (read at the top of the loop for
//     every entry, value discarded); byte pileCount; per pile: int mesoOid;
//     skip(1).
// allDamage keys become {monsterOid: [damages], mesoOid: null, ...}; the
// pile entries only explode/remove dropped meso (applyAttack removes
// ITEM-type map objects), the monster entries carry the actual damage.
// Dropping real meso piles is not needed: with pileCount=0 the packet is a
// complete one-monster attack and settles through the normal damage loop.
function buildMesoAttack({ skillid, oid, damage }) {
  const parts = [];
  parts.push(Buffer.from([0x00]));                              // skip(1)
  parts.push(Buffer.from([(1 << 4) | 0x01]));                   // numAttacked=1, numDamage=1
  const sk = Buffer.alloc(4); sk.writeInt32LE(skillid, 0); parts.push(sk);
  parts.push(Buffer.alloc(8));                                  // skip(8)
  parts.push(Buffer.from([0x00, 0x00, 2]));                     // display, toleft, stance
  parts.push(Buffer.alloc(6));                                  // skip(6) (speed tail, unread)
  const o = Buffer.alloc(4); o.writeInt32LE(oid, 0); parts.push(o);
  parts.push(Buffer.alloc(12));                                 // skip(12), NOT melee's 14
  parts.push(Buffer.from([0x01]));                              // bullets = 1 damage line
  const d = Buffer.alloc(4); d.writeInt32LE(damage, 0); parts.push(d);
  parts.push(Buffer.alloc(4));                                  // skip(4)
  parts.push(Buffer.alloc(4));                                  // trailing entry dummyOid
  parts.push(Buffer.from([0x00]));                              // meso pile count = 0
  return { opcode: OP.CLOSE_ATTACK, payload: Buffer.concat(parts) };
}

// MapObjectHandlers.cpp: SPAWN_MOB(0xEC) = int oid @2; SPAWN_MOB_C(0xEE) =
// byte mode @2 then int oid @3; KILL_MOB(0xED) = int oid @2; SHOW_MOB_HP(0xFA)
// = int oid @2 + byte hppercent @6.
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

// Mirrors UseSkillPacket (AttackAndSkillPackets.h, opcode USE_SKILL 0x5B):
//   write_time(); write_int(skillid); write_byte(level). SpecialMoveHandler
// requires the level byte to equal the character's actual skill level.
function buildUseSkill(skillid, level) {
  const t = Buffer.alloc(4); t.writeInt32LE(Math.floor(Date.now() / 1000) & 0x7fffffff, 0);
  const id = Buffer.alloc(4); id.writeInt32LE(skillid, 0);
  return { opcode: OP.SPECIAL_MOVE, payload: Buffer.concat([t, id, Buffer.from([level & 0xFF])]) };
}

export {
  buildAttack, buildMesoAttack, buildUseSkill, isFinisher, parseCharlist, findCharByName, parseServerIp,
  oidOfSpawn, oidOfKill, oidOfHp, OP, opcodeOf,
};

// One received channel packet → state side effects (PONG on PING etc.).
// DEBUG_OPS=1 logs every received opcode — garbage opcodes after a certain
// point would indicate a stream/IV desync, missing 0xFA a server-side drop.
function handleServerPacket(sess, state, p) {
  const op = opcodeOf(p);
  if (process.env.DEBUG_OPS) console.error(`  << op 0x${op.toString(16).padStart(4, '0')} len ${p.length}`);
  if (op === OP.PING) sess.sendPacket(OP.PONG, Buffer.alloc(0));
  else if (op === OP.SPAWN_MOB || op === OP.SPAWN_MOB_C) pushOid(state.oids, oidOfSpawn(p));
  else if (op === OP.KILL_MOB) state.dead.add(oidOfKill(p));
  else if (op === OP.GIVE_BUFF) state.buffSeen = true;
  return op;
}

// Receive until `deadline` (or close); per-packet timeouts are expected ticks.
async function pump(sess, state, deadline, slice = 1000) {
  while (!state.closed && Date.now() < deadline) {
    try { handleServerPacket(sess, state, await sess.recvPacket(slice)); }
    catch (e) { if (state.closed) break; }
  }
}

// Drain already-buffered packets. recvPacket timeouts consume nothing (atomic
// framing in maple_wire.mjs); must be awaited so it cannot steal a frame.
async function drain(sess, state) {
  for (;;) {
    let p;
    try { p = await sess.recvPacket(20); } catch (e) { return; }
    handleServerPacket(sess, state, p);
  }
}

const pickTarget = (state) => state.oids.find((o) => !state.dead.has(o)) ?? null;

// Finisher skills are dropped silently unless combo orbs > 0 (server gate in
// CloseRangeDamageHandler). Orbs: cast COMBO via SPECIAL_MOVE 0x5B (COMBO=1),
// then land one basic attack (skill 0) → COMBO=2 → orbs=1.
async function primeCombo(sess, state, kind) {
  const notes = [];
  const { opcode, payload } = buildUseSkill(COMBO_SKILL, COMBO_LEVEL);
  sess.sendPacket(opcode, payload);
  await pump(sess, state, Date.now() + 800, 250); // GIVE_BUFF 0x20
  notes.push('combo-buff-sent');
  const target = pickTarget(state);
  const res = target === null ? { ok: 'skipped (no mob)' }
    : await fireSkill(sess, state, { id: 0, attackCount: 1 }, kind, target);
  notes.push(`orb-hit ${res.ok}`);
  return notes.join(',');
}

// Fire one attack and classify the server's settlement response:
// 0xFA(oid) → settled; 0xED(oid) → settled (mob died); 0x1F alone → rejected.
// `layoutAccepted` skills (meso) additionally map a no-kick + enableActions
// (0x1F only) answer to "accepted-layout": the server consumed the exotic
// packet shape without disconnecting, but nothing settled.
async function fireSkill(sess, state, skill, kind, target, layoutAccepted = false) {
  const { opcode, payload } = kind === 'meso'
    ? buildMesoAttack({ skillid: skill.id, oid: target, damage: 2 })
    : buildAttack({
        kind, skillid: skill.id, hitcount: skill.attackCount,
        charge: !!skill.charge, oid: target, damage: 2,
      });
  sess.sendPacket(opcode, payload);
  const deadline = Date.now() + 2000;
  const notes = [kind === 'meso' ? 'meso-layout(0 piles)' : null].filter(Boolean);
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
  if (statChanged && layoutAccepted) {
    return { ok: 'accepted-layout', notes: `${notes.join(',') || 'enableActions only'} (no kick, exotic layout consumed)` };
  }
  return statChanged
    ? { ok: 'rejected', notes: notes.join(',') || 'enableActions only' }
    : { ok: 'timeout', notes: notes.join(',') || 'no response' };
}

// ------------------------------- flow --------------------------------------
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
  await drain(game, state);
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
  // Retried as a whole (freezes / status=7 / transition-token race all clear
  // after a pause). MACS/HWID are fixed harness ids.
  const MACS = '68-5D-43-F8-B8-6C, 7A-79-19-8B-31-3F', HWID = '685D43F8B86C_B86C7A79';
  const wantName = USER + SUFFIX;
  let game = null, csock = null, state = null, charId = null;
  for (let attempt = 1; attempt <= 3; attempt++) {
    try {
      const enter = await loginAndEnter(wantName, MACS, HWID);
      ({ game, csock, state, charId } = enter);
      report.charId = charId;
      if (!state.closed) break;
      console.log(`channel closed before/just after PLAYER_LOGGEDIN (attempt ${attempt}/3), redoing login`);
    } catch (e) {
      // login freezes, status=7 "already online" and the per-IP transition-token race clear by themselves — redo the cycle
      console.log(`login/enter attempt ${attempt}/3 failed: ${e.message}`);
    }
    try { csock?.destroy?.(); } catch (e) { /* not open yet */ }
    await sleep(2500);
  }
  if (!state || state.closed) {
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

  // Ranged archer jobs: prime Soul Arrow (RangedAttackHandler requires a
  // projectile in the USE inventory otherwise).
  if (job.kind === 'ranged') {
    const p2 = (s) => Math.floor(s.id / 100000);
    const soul = job.skills.some((s) => p2(s) === 32) ? SOUL_ARROW[32]
      : job.skills.some((s) => p2(s) === 31) ? SOUL_ARROW[31] : null;
    if (soul) {
      const { opcode, payload } = buildUseSkill(soul, SOUL_LEVEL);
      game.sendPacket(opcode, payload);
      await pump(game, state, Date.now() + 1000, 250);
      console.log(`soul arrow ${soul} lvl ${SOUL_LEVEL}: ${state.buffSeen ? 'GIVE_BUFF seen' : 'NO buff confirm'}`);
      report.soulArrow = { skill: soul, level: SOUL_LEVEL, confirmed: !!state.buffSeen };
    } else {
      console.log('no soul-arrow skill for this job; ranged attacks need real projectiles');
    }
  }

  // 5) Fire every attack skill once.
  let failed = 0;
  for (const skill of job.skills) {
    // serverBuff:true ⇒ SkillFactory.isBuff() forces the effect to load as
    // a buff on the server; an attack packet for it can never settle.
    if (isServerBuff(skill)) {
      report.skipped.push({ id: skill.id, name: skill.name, reason: 'skipped: server-buff (SkillFactory.isBuff)' });
      console.log(`[SKIPPED] ${skill.id} ${skill.name} → skipped: server-buff`);
      continue;
    }
    if (!skill.attackCount || skill.attackCount < 1) {
      report.skipped.push({ id: skill.id, name: skill.name, reason: 'attackCount<1 (buff?)' });
      console.log(`[SKIPPED] ${skill.id} ${skill.name} → attackCount<1`);
      continue;
    }
    // Special attack skills still fire: 4211006 Meso Explosion goes through
    // the meso-id packet builder (kind "meso"), 3221007 Snipe uses a plain
    // ranged layout — the server's SNIPE branch overrides the damage to
    // 195000+rand(5000) and encodes it negative (crit display), so any
    // small damage we send settles via 0xFA/0xED as usual.
    const kind = skill.kind || job.kind;
    const layoutAccepted = kind === 'meso';
    if (state.closed) {
      report.results.push({ id: skill.id, name: skill.name, kind, ok: 'disconnected', notes: 'closed before attack' });
      console.log(`[DISCONNECTED] ${skill.id} ${skill.name}`);
      failed++; continue;
    }
    await drain(game, state);
    let res = null, usedTarget = null, tries = 0, primed = null;
    while (tries < 2) {
      if (isFinisher(skill.id)) primed = await primeCombo(game, state, job.kind);
      let target = pickTarget(state);
      if (target === null) {
        await pump(game, state, Date.now() + 10000, 1000); // wait for respawn
        target = pickTarget(state);
      }
      if (target === null) {
        res = { ok: 'timeout', notes: 'no living mob' };
        break;
      }
      res = await fireSkill(game, state, skill, kind, target, layoutAccepted);
      usedTarget = target;
      tries++;
      if (res.ok === 'settled' || res.ok === 'accepted-layout') break;
      // A silent server usually means the target oid went stale (despawned /
      // died unseen — 0x1F-only "rejected" is the same symptom: damage
      // skipped, MP still deducted): mark dead and retry once on the next oid.
      state.dead.add(target);
      await drain(game, state);
    }
    const notes = primed ? `[${primed}] ${res.notes}` : res.notes;
    report.results.push({ id: skill.id, name: skill.name, kind, ok: res.ok, target: usedTarget, notes });
    console.log(`[${res.ok.toUpperCase()}] ${skill.id} ${skill.name} (${kind}) → mob ${usedTarget} ${notes}`);
    if (res.ok !== 'settled' && res.ok !== 'accepted-layout') failed++;
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
