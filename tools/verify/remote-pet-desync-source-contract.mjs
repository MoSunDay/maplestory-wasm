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
// A. MapChars — the queued-spawn lifecycle must survive clear() and remove().
// ---------------------------------------------------------------------------
const mapChars = readSource("src/client/Gameplay/MapleMap/MapChars.cpp");

const charsClear = sliceBetween(mapChars, "void MapChars::clear()", "bool MapChars::has_pending_spawn");
assert.ok(charsClear.includes("chars.clear();"), "clear() must drop instantiated chars");
assert.ok(charsClear.includes("spawns = std::queue<CharSpawn>{};"),
  "clear() must also drop queued spawns (cross-map ghost char fix)");
assert.ok(charsClear.includes("pending_pets.clear();"),
  "clear() must drop stashed pet updates too");

const charsRemove = sliceBetween(mapChars, "void MapChars::remove(int32_t cid)", "void MapChars::clear()");
assert.ok(charsRemove.includes("std::queue<CharSpawn> retained;"),
  "remove() must filter the pending spawns queue (same-batch SPAWN+REMOVE resurrect fix)");
assert.ok(charsRemove.includes("get_cid() != cid"),
  "the retained filter must drop entries owned by the removed cid");
assert.ok(charsRemove.includes("pending_pets.erase(cid);"),
  "remove() must drop stashed pet updates for the removed cid");

const instantiate = sliceBetween(mapChars, "void MapChars::instantiate_spawns(const Physics& physics)", "void MapChars::spawn(");
assert.equal(occurrences(instantiate, "apply_pets("), 2,
  "both the respawn branch and the fresh-add branch must apply SPAWN_PLAYER pets");
assert.equal(occurrences(instantiate, "replay_pending_pets("), 2,
  "both branches must replay stashed SPAWN_PET updates");
assert.equal(occurrences(instantiate, "replay_pending_events("), 2,
  "both branches must replay stashed character events in arrival order");

// A1b. MapChars — SPAWN_PLAYER carries chair and chalkboard state, and an
// airborne entry spawn is grounded through the map footholds.
assert.equal(occurrences(instantiate, "set_chair(spawn.get_chair_id())"), 2,
  "both branches must land the SPAWN_PLAYER chair id");
assert.equal(occurrences(instantiate, "set_chalkboard(spawn.get_chalkboard())"), 2,
  "both branches must land the SPAWN_PLAYER chalkboard text");
assert.ok(instantiate.includes("spawn.resolve_position(physics)") &&
  instantiate.includes("spawn.instantiate(physics)"),
  "both branches must resolve the spawn position against the footholds (airborne entry landing)");
const setChalkboard = sliceBetween(mapChars, "void MapChars::set_chalkboard", "void MapChars::update_party_member_hp");
assert.ok(setChalkboard.includes("otherchar->set_chalkboard(text);"),
  "CHALKBOARD must apply directly to instantiated characters");
assert.ok(setChalkboard.includes("stash_event(cid, event);"),
  "CHALKBOARD must be stashed when the cid is not instantiated yet");
const replayEvents = sliceBetween(mapChars, "void MapChars::replay_pending_events", "void MapChars::send_movement");
assert.ok(replayEvents.includes("case CharEvent::CHALKBOARD:"),
  "stashed CHALKBOARD events must be replayed on instantiation");

// A2. MapChars — late character events are stashed, capped and replayed.
assert.ok(charsClear.includes("pending_events.clear();"),
  "clear() must drop stashed character events too");
assert.ok(charsRemove.includes("pending_events.erase(cid);"),
  "remove() must drop stashed character events for the removed cid");

const stashEvent = sliceBetween(mapChars, "void MapChars::stash_event", "void MapChars::apply_pets");
assert.ok(stashEvent.includes("if (!has_pending_spawn(cid))"),
  "character events are only stashed for cids with a pending spawn (unknown cids stay ignored)");
assert.ok(stashEvent.includes("same_event_target("),
  "a newer event of the same kind must replace the stashed one (idempotent state)");
assert.ok(stashEvent.includes("MAX_STASHED_EVENTS_PER_CID"),
  "the per-cid cap must bound the stash for cids that never instantiate");

const sendMovement = sliceBetween(mapChars, "void MapChars::send_movement", "void MapChars::move_pet");
assert.ok(sendMovement.includes("stash_event(cid, event);"),
  "MOVE_PLAYER must be stashed when the cid is not instantiated yet");
const updateLook = sliceBetween(mapChars, "void MapChars::update_look", "void MapChars::set_expression");
assert.ok(updateLook.includes("stash_event(cid, event);"),
  "UPDATE_CHARLOOK must be stashed when the cid is not instantiated yet");

// B. SPAWN_PLAYER pet block is stored, not discarded.
const payload = readSource("src/client/Net/Protocol/SpawnPlayerPayload.cpp");
const petLoop = sliceBetween(payload, "// Pets are written as:", "recv.read_int(); // mount level");
assert.ok(petLoop.includes("pet.itemid = recv.read_int();") &&
  petLoop.includes("pet.name = recv.read_string();") &&
  petLoop.includes("pet.position = recv.read_point();") &&
  petLoop.includes("pet.stance = recv.read_byte();") &&
  petLoop.includes("pet.foothold = recv.read_int();"),
  "every parsed pet field must be stored into the payload");

const payloadHeader = readSource("src/client/Net/Protocol/SpawnPlayerPayload.h");
assert.ok(payloadHeader.includes("PetSpawnEntry pets[3];") && payloadHeader.includes("pet_count"),
  "Payload must carry pet slots for the spawn pipeline");
assert.ok(payload.includes("payload.chair_id = recv.read_int();") &&
  payloadHeader.includes("int32_t chair_id"),
  "the SPAWN_PLAYER chair id must be stored, not discarded");
assert.ok(payload.includes("payload.chalkboard = recv.read_string();") &&
  payloadHeader.includes("std::string chalkboard"),
  "the SPAWN_PLAYER chalkboard text must be stored, not discarded");

// B2. CosmeticHandlers — CHALKBOARD routes through MapChars like the other
// late character events.
const cosmetic = readSource("src/client/Net/Handlers/CosmeticHandlers.cpp");
assert.ok(cosmetic.includes("int32_t cid = recv.read_int();") &&
  cosmetic.indexOf("recv.read_int();") < cosmetic.indexOf("recv.read_byte();"),
  "CHALKBOARD reads cid then the close/open byte");
assert.ok(cosmetic.includes('Stage::get().get_chars().set_chalkboard(cid, "");') &&
  cosmetic.includes("Stage::get().get_chars().set_chalkboard(cid, text);"),
  "both close and open variants must route through MapChars");
const packetSwitch = readSource("src/client/Net/PacketSwitch.cpp");
assert.ok(packetSwitch.includes("CHALKBOARD = 164") &&
  packetSwitch.includes("emplace<CHALKBOARD, ChalkboardHandler>();"),
  "CHALKBOARD(164) must be registered in the packet switch");

// C. Handlers — spawn carries pets; SPAWN_PET survives pending owners.
const handlers = readSource("src/client/Net/Handlers/MapObjectHandlers.cpp");
const spawnChar = sliceBetween(handlers, "void SpawnCharHandler::handle", "void RemoveCharHandler::handle");
assert.ok(spawnChar.includes("charspawn.set_pets(payload.pets, payload.pet_count);") &&
  spawnChar.includes("std::move(charspawn)"),
  "SpawnCharHandler must adopt the parsed pet slots before queueing");

const spawnPet = sliceBetween(handlers, "void SpawnPetHandler::handle", "void PetMovedHandler::handle");
assert.ok(spawnPet.indexOf("update.pet.itemid = recv.read_int();") <
    spawnPet.indexOf("Stage::get().get_character(cid)"),
  "SpawnPetHandler must parse the body before resolving the owner");
assert.ok(spawnPet.includes("Stage::get().get_chars().stash_pet(cid, update);"),
  "SpawnPetHandler must stash updates for not-yet-instantiated owners instead of dropping them");

const mapCharsHeader = readSource("src/client/Gameplay/MapleMap/MapChars.h");
assert.ok(mapCharsHeader.includes("std::map<int32_t, std::vector<PetUpdate>> pending_pets;"),
  "MapChars must keep per-cid stashed pet updates");
assert.ok(mapCharsHeader.includes("std::map<int32_t, std::vector<CharEvent>> pending_events;"),
  "MapChars must keep per-cid stashed character events");

// C2. Handlers — the five late-event kinds route through MapChars instead of
// being dropped when get_char misses, and packet layouts stay correct.
const petMoved = sliceBetween(handlers, "void PetMovedHandler::handle", "void CharMovedHandler::handle");
assert.ok(petMoved.includes("Stage::get().get_chars().move_pet(cid, petindex, movements);"),
  "MOVE_PET must route through MapChars so it survives pending owners");

const showChair = sliceBetween(handlers, "void ShowChairHandler::handle", "void FacialExpressionHandler::handle");
assert.ok(showChair.includes("Stage::get().get_chars().set_chair(cid, item_id);"),
  "SHOW_CHAIR must route through MapChars so it survives pending owners");

const facial = sliceBetween(handlers, "void FacialExpressionHandler::handle", "void ShowForeignEffectHandler::handle");
assert.ok(facial.includes("Stage::get().get_chars().set_expression(cid, emote);"),
  "FACIAL_EXPRESSION must route through MapChars so it survives pending owners");

const damagePlayer = sliceBetween(handlers, "void DamagePlayerHandler::handle", "void SpawnMistHandler::handle");
assert.ok(damagePlayer.indexOf("skill == -3") >= 0 &&
  damagePlayer.indexOf("skill == -3") < damagePlayer.indexOf("recv.skip(4);") &&
  damagePlayer.indexOf("recv.skip(4);") < damagePlayer.indexOf("const int32_t damage = recv.read_int();"),
  "fall damage (skill -3) must skip the 4-byte pad before reading damage");
assert.ok(damagePlayer.indexOf("const int32_t damage = recv.read_int();") <
    damagePlayer.indexOf("if (damage <= 0)"),
  "the damage<=0 early return must stay after the pad skip");

const foreignEffect = sliceBetween(handlers, "void ShowForeignEffectHandler::handle", "void GiveForeignBuffHandler::handle");
assert.ok(foreignEffect.includes("switch (effect)"),
  "SHOW_FOREIGN_EFFECT must dispatch on the exact effect value first");
assert.ok(!foreignEffect.includes("else if (recv.length() >= 4) // skill"),
  "the length() probe must not route bare packets into the skill branch");
assert.ok(foreignEffect.includes("case 0:") && foreignEffect.includes("case 8:") &&
  foreignEffect.includes("case 10:") && foreignEffect.includes("case 13:") &&
  foreignEffect.includes("case 16:"),
  "levelup, job change, recovery, card and maker effects need exact cases");
assert.ok(foreignEffect.indexOf("show_foreign_skill_effect(recv, cid, effect);") >
    foreignEffect.indexOf("case 5:"),
  "the skill variant must only run for skill-class effect ids");
const skillEffect = sliceBetween(handlers, "void show_foreign_skill_effect", "void SpawnCharHandler::handle");
assert.ok(skillEffect.includes("if (recv.length() < 5)") &&
  skillEffect.indexOf("if (recv.length() < 5)") < skillEffect.indexOf("recv.read_int();"),
  "the skill variant must check length() before reading skillid and direction");

// D. Server correction must cancel a stale attack gate.
const otherChar = readSource("src/client/Character/OtherChar.cpp");
const respawn = sliceBetween(otherChar, "void OtherChar::respawn", "void OtherChar::update_skill");
assert.ok(respawn.indexOf("attacking = false;") <
    respawn.indexOf("if (!attacking && !is_in_item_chair())"),
  "respawn() must reset the attacking gate before the stance refresh");

console.log("PASS remote pet & desync source contract (chars queue lifecycle, SPAWN_PLAYER pets+chair+chalkboard, SPAWN_PET stash, late char-event stash incl. CHALKBOARD(164), fall-damage pad, foreign-effect dispatch, respawn attack reset, airborne spawn landing)");
