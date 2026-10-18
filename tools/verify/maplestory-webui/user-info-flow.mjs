import { sleep } from './cdp.mjs';

export async function verifyUserInfoFlow(driver, probes, requireState) {
  const characterId = await probes.playerId();
  const point = {
    x: await probes.playerScreenX(),
    y: await probes.playerScreenY() - 32
  };
  await driver.doubleClick(point.x, point.y);
  await requireState('self character info card opened', async () =>
    await probes.userInfoActive() === 1 &&
    await probes.userInfoTargetId() === characterId);
  await requireState('self character info payload loaded', async () =>
    await probes.userInfoLoaded() === 1);
  await requireState('self character info uses authentic 271x190 layout', async () =>
    await probes.userInfoWidth() === 271 && await probes.userInfoHeight() === 190);
  await sleep(500);
  await driver.screenshot('user-info-self');

  const petCount = await probes.userInfoPetCount();
  if (petCount > 0) {
    const origin = { x: await probes.userInfoX(), y: await probes.userInfoY() };
    await driver.click(origin.x + 180, origin.y + 173);
    await requireState('self pet detail expanded', async () =>
      await probes.userInfoPetExpanded() === 1);
    await requireState('pet detail is stacked below the character card', async () =>
      await probes.userInfoWidth() === 271 && await probes.userInfoHeight() === 387);
    await driver.screenshot('user-info-self-pet');
  }

  await driver.key('Escape', 'Escape', 27);
  await requireState('self character info card closed', async () =>
    await probes.userInfoActive() === 0);
  console.log(`PASS  self character info card loaded; summonedPets=${petCount}`);
}
