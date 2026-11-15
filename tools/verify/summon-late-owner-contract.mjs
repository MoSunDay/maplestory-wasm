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

const sliceFrom = (source, start) => {
  const startAt = source.indexOf(start);
  assert.ok(startAt >= 0, `source must contain: ${start}`);
  return source.slice(startAt);
};

const occurrences = (haystack, needle) => haystack.split(needle).length - 1;

// ---------------------------------------------------------------------------
// A. SummonHandlers — the owner gate is three-state and PENDING stashes.
// ---------------------------------------------------------------------------
const handlers = readSource("src/client/Net/Handlers/SummonHandlers.cpp");

const ownerStateEnum = sliceBetween(
  handlers,
  "enum class OwnerState",
  "OwnerState owner_state"
);
assert.ok(ownerStateEnum.includes("PRESENT"),
  "OwnerState must have a PRESENT state (owner visible, apply now)");
assert.ok(ownerStateEnum.includes("PENDING"),
  "OwnerState must have a PENDING state (owner queued in MapChars)");
assert.ok(ownerStateEnum.includes("GONE"),
  "OwnerState must have a GONE state (drop, owner never spawned)");

const ownerStateFn = sliceBetween(
  handlers,
  "OwnerState owner_state",
  "std::optional<Point<int16_t>> mob_body_position"
);
assert.ok(ownerStateFn.includes("get_char(cid)"),
  "owner_state must treat an instantiated character as PRESENT");
assert.ok(ownerStateFn.includes("has_pending_spawn(cid)"),
  "owner_state must consult MapChars::has_pending_spawn for PENDING");

const spawnSummon = sliceBetween(
  handlers,
  "void SpawnSummonHandler::handle",
  "void RemoveSummonHandler::handle"
);
assert.ok(spawnSummon.includes("summon::stash_event"),
  "SpawnSummonHandler must stash the SPAWN payload while the owner is queued");

const moveSummon = sliceBetween(
  handlers,
  "void MoveSummonHandler::handle",
  "void SummonAttackHandler::handle"
);
assert.ok(moveSummon.includes("apply_move("),
  "MoveSummonHandler must route movement through the shared apply_move helper");

const spawnDragon = sliceFrom(handlers, "void SpawnDragonHandler::handle");
assert.ok(spawnDragon.includes("summon::stash_event"),
  "SpawnDragonHandler must stash the dragon payload while the owner is queued");

// ---------------------------------------------------------------------------
// B. SummonPool.h — the stash data lives next to the entities.
// ---------------------------------------------------------------------------
const poolHeader = readSource("src/client/Gameplay/Summon/SummonPool.h");

assert.ok(poolHeader.includes("MAX_STASHED_SUMMON_EVENTS = 4"),
  "SummonPool.h must define MAX_STASHED_SUMMON_EVENTS = 4");

const summonEvent = sliceBetween(poolHeader, "struct SummonEvent", "/// One remote summon");
assert.ok(summonEvent.includes("SPAWN"),
  "SummonEvent must carry a SPAWN kind");
assert.ok(summonEvent.includes("MOVE"),
  "SummonEvent must carry a MOVE kind");

const poolStruct = sliceBetween(poolHeader, "struct SummonPool", "/// Spawn a summon from");
assert.ok(poolStruct.includes("pending_events"),
  "SummonPool must own the pending_events stash keyed by owner cid");

// ---------------------------------------------------------------------------
// C. SummonPool.cpp — override, cap, replay and drop semantics.
// ---------------------------------------------------------------------------
const poolSource = readSource("src/client/Gameplay/Summon/SummonPool.cpp");
const stashSource = readSource("src/client/Gameplay/Summon/SummonStash.cpp");

const stashFn = sliceBetween(
  stashSource,
  "void stash_event(",
  "void replay_stashed("
);
assert.ok(stashFn.includes("std::find_if"),
  "stash_event must replace an older event in place via std::find_if");
assert.ok(stashFn.includes("entry.kind == event.kind"),
  "stash_event replacement must match on kind");
assert.ok(stashFn.includes("entry.oid == event.oid"),
  "stash_event replacement must match on oid");
assert.ok(stashFn.includes("MAX_STASHED_SUMMON_EVENTS"),
  "stash_event must enforce the per-owner cap");
assert.ok(stashFn.includes("while (events.size() > MAX_STASHED_SUMMON_EVENTS)"),
  "stash_event must enforce the cap with a loop");
assert.ok(stashFn.includes("entry.kind == SummonEvent::MOVE"),
  "cap eviction must prefer dropping a MOVE over a SPAWN");
assert.ok(stashFn.includes("victim = events.begin()"),
  "an all-SPAWN stash falls back to dropping its oldest entry");
assert.ok(stashFn.includes("events.erase(victim)"),
  "cap eviction must erase the chosen victim");

const replayFn = sliceBetween(
  stashSource,
  "void replay_stashed(",
  "void drop_stashed("
);
assert.ok(replayFn.includes("spawn(pool"),
  "replay_stashed must instantiate stashed SPAWN events via spawn(pool, ...)");
assert.ok(replayFn.includes("move(pool"),
  "replay_stashed must apply stashed MOVE events via move(pool, ...)");
assert.ok(replayFn.includes("pending_events.erase(iter)"),
  "replay_stashed must drop the owner's stash after replaying it");

const clearFn = sliceBetween(
  poolSource,
  "void clear(SummonPool& pool)",
  "size_t size("
);
assert.ok(clearFn.includes("pending_events.clear()"),
  "clear must also drop the stashed events (map change drops the stash)");

const dropFn = sliceBetween(
  stashSource,
  "void drop_stashed(",
  "void drop_stashed_oid("
);
assert.ok(dropFn.includes("pending_events.erase(owner_cid)"),
  "drop_stashed must erase the owner's stash by cid");

const dropOidFn = sliceFrom(stashSource, "void drop_stashed_oid(");
assert.ok(dropOidFn.includes("std::remove_if"),
  "drop_stashed_oid must filter the owner's events with std::remove_if");
assert.ok(dropOidFn.includes("entry.oid == oid"),
  "drop_stashed_oid must cancel only the events of the removed oid");
assert.ok(dropOidFn.includes("events.empty()"),
  "drop_stashed_oid must detect an owner left without events");
assert.ok(dropOidFn.includes("pending_events.erase(owner)"),
  "drop_stashed_oid must erase an owner whose stash became empty");

// ---------------------------------------------------------------------------
// D. Stage + RemoveCharHandler — replay after instantiation, drop on leave.
// ---------------------------------------------------------------------------
const stageSource = readSource("src/client/Gameplay/Stage.cpp");
const updateFn = sliceBetween(
  stageSource,
  "void Stage::update()",
  "void Stage::show_character_effect"
);
const charsUpdateAt = updateFn.indexOf("chars.update(physics);");
const replayAt = updateFn.indexOf("summon::replay_stashed(");
const summonUpdateAt = updateFn.indexOf("summon::update(");
assert.ok(charsUpdateAt >= 0, "Stage::update must still run chars.update");
assert.ok(replayAt > charsUpdateAt,
  "replay_stashed must run after chars.update so instantiate_spawns has run");
assert.ok(summonUpdateAt > replayAt,
  "replay_stashed must run before summon::update consumes the pool");

const mapObjectHandlers = readSource("src/client/Net/Handlers/MapObjectHandlers.cpp");
const removeChar = sliceBetween(
  mapObjectHandlers,
  "void RemoveCharHandler::handle",
  "void SpawnPetHandler::handle"
);
assert.ok(removeChar.includes("get_chars().remove(cid)"),
  "RemoveCharHandler must still remove the character");
assert.ok(removeChar.includes("summon::drop_stashed(Stage::get().get_summons(), cid)"),
  "RemoveCharHandler must drop the departing cid's stashed summon events");

// ---------------------------------------------------------------------------
// E. MapChars.h — has_pending_spawn is a public query.
// ---------------------------------------------------------------------------
const mapCharsHeader = readSource("src/client/Gameplay/MapleMap/MapChars.h");
assert.equal(occurrences(mapCharsHeader, "has_pending_spawn"), 1,
  "MapChars.h must declare has_pending_spawn exactly once");
const publicSection = sliceBetween(mapCharsHeader, "public:", "private:");
assert.ok(publicSection.includes("has_pending_spawn"),
  "has_pending_spawn must live in the public section (needed by summon handlers)");

// ---------------------------------------------------------------------------
// F. RemoveSummonHandler — REMOVE must apply no matter what the owner
//    gate says: REMOVE_CHAR arrives first on a leave, so the stash is
//    cancelled unconditionally, then the idempotent live remove runs.
// ---------------------------------------------------------------------------
const removeSummon = sliceBetween(
  handlers,
  "void RemoveSummonHandler::handle",
  "void MoveSummonHandler::handle"
);
assert.ok(!removeSummon.includes("switch (owner_state"),
  "RemoveSummonHandler must not gate on owner visibility (GONE used to skip the remove and leave ghosts)");
assert.ok(!/return;/.test(removeSummon),
  "RemoveSummonHandler must have no early return before the remove");
assert.ok(removeSummon.includes("summon::drop_stashed_oid("),
  "RemoveSummonHandler must cancel the owner's stashed events");
assert.ok(removeSummon.includes("payload.owner_cid"),
  "drop_stashed_oid must be called with the packet's owner cid");
assert.ok(removeSummon.includes("payload.oid"),
  "drop_stashed_oid must be called with the packet's summon oid");
assert.equal(occurrences(removeSummon, "summon::remove("), 1,
  "RemoveSummonHandler must keep exactly one idempotent live remove");
const dropAt = removeSummon.indexOf("summon::drop_stashed_oid(");
const removeLiveAt = removeSummon.indexOf("summon::remove(");
assert.ok(dropAt >= 0 && removeLiveAt > dropAt,
  "the stash must be cancelled before the idempotent live remove, for PENDING and GONE owners alike");

// ---------------------------------------------------------------------------
// F2. Move/attack/damage decoupled from owner visibility — the server
//     keeps summons alive (and broadcasting) after the owner leaves the
//     viewer's range, so a live entity always consumes the event.
// ---------------------------------------------------------------------------
const moveSummonFn = sliceBetween(
  handlers,
  "void MoveSummonHandler::handle",
  "void SummonAttackHandler::handle"
);
assert.ok(moveSummonFn.includes("apply_move("),
  "MoveSummonHandler must route through the shared apply_move helper");
const applyMoveFn = sliceBetween(
  handlers,
  "void apply_move(",
  "    void SpawnSummonHandler::handle"
);
assert.ok(applyMoveFn.includes("summon::find(pool, oid)"),
  "apply_move must apply movement whenever the entity already exists");
const findAt = applyMoveFn.indexOf("summon::find(pool, oid)");
const stashAt = applyMoveFn.indexOf("summon::stash_event(");
assert.ok(stashAt > findAt,
  "apply_move may only stash when the entity does not exist yet");

const attackHandler = sliceBetween(
  handlers,
  "void SummonAttackHandler::handle",
  "void DamageSummonHandler::handle"
);
assert.ok(attackHandler.includes("summon::find("),
  "SummonAttackHandler must gate on the summon entity, not the owner");
assert.ok(!attackHandler.includes("owner_state"),
  "SummonAttackHandler must not consult owner visibility");
const damageHandler = sliceBetween(
  handlers,
  "void DamageSummonHandler::handle",
  "void SpawnDragonHandler::handle"
);
assert.ok(damageHandler.includes("summon::find("),
  "DamageSummonHandler must gate on the summon entity, not the owner");
assert.ok(!damageHandler.includes("owner_state"),
  "DamageSummonHandler must not consult owner visibility");

// ---------------------------------------------------------------------------
// G. Dragons — MOVE_DRAGON(182)/REMOVE_DRAGON(183) registered and routed
//    through the same entity-first semantics; removal cancels the stash.
// ---------------------------------------------------------------------------
const switchSource = readSource("src/client/Net/PacketSwitch.cpp");
assert.ok(switchSource.includes("MOVE_DRAGON             = 182"),
  "PacketSwitch must define MOVE_DRAGON = 182");
assert.ok(switchSource.includes("REMOVE_DRAGON           = 183"),
  "PacketSwitch must define REMOVE_DRAGON = 183");
assert.ok(switchSource.includes("emplace<MOVE_DRAGON, MoveDragonHandler>()"),
  "PacketSwitch must register MoveDragonHandler (no more Opcode: 182 warns)");
assert.ok(switchSource.includes("emplace<REMOVE_DRAGON, RemoveDragonHandler>()"),
  "PacketSwitch must register RemoveDragonHandler (no more Opcode: 183 warns)");

const moveDragon = sliceBetween(
  handlers,
  "void MoveDragonHandler::handle",
  "void RemoveDragonHandler::handle"
);
assert.ok(moveDragon.includes("parse_dragon_move"),
  "MoveDragonHandler must parse the dragon move payload");
assert.ok(moveDragon.includes("apply_move("),
  "MoveDragonHandler must reuse the entity-first apply_move helper");
assert.ok(moveDragon.includes("payload.cid, payload.cid"),
  "the dragon's object id is the owner cid itself");

const removeDragon = sliceFrom(handlers, "void RemoveDragonHandler::handle");
assert.ok(removeDragon.includes("parse_dragon_remove"),
  "RemoveDragonHandler must parse the dragon remove payload");
assert.ok(removeDragon.includes("summon::drop_stashed_oid("),
  "RemoveDragonHandler must cancel a stashed dragon SPAWN/MOVE first");
assert.ok(removeDragon.includes("summon::remove("),
  "RemoveDragonHandler must run the idempotent live remove");
const dragonDropAt = removeDragon.indexOf("summon::drop_stashed_oid(");
const dragonRemoveAt = removeDragon.indexOf("summon::remove(");
assert.ok(dragonDropAt >= 0 && dragonRemoveAt > dragonDropAt,
  "the dragon stash cancel must precede the live remove");

console.log("PASS summon late-owner stash source contract (owner-late SPAWN/MOVE replay, same-kind override, SPAWN-preferring cap 4, clear-on-leave, unconditional remove-cancels-stash, entity-gated move/attack/damage, dragon 182/183 wired)");
