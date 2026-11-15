// Pet options panel interaction helpers, split from pet-automation-flow.mjs
// to respect the per-file line budget. Every panel coordinate is resolved
// through the mspet_options_* probes (slider percent inverse, potion
// selector, buff slots, OK button) instead of hard-coded screen offsets, so
// the flows survive panel repositioning and resolution changes.
import { sleep } from './cdp.mjs';

// pet_automation::MAX_BUFF_SKILLS: draft probes read one slot at a time.
export const PET_BUFF_SLOTS = 12;
// Skill book geometry comes from the msskillbook_* probes: the window
// position is loaded from user Settings, so tab and icon targets must be
// queried instead of hard-coded.
const SKILLBOOK_ROWS = 4;
// UIStatusbar::Buttons::BT_SKILL; the status bar skill icon toggles the same
// panel as the K shortcut, without depending on canvas keyboard focus.
const STATUSBAR_SKILL_BUTTON = 11;
// UIElement::PETOPTIONS. Buff slots sit below the skill book, so the
// drop target must resolve straight to the panel without click-to-raise.
const PET_OPTIONS_ELEMENT = 23;
// A click right of the potion selector center cycles to the next candidate;
// the fixed offset keeps browser pointer rounding from flipping direction.
const POTION_NEXT_OFFSET = 40;

async function statusbarPressCount(driver) {
  return driver.evaluate(
    "Module.ccall('msstatusbar_press_count','number',[],[])");
}

// Pressing the status bar skill button is proven landed when the monotonic
// press counter advanced; otherwise the click was dropped by the asset
// blocking gate (UI::send_cursor gates on asset_loading_blocks_input).
async function pressSkillButton(driver, probes) {
  const bounds = await probes.statusbarControlBounds(STATUSBAR_SKILL_BUTTON);
  if (!bounds || bounds.right <= bounds.left || bounds.bottom <= bounds.top)
    return false;
  const before = await statusbarPressCount(driver);
  await driver.click(
    (bounds.left + bounds.right) / 2,
    (bounds.top + bounds.bottom) / 2
  );
  await sleep(400);
  const after = await statusbarPressCount(driver);
  return Number.isFinite(before) && before >= 0 && after === before + 1;
}

export async function openSkillbook(driver, probes, requireState) {
  if (await probes.skillbookActive() === 1) return;
  // The status bar skill button routes through UI::send_menu and ignores
  // the character keymap, unlike the K shortcut: a server keymap packet can
  // rebind K to a skill cast (key 37 rows), and the cast then streams skill
  // assets that would gate the next input. Click the button first and only
  // fall back to K when the button is missing or its press never landed.
  for (let attempt = 0; attempt < 4; attempt += 1) {
    await requireState('skill book input ready', async () =>
      await probes.assetBlocking() === 0, 30);
    if (!await pressSkillButton(driver, probes)) {
      await driver.key('k', 'KeyK', 75, 'k');
      await sleep(400);
    }
    for (let i = 0; i < 6 && await probes.skillbookActive() !== 1; i += 1)
      await sleep(500);
    if (await probes.skillbookActive() === 1) return;
  }
  await requireState('skill book opened', async () =>
    await probes.skillbookActive() === 1, 10);
}

export async function closeSkillbook(driver, probes) {
  if (await probes.skillbookActive() !== 1) return;
  for (let attempt = 0; attempt < 3 && await probes.skillbookActive() === 1;
    attempt += 1) {
    if (!await pressSkillButton(driver, probes)) {
      await driver.key('k', 'KeyK', 75, 'k');
      await sleep(300);
    } else {
      await sleep(300);
    }
  }
}

async function visibleSkillRows(probes, skillId) {
  const rows = [];
  for (let row = 0; row < SKILLBOOK_ROWS; row += 1) {
    if (await probes.skillbookRowSkill(row) === skillId) rows.push(row);
  }
  return rows;
}

async function selectSkillTab(driver, probes, tab, skillId) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    const tabX = await probes.skillbookTabX(tab);
    const tabY = await probes.skillbookTabY(tab);
    if (tabX < 0 || tabY < 0)
      throw new Error(`skill book tab ${tab} unavailable: ${tabX},${tabY}`);
    await driver.click(tabX, tabY);
    await sleep(350);
    const rows = await visibleSkillRows(probes, skillId);
    if (rows.length > 0) return rows;
  }
  throw new Error(`skill ${skillId} not visible on tab ${tab}`);
}

async function frontAt(driver, x, y) {
  return driver.evaluate(
    `Module.ccall('msui_front_at','number',['number','number'],[${x},${y}])`);
}

// Drags start on a skill-book icon, so the release must resolve to the
// pet options panel on its own. Asserting the front window at the buff
// slot catches any later layering/hit-test regression.
async function requirePetOptionsDropTarget(driver, x, y) {
  const front = await frontAt(driver, x, y);
  if (front !== PET_OPTIONS_ELEMENT)
    throw new Error(`pet options is not the drop target at ${x},${y}: ` +
      `front=${front}`);
}

export async function draftBuffSkills(probes) {
  const slots = [];
  for (let slot = 0; slot < PET_BUFF_SLOTS; slot += 1)
    slots.push(await probes.petOptionsBuffDraft(slot));
  return slots;
}

export function draftBuffCount(slots) {
  return slots.filter((skillId) => skillId > 0).length;
}

export async function dragSkillToBuffSlot(driver, probes, requireState, tab, skillId, slot) {
  const rows = await selectSkillTab(driver, probes, tab, skillId);
  const x = await probes.petOptionsBuffSlotX(slot);
  const y = await probes.petOptionsBuffSlotY(slot);
  if (!Number.isFinite(x) || !Number.isFinite(y) || x < 0 || y < 0)
    throw new Error(`buff slot ${slot} unavailable: ${x},${y}`);
  const liveBefore = await probes.petAutoBuffSkill(slot);
  await requirePetOptionsDropTarget(driver, x, y);
  let draft = 0;
  for (const row of rows) {
    const iconX = await probes.skillbookIconX(row);
    const iconY = await probes.skillbookIconY(row);
    if (iconX < 0 || iconY < 0)
      throw new Error(`skill ${skillId} icon unavailable: ${iconX},${iconY}`);
    await driver.drag(iconX, iconY, x, y);
    await sleep(350);
    draft = await probes.petOptionsBuffDraft(slot);
    const front = await frontAt(driver, x, y);
    console.log(`INFO  buff drag tab=${tab} skill=${skillId} row=${row} ` +
      `icon=${iconX},${iconY} slot=${x},${y} front=${front} ` +
      `selected=${await probes.skillbookSelected()} draft=${draft} ` +
      `live=${await probes.petAutoBuffSkill(slot)}`);
    // A rejected drop leaves the draft slot empty; retry from the next
    // visible skill book row before giving up.
    if (draft === skillId) break;
  }
  if (draft !== skillId)
    throw new Error(`buff draft slot ${slot} expected ${skillId}, got ${draft}`);
  const live = await probes.petAutoBuffSkill(slot);
  if (live !== liveBefore)
    throw new Error(`buff drag persisted slot ${slot} before OK: ` +
      `${liveBefore} -> ${live}`);
  if (await probes.skillbookSelected() !== skillId)
    throw new Error(`skill book selected mismatch after dragging ${skillId}`);
  console.log(`PASS  buff slot ${slot} drafts skill ${skillId} without persisting`);
}

// slider_x_for_percent inverts the panel mapping, so the click maps back to
// exactly the requested percent when send_cursor resolves it.
export async function clickSliderAtPercent(driver, probes, hp, percent) {
  const x = await probes.petOptionsSliderX(hp, percent);
  const y = await probes.petOptionsSliderY(hp);
  if (!Number.isFinite(x) || !Number.isFinite(y) || x < 0 || y < 0)
    throw new Error(`pet options ${hp ? 'HP' : 'MP'} slider unavailable: ${x},${y}`);
  // The browser -> WASM cursor transport truncates to whole game pixels
  // (Window.cpp to_design_position), and the fractional CSS canvas scale
  // (662/800) leaves the round trip just under the requested coordinate,
  // which drops the slider one value step near the right end. Aim half a
  // game pixel beyond the probe coordinate so the truncation keeps it.
  await driver.click(x + 0.5, y + 0.5);
  await sleep(250);
}

// The panel exposes one selector row per resource; clicking right of its
// center cycles candidates forward until the draft reads the target item.
export async function selectPotion(driver, probes, readSelection, target, hp) {
  for (let attempt = 0; attempt < 16 && await readSelection() !== target; attempt += 1) {
    const x = await probes.petOptionsPotionX(hp, true);
    const y = await probes.petOptionsPotionY(hp, true);
    if (!Number.isFinite(x) || !Number.isFinite(y) || x < 0 || y < 0)
      throw new Error(`pet options ${hp ? 'HP' : 'MP'} potion selector unavailable: ${x},${y}`);
    await driver.click(x + POTION_NEXT_OFFSET, y);
    await sleep(250);
  }
  const selected = await readSelection();
  if (selected !== target)
    throw new Error(`Could not select potion ${target}; selected=${selected}`);
}
