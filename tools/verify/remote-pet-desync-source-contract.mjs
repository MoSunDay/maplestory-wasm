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

const instantiate = sliceBetween(mapChars, "void MapChars::instantiate_spawns()", "void MapChars::spawn(");
assert.equal(occurrences(instantiate, "apply_pets("), 2,
  "both the respawn branch and the fresh-add branch must apply SPAWN_PLAYER pets");
assert.equal(occurrences(instantiate, "replay_pending_pets("), 2,
  "both branches must replay stashed SPAWN_PET updates");

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

// D. Server correction must cancel a stale attack gate.
const otherChar = readSource("src/client/Character/OtherChar.cpp");
const respawn = sliceBetween(otherChar, "void OtherChar::respawn", "void OtherChar::update_skill");
assert.ok(respawn.indexOf("attacking = false;") <
    respawn.indexOf("if (!attacking && !is_in_item_chair())"),
  "respawn() must reset the attacking gate before the stance refresh");

console.log("PASS remote pet & desync source contract (chars queue lifecycle, SPAWN_PLAYER pets, SPAWN_PET stash, respawn attack reset)");
