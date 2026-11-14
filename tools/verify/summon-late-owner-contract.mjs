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
assert.ok(moveSummon.includes("summon::stash_event"),
  "MoveSummonHandler must stash the MOVE fragments while the owner is queued");
assert.equal(occurrences(moveSummon, "summon::move("), 1,
  "MoveSummonHandler must still apply movement directly when PRESENT");

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

const stashFn = sliceBetween(
  poolSource,
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
assert.ok(stashFn.includes("events.erase(events.begin())"),
  "stash_event must drop the oldest event when the cap is exceeded");

const replayFn = sliceBetween(
  poolSource,
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
  "void stash_event("
);
assert.ok(clearFn.includes("pending_events.clear()"),
  "clear must also drop the stashed events (map change drops the stash)");

const dropFn = sliceBetween(
  poolSource,
  "void drop_stashed(",
  "size_t size("
);
assert.ok(dropFn.includes("pending_events.erase(owner_cid)"),
  "drop_stashed must erase the owner's stash by cid");

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

console.log("PASS summon late-owner stash source contract (owner-late SPAWN/MOVE replay, same-kind override, cap 4, clear-on-leave)");
