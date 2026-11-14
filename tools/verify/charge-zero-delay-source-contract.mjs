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
// A. Combat::push_attack — the remote attack must be pushed with delay 0.
// ---------------------------------------------------------------------------
const combat = readSource("src/client/Gameplay/Combat/Combat.cpp");
const pushAttack = sliceBetween(
  combat,
  "void Combat::push_attack",
  "void Combat::apply_attack");

assert.ok(pushAttack.includes("attackresults.push(0, attack);"),
  "push_attack must push the broadcast AttackResult with a literal 0 delay");

assert.ok(!pushAttack.includes("std::clamp(attack.charge"),
  "the broadcast charge value must never become the queue delay; " +
  "the attacking client already wound up and the server settled the hits");

assert.ok(pushAttack.includes("2026-11-06/remote-action-sync"),
  "push_attack must reference the changelog that removed the fixed " +
  "400ms delay: features/changelog/2026-11-06/remote-action-sync.md");

assert.equal(occurrences(pushAttack, "attackresults.push("), 1,
  "push_attack must contain exactly one queue push (no delay branches)");

// ---------------------------------------------------------------------------
// B. AttackHandlers — charge field parsing stays untouched (protocol layer).
// ---------------------------------------------------------------------------
const handlers = readSource("src/client/Net/Handlers/AttackHandlers.cpp");
assert.ok(handlers.includes("attack.charge = recv.read_int();"),
  "AttackHandlers must keep parsing the trailing charge int into attack.charge");
assert.ok(handlers.includes("bool has_broadcast_charge(int32_t skillid)"),
  "the has_broadcast_charge guard must stay; only breath/Big Bang skills " +
  "carry the extra charge int");

// ---------------------------------------------------------------------------
// C. AttackResult — the charge field itself must still exist.
// ---------------------------------------------------------------------------
const attackHeader = readSource("src/client/Gameplay/Combat/Attack.h");
const attackResult = sliceBetween(
  attackHeader,
  "struct AttackResult",
  "struct AttackUser");
assert.ok(attackResult.includes("charge"),
  "AttackResult must keep its charge field; parsing fills it, push_attack " +
  "merely no longer replays it as a delay");

console.log("PASS charge-zero-delay source contract (remote attack push delay is 0, charge parsing retained)");
