const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const { pathToFileURL } = require('node:url');
const PptxGenJS = require('pptxgenjs');
const sharp = require('sharp');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'generated/ui_icon_comparison_20261105');
const width = 1600, height = 900;
const color = { ink: '213B38', teal: '087F7A', muted: '657773', line: 'D4DFDA' };
const narration = [
  'プレイ画面では、文字中心だった道具の選択を、',
  '実際の道具や種袋の形を使ったアイコンに変えました。',
  '下の拡大部分を見ると、違いが分かります。',
  '選んだ道具の名前や、次にできる操作は短い文章で残しています。'
];
const notes = '【話す・約20秒の目安】\n' + narration.join('\n') +
  '\n\n【操作・参考：読まない】\n左が変更前、右が変更後。下の道具選択の拡大を指す。\n' +
  '異なる時期・状態の実画面で、撮影条件は一致していない。UIの表示方法を比較する資料。\n' +
  '背景、作物、日付の変化をUI改善の成果として説明しない。初見の操作時間や分かりやすさの改善率は未測定。\n' +
  '元の7枚目の売却動画は残し、その直前に挿入する。追加分を含めた5分の実測練習は別途必要。';

const escapeHtml = text => text.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const elements = [];
const text = (value, x, y, w, h, size, fill = color.ink, bold = false) => elements.push({ type: 'text', value, x, y, w, h, size, fill, bold });
const image = (file, x, y, w, h) => elements.push({ type: 'image', file, x, y, w, h });
const box = (x, y, w, h, fill = color.teal) => elements.push({ type: 'box', x, y, w, h, fill });

(async () => {
  await fs.mkdir(path.join(out, 'assets'), { recursive: true });
  const sources = [
    ['before-original.png', 'C:/Users/K024G/AppData/Local/Temp/codex-clipboard-88e096c1-9266-4a10-8926-c7e6df921804.png'],
    ['after-original.png', path.join(root, 'generated/ui_progress_no_bottom_notes_20261104/assets/current-hud.png')]
  ];
  const provenance = [];
  for (const [name, source] of sources) {
    const bytes = await fs.readFile(source);
    await fs.writeFile(path.join(out, 'assets', name), bytes);
    const metadata = await sharp(bytes).metadata();
    provenance.push({ name, source, sha256: crypto.createHash('sha256').update(bytes).digest('hex'), width: metadata.width, height: metadata.height });
  }
  // These are lossless document crops; the source screenshots remain unchanged.
  const crop = async (source, name, rect) => sharp(path.join(out, 'assets', source)).extract(rect).png().toFile(path.join(out, 'assets', name));
  await crop('before-original.png', 'before-game.png', { left: 1, top: 31, width: 1278, height: 689 });
  await crop('before-original.png', 'before-tools.png', { left: 402, top: 647, width: 616, height: 69 });
  await crop('after-original.png', 'after-tools.png', { left: 380, top: 620, width: 520, height: 80 });

  text('プレイ画面 / アイコン化', 64, 32, 1464, 34, 26, color.teal, true);
  text('道具と種を、実物の形で選ぶ', 64, 82, 1472, 76, 52, color.ink, true);
  text('変更前', 64, 177, 712, 42, 32, color.muted, true);
  text('変更後', 824, 177, 712, 42, 32, color.teal, true);
  image('before-game.png', 64, 243, 712, 712 * 689 / 1278);
  image('after-original.png', 824, 235, 712, 400.5);
  box(64 + 400 * 712 / 1278, 243 + 614 * 712 / 1278, 618 * 712 / 1278, 72 * 712 / 1278);
  box(824 + 378 * 712 / 1280, 235 + 618 * 712 / 1280, 524 * 712 / 1280, 83 * 712 / 1280);
  text('道具選択を拡大', 64, 656, 1472, 34, 26, color.muted);
  image('before-tools.png', 64, 704, 712, 712 * 69 / 616);
  image('after-tools.png', 824, 697, 712, 712 * 80 / 520);
  text('道具名をボタンに表示', 64, 827, 712, 42, 30, color.ink, true);
  text('道具・種袋をアイコンで表示', 824, 827, 712, 42, 30, color.ink, true);
  for (const e of elements) {
    if (e.x < 0 || e.y < 0 || e.x + e.w > width || e.y + e.h > height) throw new Error('Slide geometry overflow');
  }

  const pptx = new PptxGenJS();
  pptx.defineLayout({ name: 'MATCH', width: 13.333333, height: 7.5 });
  pptx.layout = 'MATCH';
  pptx.author = '水路農業';
  pptx.title = 'アイコン化のビフォー・アフター';
  pptx.lang = 'ja-JP';
  pptx.theme = { headFontFace: 'Yu Gothic', bodyFontFace: 'Yu Gothic', lang: 'ja-JP' };
  const slide = pptx.addSlide();
  slide.background = { color: 'FFFFFF' };
  for (const e of elements) {
    const pos = { x: e.x / 120, y: e.y / 120, w: e.w / 120, h: e.h / 120 };
    if (e.type === 'text') slide.addText(e.value, { ...pos, margin: 0, fontFace: 'Yu Gothic', fontSize: e.size * 0.6, color: e.fill, bold: e.bold, breakLine: false, valign: 'mid', fit: 'shrink' });
    if (e.type === 'image') slide.addImage({ ...pos, path: path.join(out, 'assets', e.file), altText: e.file });
    if (e.type === 'box') slide.addShape(pptx.ShapeType.rect, { ...pos, fill: { color: 'FFFFFF', transparency: 100 }, line: { color: e.fill, width: 1.5 } });
  }
  slide.addNotes(notes);
  await pptx.writeFile({ fileName: path.join(out, 'アイコン化_比較ページ.pptx') });
  const htmlElements = elements.map(e => {
    const pos = `left:${e.x}px;top:${e.y}px;width:${e.w}px;height:${e.h}px;`;
    if (e.type === 'text') return `<div data-check class="text" style="${pos}font-size:${e.size}px;color:#${e.fill};font-weight:${e.bold ? 700 : 400}">${escapeHtml(e.value)}</div>`;
    if (e.type === 'image') return `<img data-check src="assets/${e.file}" style="${pos}" alt="${escapeHtml(e.file)}">`;
    return `<div class="box" style="${pos}border:2px solid #${e.fill}"></div>`;
  }).join('\n');
  await fs.writeFile(path.join(out, '比較ページ.html'), `<!doctype html><html lang="ja"><meta charset="utf-8"><title>アイコン化の比較</title><style>@page{size:1600px 900px;margin:0}*{box-sizing:border-box}body{margin:0;background:#fff;font-family:"Yu Gothic","Meiryo",sans-serif}main{position:relative;width:1600px;height:900px}main>*{position:absolute}.text{display:flex;align-items:center;white-space:nowrap;line-height:1.3}.box{pointer-events:none}</style><main>${htmlElements}</main></html>`);
  await fs.writeFile(path.join(out, '追加ページのカンペ.txt'), 'アイコン化の比較（新7枚目）\n\n' + narration.join('\n\n') + '\n\n【短縮版】\n道具と種袋を実物の形で表示し、選んだ道具の名前は短く残しました。\n\n【読まないメモ】\n異なる状態の画面です。UIの表示方法を比較します。\n約20秒の目安です。追加後は発表全体を再計測してください。\n');
  await fs.writeFile(path.join(out, 'comparison.json'), JSON.stringify({ elements, notes, provenance }, null, 2));

  const browser = await chromium.launch({ executablePath: 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', headless: true });
  try {
    const page = await browser.newPage({ viewport: { width, height }, deviceScaleFactor: 1 });
    await page.goto(pathToFileURL(path.join(out, '比較ページ.html')).href);
    await page.evaluate(() => document.fonts.ready);
    const failures = await page.locator('[data-check]').evaluateAll(nodes => nodes.flatMap(n => {
      const r = n.getBoundingClientRect();
      const issues = [];
      if (r.right > 1600.5 || r.bottom > 900.5 || n.scrollWidth > n.clientWidth + 1 || n.scrollHeight > n.clientHeight + 1) issues.push(n.textContent || n.getAttribute('alt'));
      if (n.tagName === 'IMG' && (!n.complete || !n.naturalWidth)) issues.push('missing image');
      return issues;
    }));
    if (failures.length) throw new Error(JSON.stringify(failures));
    await page.screenshot({ path: path.join(out, 'アイコン化_比較ページ.png') });
    await page.pdf({ path: path.join(out, 'アイコン化_比較ページ.pdf'), printBackground: true, preferCSSPageSize: true });
    await fs.writeFile(path.join(out, 'validation.json'), JSON.stringify({ geometry: 'PASS', textAndImages: 'PASS', failures, sourcePixelsUnmodified: true, generatedSlides: 1, cloudInsertion: 'not-yet-verified' }, null, 2));
  } finally { await browser.close(); }
  console.log(out);
})().catch(error => { console.error(error); process.exitCode = 1; });
