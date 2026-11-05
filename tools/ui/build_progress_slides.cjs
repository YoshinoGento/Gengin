const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const cp = require('node:child_process');
const { pathToFileURL } = require('node:url');
const PptxGenJS = require('pptxgenjs');
const JSZip = require('jszip');
const { chromium } = require('playwright');

const root = path.resolve(__dirname, '../..');
const source = path.join(root, 'generated/ui_progress_report_20261102');
const out = path.join(root, 'generated/ui_progress_slides_20261103');
const stem = 'LE3C_28_ヨシノ_ゲント_UI改善_試作';
const W = 1600, H = 900, PX = 120;
const C = { ink: '243B38', teal: '087F7A', muted: '62736F', line: 'D5DFDB', pale: 'EDF5F2', gold: '956B1C', warm: 'F7F2E7', white: 'FFFFFF' };
const hash = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const escape = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const ensure = p => fs.mkdirSync(p, { recursive: true });
function walk(dir) {
  return fs.existsSync(dir) ? fs.readdirSync(dir, { withFileTypes: true }).flatMap(e => e.isDirectory() ? walk(path.join(dir, e.name)) : [path.join(dir, e.name)]) : [];
}
function protectedState() {
  const names = cp.execFileSync('git', ['ls-files', '-z', '--cached', '--others', '--exclude-standard', 'project'], { cwd: root, encoding: 'utf8' }).split('\0').filter(Boolean);
  const files = new Set([...names.map(p => path.join(root, p)), ...walk(source), ...walk(path.join(root, 'project/Settings'))]);
  return Object.fromEntries([...files].filter(p => fs.existsSync(p)).sort().map(p => [path.relative(root, p), hash(p)]));
}
ensure(out);
for (const dir of ['assets', 'preview', 'review']) ensure(path.join(out, dir));
const protection = protectedState();
const baseline = path.join(out, 'protected_hashes.json');
if (fs.existsSync(baseline) && JSON.stringify(JSON.parse(fs.readFileSync(baseline))) !== JSON.stringify(protection)) throw Error('Protected source or support kit changed');
fs.writeFileSync(baseline, JSON.stringify(protection, null, 2));

const mediaNames = ['02_hud_after_20261019.png', '01_hud_before_20261016.png', '03_menu_before.png', '04_menu_after.png', '10_shop_after.jpg', '12_sale_after.jpg'];
const provenance = [];
for (const name of mediaNames) {
  const input = path.join(source, 'images', name), output = path.join(out, 'assets', name);
  fs.copyFileSync(input, output);
  if (hash(input) !== hash(output)) throw Error(`Image changed: ${name}`);
  provenance.push({ name, source: path.relative(root, input), sha256: hash(output) });
}

const deck = [];
function slide(title, section, seconds, notes) {
  const s = { title, section, seconds, notes, elements: [] };
  deck.push(s);
  return s;
}
function text(s, value, x, y, w, h, size = 32, color = C.ink, bold = false, align = 'left') {
  s.elements.push({ type: 'text', value, x, y, w, h, size, color, bold, align });
}
function box(s, x, y, w, h, fill, stroke = fill, thickness = 1) { s.elements.push({ type: 'rect', x, y, w, h, fill, stroke, thickness }); }
function line(s, x, y, w, h = 0, color = C.line, arrow = false) { s.elements.push({ type: 'line', x, y, w, h, color, arrow }); }
function image(s, name, x, y, w, h) { s.elements.push({ type: 'image', name, x, y, w, h }); }
function header(s, subtitle) {
  text(s, s.section, 64, 26, 1200, 30, 21, C.teal, true);
  text(s, s.title, 64, 78, 1480, 68, 47, C.ink, true);
  text(s, subtitle, 64, 153, 1470, 40, 26, C.muted);
}
function footer(s, evidence) {
  line(s, 64, 854, 1472);
  text(s, evidence, 64, 867, 1360, 24, 18, C.muted);
  text(s, `${deck.indexOf(s) + 1} / 6`, 1455, 866, 80, 26, 19, C.muted, false, 'right');
}

{
  const s = slide('操作の目的ごとに\n情報を分けるUI改善', '制作進捗 / UI改善', 25,
    '目安25秒。LE3C、28番の吉野絃音です。制作している「水路農業」のUI改善について発表します。このゲームでは土地と水を整えながら野菜を育てます。今回は、操作や説明が多くなっていた画面を、何をするための情報かという視点で整理しました。ゲーム中の表示に加え、メニューと種の購入、収穫物の売却を見直しました。\n\n画面は10月19日の記録です。最新のグラフィック全体を紹介する画像ではありません。');
  text(s, s.section, 64, 30, 1300, 32, 22, C.teal, true);
  text(s, s.title, 64, 91, 1470, 143, 55, C.ink, true);
  image(s, mediaNames[0], 64, 268, 970, 569);
  text(s, '水路農業', 1094, 310, 440, 70, 51, C.teal, true);
  text(s, '土地と水を整え、\n野菜を育てるゲーム', 1094, 399, 440, 100, 33);
  line(s, 1094, 537, 440);
  text(s, '今回の範囲', 1094, 564, 440, 40, 25, C.muted, true);
  text(s, 'HUD・メニュー\n種の購入・収穫物の売却', 1094, 615, 440, 95, 30);
  text(s, 'LE3C 28  吉野絃音', 1094, 771, 440, 43, 29, C.muted);
  footer(s, 'ゲーム画面：2026年10月19日の記録 / 発表構成の試作');
}
{
  const s = slide('情報を消すのではなく、見る場面を分ける', '01 課題と整理方針', 45,
    '目安45秒。以前の画面は、畑の状態、操作説明、道具の切替などが同時に並び、どこを見ればよいか分かりにくい状態でした。そこで、常に見る情報、操作するときに見る情報、必要なときに詳しく見る情報の三つに分けました。道具や種は実際のアイテムの見た目を使いました。ただし、全てをアイコンにせず、目的や金額のように文字や数字が必要な部分は残しています。\n\n旧HUDと前ページのHUDは日付・畑・持ち物・解像度が異なります。厳密な画面占有率や使いやすさの比較ではありません。');
  header(s, '以前のHUDでは、状態・説明・操作が同時に並んでいた');
  text(s, '以前の画面', 64, 221, 850, 40, 27, C.muted, true);
  image(s, mediaNames[1], 64, 278, 850, 478.125);
  const rows = [ ['常に見る', '道具・種・所持金', C.teal], ['操作するとき', '購入数・売却数・金額', '496C91'], ['詳しく知りたいとき', '品質・大きさ・出品予約', C.gold] ];
  rows.forEach(([a, b, color], i) => {
    const y = 231 + i * 168;
    box(s, 970, y, 5, 116, color);
    text(s, a, 1000, y, 520, 48, 33, color, true);
    text(s, b, 1000, y + 57, 520, 46, 30);
    if (i < 2) line(s, 1000, y + 135, 524);
  });
  text(s, '一覧で判断し、必要なときに詳細へ進む。', 64, 793, 1472, 45, 32, C.teal, true);
  footer(s, '旧HUD：ユーザー提供画像 / 画像間で日付・畑・持ち物が異なるため、配置の説明として使用');
}
{
  const s = slide('メニューは「目的」を選んでから詳しく見る', '02 メニューの改善', 55,
    '目安55秒。こちらが農作業メニューの比較です。以前は道具や種の切替と、種屋や収穫物の操作が同じ一覧に並んでいました。変更後は、まず行いたいことを選ぶ形にしました。本文の操作ボタンは14項目から6項目に整理しています。上のタブと下の戻るボタンは数に含めていません。機能を削除したわけではなく、従来の操作は詳細画面に残しています。設定も音、カメラ、モードに分けました。一方で詳細を開く手間は増えるので、入口が見つかるかは確認が必要です。\n\n比較は大会1日目・300G・種0個で一致。右画像は売却入口を「収穫物・売却」へ改名する前の記録です。無効状態の本文ボタンも数えています。14→6は操作時間や使いやすさの改善率ではありません。');
  header(s, '従来の操作は削除せず、「詳しい操作」に残した');
  text(s, '変更前', 64, 213, 704, 40, 28, C.muted, true);
  text(s, '変更後', 832, 213, 704, 40, 28, C.teal, true);
  image(s, mediaNames[2], 64, 269, 704, 412.955);
  image(s, mediaNames[3], 832, 269, 704, 412.955);
  text(s, '本文の操作', 64, 716, 260, 42, 29, C.muted);
  text(s, '14', 344, 701, 130, 87, 66, C.muted, true);
  line(s, 490, 744, 88, 0, C.teal, true);
  text(s, '6', 609, 701, 100, 87, 66, C.teal, true);
  text(s, '一覧を絞る / 詳細への入口を残す', 832, 723, 704, 50, 32, C.teal, true);
  text(s, '項目', 717, 734, 86, 42, 26, C.muted);
  footer(s, '同じ初期状態で撮影。本文ボタンのみ集計（無効を含む、タブ・戻るを除く）。右画像は売却入口の改名前。');
}
{
  const s = slide('購入・売却に必要な数字を、操作の近くへ', '03 購入・売却の改善', 60,
    '目安60秒。ここでは変更後の二つの画面を示します。左は種屋です。買う種類を選び、個数と合計金額を確認して購入する流れに整理しました。購入後の所持金も操作する場所の近くで確認できます。右は収穫物の売却です。詳しい記録を最初から見せる形から、作物ごとに売る個数と金額を確認する一覧へ変更しました。品質や大きさ、出品予約は詳細に残しています。売却は確認画面を通し、保護した作物は対象から除きます。購入と売却それぞれで、その場で判断するための情報をまとめたことが改善点です。\n\n両方とも11月2日の変更後の画面であり、左右のビフォー・アフターではありません。フリー9日目・300G・各種5個・収穫物10個、うち保護1個という検証用データです。通常の育成結果ではありません。');
  header(s, '変更後の2例：買うための情報と、売るための情報をそれぞれ整理');
  text(s, '種屋', 64, 213, 704, 42, 30, C.teal, true);
  text(s, '収穫物の売却', 832, 213, 704, 42, 30, C.teal, true);
  image(s, mediaNames[4], 64, 269, 704, 412.955);
  image(s, mediaNames[5], 832, 269, 704, 412.955);
  text(s, '種類 → 個数 → 合計金額', 64, 719, 704, 49, 35, C.ink, true);
  text(s, '売却は一覧 / 品質・予約は詳細', 832, 719, 704, 49, 32, C.ink, true);
  text(s, '購入後の所持金まで同じ場所で確認', 64, 780, 704, 38, 25, C.muted);
  text(s, '保護して残す個数も、売る前に確認', 832, 780, 704, 38, 25, C.muted);
  footer(s, '両方とも変更後。2026年11月2日、同じ検証用データで撮影（通常プレイの育成成果ではない）。');
}
{
  const s = slide('UIでは在庫を変えず、Systemへ処理を渡す', '04 プログラムの仕組み', 70,
    '目安70秒。プログラムでは画面表示と在庫の変更を分けています。UIは売却が選ばれたという通知を返します。Controllerがそれを確認用のSystemへ渡し、Systemが売る個数や金額などの見積もりを保持します。最後の確認ボタンを押したときに、現在の値ともう一度照合します。例えば、確認中に在庫や所持金が変わった場合、表示した内容と違う取引にならないよう、そのまま売らずに再確認します。一致した場合だけ既存の売買用Systemが在庫とお金を変更します。価格や保護のルールをUIへ書き直さず、元の処理を利用しています。\n\n図はメニュー売却の経路のみ。従来のF/V即売却ショートカットは未変更です。照合する見積もりは作物・個数・受取額・保護数・所持金・読み込み世代。今回、既存Controller全体の責務混在までは解消していません。');
  header(s, 'メニューから売却する場合：通知 → 確認 → 状態の変更');
  const cols = [
    ['1  UI', '操作を通知', 'View::Hit'],
    ['2  Controller', '処理を渡す', 'HandleSaleRequest'],
    ['3  確認用System', '見積もりを照合', 'FarmSaleReviewSystem'],
    ['4  売買用System', 'お金・在庫を変更', 'FarmEconomySystem'],
  ];
  cols.forEach(([role, verb, code], i) => {
    const x = 64 + i * 384;
    box(s, x, 270, 320, 230, i === 3 ? C.pale : 'F5F7F6', C.line);
    text(s, role, x + 18, 293, 284, 44, 27, C.teal, true);
    text(s, verb, x + 18, 361, 284, 48, 29, C.ink, true);
    text(s, code, x + 18, 444, 284, 34, 20, C.muted);
    if (i < 3) line(s, x + 327, 383, 48, 0, C.muted, true);
  });
  text(s, '一致', 1154, 338, 62, 33, 22, C.teal, true);
  line(s, 992, 507, 0, 48, C.gold, true);
  box(s, 760, 576, 464, 80, C.warm);
  text(s, '不一致なら売らずに再確認', 778, 596, 428, 43, 29, C.gold, true, 'center');
  text(s, '例：確認中に所持金や在庫が変わった場合', 64, 723, 1470, 48, 35, C.ink, true);
  text(s, '表示した条件と異なる取引を、そのまま実行しない。', 64, 786, 1470, 43, 31, C.teal, true);
  footer(s, '価格計算・保護ルールは既存のEconomy処理を再利用。図はメニュー経路のみ（F/V即売却は従来のまま）。');
}
{
  const s = slide('表示の整理と動作確認まで。次は初見で検証する', '05 確認結果と次の課題', 45,
    '目安45秒。改修時には購入と売却、取消、詳細から戻る操作をマウスで確認しました。検証用の作物10個のうち1個を保護した状態では、9個だけが売られ、保護した作物が残ることも確認しています。自動テストとDebug、Releaseのビルドも通しています。ただし、操作時間や初めて触る人の迷いやすさはまだ測定していません。次は購入と売却を説明なしで行ってもらい、迷った場所を確認します。今回は情報を目的ごとに分け、必要な操作を見つけやすくするための整理を進めました。以上です。\n\n確認済みは11月2日の改修時の記録。今回のスライド作成時にゲームを再ビルド・再検証していません。全UI完成や操作時間短縮の証明ではありません。入力機器、別PC/GPU/DPI、全シーズンの回帰確認は未完了です。');
  header(s, '実装できたことと、まだ確かめていないことを分ける');
  text(s, '確認済み', 64, 235, 704, 56, 39, C.teal, true);
  text(s, '次に確認する', 832, 235, 704, 56, 39, C.gold, true);
  line(s, 64, 308, 704, 0, C.teal);
  line(s, 832, 308, 704, 0, C.gold);
  const left = ['購入・売却・取消・詳細の往復', '保護した作物を残して売却', '自動テスト / Debug・Releaseビルド'];
  const right = ['説明なしで購入・売却できるか', '詳細への入口が見つかるか', '実キーボード・パッド・別PC'];
  for (let i = 0; i < 3; i++) {
    text(s, `${i + 1}`, 64, 346 + i * 113, 50, 46, 31, C.teal, true);
    text(s, left[i], 124, 346 + i * 113, 635, 80, 31);
    text(s, `${i + 1}`, 832, 346 + i * 113, 50, 46, 31, C.gold, true);
    text(s, right[i], 892, 346 + i * 113, 635, 80, 31);
  }
  box(s, 64, 718, 1472, 105, C.pale);
  text(s, '目的ごとに情報を整理。使いやすさは、初見の操作で確かめる。', 90, 749, 1420, 52, 36, C.teal, true, 'center');
  footer(s, '動作確認は2026年11月2日の改修時の記録。初見ユーザーの操作時間・達成率は未測定。');
}

function svg(s) {
  return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}"><defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 Z" fill="#62736F"/></marker></defs><rect width="1600" height="900" fill="white"/>${s.elements.map(e => {
    if (e.type === 'text') return `<foreignObject x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}"><div xmlns="http://www.w3.org/1999/xhtml" data-text="1" style="font-family:Meiryo,sans-serif;font-size:${e.size}px;color:#${e.color};font-weight:${e.bold ? 700 : 400};line-height:1.25;white-space:pre-wrap;text-align:${e.align};letter-spacing:0;margin:0">${escape(e.value)}</div></foreignObject>`;
    if (e.type === 'rect') return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="#${e.fill}" stroke="#${e.stroke}" stroke-width="${e.thickness}"/>`;
    if (e.type === 'line') return `<line x1="${e.x}" y1="${e.y}" x2="${e.x + e.w}" y2="${e.y + e.h}" stroke="#${e.color}" stroke-width="2" ${e.arrow ? 'marker-end="url(#arrow)"' : ''}/>`;
    const data = fs.readFileSync(path.join(out, 'assets', e.name)).toString('base64');
    return `<image x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" xlink:href="data:image/${e.name.endsWith('.png') ? 'png' : 'jpeg'};base64,${data}"/>`;
  }).join('')}</svg>`;
}

(async () => {
  if (deck.length !== 6 || deck.reduce((a, s) => a + s.seconds, 0) !== 300) throw Error('Invalid slide/timing count');
  const pptx = new PptxGenJS();
  pptx.defineLayout({ name: 'UI_WIDE', width: W / PX, height: H / PX });
  pptx.layout = 'UI_WIDE';
  pptx.author = '吉野絃音';
  pptx.subject = '水路農業 UI改善の進捗報告・試作';
  pptx.title = '操作の目的ごとに情報を分けるUI改善';
  pptx.lang = 'ja-JP';
  pptx.theme = { headFontFace: 'Meiryo', bodyFontFace: 'Meiryo', lang: 'ja-JP' };
  const geometryChecks = [];
  for (const [i, s] of deck.entries()) {
    const p = pptx.addSlide();
    p.background = { color: C.white };
    for (const e of s.elements) {
      if (e.x < 0 || e.y < 0 || e.x + e.w > W + .1 || e.y + e.h > H + .1) throw Error(`Outside slide ${i + 1}: ${JSON.stringify(e)}`);
      const g = { x: e.x / PX, y: e.y / PX, w: e.w / PX, h: e.h / PX };
      if (e.type === 'text') p.addText(e.value, { ...g, fontFace: 'Meiryo', fontSize: e.size * 72 / PX, color: e.color, bold: e.bold, margin: 0, breakLine: false, valign: 'top', align: e.align, charSpacing: 0, paraSpaceAfter: 0, lineSpacingMultiple: 1.0 });
      else if (e.type === 'image') p.addImage({ ...g, path: path.join(out, 'assets', e.name), altText: e.name });
      else if (e.type === 'rect') p.addShape(pptx.ShapeType.rect, { ...g, fill: { color: e.fill }, line: { color: e.stroke, width: e.thickness * 72 / PX }, radius: 0 });
      else p.addShape(pptx.ShapeType.line, { ...g, line: { color: e.color, width: 1.2, ...(e.arrow ? { endArrowType: 'triangle' } : {}) } });
    }
    p.addNotes(s.notes);
    fs.writeFileSync(path.join(out, 'preview', `slide-${i + 1}.svg`), svg(s));
    geometryChecks.push({ slide: i + 1, shapes: s.elements.length, bounds: 'PASS' });
  }
  const pptxPath = path.join(out, `${stem}.pptx`);
  await pptx.writeFile({ fileName: pptxPath, compression: true });
  const zip = await JSZip.loadAsync(fs.readFileSync(pptxPath));
  const slides = Object.keys(zip.files).filter(n => /^ppt\/slides\/slide\d+\.xml$/.test(n));
  const notes = Object.keys(zip.files).filter(n => /^ppt\/notesSlides\/notesSlide\d+\.xml$/.test(n));
  if (slides.length !== 6 || notes.length !== 6) throw Error('Missing slides or notes');
  for (const f of [...slides, ...notes]) if (!(await zip.file(f).async('string')).includes('<a:t>')) throw Error(`Empty text: ${f}`);
  fs.writeFileSync(path.join(out, 'deck.json'), JSON.stringify(deck, null, 2));
  fs.writeFileSync(path.join(out, '発表者ノート.txt'), deck.map((s, i) => `${i + 1}. ${s.title.replaceAll('\n', ' ')}\n${s.notes}`).join('\n\n'));
  const browser = await chromium.launch({ headless: true, channel: 'msedge' });
  const layoutChecks = [];
  try {
    const page = await browser.newPage({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
    for (let i = 0; i < 6; i++) {
      await page.goto(pathToFileURL(path.join(out, 'preview', `slide-${i + 1}.svg`)).href);
      await page.evaluate(() => document.fonts.ready);
      const overflow = await page.evaluate(() => [...document.querySelectorAll('[data-text]')].flatMap(el => {
        const r = el.getBoundingClientRect(), parent = el.parentElement.getBoundingClientRect();
        return r.height > parent.height + 1 || el.scrollWidth > parent.width + 1 ? [{ text: el.textContent, height: r.height, allowed: parent.height }] : [];
      }));
      if (overflow.length) throw Error(`Text overflow ${i + 1}: ${JSON.stringify(overflow)}`);
      await page.screenshot({ path: path.join(out, 'preview', `slide-${i + 1}.png`) });
      layoutChecks.push({ slide: i + 1, text_fit: 'PASS', rendering: 'same-model browser preview; verify Google import separately' });
    }
    fs.writeFileSync(path.join(out, 'index.html'), `<!doctype html><html lang="ja"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>UI改善・6枚の試作</title><style>body{margin:0;background:#e9efec;color:#243b38;font:18px/1.7 Meiryo,sans-serif}header,main{max-width:1440px;margin:auto;padding:24px}h1{font-size:30px}a{color:#087f7a}figure{margin:0 0 32px}img{width:100%;height:auto;display:block}figcaption{padding:8px 0} @media print{header,figcaption{display:none}body{background:white}main{padding:0;max-width:none}figure{margin:0;break-after:page}img{width:100vw;height:100vh;object-fit:contain}} @page{size:13.333333in 7.5in;margin:0}</style><header><h1>操作の目的ごとに情報を分けるUI改善</h1><p>5分・6枚の試作。Googleスライドへの取り込み結果は別途確認します。</p><p><a href="${encodeURIComponent(stem)}.pptx">編集用PPTX</a> / <a href="発表者ノート.txt">発表者ノート</a></p></header><main>${deck.map((s, i) => `<figure><img src="preview/slide-${i + 1}.png" alt="${escape(s.title)}"><figcaption>${i + 1} / ${s.seconds}秒 / ${escape(s.title)}</figcaption></figure>`).join('')}</main></html>`);
    await page.goto(pathToFileURL(path.join(out, 'index.html')).href);
    await page.waitForFunction(() => [...document.images].every(i => i.complete && i.naturalWidth > 0));
    await page.pdf({ path: path.join(out, `${stem}_見本.pdf`), printBackground: true, preferCSSPageSize: true });
  } finally { await browser.close(); }
  if (JSON.stringify(protectedState()) !== JSON.stringify(protection)) throw Error('Game or support kit changed');
  fs.writeFileSync(path.join(out, 'validation.json'), JSON.stringify({ date: '2026-11-03', slides: 6, notes: 6, seconds: 300, media: provenance, geometryChecks, layoutChecks, original_files_unchanged: true, pptx_sha256: hash(pptxPath), pptx_bytes: fs.statSync(pptxPath).size, cloud_import: 'PENDING', limitations: ['Local previews use the shared layout model, not Google Slides rendering.', 'No game build or runtime tests performed.', 'Rehearsal timing and first-time usability remain unmeasured.'] }, null, 2));
  console.log(`PASS: 6 editable slides, 6 notes, ${fs.statSync(pptxPath).size} bytes. Google import pending.`);
})().catch(error => { console.error(error); process.exitCode = 1; });
