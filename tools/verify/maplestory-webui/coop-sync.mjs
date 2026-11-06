// Dual-client cooperative acceptance flow.
//
// Driven by social-flow.mjs (login/boot). Verifies grounding/visibility,
// bidirectional smooth walking, simultaneous movement, expression relay and
// two-way attacks. Paired samples use one evaluate() per page.
import fs from 'fs';
import { sleep } from './cdp.mjs';
import { startFrameSampler, stopFrameSampler, startMotionSampler,
  stopMotionSampler, summarizeMotion } from './coop-motion.mjs';

const ATTACK_STANCES = new Set([
  10, 11, 12, 13,                         // shot
  15, 16, 17, 18, 19, 20,                 // stab
  23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33 // swing
]);
const SYNC_TOLERANCE = Number(process.env.COOP_SYNC_TOLERANCE || 48);
const MAX_STEP = Number(process.env.COOP_MAX_STEP || 80);
const MAX_P95_FRAME_MS = Number(process.env.COOP_MAX_P95_FRAME_MS || 40);
const MIN_P95_SAMPLES = 30;
const WALK_SAMPLES = 10, SAMPLE_INTERVAL_MS = 100;

async function snapshot(driver, remoteId) {
  return driver.evaluate(`(() => {
    const value = (name, argument) => argument === undefined
      ? Module.ccall(name, 'number', [], [])
      : Module.ccall(name, 'number', ['number'], [argument]);
    const remote = ${Number(remoteId)};
    return {
      now: performance.now(),
      map: value('msmap_id'),
      localX: value('msplayer_x'),
      localY: value('msplayer_y'),
      localScreenX: value('msplayer_screen_x'),
      localScreenY: value('msplayer_screen_y'),
      remoteVisible: value('mschar_visible', remote),
      remoteX: value('mschar_x', remote),
      remoteY: value('mschar_y', remote),
      remoteScreenX: value('mschar_screen_x', remote),
      remoteScreenY: value('mschar_screen_y', remote),
      remoteStance: value('mschar_stance', remote),
      remoteFrame: value('mschar_frame', remote),
      remoteExpression: value('mschar_expression', remote),
      otherCharacters: value('msother_character_count'),
      frameAvgUs: value('msframe_avg_us')
    };
  })()`);
}

function pair(mover, observer, moverId, observerId) {
  return Promise.all([
    snapshot(mover, observerId),
    snapshot(observer, moverId)
  ]);
}

function maxStep(values) {
  let worst = 0;
  for (let index = 1; index < values.length; index += 1) {
    worst = Math.max(worst, Math.abs(values[index] - values[index - 1]));
  }
  return worst;
}

function maxAbs(values) {
  return values.reduce((worst, value) => Math.max(worst, Math.abs(value)), 0);
}

function range(values) {
  return values.length ? Math.max(...values) - Math.min(...values) : 0;
}

function distinct(values) {
  return new Set(values).size;
}

function directionName(direction) {
  return direction === 'left' ? 'ArrowLeft' : 'ArrowRight';
}

function directionCode(direction) {
  return direction === 'left'
    ? { key: 'ArrowLeft', code: 'ArrowLeft', virtualKeyCode: 37 }
    : { key: 'ArrowRight', code: 'ArrowRight', virtualKeyCode: 39 };
}

async function dispatch(driver, direction, type) {
  const key = directionCode(direction);
  await driver.send('Input.dispatchKeyEvent', {
    type,
    key: key.key,
    code: key.code,
    windowsVirtualKeyCode: key.virtualKeyCode,
    nativeVirtualKeyCode: key.virtualKeyCode
  });
}

function checkPair(label, mover, observer, failures, counters) {
  if (mover.map !== observer.map) {
    failures.push(`${label}: map ${mover.map} != ${observer.map}`);
  }
  if (mover.remoteVisible !== 1 || observer.remoteVisible !== 1) {
    failures.push(`${label}: remote visibility ${observer.remoteVisible}/${mover.remoteVisible}`);
  }
  if (mover.otherCharacters < 1 || observer.otherCharacters < 1) {
    failures.push(`${label}: other character count ${observer.otherCharacters}/${mover.otherCharacters}`);
  }
  const dx = Math.abs(mover.localX - observer.remoteX);
  const dy = Math.abs(mover.localY - observer.remoteY);
  counters.worstSync = Math.max(counters.worstSync, dx, dy);
  if (dx > SYNC_TOLERANCE || dy > SYNC_TOLERANCE) {
    failures.push(`${label}: sync error ${dx},${dy}px > ${SYNC_TOLERANCE}px`);
  }
}

async function walk(mover, observer, moverId, observerId, direction, label, screenshot) {
  const samples = [];
  const failures = [];
  const counters = { worstSync: 0 };
  await startMotionSampler(observer, moverId);
  await dispatch(mover, direction, 'keyDown');
  let motionFrames;
  try {
    for (let index = 0; index < WALK_SAMPLES; index += 1) {
      const [moverSample, observerSample] = await pair(
        mover, observer, moverId, observerId);
      samples.push({ mover: moverSample, observer: observerSample });
      checkPair(`${label}-${index}`, moverSample, observerSample, failures, counters);
      await sleep(SAMPLE_INTERVAL_MS);
    }
    motionFrames = await stopMotionSampler(observer);
    if (screenshot) {
      await Promise.all([
        mover.screenshot(`coop-${label}-mover`),
        observer.screenshot(`coop-${label}-observer`)
      ]);
    }
  } finally {
    await dispatch(mover, direction, 'keyUp');
    await sleep(350);
  }
  const motion = summarizeMotion(motionFrames);
  const result = {
    label,
    direction,
    localRange: range(samples.map(sample => sample.mover.localX)),
    remoteRange: range(samples.map(sample => sample.observer.remoteX)),
    distinctRemoteX: distinct(samples.map(sample => sample.observer.remoteX)),
    distinctFrames: distinct(samples.map(sample => sample.observer.remoteFrame)),
    stances: [...new Set(samples.map(sample => sample.observer.remoteStance))],
    worstSync: counters.worstSync,
    worstStep: maxStep(samples.map(sample => sample.observer.remoteX)),
    motion,
    failures
  };
  console.log(`COOP walk ${label} dir=${direction} localRange=${result.localRange} ` +
    `remoteRange=${result.remoteRange} distinctX=${result.distinctRemoteX} ` +
    `frames=${result.distinctFrames} sync=${result.worstSync} step=${result.worstStep} ` +
    `motionStep=${motion.maxStep} p95Step=${motion.p95Step.toFixed(2)} ` +
    `still=${motion.longestStill}/${motion.frames}`);
  if (result.distinctRemoteX < 5 || result.distinctFrames < 2 || result.remoteRange < 30) {
    result.failures.push(`${label}: remote motion too coarse`);
  }
  if (result.worstStep > MAX_STEP) {
    result.failures.push(`${label}: largest remote step ${result.worstStep}px > ${MAX_STEP}px`);
  }
  return result;
}

async function settlePair(primaryDriver, secondaryDriver, primaryId, secondaryId) {
  const [primarySample, secondarySample] = await Promise.all([
    snapshot(primaryDriver, secondaryId),
    snapshot(secondaryDriver, primaryId)
  ]);
  const yDelta = Math.abs(primarySample.localY - secondarySample.remoteY);
  const reverseYDelta = Math.abs(secondarySample.localY - primarySample.remoteY);
  console.log(`COOP settle yDelta=${yDelta}/${reverseYDelta} ` +
    `stance=${secondarySample.remoteStance}/${primarySample.remoteStance}`);
  return { primarySample, secondarySample, yDelta, reverseYDelta };
}

async function waitForGround(primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor) {
  await waitFor('both avatars grounded and mutually visible', async () => {
    const [primarySample, secondarySample] = await Promise.all([
      snapshot(primaryDriver, secondaryId),
      snapshot(secondaryDriver, primaryId)
    ]);
    return primarySample.remoteVisible === 1 && secondarySample.remoteVisible === 1 &&
      primarySample.map === secondarySample.map &&
      Math.abs(primarySample.localY - secondarySample.remoteY) <= 6 &&
      Math.abs(secondarySample.localY - primarySample.remoteY) <= 6;
  }, 15);
}

async function expressionRelay(primaryDriver, secondaryDriver, primaryId, waitFor) {
  await waitFor('primary input ready for expression', () =>
    primaryDriver.evaluate("Module.ccall('msime_field_focused', 'number', [], []) === 0"), 5);
  await primaryDriver.key('F1', 'F1', 112);
  await waitFor('primary expression relayed to secondary', () =>
    snapshot(secondaryDriver, primaryId).then(sample => sample.remoteExpression === 2), 10);
  await primaryDriver.screenshot('coop-expression-primary');
  await secondaryDriver.screenshot('coop-expression-secondary');
  await waitFor('relayed expression timed out cleanly', () =>
    snapshot(secondaryDriver, primaryId).then(sample => sample.remoteExpression === 0), 12);
  return { expression: 2 };
}

async function attackRelay(primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor) {
  const seen = { primarySeesSecondary: false, secondarySeesPrimary: false };
  for (let attempt = 0; attempt < 6 && !(seen.primarySeesSecondary && seen.secondarySeesPrimary); attempt += 1) {
    await waitFor('attack inputs ready', async () =>
      await primaryDriver.evaluate("Module.ccall('msime_field_focused', 'number', [], []) === 0") &&
      await secondaryDriver.evaluate("Module.ccall('msime_field_focused', 'number', [], []) === 0"), 5);
    await Promise.all([
      primaryDriver.keyHold('Control', 'ControlLeft', 17, 120),
      secondaryDriver.keyHold('Control', 'ControlLeft', 17, 120)
    ]);
    for (let poll = 0; poll < 16 && !(seen.primarySeesSecondary && seen.secondarySeesPrimary); poll += 1) {
      const [primarySample, secondarySample] = await Promise.all([
        snapshot(primaryDriver, secondaryId),
        snapshot(secondaryDriver, primaryId)
      ]);
      if (ATTACK_STANCES.has(primarySample.remoteStance)) seen.primarySeesSecondary = true;
      if (ATTACK_STANCES.has(secondarySample.remoteStance)) seen.secondarySeesPrimary = true;
      await sleep(70);
    }
  }
  await primaryDriver.screenshot('coop-attack-primary');
  await secondaryDriver.screenshot('coop-attack-secondary');
  if (!seen.primarySeesSecondary || !seen.secondarySeesPrimary) {
    throw new Error(`two-way attack relay missing: ${JSON.stringify(seen)}`);
  }
  console.log('PASS  simultaneous attack animation relayed in both directions');
  return seen;
}

export async function runCoopSync({
  primaryDriver,
  secondaryDriver,
  primaryId,
  secondaryId,
  waitFor
}) {
  const failures = [];
  const results = { primaryId, secondaryId, walks: [], frames: {} };

  await waitForGround(primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor);
  await Promise.all([
    primaryDriver.screenshot('coop-before-primary'),
    secondaryDriver.screenshot('coop-before-secondary')
  ]);
  const settled = await settlePair(primaryDriver, secondaryDriver, primaryId, secondaryId);
  if (settled.yDelta > 6 || settled.reverseYDelta > 6) {
    throw new Error(`avatars not grounded: y delta ${settled.yDelta}/${settled.reverseYDelta}`);
  }

  // One direction for each character. If the first walk hits a wall, try the
  // opposite direction once; the acceptance criterion is smooth motion, not a
  // particular map edge.
  for (const [direction, suffix] of [['right', 'primary'], ['left', 'secondary']]) {
    const mover = suffix === 'primary' ? primaryDriver : secondaryDriver;
    const observer = suffix === 'primary' ? secondaryDriver : primaryDriver;
    const moverId = suffix === 'primary' ? primaryId : secondaryId;
    const observerId = suffix === 'primary' ? secondaryId : primaryId;
    const first = await walk(
      mover, observer, moverId, observerId, direction, `${suffix}-${direction}`, true);
    let chosen = first;
    if (first.remoteRange < 30) {
      const opposite = direction === 'left' ? 'right' : 'left';
      chosen = await walk(
        mover, observer, moverId, observerId, opposite, `${suffix}-${opposite}-retry`, true);
    }
    results.walks.push(chosen);
    failures.push(...chosen.failures);
  }

  await waitForGround(primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor);
  await settlePair(primaryDriver, secondaryDriver, primaryId, secondaryId);

  // Simultaneous cooperative movement plus an in-page frame-time sample.
  const [primaryStart, secondaryStart] = await Promise.all([
    snapshot(primaryDriver, secondaryId),
    snapshot(secondaryDriver, primaryId)
  ]);
  const primaryDirection = secondaryStart.localX < primaryStart.localX ? 'left' : 'right';
  const secondaryDirection = primaryStart.localX < secondaryStart.localX ? 'left' : 'right';
  await Promise.all([
    startFrameSampler(primaryDriver),
    startFrameSampler(secondaryDriver),
    startMotionSampler(primaryDriver, secondaryId),
    startMotionSampler(secondaryDriver, primaryId),
    dispatch(primaryDriver, primaryDirection, 'keyDown'),
    dispatch(secondaryDriver, secondaryDirection, 'keyDown')
  ]);
  const together = [];
  const togetherCounters = { worstSync: 0 };
  try {
    for (let index = 0; index < WALK_SAMPLES; index += 1) {
      const [primarySample, secondarySample] = await Promise.all([
        snapshot(primaryDriver, secondaryId),
        snapshot(secondaryDriver, primaryId)
      ]);
      together.push({ primary: primarySample, secondary: secondarySample });
      checkPair(`together-primary-${index}`, primarySample, secondarySample, failures, togetherCounters);
      checkPair(`together-secondary-${index}`, secondarySample, primarySample, failures, togetherCounters);
      await sleep(SAMPLE_INTERVAL_MS);
    }
  } finally {
    const [primaryFrames, secondaryFrames, primaryMotionFrames, secondaryMotionFrames] =
      await Promise.all([
        stopFrameSampler(primaryDriver),
        stopFrameSampler(secondaryDriver),
        stopMotionSampler(primaryDriver),
        stopMotionSampler(secondaryDriver)
      ]);
    await Promise.all([
      dispatch(primaryDriver, primaryDirection, 'keyUp'),
      dispatch(secondaryDriver, secondaryDirection, 'keyUp')
    ]);
    results.frames = { primary: primaryFrames, secondary: secondaryFrames };
    results.motion = {
      primary: summarizeMotion(primaryMotionFrames),
      secondary: summarizeMotion(secondaryMotionFrames)
    };
  }
  await Promise.all([
    primaryDriver.screenshot('coop-together-primary'),
    secondaryDriver.screenshot('coop-together-secondary')
  ]);
  const togetherResult = {
    primaryDirection,
    secondaryDirection,
    primaryLocalRange: range(together.map(sample => sample.primary.localX)),
    secondaryLocalRange: range(together.map(sample => sample.secondary.localX)),
    primaryRemoteRange: range(together.map(sample => sample.primary.remoteX)),
    secondaryRemoteRange: range(together.map(sample => sample.secondary.remoteX)),
    worstSync: togetherCounters.worstSync,
    worstStep: Math.max(
      maxStep(together.map(sample => sample.primary.remoteX)),
      maxStep(together.map(sample => sample.secondary.remoteX))
    )
  };
  results.together = togetherResult;
  console.log(`COOP together localRange=${togetherResult.primaryLocalRange}/` +
    `${togetherResult.secondaryLocalRange} remoteRange=${togetherResult.primaryRemoteRange}/` +
    `${togetherResult.secondaryRemoteRange} sync=${togetherResult.worstSync} ` +
    `step=${togetherResult.worstStep}`);
  if (togetherResult.primaryLocalRange < 30 || togetherResult.secondaryLocalRange < 30) {
    failures.push('together: both local characters must move');
  }
  if (togetherResult.primaryRemoteRange < 25 || togetherResult.secondaryRemoteRange < 25) {
    failures.push('together: both observers must see the peer move');
  }

  for (const [label, sample] of Object.entries(results.frames)) {
    if (sample.frames.length < MIN_P95_SAMPLES || sample.p95 > MAX_P95_FRAME_MS) {
      failures.push(`${label}: frame p95 ${sample.p95}ms over ${sample.frames.length} samples ` +
        `(limit ${MAX_P95_FRAME_MS}ms)`);
    }
  }
  console.log(`COOP frame p95 main=${results.frames.primary.p95}ms ` +
    `peer=${results.frames.secondary.p95}ms`);
  console.log(`COOP together motionStep main=${results.motion.primary.maxStep} ` +
    `peer=${results.motion.secondary.maxStep} p95 ` +
    `${results.motion.primary.p95Step.toFixed(2)}/` +
    `${results.motion.secondary.p95Step.toFixed(2)}`);
  for (const [label, motion] of Object.entries(results.motion)) {
    if (motion.frames < MIN_P95_SAMPLES || motion.distinctX < 5 || motion.maxStep > 24 ||
        motion.p95Step > 12 || motion.p95FrameInterval > MAX_P95_FRAME_MS) {
      failures.push(`${label}: remote motion not smooth ${JSON.stringify(motion)}`);
    }
  }

  await sleep(500);
  results.expression = await expressionRelay(
    primaryDriver, secondaryDriver, primaryId, waitFor);
  await waitForGround(primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor);
  results.attacks = await attackRelay(
    primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor);
  await waitForGround(primaryDriver, secondaryDriver, primaryId, secondaryId, waitFor);
  await Promise.all([
    primaryDriver.screenshot('coop-after-primary'),
    secondaryDriver.screenshot('coop-after-secondary')
  ]);

  results.failures = failures;
  const artifact = `${primaryDriver.artifactDir}/coop-acceptance.json`;
  fs.writeFileSync(artifact, `${JSON.stringify(results, null, 2)}\n`);
  console.log(`coop acceptance artifact: ${artifact}`);
  if (failures.length) {
    throw new Error(`co-op acceptance failed: ${failures.slice(0, 8).join('; ')}`);
  }
  if (primaryDriver.errors.length || secondaryDriver.errors.length) {
    throw new Error(`browser exceptions: ${primaryDriver.errors.length + secondaryDriver.errors.length}`);
  }
  console.log('PASS  dual-account cooperative movement/expression/attack acceptance');
  return results;
}
