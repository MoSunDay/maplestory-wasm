// PQ e2e harness —— 走位 / NPC 对话开闭 / 聊天发送层（拆自 pq-harness.mjs）。
import { sleep } from './cdp.mjs';
import { playerX, probe, waitFor } from './pq-harness-game.mjs';

export async function walkTowards(driver, targetX, maxSeconds = 120) {
  const start = Date.now();
  let lastX = null;
  let stuck = 0;
  while (Date.now() - start < maxSeconds * 1000) {
    const x = await playerX(driver);
    const dx = targetX - x;
    if (Math.abs(dx) <= 10) return x;
    if (lastX !== null && Math.abs(x - lastX) < 2) {
      stuck += 1;
      if (stuck >= 3) {
        // 「按住方向 + 起跳」跨越台阶/矮墙（原地跳无法产生水平位移）。
        const right = targetX > x;
        const hold = driver.keyHold(
          right ? 'ArrowRight' : 'ArrowLeft',
          right ? 'ArrowRight' : 'ArrowLeft',
          right ? 39 : 37, 900);
        await sleep(250);
        // 按住跳跃键一段时间（瞬时 keyDown/keyUp 在负载下会被客户端丢帧）
        await driver.keyHold('Alt', 'AltLeft', 18, 350);
        await hold;
        await sleep(500);
        const after = await playerX(driver);
        if (Math.abs(after - x) < 2 && stuck >= 8)
          throw new Error(`walk stuck at x=${x} target=${targetX}`);
      }
    } else stuck = 0;
    lastX = x;
    // 长距 hold 1100ms：负载下 CDP 探测往返 ~300-800ms，短 hold 的有效
    // 速度只有 ~70px/s（Pirate 2767px 平走要 40-90s）；加长 hold 把有效
    // 速度提到 ~110px/s 以上，短距档保持原值避免过冲。
    const ms = Math.abs(dx) > 400 ? 1100 : Math.abs(dx) > 100 ? 300 : 140;
    if (targetX > x) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
    else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
    await sleep(70);
  }
  const finalX = await playerX(driver);
  if (Math.abs(finalX - targetX) > 10)
    throw new Error(`walk timeout: x=${finalX} target=${targetX}`);
  return finalX;
}

// 走到与 NPC 同一垂直层（对话要求 |dy|<=80）。多层的招募地图上，出生点常
// 在顶层而 NPC 在地面：先正常走到 NPC 的 x，若层不对则向两侧「越过去」扫
// 探（借坡道/台阶/掉落换层），找到同层位置后再折返到 NPC 的 x。
export async function approachNpcLevel(driver, npcX, npcY, label) {
  const playerY = () => probe(driver, 'msplayer_y');
  await walkTowards(driver, npcX, 150).catch(error => {
    console.log(`INFO  ${label}: ${error.message}（继续找同层路径）`);
  });
  if (Math.abs((await playerY()) - npcY) <= 60) return true;
  const firstDir = Math.sign(npcX - (await playerX(driver))) || 1;
  for (const dir of [firstDir, -firstDir]) {
    const right = dir > 0;
    let lastX = await playerX(driver);
    for (let step = 0; step < 70; step += 1) {
      await driver.keyHold(right ? 'ArrowRight' : 'ArrowLeft',
        right ? 'ArrowRight' : 'ArrowLeft', right ? 39 : 37, 300);
      await sleep(120);
      const x = await playerX(driver);
      const y = await playerY();
      if (Math.abs(y - npcY) <= 60) {
        await walkTowards(driver, npcX, 90);
        if (Math.abs((await playerY()) - npcY) <= 60) return true;
        break;
      }
      if (Math.abs(x - lastX) < 2 && step > 3) break;
      lastX = x;
    }
    await walkTowards(driver, npcX, 90).catch(() => {});
  }
  // NPC 在玩家上方且平走换层失败：招募图多为「台阶跳跃图」，用「按住
  // 朝向 + 起跳」的棘轮攀爬（CWKPQ 山谷已验证的原语）。台阶常左右交错
  // （如 Ludi 221024500 左侧楼梯），固定方向会卡在台阶边缘，改为每拍
  // 左右交替、短按（窄步不越界）；每拍检查 |dy|≤60 即停，成功后走回 NPC x。
  if ((await playerY()) - npcY > 60) {
    for (let roundDir of [firstDir, -firstDir]) {
      let lastY = await playerY();
      let flat = 0;
      for (let step = 0; step < 60; step += 1) {
        const right = (step % 2 === 0 ? roundDir : -roundDir) > 0;
        const hold = driver.keyHold(right ? 'ArrowRight' : 'ArrowLeft',
          right ? 'ArrowRight' : 'ArrowLeft', right ? 39 : 37, 260);
        await sleep(120);
        await driver.keyHold('Alt', 'AltLeft', 18, 60);
        await hold;
        await sleep(380);
        const y = await playerY();
        if (Math.abs(y - npcY) <= 60) {
          await walkTowards(driver, npcX, 90).catch(() => {});
          if (Math.abs((await playerY()) - npcY) <= 60) return true;
          break;
        }
        if (Math.abs(y - lastY) < 2) flat += 1; else flat = 0;
        lastY = y;
        if (flat >= 10) break;
      }
      await walkTowards(driver, npcX, 90).catch(() => {});
      if (Math.abs((await playerY()) - npcY) <= 60) return true;
    }
  }
  console.log(`INFO  ${label}: 未能对齐 NPC 层（玩家 y=${await playerY()}，NPC y=${npcY}）`);
  return false;
}

export async function openNpcAt(driver, npcX, npcY, label) {
  const playerY = () => probe(driver, 'msplayer_y');
  let levelOk = await approachNpcLevel(driver, npcX, npcY, label);
  const finalX = await playerX(driver);
  if (Math.abs(finalX - npcX) > 120) throw new Error(`${label}: 走位失败（x=${finalX}）`);
  console.log(`INFO  ${label}: Space 前位置 x=${finalX} y=${await playerY()}（目标 ${npcX},${npcY}）`);
  await sleep(400);
  for (let attempt = 0; attempt < 4; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 1 ||
      await probe(driver, 'msshop_active') === 1, 8)) return;
    // NPC 常站在矮台上：小高度差时原地跳一下再试 Space。
    if (!levelOk && attempt === 1) {
      await driver.keyHold('Alt', 'AltLeft', 18, 300);
      await sleep(600);
    }
    await sleep(500);
  }
  const y = await playerY();
  throw new Error(`${label}: Space 未打开对话` +
    (Math.abs(y - npcY) <= 80 ? '' : `（玩家 y=${y} 与 NPC y=${npcY} 不同层）`));
}

export async function openNpc(driver, x, label) {
  const finalX = await walkTowards(driver, x, 150);
  if (Math.abs(finalX - x) > 100) throw new Error(`${label}: 走位失败（x=${finalX}）`);
  await sleep(400);
  for (let attempt = 0; attempt < 3; attempt += 1) {
    await driver.key('Space', 'Space', 32);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 1 ||
      await probe(driver, 'msshop_active') === 1, 8)) return;
    await sleep(500);
  }
  throw new Error(`${label}: Space 未打开对话/商店`);
}

// 只在对话仍打开时按 Escape（无对话时 Escape 会打开系统菜单，吞掉后续按键）。
export async function closeDialogue(driver, label) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    if (await probe(driver, 'msnpc_active') === 0 &&
      await probe(driver, 'msshop_active') === 0) {
      await sleep(300);
      return true;
    }
    await driver.key('Escape', 'Escape', 27);
    if (await waitFor(async () => await probe(driver, 'msnpc_active') === 0 &&
      await probe(driver, 'msshop_active') === 0, 5)) {
      await sleep(400);
      return true;
    }
  }
  throw new Error(`${label}: 窗口未关闭`);
}

// gohome-flow.mjs 的聊天发送原语：Enter 开聊天框 -> 输入 -> Enter 发送。
export async function sendChat(driver, message) {  await driver.key('Enter', 'Enter', 13);
  await sleep(150);
  await driver.typeAscii(message);
  await sleep(200);
  await driver.key('Enter', 'Enter', 13);
}
