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
//   E. wz recruit-map life placement (NPC cy must not sit above its
//      foothold surface, else the client drops the NPC to a lower
//      platform and keyboard talk silently fails).

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

    // fill = min(MAX_TEAMMATES, minPlayers-1)：minPlayers=1 会静默得到 0
    // 补位（Ludi/Orbis/Pirate/Ellin 曾因此 spawned=0 且无 [SoloPQ] 日志）。
    // 天然可单刷的副本（LudiMaze/BossRush）保持 1，e2e 以 soloOptional 豁免。
    const minMatch = source.match(/var\s+minPlayers\s*=\s*(\d+)/);
    assertTrue(minMatch !== null, `${eventName} declares a minPlayers constant`);
    const minPlayers = minMatch ? Number(minMatch[1]) : 0;
    const soloCapable = eventName === 'LudiMazePQ' || eventName === 'BossRushPQ';
    assertTrue(
      soloCapable ? minPlayers === 1 : minPlayers >= 2,
      `${eventName} minPlayers=${minPlayers} ${soloCapable ? 'must stay 1 (solo-capable)' : 'must be >=2 so solo fill spawns teammates'}`
    );

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

// ----------------------------------------- E. wz recruit-map life placement
// The client places NPCs from the spawn packet via get_y_below, and
// Footholdtree::get_fhid_below only accepts footholds whose surface at x
// satisfies ycomp >= cy. A life cy ABOVE the intended foothold surface
// (OrbisPQ 2013000 once had cy=-41 vs fh14 surface -41.8 at x=198) snaps
// the NPC to a lower platform, so |dy| > 80 in MapNpcs::talk_to_nearest
// and Space never sends NPC_TALK. Guard every recruit-map NPC life entry.
const RECRUIT_MAP_LIFE = [
  ['KerningPQ', 103000000, 9020000],
  ['LudiPQ', 221024500, 2040034],
  ['LudiMazePQ', 220000000, 9103001],
  ['OrbisPQ', 200080101, 2013000],
  ['PiratePQ', 251010404, 2094000],
  ['HenesysPQ', 100000200, 1012112],
  ['EllinPQ', 300030100, 2133000],
  ['MagatiaPQ_A', 261000021, 2112003],
  ['MagatiaPQ_Z', 261000011, 2112004],
  ['AmoriaPQ', 670010100, 9201048],
  ['CWKPQ', 610030020, 9201113],
  ['TreasurePQ', 674030100, 9220018],
  ['BossRushPQ', 970030000, 9000037],
];

function mapXmlPath(mapId) {
  return `wz/Map.wz/Map/Map${Math.floor(mapId / 100000000)}/${String(mapId).padStart(9, '0')}.img.xml`;
}

function xmlInt(block, name) {
  const m = block.match(new RegExp(`name="${name}" value="(-?\\d+)"`));
  return m ? Number(m[1]) : null;
}

function verifyRecruitMapLife() {
  for (const [eventName, mapId, npcId] of RECRUIT_MAP_LIFE) {
    const xml = read(mapXmlPath(mapId));

    // Foothold table: innermost imgdirs that open with an x1 int.
    const footholds = new Map();
    for (const m of xml.matchAll(/<imgdir name="(\d+)">\s*<int name="x1"/g)) {
      const block = xml.slice(m.index, xml.indexOf('</imgdir>', m.index));
      const coords = ['x1', 'y1', 'x2', 'y2'].map(f => xmlInt(block, f));
      if (coords.every(v => v !== null)) {
        footholds.set(Number(m[1]), { x1: coords[0], y1: coords[1], x2: coords[2], y2: coords[3] });
      }
    }

    // NPC life entries: imgdir bodies carrying a type="n" string.
    const lifeEntries = [];
    for (const m of xml.matchAll(/<imgdir name="\d+">/g)) {
      const block = xml.slice(m.index, xml.indexOf('</imgdir>', m.index));
      if (/<string name="type" value="n"\/>/.test(block)) lifeEntries.push(block);
    }
    assertTrue(lifeEntries.length > 0, `${eventName} map ${mapId} declares at least one npc life entry`);

    const entry = lifeEntries.find(
      b => (b.match(/name="id" value="(\d+)"/) || [])[1] === String(npcId)
    );
    assertTrue(entry !== undefined, `${eventName} map ${mapId} life contains recruiter npc ${npcId}`);

    const x = xmlInt(entry, 'x');
    const cy = xmlInt(entry, 'cy');
    const fh = xmlInt(entry, 'fh');
    assertTrue(x !== null && cy !== null && fh !== null, `${eventName} npc ${npcId} life entry has x/cy/fh`);

    const foot = footholds.get(fh);
    assertTrue(foot !== undefined, `${eventName} npc ${npcId} life fh=${fh} exists in map footholds`);
    assertTrue(foot.x1 !== foot.x2, `${eventName} npc ${npcId} life fh=${fh} is a floor, not a wall`);

    const lo = Math.min(foot.x1, foot.x2);
    const hi = Math.max(foot.x1, foot.x2);
    assertTrue(x >= lo - 2 && x <= hi + 2,
      `${eventName} npc ${npcId} life x=${x} lies within fh=${fh} span [${lo},${hi}]`);

    const t = (Math.min(Math.max(x, lo), hi) - foot.x1) / (foot.x2 - foot.x1);
    const surface = foot.y1 + t * (foot.y2 - foot.y1);
    assertTrue(cy <= surface + 0.25,
      `${eventName} npc ${npcId} life cy=${cy} sits above fh=${fh} surface ${surface.toFixed(1)} at x=${x}: `
      + 'client get_fhid_below (ycomp >= fy) drops the NPC onto a lower platform and keyboard talk silently fails');
  }
  console.log(`PASS solo PQ recruit-map life placement (${RECRUIT_MAP_LIFE.length} maps, cy <= fh surface)`);
}

verifyJavaContract();
verifyEventScripts();
verifyRecruitmentNpcs();
verifyInRunMechanics();
verifyRecruitMapLife();
console.log('PASS solo PQ (NPC teammates) server contract');
