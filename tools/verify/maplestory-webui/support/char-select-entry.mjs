// 选角屏 "进入游戏"（BtSelect）点击锚点：选角界面有两代绘制，按钮位置不同。
//   2023 扁平屏：BtSelect sprite 140x52 锚在 charselect::START_POS(612,336)，
//     热区 (586,427)-(726,479)，中心 (656,453)。
//   079MAX4 位图屏：BtSelect sprite 94x43 锚在 charselect_079::START_POS
//     (440,495)，origin (0,0) → 中心 (487,516)。
// 两代由只读 wasm 探针 msui_charselect_bitmap 区分（1=079 位图，其余=扁平）；
// 探针缺失（旧构建）时按扁平锚点处理，流程照常可跑。
// 坐标均为设计空间 800x600，driver.click/retryClick 内部自行补 VIEW_Y_OFFSET。

export const MODERN_START_POINT = Object.freeze({ x: 656, y: 453 });
export const BITMAP_START_POINT = Object.freeze({ x: 487, y: 516 });

const BITMAP_PROBE = "Module.ccall('msui_charselect_bitmap', 'number', [], [])";

// 解析当前选角屏的 BtSelect 中心点；探针调用失败（超时/未导出）按扁平屏处理。
export async function charSelectStartPoint(driver) {
  let bitmap = 0;
  try { bitmap = await driver.evaluate(BITMAP_PROBE); } catch { bitmap = 0; }
  return bitmap === 1 ? { ...BITMAP_START_POINT } : { ...MODERN_START_POINT };
}

// 选角后点 BtSelect 进游戏：先按当前代际取锚点，再复用 retryClick 抗事件丢弃。
export async function clickCharSelectStart(driver, predicate, tries = 8, settleMs = 4000) {
  const point = await charSelectStartPoint(driver);
  return await driver.retryClick(point.x, point.y, predicate, tries, settleMs);
}
