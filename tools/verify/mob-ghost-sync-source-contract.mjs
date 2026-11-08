import assert from "node:assert/strict";
import fs from "node:fs";

const readSource = (path) => fs.readFileSync(path, "utf8");

const sliceBetween = (source, start, end) => {
  const startAt = source.indexOf(start);
  assert.ok(startAt >= 0, `source must contain: ${start}`);
  const endAt = source.indexOf(end, startAt);
  assert.ok(endAt > startAt, `source must contain ${end} after: ${start}`);
  return source.slice(startAt, endAt);
};

const occurrences = (haystack, needle) => haystack.split(needle).length - 1;

// ---------------------------------------------------------------------------
// A. CharLook::update — animation timers must clamp instead of underflowing.
// ---------------------------------------------------------------------------
const charLook = readSource("src/client/Character/Look/CharLook.cpp");
const update = sliceBetween(
  charLook,
  "bool CharLook::update(uint16_t timestep)",
  "void CharLook::set_body");

assert.ok(!update.includes("delta = delay - stelapsed;"),
  "bare `delay - stelapsed` underflows when stelapsed > delay; clamp it");
assert.ok(!update.includes("expdelta = expdelay - expelapsed;"),
  "bare `expdelay - expelapsed` underflows when expelapsed > expdelay; clamp it");

const stanceClamp = "uint16_t delta = stelapsed < delay ? delay - stelapsed : 0;";
assert.equal(occurrences(update, stanceClamp), 2,
  "both the stance loop and the action branch must clamp their frame delta");
const expressionClamp =
  "uint16_t expdelta = expelapsed < expdelay ? expdelay - expelapsed : 0;";
assert.equal(occurrences(update, expressionClamp), 1,
  "the expression loop must clamp its frame delta (stuck blink frame fix)");

// ---------------------------------------------------------------------------
// B. MapMobs — the queued-spawn lifecycle must survive clear() and remove().
// ---------------------------------------------------------------------------
const mapMobs = readSource("src/client/Gameplay/MapleMap/MapMobs.cpp");

const removeFn = sliceBetween(
  mapMobs,
  "void MapMobs::remove(int32_t oid, int8_t animation)",
  "void MapMobs::clear()");
assert.ok(removeFn.includes("std::queue<MobSpawn> retained;"),
  "remove() must filter pending spawns through a retained queue");
assert.ok(removeFn.includes("get_oid() != oid"),
  "remove() must drop queued spawns whose oid matches the kill");
assert.ok(removeFn.includes("spawns = std::move(retained);"),
  "remove() must swap the filtered queue back in");
assert.ok(removeFn.indexOf("std::queue<MobSpawn> retained;")
  < removeFn.indexOf("if (Optional<Mob> mob = mobs.get(oid))"),
  "queued-spawn filtering must run before the instantiated-mob kill");

const clearFn = sliceBetween(
  mapMobs,
  "void MapMobs::clear()",
  "void MapMobs::set_control");
assert.ok(clearFn.includes("mobs.clear();"),
  "clear() must still drop instantiated mobs");
assert.ok(clearFn.includes("spawns = std::queue<MobSpawn>{};"),
  "clear() must also drain the pending spawn queue (cross-map SPAWN_MOB)");

// ---------------------------------------------------------------------------
// C. Mob::respawn — a repeat SPAWN for an existing oid must fully revive it.
// ---------------------------------------------------------------------------
const mobPresentation =
  readSource("src/client/Gameplay/MapleMap/MobPresentation.cpp");
const respawn = sliceBetween(
  mobPresentation,
  "void Mob::respawn(Point<int16_t> position, uint8_t stancebyte, uint16_t fh)",
  "void Mob::send_movement");
for (const reset of ["dying = false;", "dead = false;",
                     "fading = false;", "hppercent = 0;"]) {
  assert.ok(respawn.includes(reset),
    `Mob::respawn must reset leftover death state: ${reset}`);
}

// ---------------------------------------------------------------------------
// D. Server MapleMap.killMonster — broadcast the kill before settlement.
// ---------------------------------------------------------------------------
const mapleMap =
  readSource("link_repos/MapleStory-Server/src/server/maps/MapleMap.java");
const killMonster = sliceBetween(
  mapleMap,
  "public void killMonster(final MapleMonster monster, final MapleCharacter chr, final boolean withDrops, int animation)",
  "public void killFriendlies");

const broadcast =
  "broadcastMessage(MaplePacketCreator.killMonster(monster.getObjectId(), animation), monster.getPosition());";
assert.equal(occurrences(killMonster, broadcast), 2,
  "killMonster must broadcast KILL_MONSTER exactly once per branch");

const guard = "if(removeKilledMonsterObject(monster)) {";
const assertBranch = (name, branch, settlement) => {
  const guardAt = branch.indexOf(guard);
  assert.ok(guardAt >= 0, `${name} branch must guard on removeKilledMonsterObject`);
  const broadcastAt = branch.indexOf(broadcast);
  assert.ok(broadcastAt > guardAt, `${name} branch must broadcast inside the guard`);
  assert.ok(/^\s*$/.test(branch.slice(guardAt + guard.length, broadcastAt)),
    `${name} branch must broadcast immediately after the guard opens`);
  for (const marker of settlement) {
    const markerAt = branch.indexOf(marker);
    assert.ok(markerAt > broadcastAt,
      `${name} branch: kill broadcast must precede ${marker}`);
  }
};

// chr == null branch: script kills still broadcast before dispatchMonsterKilled.
assertBranch("chr==null",
  sliceBetween(killMonster, "if (chr == null) {", "} else {"),
  ["dispatchMonsterKilled"]);

// chr != null branch: broadcast first, then exp/drop settlement.
const elseBranch = killMonster.slice(killMonster.indexOf("} else {"));
assertBranch("chr!=null", elseBranch,
  ["monster.killBy(", "dropFromMonster(", "dispatchMonsterKilled"]);

const settledAt = elseBranch.indexOf("monster.dispatchMonsterKilled(true);");
assert.ok(settledAt > 0, "chr!=null branch must call dispatchMonsterKilled(true)");
assert.ok(!elseBranch.slice(settledAt).includes(broadcast),
  "old ordering kept a trailing kill broadcast after settlement; it must be gone");

console.log("PASS mob ghost/desync sync source contract (CharLook timers, spawn queue, respawn reset, killMonster broadcast order)");
