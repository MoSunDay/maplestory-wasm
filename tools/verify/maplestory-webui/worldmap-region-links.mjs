// 世界地图（W 键）区域链接命中修复的端到端验证。
//
//   * W 打开世界地图：从 211000000（冰封雪域）进入应落在用户所在区域
//     WorldMap021；Escape 链先关 post-Chaos 搜索面板，再 021→020→根图。
//   * 根图 13 个区域链接（000/010/020/030/040/050/060/070/080/090/100/
//     140/142）全部可点击且 resolved 命中各自目标：内部测试点用
//     msworldmap_link_hit 在页面内一次 evaluate 扫描得到（5x5 腐蚀容差
//     吸收客户端与 CDP 坐标 ~2px CSS 缩入舍入；逐点 CDP 往返会慢到
//     不可用）。修复前：NX 子节点按名称字符串序迭代（0,1,10,11,12,2…）
//     导致按钮错位，BT_LINK0..9 上限把 "7"/"8"/"9"（→070/080/090）挤出；
//     linkImg 矩形大面积重叠 + 基类首个命中矩形获胜，点玩具城会进
//     ElNath。修复后按 alpha 掩码逐像素命中、最小面积决胜。
//   * 海域点击（任何链接形状之外）不导航。
//   * 子区域递归：Victoria(010) 区域图内的链接同样按形状可点。
//   * 关闭按钮生效；关闭后重开仍落在用户区域。
//
// 探针 _msworldmap_*（active/geometry/link_count/link_bounds/link_state/
// link_target_hash/link_hit/resolved_hash/user_hash/parent_hash/name_hash/
// search）由 src/client/CMakeLists.txt 的 EXPORTED_FUNCTIONS 导出，实现于
// src/client/Gameplay/Probes/UIWorldMapProbes.cpp；窗口修复位于
// IO/UITypes/UIWorldMap.{h,cpp}。
//
// 前置（操作者执行，凭据不入代码仓库）：
//   update accounts set loggedin=0 where name='fmflow1';
//   fmflow1 的 card0 角色需位于 211000000（冰封雪域）；
//   keymap 需有 W=17→(1,3)（KeyType::MENU/ACTION=WORLDMAP 默认绑定）。
//   ccall 返回有符号 int32，哈希须 `>>> 0` 再比较。
//
// 运行：node tools/verify/maplestory-webui/worldmap-region-links.mjs
//   （可用 WORLDMAP_DEBUG_PORT / E2E_URL / CHROME_BIN 覆盖默认值）
import fs from 'node:fs';
import { BrowserDriver, sleep } from './cdp.mjs';

const debugPort = Number(process.env.WORLDMAP_DEBUG_PORT || 9287);
const chromeBinary = process.env.CHROME_BIN || '/usr/bin/chromium-browser';
const uiState = { login: 1, world: 2, characters: 3, game: 5 };
const H = { 'WorldMap': 0x9a5b5d43, 'WorldMap000': 0xdae471d1, 'WorldMap010': 0xd4e229c8,
  'WorldMap020': 0x46e99903, 'WorldMap021': 0x45e99770, 'WorldMap030': 0x60e7835a,
  'WorldMap040': 0xd2eef295, 'WorldMap050': 0x4cebe10c, 'WorldMap060': 0xdef382a7,
  'WorldMap070': 0xd8f13a9e, 'WorldMap080': 0xcad0d0e9, 'WorldMap090': 0xc4ce88e0,
  'WorldMap100': 0x5b0fd726, 'WorldMap140': 0xe3061fe2, 'WorldMap142': 0xe1061cbc };
const hex = x => '0x' + (x >>> 0).toString(16);

const probe = (driver, name, args = '[]') => driver.evaluate(`Module.ccall(${JSON.stringify(name)}, 'number', [], ${args})`);
const evalx = (driver, expr) => driver.evaluate(expr);
async function waitFor(predicate, seconds = 45) { for (let i = 0; i < seconds * 4; i += 1) { if (await predicate()) return true; await sleep(250); } return false; }
async function enterGame(driver) {
  await waitFor(() => evalx(driver, "typeof Module !== 'undefined' && Module.calledRun === true && typeof Module.ccall === 'function'"), 90);
  await waitFor(async () => await probe(driver, 'msui_state') === uiState.login, 30);
  await driver.clickField(340, 260); await driver.typeAscii('fmflow1');
  await driver.clickField(340, 285); await driver.typeAscii('fmflow1');
  await driver.key('Enter', 'Enter', 13);
  await waitFor(async () => await probe(driver, 'msui_state') === uiState.world, 45);
  await driver.retryClick(510, 216, async () => await probe(driver, 'msui_state') === uiState.characters);
  await sleep(2500);
  let entered = false;
  for (let a = 0; a < 4 && !entered; a += 1) { await driver.doubleClick(130, 210);
    for (let i = 0; i < 5 && !entered; i += 1) { entered = await probe(driver, 'msui_state') === uiState.game; if (!entered) await sleep(1000); } }
  if (!entered) throw new Error('game UI did not activate');
  await sleep(4000);
}

const resolved = async driver => (await probe(driver, 'msworldmap_resolved_hash')) >>> 0;
const active = driver => probe(driver, 'msworldmap_active');

async function gotoRoot(driver) {
  if (!(await active(driver))) { await driver.key('w', 'KeyW', 87); await sleep(2200); }
  for (let i = 0; i < 5 && (await resolved(driver)) !== H.WorldMap; i += 1) {
    await driver.key('Escape', 'Escape', 27); await sleep(1200);
  }
  return (await resolved(driver)) === H.WorldMap;
}

const driver = new BrowserDriver({ binary: chromeBinary, debugPort });
let pass = 0, fail = 0;
const check = (name, ok, detail = '') => { console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  ' + detail : ''}`); ok ? pass++ : fail++; };
try {
  await driver.start();
  await driver.navigate(process.env.E2E_URL || 'http://127.0.0.1:8000/web/index.html');
  await enterGame(driver);
  console.log('map id:', await probe(driver, 'msmap_id'));
  await waitFor(async () => (await probe(driver, 'msstage_loading')) === 0, 60);
  await sleep(1500);

  for (let a = 0; a < 3 && (await active(driver)) !== 1; a += 1) { await driver.key('w', 'KeyW', 87); await sleep(2500); }
  check('W opens at user region (WorldMap021)', (await resolved(driver)) === H.WorldMap021, hex(await resolved(driver)));
  await driver.key('Escape', 'Escape', 27); await sleep(1300); // 关搜索面板
  await driver.key('Escape', 'Escape', 27); await sleep(1300); // 021 → 020
  check('ESC chain 021→020', (await resolved(driver)) === H.WorldMap020, hex(await resolved(driver)));
  await driver.key('Escape', 'Escape', 27); await sleep(1300); // 020 → WorldMap
  check('ESC chain 020→WorldMap', (await resolved(driver)) === H.WorldMap, hex(await resolved(driver)));

  const links = await probe(driver, 'msworldmap_link_count');
  check('root map exposes all 13 links', links === 13, `links=${links}`);

  for (let i = 0; i < links; i += 1) {
    if (!(await gotoRoot(driver))) { check(`link#${i} precondition root`, false); continue; }
    const l = await probe(driver, 'msworldmap_link_bounds', `[${i},0]`);
    const t = await probe(driver, 'msworldmap_link_bounds', `[${i},1]`);
    const r = await probe(driver, 'msworldmap_link_bounds', `[${i},2]`);
    const b = await probe(driver, 'msworldmap_link_bounds', `[${i},3]`);
    const targetHash = (await probe(driver, 'msworldmap_link_target_hash', `[${i}]`)) >>> 0;
    // 形状内部点（含 ±2px 容差）：整段扫描放进页面内一次执行，避免上千次 CDP 往返
    const pt = await evalx(driver, `(() => {
      const hit = (x, y) => Module.ccall('msworldmap_link_hit', 'number', [], [x, y]);
      const solid = (x, y) => {
        for (let dy = -2; dy <= 2; dy++)
          for (let dx = -2; dx <= 2; dx++)
            if (hit(x + dx, y + dy) !== ${i}) return false;
        return true;
      };
      for (let y = ${t} + 3; y < ${b} - 3; y += 2)
        for (let x = ${l} + 3; x < ${r} - 3; x += 2)
          if (solid(x, y)) return { x, y };
      return null;
    })()`);
    const targetName = Object.keys(H).find(k => H[k] === targetHash) || hex(targetHash);
    if (!pt) { check(`link#${i} ${targetName} interior point`, false, `rect=[${l},${t}-${r},${b}]`); continue; }
    await driver.click(pt.x, pt.y); await sleep(1800);
    const got = await resolved(driver);
    check(`click link#${i} → ${targetName}`, got === targetHash, `resolved=${hex(got)} at (${pt.x},${pt.y})`);
    await driver.key('Escape', 'Escape', 27); await sleep(1300);
  }

  if (!(await gotoRoot(driver))) check('ocean precondition', false);
  else {
    const before = await resolved(driver);
    // 在 (410,240) 附近找一个任何链接形状之外的点（矩形交集内的海域）
    const ocean = await evalx(driver, `(() => {
      const hit = (x, y) => Module.ccall('msworldmap_link_hit', 'number', [], [x, y]);
      const clear = (x, y) => {
        for (let dy = -2; dy <= 2; dy++)
          for (let dx = -2; dx <= 2; dx++)
            if (hit(x + dx, y + dy) !== -1) return false;
        return true;
      };
      for (let y = 240; y < 256; y += 2)
        for (let x = 400; x < 430; x += 2)
          if (clear(x, y)) return { x, y };
      return null;
    })()`);
    if (!ocean) { check('ocean point found', false); }
    else {
      await driver.click(ocean.x, ocean.y); await sleep(1300);
      check('ocean click does not navigate', (await resolved(driver)) === before && before === H.WorldMap, `at (${ocean.x},${ocean.y})`);
    }
  }

  // --- 子区域递归：Victoria(010) 区域图内的链接同样按形状可点
  if (await gotoRoot(driver)) {
    await driver.click(194, 144); await sleep(1800); // Victoria 岛形状内部点
    const sub = await resolved(driver);
    if (sub === H.WorldMap010) {
      const sl = await probe(driver, 'msworldmap_link_bounds', '[0,0]');
      const st = await probe(driver, 'msworldmap_link_bounds', '[0,1]');
      const sr = await probe(driver, 'msworldmap_link_bounds', '[0,2]');
      const sb = await probe(driver, 'msworldmap_link_bounds', '[0,3]');
      const starget = (await probe(driver, 'msworldmap_link_target_hash', '[0]')) >>> 0;
      const spt = await evalx(driver, `(() => {
        const hit = (x, y) => Module.ccall('msworldmap_link_hit', 'number', [], [x, y]);
        const solid = (x, y) => {
          for (let dy = -2; dy <= 2; dy++)
            for (let dx = -2; dx <= 2; dx++)
              if (hit(x + dx, y + dy) !== 0) return false;
          return true;
        };
        for (let y = ${st} + 3; y < ${sb} - 3; y += 2)
          for (let x = ${sl} + 3; x < ${sr} - 3; x += 2)
            if (solid(x, y)) return { x, y };
        return null;
      })()`);
      if (spt) {
        await driver.click(spt.x, spt.y); await sleep(1800);
        const sgot = await resolved(driver);
        check('sub-region link inside WorldMap010', sgot === starget,
          `resolved=${hex(sgot)} target=${hex(starget)} at (${spt.x},${spt.y})`);
      } else check('sub-region interior point', false, `rect=[${sl},${st}-${sr},${sb}]`);
    } else check('enter Victoria region', false, hex(sub));
  }

  if (await gotoRoot(driver)) {
    const wasActive = await active(driver);
    await driver.click(766, 16); await sleep(1300);
    check('close button deactivates window', wasActive === 1 && (await active(driver)) === 0);
  } else check('close precondition', false);

  for (let a = 0; a < 3 && (await active(driver)) !== 1; a += 1) { await driver.key('w', 'KeyW', 87); await sleep(2200); }
  check('reopen after close', (await active(driver)) === 1 && (await resolved(driver)) === H.WorldMap021);
  console.log(`\n${pass} passed, ${fail} failed`);
} catch (error) {
  console.error('flow aborted:', error.message);
} finally {
  if (driver.errors.length) fs.writeFileSync(`${driver.artifactDir}/browser-errors.log`, `${driver.errors.join('\n')}\n`);
  fs.writeFileSync(`${driver.artifactDir}/browser-console.log`, `${driver.logs.join('\n')}\n`);
  console.log('artifacts:', driver.artifactDir);
  await driver.stop();
}
process.exitCode = fail ? 1 : 0;
