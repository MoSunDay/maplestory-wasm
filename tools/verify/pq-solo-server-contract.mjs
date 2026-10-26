#!/usr/bin/env node
// Offline contract test for the "solo PQ with NPC teammates" feature
// (ServerConstants.USE_PARTYQUEST_SOLO_MODE) on the linked
// MapleStory-Server checkout. Verifies, at the structural level:
//   A. Java contract (constants, EventManager/EventInstanceManager hooks,
//      SoloPq roster of non-interactive teammate NPCs with wz art),
//   B. event scripts (solo teammate setup, min-player relaxation, no stale
//      isEventTeamLackingNow calls, CWKPQ lazy attach),
//   C. recruitment NPC scripts (solo entry + 单人模式 prompt),
//   D. in-run mechanics (stage NPC trio, CWKPQ relic auto-grant, FM guide
//      labels with corrected level ranges and no forbidden macro letters).

import fs from 'fs';
import path from 'path';
import process from 'process';
import vm from 'vm';

const root = path.resolve(process.argv[2] || 'link_repos/MapleStory-Server');

function resolvePath(relativePath) {
  return path.join(root, relativePath);
}

function exists(relativePath) {
  return fs.existsSync(resolvePath(relativePath));
}

function read(relativePath) {
  const file = resolvePath(relativePath);
  if (!fs.existsSync(file)) throw new Error(`missing ${relativePath}`);
  return fs.readFileSync(file, 'utf8');
}

function assertMatch(source, pattern, description) {
  if (!pattern.test(source)) throw new Error(`missing contract: ${description}`);
}

function assertTrue(condition, description) {
  if (!condition) throw new Error(`violated contract: ${description}`);
}

function parseJavaScript(relativePath) {
  const source = read(relativePath);
  new vm.Script(source, { filename: relativePath });
  return source;
}

// Extracts the body of `function name(...)` via brace matching.
function extractFunction(source, name) {
  const start = source.search(new RegExp(`function\\s+${name}\\s*\\(`));
  if (start < 0) throw new Error(`missing function ${name}`);
  const openBrace = source.indexOf('{', start);
  if (openBrace < 0) throw new Error(`malformed function ${name}`);
  let depth = 0;
  for (let i = openBrace; i < source.length; i += 1) {
    if (source[i] === '{') depth += 1;
    if (source[i] === '}') {
      depth -= 1;
      if (depth === 0) return source.slice(openBrace + 1, i);
    }
  }
  throw new Error(`unbalanced braces in function ${name}`);
}

function extractArrayStrings(source, arrayName) {
  const declaration = source.match(new RegExp(`(?:var|const|let)\\s+${arrayName}\\s*=\\s*\\[([\\s\\S]*?)\\]`));
  if (!declaration) throw new Error(`missing array ${arrayName}`);
  return [...declaration[1].matchAll(/'([^']*)'/g)].map((match) => match[1]);
}

// ---------------------------------------------------------------- A. Java
function verifyJavaContract() {
  const constants = read('src/constants/ServerConstants.java');
  assertMatch(constants, /USE_PARTYQUEST_SOLO_MODE\s*=\s*true/, 'ServerConstants enables solo PQ mode');

  const eventManager = read('src/scripting/event/EventManager.java');
  assertMatch(eventManager, /isSoloPqModeEnabled/, 'EventManager.isSoloPqModeEnabled gate');

  const instanceManager = read('src/scripting/event/EventInstanceManager.java');
  for (const member of [
    'setupSoloTeammates',
    'isSoloPqFillActive',
    'isSoloTeamLackingNow',
    'stationSoloTeammates',
    'countSoloTeammatesInArea',
    'getSoloTeammateNames',
  ]) {
    assertMatch(instanceManager, new RegExp(member), `EventInstanceManager.${member}`);
  }

  const roster = read('src/server/partyquest/solo/SoloPqRoster.java');
  read('src/server/partyquest/solo/SoloPqTeammates.java');
  read('src/server/partyquest/solo/SoloPqAssist.java');

  const idList = roster.match(/NPC_IDS\s*=\s*\{([^}]*)\}/);
  if (!idList) throw new Error('SoloPqRoster.NPC_IDS not found');
  const npcIds = [...idList[1].matchAll(/\d+/g)].map((match) => Number(match[0]));
  assertTrue(npcIds.length === 5, `solo roster must hold 5 teammate NPCs, found ${npcIds.length}`);
  assertTrue(new Set(npcIds).size === 5, 'solo roster teammate NPC ids must be distinct');
  for (const npcId of npcIds) {
    assertTrue(!exists(`scripts/npc/${npcId}.js`), `teammate NPC ${npcId} must not have an interactive dialog script`);
    read(`wz/Npc.wz/${npcId}.img.xml`); // server art mirror must exist
  }
  console.log(`PASS solo PQ Java contract (roster ${npcIds.join(',')})`);
  return npcIds;
}

// -------------------------------------------------------- B. Event scripts
const PARTY_EVENT_SCRIPTS = [
  'KerningPQ', 'LudiPQ', 'LudiMazePQ', 'OrbisPQ', 'PiratePQ', 'HenesysPQ',
  'EllinPQ', 'MagatiaPQ_A', 'MagatiaPQ_Z', 'AmoriaPQ', 'TreasurePQ', 'BossRushPQ',
];

function verifyEventScripts() {
  for (const eventName of [...PARTY_EVENT_SCRIPTS, 'CWKPQ']) {
    const source = parseJavaScript(`scripts/event/${eventName}.js`);

    const setupBody = extractFunction(source, 'setup');
    assertMatch(setupBody, /setProperty\("minPlayers"/, `${eventName} setup declares minPlayers`);

    const afterSetupBody = extractFunction(source, 'afterSetup');
    assertMatch(afterSetupBody, /eim\.setupSoloTeammates\(\)/, `${eventName} afterSetup attaches solo teammates`);

    if (/eim\.isEventTeamLackingNow\(/.test(source)) {
      throw new Error(`${eventName} must use eim.isSoloTeamLackingNow, not eim.isEventTeamLackingNow`);
    }
  }

  for (const eventName of PARTY_EVENT_SCRIPTS) {
    const source = parseJavaScript(`scripts/event/${eventName}.js`);
    const eligibleBody = extractFunction(source, 'getEligibleParty');
    assertMatch(eligibleBody, /minOk\s*=\s*em\.isSoloPqModeEnabled\(\)\s*\?\s*1/, `${eventName} getEligibleParty relaxes the minimum when solo`);
  }

  const cwk = parseJavaScript('scripts/event/CWKPQ.js');
  const changedMapBody = extractFunction(cwk, 'changedMap');
  assertMatch(changedMapBody, /isSoloPqFillActive\(\)[\s\S]{0,200}eim\.setupSoloTeammates\(\)/, 'CWKPQ changedMap lazily attaches solo teammates on first entry');

  console.log(`PASS solo PQ event scripts (${[...PARTY_EVENT_SCRIPTS, 'CWKPQ'].length} events)`);
}

// --------------------------------------------------- C. Recruitment NPCs
const PARTY_RECRUITMENT_NPCS = [
  9020000, 2040034, 9103001, 2013000, 2094000, 1012112, 2133000,
  2112003, 2112004, 9201048, 9220018, 9000037,
];
const CWK_RECRUITMENT_NPC = 9201113;

function verifyRecruitmentNpcs() {
  for (const npcId of PARTY_RECRUITMENT_NPCS) {
    const source = parseJavaScript(`scripts/npc/${npcId}.js`);
    assertMatch(source, /em\.isSoloPqModeEnabled\(\)/, `NPC ${npcId} checks the solo PQ mode gate`);
    assertMatch(source, /em\.startInstance\((?:[^,()]+,\s*)?cm\.getPlayer\(\)\)/, `NPC ${npcId} starts the instance for the solo player`);
    assertMatch(source, /sendYesNo\([^;]*单人模式/, `NPC ${npcId} offers 单人模式 via sendYesNo`);
  }

  // CWKPQ recruits through an expedition: solo relaxes the min-size check instead.
  const cwkNpc = parseJavaScript(`scripts/npc/${CWK_RECRUITMENT_NPC}.js`);
  assertMatch(cwkNpc, /em\.isSoloPqModeEnabled\(\)/, 'NPC 9201113 checks the solo PQ mode gate');
  assertMatch(cwkNpc, /em\.startInstance\(expedition\)/, 'NPC 9201113 starts the expedition instance');
  const minSizeIndex = cwkNpc.search(/getMinSize\(\)/);
  assertTrue(minSizeIndex >= 0, 'NPC 9201113 reads the expedition getMinSize');
  const around = cwkNpc.slice(Math.max(0, minSizeIndex - 400), minSizeIndex + 600);
  assertMatch(around, /isSoloPqModeEnabled\(\)\s*&&[^;\n]*size\s*<=\s*1/, 'NPC 9201113 relaxes the expedition min-size check when solo');

  console.log(`PASS solo PQ recruitment NPCs (${PARTY_RECRUITMENT_NPCS.length} party recruiters + CWKPQ expedition)`);
}

// ---------------------------------------------------- D. In-run mechanics
const STAGE_TRIO_NPCS = [9020001, 2013001, 9201044];

function verifyInRunMechanics() {
  for (const npcId of STAGE_TRIO_NPCS) {
    const source = parseJavaScript(`scripts/npc/${npcId}.js`);
    assertMatch(source, /stationSoloTeammates/, `NPC ${npcId} stations solo teammates`);
    assertMatch(source, /countSoloTeammatesInArea/, `NPC ${npcId} counts solo teammates in the stage area`);
    assertMatch(source, /isSoloPqFillActive/, `NPC ${npcId} checks the solo fill gate`);
  }

  const relics = parseJavaScript('scripts/npc/9201110.js');
  assertMatch(relics, /isSoloPqFillActive/, 'NPC 9201110 checks the solo fill gate');
  assertMatch(relics, /isSoloPqFillActive\(\)\s*&&\s*eim\.getIntProperty\("glpq5"\)\s*<\s*4[\s\S]{0,200}setIntProperty\("glpq5",\s*4\)/, 'NPC 9201110 auto-grants the remaining glpq5 relics when solo');

  const guide = parseJavaScript('scripts/npc/9010000.js');
  const labels = extractArrayStrings(guide, 'PQ_LABELS');
  assertTrue(labels.length === 13, `FM guide must list 13 PQ labels, found ${labels.length}`);
  const expectedRanges = [
    [/迷宫/, '51-70级'],
    [/海盗/, '55-100级'],
    [/玛加提亚A路/, '71-85级'],
    [/玛加提亚Z路/, '71-85级'],
    [/宝物/, '140级以上'],
    [/月妙/, '10级以上'],
    [/艾琳/, '44级以上'],
  ];
  for (const [keyword, levelRange] of expectedRanges) {
    const matched = labels.find((label) => keyword.test(label) && label.includes(levelRange));
    if (!matched) throw new Error(`FM guide label for ${keyword} must show ${levelRange}, got: ${labels.join(' | ')}`);
  }
  for (const label of labels) {
    assertTrue(!/^[bdefgknr]/i.test(label), `FM guide label must not start with a forbidden dialog-macro letter: ${label}`);
  }
  const menu = extractFunction(guide, 'buildMenu');
  assertMatch(menu, /单人模式/, 'FM guide menu text mentions 单人模式');

  console.log('PASS solo PQ in-run mechanics (stage trio, CWKPQ relics, FM guide labels)');
}

verifyJavaContract();
verifyEventScripts();
verifyRecruitmentNpcs();
verifyInRunMechanics();
console.log('PASS solo PQ (NPC teammates) server contract');
