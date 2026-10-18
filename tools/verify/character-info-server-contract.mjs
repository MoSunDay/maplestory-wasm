import fs from 'fs';
import path from 'path';

const root = process.argv[2] || 'link_repos/MapleStory-Server';

function read(relative) {
  return fs.readFileSync(path.join(root, relative), 'utf8');
}

function requirePattern(source, pattern, label) {
  if (!pattern.test(source)) throw new Error(`Missing linked-server contract: ${label}`);
}

const receiveOpcodes = read('src/net/opcodes/RecvOpcode.java');
const sendOpcodes = read('src/net/opcodes/SendOpcode.java');
const charHandler = read('src/net/server/channel/handlers/CharInfoRequestHandler.java');
const fameHandler = read('src/net/server/channel/handlers/GiveFameHandler.java');
const creator = read('src/tools/MaplePacketCreator.java');
const constants = read('src/constants/ServerConstants.java');

requirePattern(receiveOpcodes, /GIVE_FAME\(0x5F\)/, 'GIVE_FAME opcode 0x5F');
requirePattern(receiveOpcodes, /CHAR_INFO_REQUEST\(0x61\)/,
  'CHAR_INFO_REQUEST opcode 0x61');
requirePattern(sendOpcodes, /FAME_RESPONSE\(0x26\)/, 'FAME_RESPONSE opcode 0x26');
requirePattern(sendOpcodes, /CHAR_INFO\(0x3D\)/, 'CHAR_INFO opcode 0x3D');
requirePattern(constants, /USE_CUSTOM_CLIENT\s*=\s*true/,
  'custom-client character level format enabled');

requirePattern(charHandler, /slea\.skip\(4\);\s*int cid = slea\.readInt\(\)/s,
  'character info request timestamp then character id');
requirePattern(fameHandler, /getMapObject\(slea\.readInt\(\)\);\s*int mode = slea\.readByte\(\)/s,
  'fame target then increase/decrease mode');
requirePattern(fameHandler, /int famechange = 2 \* mode - 1/,
  'mode 0 decreases and mode 1 increases');

requirePattern(creator,
  /charInfo\(MapleCharacter chr\).*?writeInt\(chr\.getId\(\)\).*?USE_CUSTOM_CLIENT\).*?writeShort\(chr\.getLevel\(\)\).*?writeShort\(chr\.getJob\(\)\.getId\(\)\).*?writeShort\(chr\.getFame\(\)\)/s,
  'character id, short level, job, and fame order');
requirePattern(creator,
  /for \(int i = 0; i < 3; i\+\+\).*?getUniqueId\(\).*?getItemId\(\).*?getName\(\).*?getLevel\(\).*?getCloseness\(\).*?getFullness\(\)/s,
  'up to three summoned pet detail records');
requirePattern(creator,
  /giveFameResponse\(int mode, String charname, int newfame\).*?write\(0\).*?writeMapleAsciiString\(charname\).*?write\(mode\).*?writeShort\(newfame\).*?writeShort\(0\)/s,
  'fame success response field widths');

console.log('PASS linked-server character info, pet detail, and fame contract');
