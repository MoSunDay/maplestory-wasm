import { sleep } from './cdp.mjs';

async function requireState(name, predicate, timeoutSeconds = 30) {
  for (let second = 0; second < timeoutSeconds; second += 1) {
    if (await predicate()) {
      console.log(`PASS  ${name}`);
      return;
    }
    await sleep(1000);
  }
  throw new Error(`Timed out: ${name}`);
}

export async function verifyImeChatFlow(driver, chatMessage) {
  await driver.key('Enter', 'Enter', 13);
  await requireState('chat field owns browser IME', async () =>
    await driver.evaluate("MapleWasmIME.active && document.activeElement.id === 'ime-input'"));

  const composition = await driver.composeWithPreedit('zhongwenliaotian', chatMessage);
  if (composition.preeditCalls.length !== 0) {
    throw new Error(`Pinyin preedit reached the game: ${JSON.stringify(composition.preeditCalls)}`);
  }
  if (composition.leakedKeys.length !== 0) {
    throw new Error(`IME-owned keys leaked to the window: ${JSON.stringify(composition.leakedKeys)}`);
  }
  if (!composition.committedCalls.some(call => call[0] === chatMessage)) {
    throw new Error(`Committed Chinese text was not synced: ${JSON.stringify(composition.committedCalls)}`);
  }
  console.log('PASS  pinyin stays in IME until Chinese candidate commit');

  await requireState('Chinese chat text entered', async () =>
    await driver.evaluate(`document.getElementById('ime-input').value === ${JSON.stringify(chatMessage)}`));
  await sleep(150);
  await driver.key('Enter', 'Enter', 13);
  await sleep(1500);
  await driver.screenshot('09-chinese-chat');
}
