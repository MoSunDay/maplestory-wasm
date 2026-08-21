#!/usr/bin/env node

import fs from 'fs';
import path from 'path';
import process from 'process';
import vm from 'vm';

const root = path.resolve(process.argv[2] || 'link_repos/MapleStory-Server');

function read(relativePath) {
  const file = path.join(root, relativePath);
  if (!fs.existsSync(file)) throw new Error(`missing ${relativePath}`);
  return fs.readFileSync(file, 'utf8');
}

function assertMatch(source, pattern, description) {
  if (!pattern.test(source)) throw new Error(`missing contract: ${description}`);
}

function parseJavaScript(relativePath) {
  const source = read(relativePath);
  new vm.Script(source, { filename: relativePath });
  return source;
}

function countMob(mapXml, mobId) {
  return [...mapXml.matchAll(new RegExp(`<string name="id" value="${mobId}"`, 'g'))].length;
}

const event = parseJavaScript('scripts/event/KerningPQ.js');
assertMatch(event, /minPlayers\s*=\s*3\s*,\s*maxPlayers\s*=\s*4/, 'party size 3-4');
assertMatch(event, /minLevel\s*=\s*21\s*,\s*maxLevel\s*=\s*30/, 'level range 21-30');
assertMatch(event, /entryMap\s*=\s*103000800/, 'entry map');
assertMatch(event, /clearMap\s*=\s*103000805/, 'bonus map');
assertMatch(event, /exitMap\s*=\s*103000890/, 'exit map');
assertMatch(event, /eventTime\s*=\s*30/, '30 minute timer');
assertMatch(event, /setExclusiveItems\(itemSet\)/, 'exclusive coupon and pass cleanup');
assertMatch(event, /setEventRewards\(eim\)/, 'completion rewards');

const entryNpc = parseJavaScript('scripts/npc/9020000.js');
const stageNpc = parseJavaScript('scripts/npc/9020001.js');
assertMatch(entryNpc, /getEventManager\("KerningPQ"\)/, 'Lakelis starts KerningPQ');
assertMatch(stageNpc, /hasItem\(4001008,\s*numpasses\)/, 'stage pass validation');
assertMatch(stageNpc, /itemQuantity\(4001007\)\s*==\s*answer/, 'stage-one coupon validation');
assertMatch(stageNpc, /haveItem\(4001008,\s*10\)/, 'final ten-pass validation');
assertMatch(stageNpc, /showClearEffect\(true\)/, 'clear field effect');
assertMatch(stageNpc, /showWrongEffect\(\)/, 'wrong-answer field effect');

for (let stage = 0; stage <= 4; stage += 1) {
  const portal = parseJavaScript(`scripts/portal/kpq${stage}.js`);
  assertMatch(portal, new RegExp(`${stage + 1}stageclear`), `stage ${stage + 1} portal gate`);
}

const mapDirectory = 'wz/Map.wz/Map/Map1';
for (const mapId of [103000800, 103000801, 103000802, 103000803, 103000804, 103000805, 103000890]) {
  read(`${mapDirectory}/${mapId}.img.xml`);
}

const finalStage = read(`${mapDirectory}/103000804.img.xml`);
const expectedMobs = new Map([[9300000, 6], [9300002, 3], [9300003, 1]]);
for (const [mobId, expected] of expectedMobs) {
  const actual = countMob(finalStage, mobId);
  if (actual !== expected) throw new Error(`map 103000804 mob ${mobId}: expected ${expected}, got ${actual}`);
}

const database = read('sql/db_database.sql');
for (const [mobId, itemId] of [[9300000, 4001008], [9300001, 4001007], [9300002, 4001008], [9300003, 4001008]]) {
  assertMatch(database, new RegExp(`\\([^\\n]*${mobId},\\s*${itemId},`), `drop ${mobId} -> ${itemId}`);
}

console.log('PASS Kerning PQ scripts, maps, mobs, drops, timer, gates, and rewards contract');
