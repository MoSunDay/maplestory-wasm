import assert from "node:assert/strict";
import fs from "node:fs";

const monster = fs.readFileSync(
  "link_repos/MapleStory-Server/src/server/life/MapleMonster.java", "utf8");
const character = fs.readFileSync(
  "link_repos/MapleStory-Server/src/client/MapleCharacter.java", "utf8");

const fullShare = monster.indexOf("personalExpReward.put(mc, (float) exp);");
const partyLoop = monster.indexOf(
  "distributeExperienceToParty(party.getKey()");
assert.ok(fullShare > 0, "killer party must receive the monster's full base exp");
assert.ok(partyLoop > fullShare,
  "damage-weighted party bonuses must be skipped for the killer party");
assert.ok(monster.indexOf("partyExp.remove(killer.getParty().getId());") < partyLoop,
  "full party sharing must replace, not add to, the killer party bonus");
for (const required of [
  "MapleCharacter killer = map.getCharacterById(killerId);",
  "killer.getPartyMembersOnSameMap()",
  "mc.getLevel() >= minThresholdLevel",
  "Math.abs(killerPartyLevel - mc.getLevel())",
  "underleveled.add(mc);",
]) {
  assert.ok(monster.includes(required), `MapleMonster missing: ${required}`);
}

const pickupStart = character.indexOf(
  "pickupItem(MapleMapObject ob, int petIndex)");
assert.ok(pickupStart > 0, "pickupItem implementation must be present");
const pickup = character.slice(pickupStart, pickupStart + 16000);
assert.ok(pickup.includes("getPartyMembersOnSameMap()"),
  "meso pickups must locate same-map party members");
assert.ok(pickup.includes("int mesosamm = mapitem.getMeso() / mpcs.size();"),
  "meso must be split evenly between same-map party members");
assert.ok(pickup.includes("partymem.gainMeso(mesosamm, true, true, false);"),
  "every same-map party member must receive the split meso");
assert.ok(pickup.includes("this.gainMeso(mapitem.getMeso(), true, true, false);"),
  "solo pickups must retain the full meso amount");

console.log("PASS full party EXP sharing and even meso split server contract");
