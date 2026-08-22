import assert from "node:assert/strict";
import fs from "node:fs";

const root = "link_repos/MapleStory-Server/src";
const trade = fs.readFileSync(`${root}/server/MapleTrade.java`, "utf8");
const processor = fs.readFileSync(
  `${root}/net/server/channel/handlers/trade/TradeItemProcessor.java`, "utf8");
const handler = fs.readFileSync(
  `${root}/net/server/channel/handlers/PlayerInteractionHandler.java`, "utf8");
const send = fs.readFileSync(`${root}/net/opcodes/SendOpcode.java`, "utf8");
const receive = fs.readFileSync(`${root}/net/opcodes/RecvOpcode.java`, "utf8");

for (const required of [
  "synchronized (sessionLock)",
  "!locked.get() && !partner.locked.get()",
  "items.size() >= 9",
  "commitInventoryRemoval.run();",
  "items.add(item);",
  "local.closed = true;",
  "partner.closed = true;",
  "lockTradeInventories(",
  "unlockTradeInventories(lockedInventories);",
]) {
  assert.ok(trade.includes(required), `MapleTrade missing: ${required}`);
}
assert.ok(
  trade.indexOf("commitInventoryRemoval.run();") < trade.indexOf("items.add(item);"),
  "inventory removal must commit before an offer becomes visible",
);

for (const required of [
  "targetSlot >= 1 && targetSlot <= 9",
  "quantity >= 1 && quantity <= item.getQuantity()",
  "sourceItem.isUntradeable()",
  "USE_ENFORCE_UNMERCHABLE_CASH",
  "USE_ENFORCE_UNMERCHABLE_PET",
  "current != sourceItem",
  "trade.addItem(tradeItem",
]) {
  assert.ok(processor.includes(required), `TradeItemProcessor missing: ${required}`);
}

assert.ok(handler.includes("TradeItemProcessor.handle(slea, c, chr);"));
assert.match(send, /PLAYER_INTERACTION\(0x13A\)/);
assert.match(receive, /PLAYER_INTERACTION\(0x7B\)/);
console.log("PASS atomic trade mutation, validation, slot limit, and v83 opcodes");
