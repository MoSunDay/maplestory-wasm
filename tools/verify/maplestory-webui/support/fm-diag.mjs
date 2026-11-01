// FM NPC 诊断：进场后输出玩家 x/y，走位至 NPC 附近再输出，并截图。
import { BrowserDriver, sleep } from '../cdp.mjs';
import { loginAndEnterGame } from '../pq-harness-login.mjs';
import { waitForMapLoad } from './fm-map-retry.mjs';

const driver = new BrowserDriver({
  binary: process.env.CHROME_BIN || '/usr/bin/chromium-browser',
  debugPort: 9299,
  artifactDir: '/tmp/fm-diag',
  profileDir: '/tmp/maplestory-webui-fm-diag',
});
const probe = (name) => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], [])`);
const assetsIdle = async () => await probe('msasset_blocking') === 0 &&
  await driver.evaluate("document.getElementById('asset-loading-screen').classList.contains('is-hidden')");

try {
  await driver.start();
  await driver.navigate(process.env.DIAG_URL || 'http://127.0.0.1:8899/web/index.html');
  await loginAndEnterGame(driver, {
    account: 'test2', password: 'test', cardIndex: 0,
    pageUrl: process.env.DIAG_URL || 'http://127.0.0.1:8899/web/index.html',
  });
  await waitForMapLoad(driver, 910000000);
  for (let i = 0; i < 20 && !await assetsIdle(); i += 1) await sleep(3000);
  console.log(`DIAG map=${await probe('msmap_id')} x=${await probe('msplayer_x')} y=${await probe('msplayer_y')}`);
  await driver.screenshot('fm-diag-1');
  for (const target of [300, 450, 550, 450]) {
    for (let t = 0; t < 40; t += 1) {
      const x = await probe('msplayer_x');
      const dx = target - x;
      if (Math.abs(dx) <= 8) break;
      const ms = Math.abs(dx) > 400 ? 500 : Math.abs(dx) > 100 ? 300 : 140;
      if (dx > 0) await driver.keyHold('ArrowRight', 'ArrowRight', 39, ms);
      else await driver.keyHold('ArrowLeft', 'ArrowLeft', 37, ms);
      await sleep(120);
    }
    await sleep(400);
    console.log(`DIAG at target=${target}: x=${await probe('msplayer_x')} y=${await probe('msplayer_y')}`);
  }
  await driver.key('Space', 'Space', 32);
  await sleep(2500);
  console.log(`DIAG after Space: front=${await driver.evaluate("Module.ccall('msui_front_at','number',['number','number'],[400,240])")} npctalk=${await probe('msnpc_active')} state=${await probe('msui_state')}`);
  await driver.screenshot('fm-diag-2');
} finally {
  await driver.stop();
}
