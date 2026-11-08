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

const monster = readSource(
  "link_repos/MapleStory-Server/src/server/life/MapleMonster.java");
const character = readSource(
  "link_repos/MapleStory-Server/src/client/MapleCharacter.java");

// Killer-party block: unconditional full-share (1:1) for all same-map members.
const fullShare = "personalExpReward.put(mc, (float) exp);";
const killerBlock = sliceBetween(
  monster,
  "MapleCharacter killer = map.getCharacterById(killerId);",
  "int mostDamageCid");

assert.ok(killerBlock.includes("killer.getPartyMembersOnSameMap()"),
  "every same-map party member must be eligible for the killer-party share");
assert.ok(killerBlock.includes(fullShare),
  "every same-map party member must receive the monster's full base exp");
for (const gate of ["minThresholdLevel", "killerPartyLevel", "underleveled.add"]) {
  assert.ok(!killerBlock.includes(gate),
    `killer-party full share must be unconditional, no ${gate} gate`);
}
assert.ok(
  killerBlock.indexOf("partyExp.remove(killer.getParty().getId());")
    < killerBlock.indexOf(fullShare),
  "full party sharing must replace, not add to, the killer party bonus");

const fullShareAt = monster.indexOf(fullShare);
const partyLoopAt = monster.indexOf("distributeExperienceToParty(party.getKey()");
assert.ok(fullShareAt > 0, "killer party must receive the monster's full base exp");
assert.ok(partyLoopAt > fullShareAt,
  "damage-weighted party bonuses must be skipped for the killer party");

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

console.log("PASS unconditional 1:1 party EXP sharing and even meso split server contract");
