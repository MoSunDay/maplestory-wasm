// 装备防御结算实测：真实 WASM 客户端登录后经 InventoryStatProbe 探针读取
// EQUIPPED/EQUIP 槽位分布与 WDEF/MDEF 总值，对照服务器数据库中该角色的
// 已穿（8 件）与背包（4 件）装备，判定 SetField 登录解析是否把背包装备
// 误计入 EQUIPPED（那会让穿与不穿的属性、受击伤害完全一致）。
// 用法: node tools/verify/maplestory-webui/equip-stat-flow.mjs
import { BrowserDriver, sleep } from './cdp.mjs';
import { loginAndEnterGame } from './pq-harness-login.mjs';

const url = process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html';
const account = process.env.E2E_ACCOUNT || 'test1';
const password = process.env.E2E_PASSWORD || 'test1';
const binary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const INV = Object.freeze({ EQUIP: 1, EQUIPPED: 6 });
const STAT = Object.freeze({ WDEF: 8, MDEF: 9, WATK: 6 });

const driver = new BrowserDriver({
  binary, debugPort: 9301, artifactDir: '/tmp/equip-stat-flow',
});
const ccall1 = (name, a) => driver.evaluate(
  `Module.ccall('${name}','number',['number'],[${a}])`);
const ccall2 = (name, a, b) => driver.evaluate(
  `Module.ccall('${name}','number',['number','number'],[${a},${b}])`);
async function waitFor(predicate, seconds, label) {
  for (let i = 0; i < seconds * 5; i += 1) {
    if (await predicate()) return true;
    await sleep(200);
  }
  throw new Error(`Timed out: ${label}`);
}
const slots = async (type) => {
  const found = [];
  for (let slot = 1; slot <= 120; slot += 1) {
    const item = await ccall2('msinv_slot_item', type, slot);
    if (item) found.push({ slot, item });
  }
  return found;
};

let exitCode = 0;
try {
  await driver.start();
  await driver.send('Page.navigate', { url });
  await waitFor(() => driver.evaluate(
    "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90, 'wasm ready');
  await loginAndEnterGame(driver, { account, password, cardIndex: 0, pageUrl: url });
  await sleep(5000);

  const equipped = await slots(INV.EQUIPPED);
  const bag = await slots(INV.EQUIP);
  const wdef = await ccall1('msstat_total', STAT.WDEF);
  const mdef = await ccall1('msstat_total', STAT.MDEF);
  const watk = await ccall1('msstat_total', STAT.WATK);

  console.log(`EQUIPPED(${equipped.length}): ${JSON.stringify(equipped)}`);
  console.log(`EQUIP bag(${bag.length}): ${JSON.stringify(bag)}`);
  console.log(`totals: WDEF=${wdef} MDEF=${mdef} WATK=${watk}`);

  // test1Hero 数据库事实: 已穿 8 件, 背包 4 件 (槽 2..5)。
  const expectedBagIds = [1302000, 1002067, 1302000, 1052155].sort();
  const bagIds = bag.map(e => e.item).sort();
  const equippedIds = equipped.map(e => e.item).sort();
  const ok = equipped.length === 8
    && bag.length === 4
    && JSON.stringify(bagIds) === JSON.stringify(expectedBagIds)
    && !equippedIds.includes(1302000)
    && !equippedIds.includes(1002067)
    && !equippedIds.includes(1052155);
  console.log(ok ? 'PASS 登录装备解析与数据库一致' : 'FAIL 装备分布与数据库不符');
  if (!ok) exitCode = 1;

  // 真实 wasm 二进制上的公式对拍:
  //   base = mobatk/2 + mobatk/DEF   (防御贡献)
  //   dmg  = base * (1 - reducedamage), Achilles Lv30 x=800 => 减伤 20%。
  // WDEF=198: (500+5)*0.8 = 404; 裸装(DEF~4) 会得到 (500+250)*0.8 = 600,
  // 404 vs 600 的差异即防御可见贡献。旧公式(x/1000=0.8)曾把伤害压到 101。
  const pdmg = await ccall2('msstat_damage', 1000, 0);
  const mdmg = await ccall2('msstat_damage', 1000, 1);
  // C++ 侧运算顺序: base - trunc(base * reducedamage)。
  const dmg = (def) => {
    const base = Math.floor(1000 / 2) + Math.floor(1000 / def);
    return base - Math.trunc(base * 0.2);
  };
  const expectP = dmg(wdef);
  const expectM = dmg(mdef);
  console.log(`damage probes: phys=${pdmg} (expect ${expectP}) magic=${mdmg} (expect ${expectM})`);
  const dmgOk = pdmg === expectP && mdmg === expectM && pdmg < 600 && mdmg < 600;
  console.log(dmgOk
    ? 'PASS 防御参与受击伤害计算, Achilles 减伤为 20% (wasm 运行时)'
    : 'FAIL 受击伤害未反映防御值或减伤比例错误');
  if (!dmgOk) exitCode = 1;
} catch (error) {
  console.error('HARNESS ERROR', error);
  exitCode = 2;
} finally {
  await driver.stop();
  process.exit(exitCode);
}
