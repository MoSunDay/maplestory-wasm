// In-page motion samplers for the co-op acceptance flow. Each rAF callback is
// one synchronous evaluate(): the remote position, stance, frame number and
// timestamp all belong to the same rendered frame.
function percentile(values, fraction) {
  if (!values.length) return Infinity;
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.min(sorted.length - 1, Math.ceil(sorted.length * fraction) - 1)];
}

export async function startFrameSampler(driver) {
  await driver.evaluate(`(() => {
    window.__coopFrameSample = { active: true, previous: 0, frames: [] };
    const tick = now => {
      const sample = window.__coopFrameSample;
      if (!sample || !sample.active) return;
      if (sample.previous > 0) sample.frames.push(now - sample.previous);
      sample.previous = now;
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    return true;
  })()`);
}

export async function stopFrameSampler(driver) {
  const frames = await driver.evaluate(`(() => {
    const sample = window.__coopFrameSample;
    sample.active = false;
    return sample.frames.slice(2);
  })()`);
  return { frames, p95: percentile(frames, 0.95) };
}

export async function startMotionSampler(driver, remoteId) {
  await driver.evaluate(`(() => {
    window.__coopMotionSample = {
      active: true,
      remote: ${Number(remoteId)},
      frames: []
    };
    const value = (name, argument) => Module.ccall(name, 'number', ['number'], [argument]);
    const tick = now => {
      const sample = window.__coopMotionSample;
      if (!sample || !sample.active) return;
      sample.frames.push({
        now,
        x: value('mschar_x', sample.remote),
        y: value('mschar_y', sample.remote),
        screenX: value('mschar_screen_x', sample.remote),
        stance: value('mschar_stance', sample.remote),
        frame: value('mschar_frame', sample.remote)
      });
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    return true;
  })()`);
}

export async function stopMotionSampler(driver) {
  return driver.evaluate(`(() => {
    const sample = window.__coopMotionSample;
    sample.active = false;
    return sample.frames.slice(2);
  })()`);
}

export function summarizeMotion(frames) {
  const valid = frames.filter(frame => frame.x !== -32768);
  const x = valid.map(frame => frame.x);
  const deltas = [];
  const intervals = [];
  for (let index = 1; index < valid.length; index += 1) {
    deltas.push(Math.abs(x[index] - x[index - 1]));
    intervals.push(valid[index].now - valid[index - 1].now);
  }
  let longestStill = 0;
  let still = 0;
  let stillFrames = 0;
  for (const delta of deltas) {
    if (delta === 0) {
      still += 1;
      stillFrames += 1;
    } else {
      still = 0;
    }
    longestStill = Math.max(longestStill, still);
  }
  return {
    frames: valid.length,
    distinctX: new Set(x).size,
    range: x.length ? Math.max(...x) - Math.min(...x) : 0,
    maxStep: deltas.length ? Math.max(...deltas) : 0,
    p95Step: percentile(deltas, 0.95),
    maxFrameInterval: intervals.length ? Math.max(...intervals) : 0,
    p95FrameInterval: percentile(intervals, 0.95),
    longestStill,
    stillRatio: deltas.length ? stillFrames / deltas.length : 0,
    distinctFrames: new Set(valid.map(frame => frame.frame)).size,
    stances: [...new Set(valid.map(frame => frame.stance))]
  };
}
