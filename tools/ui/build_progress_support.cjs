// Offline authoring assets only. Never modifies the game or original evidence.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const cp = require('node:child_process');
const { pathToFileURL } = require('node:url');
const { chromium } = require('playwright');
const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'generated/ui_progress_report_20261102');
const sha = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const read = p => JSON.parse(fs.readFileSync(path.join(root, p), 'utf8'));
const ensure = p => fs.mkdirSync(p, { recursive: true });
const esc = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
function filesBelow(dir) {
  if (!fs.existsSync(dir)) return [];
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(e => e.isDirectory()
    ? filesBelow(path.join(dir, e.name)) : [path.join(dir, e.name)]);
}
function protectedState() {
  const code = cp.execFileSync('git', ['ls-files', '-z', '--cached', '--others', '--exclude-standard', 'project'],
    { cwd: root, encoding: 'utf8' }).split('\0').filter(Boolean);
  const roots = ['project/Settings', 'generated/monthly_202610/ready', 'generated/monthly_202610/ready_small',
    'generated/exhibitions/gameover_demo_submission_20261028', 'generated/ui_presentation_20261016',
    'generated/ui_presentation_20261102/presentation', 'generated/ui_trade_20261102/presentation'];
  const all = new Set([...code.map(p => path.join(root, p)), ...roots.flatMap(p => filesBelow(path.join(root, p)))]);
  return Object.fromEntries([...all].filter(p => fs.existsSync(p)).sort().map(p => [path.relative(root, p), sha(p)]));
}
ensure(out);
const protectionPath = path.join(out, 'protected_hashes.json');
const initial = protectedState();
if (fs.existsSync(protectionPath)) {
  if (JSON.stringify(initial) !== JSON.stringify(JSON.parse(fs.readFileSync(protectionPath, 'utf8'))))
    throw Error('Protected game or previous evidence changed since preparation');
} else fs.writeFileSync(protectionPath, JSON.stringify(initial, null, 2));
for (const folder of ['images', 'diagrams', 'editable', 'video', 'review']) ensure(path.join(out, folder));

const menu = 'generated/ui_presentation_20261102/presentation';
const trade = 'generated/ui_trade_20261102/presentation';
const media = [
  ['generated/ui_presentation_20261016/assets/before_original.png', 'images/01_hud_before_20261016.png', '旧HUD・ユーザー提供画像。後画像とは畑・日付・解像度が異なる'],
  ['generated/codex_checks/farm_item_icons_20261019/runtime_hud.png', 'images/02_hud_after_20261019.png', '10月19日の実アイテム型アイコンの記録。11月2日の再撮影ではない'],
  [`${menu}/before/01_farm.png`, 'images/03_menu_before.png', '11月2日メニュー改修前・大会1日目・300G・種0個'],
  [`${menu}/after/01_farm.png`, 'images/04_menu_after.png', '同条件。売却入口の改名より前の記録'],
  [`${menu}/after/06_farm_details.png`, 'images/05_menu_details.png', '残した従来操作の詳細画面'],
  [`${menu}/before/05_settings.png`, 'images/06_settings_before.png', '設定の変更前'],
  [`${menu}/after/05_settings.png`, 'images/07_settings_after.png', '音・カメラ・モードに整理した設定'],
  [`${menu}/after/07_audio.png`, 'images/08_audio.png', 'ゲーム内から開いた音量スライダー'],
  [`${trade}/before/01_shop.jpg`, 'images/09_shop_before.jpg', '11月2日・フリー9日目・300G・各種5個の検証データ'],
  [`${trade}/after/01_shop.jpg`, 'images/10_shop_after.jpg', '種屋の同条件比較。ニンジン購入数1'],
  [`${trade}/before/03_inventory.jpg`, 'images/11_inventory_before.jpg', '収穫記録単位の旧入口。10個うち1個保護'],
  [`${trade}/after/03_sale_overview.jpg`, 'images/12_sale_after.jpg', '同じ検証データ。入口を作物種類単位の売却一覧へ変更'],
  [`${trade}/after/04_sale_confirm.jpg`, 'images/13_sale_confirm.jpg', '9個を1390Gで売る見積もり。保護1個は売らない'],
  [`${trade}/after/07_sale_complete.jpg`, 'images/14_sale_complete.jpg', '取引後。保護した1個が残る。変更前比較には使わない'],
  [`${trade}/after/05_storage_details.jpg`, 'images/15_storage_details.jpg', '品質・サイズ・保護・出品予約を残した詳細画面'],
  ['generated/ui_presentation_20261016/video/03_selection_and_planting_edited.mp4', 'video/hud_history_14s.mp4',
    '過去のHUD操作記録。種選択7秒＋植え付け7秒。中間カットあり、等速、無音。最新の種袋意匠や今回の売買UIの証拠ではない'],
];
const verified = new Map();
for (const base of [menu, trade]) {
  for (const [relative, expected] of Object.entries(read(`${base}/image_hashes.json`)))
    verified.set(`${base}/${relative}`, expected);
}
for (const entry of media) {
  const [source, destination] = entry;
  const from = path.join(root, source);
  if (verified.has(source) && sha(from) !== verified.get(source)) throw Error(`Original capture changed: ${source}`);
  fs.copyFileSync(from, path.join(out, destination));
  if (sha(from) !== sha(path.join(out, destination))) throw Error(`Copy mismatch: ${destination}`);
}

const ink = '#263630', muted = '#586561', teal = '#14756f', gray = '#687878', gold = '#936b20';
const text = (x, y, value, size = 32, color = ink, weight = 400, anchor = 'start') =>
  `<text x="${x}" y="${y}" font-size="${size}" font-weight="${weight}" fill="${color}" text-anchor="${anchor}">${esc(value)}</text>`;
const rect = (x, y, w, h, fill, stroke = 'none') =>
  `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="6" fill="${fill}" stroke="${stroke}" stroke-width="2"/>`;
const arrow = (x1, y1, x2, y2) => `<path d="M${x1},${y1} L${x2},${y2}" fill="none" stroke="${gray}" stroke-width="3" marker-end="url(#arrow)"/>`;
const figures = [];
function figure(name, title, body) {
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="900" viewBox="0 0 1920 900" role="img" aria-label="${esc(title)}">
<title>${esc(title)}</title><defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" fill="${gray}"/></marker></defs>
<rect width="1920" height="900" fill="white"/><g font-family="Meiryo, Yu Gothic, sans-serif" letter-spacing="0">${body}</g></svg>`;
  fs.writeFileSync(path.join(out, `editable/${name}.svg`), svg);
  figures.push({ name, title });
}

let layers = text(60, 80, '情報を消すのではなく、見る場面を分ける', 48, ink, 700);
const layerRows = [
  [190, '常に見る', '道具・種・所持金', 'アイコンと数字で、位置を固定する', teal],
  [370, '操作するときに見る', '買う個数・合計・売る個数', '判断に必要な情報を、操作の近くに置く', '#536d99'],
  [550, '必要なときに詳しく見る', '品質・大きさ・出品予約', '一覧から詳細へ進めるように残す', gold],
];
for (const [y, heading, example, reason, color] of layerRows) {
  layers += `<line x1="60" y1="${y - 34}" x2="1860" y2="${y - 34}" stroke="#d4ded8" stroke-width="2"/>`;
  layers += text(60, y + 28, heading, 34, color, 700) + text(650, y + 12, example, 38, ink, 700)
    + text(650, y + 70, reason, 32, muted);
}
layers += text(60, 770, '例：売却の一覧では金額を見る。作物の品質は「保管・大会の詳細」で見る。', 34)
  + text(60, 837, '詳細へ進む手間は増えるため、初めて触る人が迷わないかは別に確認する。', 30, muted);
figure('01_information_layers', '表示する情報を用途で分ける', layers);

let flow = text(60, 80, '売却ボタンを押したあと、どこが処理する？', 48, ink, 700)
  + text(60, 138, '表示・入力通知と、実際のお金・在庫の変更を分ける', 32, muted);
const nodes = [
  ['1  UI', '表示と入力通知', 'View::Hit', '売却操作のRequestを返す'],
  ['2  Controller', '処理先を選ぶ', 'HandleSaleRequest', '操作をSystemへ渡す'],
  ['3  確認用System', '見積もりを再確認', 'FarmSaleReviewSystem', '表示した条件と照合する'],
  ['4  売買用System', 'お金と在庫を変更', 'FarmEconomySystem', '保護した作物は残す'],
];
nodes.forEach(([heading, purpose, code, detail], i) => {
  const x = 60 + i * 470;
  flow += rect(x, 220, 390, 260, i === 3 ? '#e8f2ee' : '#f2f5f4', '#c4d3cc')
    + text(x + 22, 270, heading, 32, teal, 700)
    + text(x + 22, 332, purpose, 31, ink, 700)
    + text(x + 22, 393, code, 23, muted)
    + text(x + 22, 448, detail, 26);
  if (i < 3) flow += arrow(x + 398, 350, x + 456, 350);
});
flow += text(1425, 322, '一致', 26, teal, 700, 'middle')
  + arrow(1195, 488, 1195, 560)
  + rect(980, 577, 430, 76, '#faf4e8')
  + text(1195, 627, '不一致：売らずに再確認', 30, gold, 700, 'middle')
  + `<path d="M1665,488 V710 H255 V505" fill="none" stroke="${gray}" stroke-width="3" marker-end="url(#arrow)"/>`
  + text(455, 696, '更新した状態から、表示用データを作り直す', 30, muted)
  + text(60, 812, '確認する値：作物・個数・受取額・保護数・所持金・読み込み世代', 29)
  + text(60, 864, '現在のメニュー売却経路の概略。価格計算と保護ルールは既存の処理を再利用。', 27, muted);
figure('02_sale_request_flow', '売却処理の責務分担', flow);

let count = text(60, 80, '一覧の操作を14項目から6項目に整理', 48, ink, 700)
  + text(60, 144, '農作業メニュー本文の操作数', 32, muted);
const originX = 420, unit = 90, max = 14;
count += text(60, 285, '変更前', 42, gray, 700) + rect(originX, 224, unit * 14, 100, gray)
  + text(originX + unit * 14 + 24, 288, '14', 44, ink, 700)
  + text(60, 465, '変更後', 42, teal, 700) + rect(originX, 404, unit * 6, 100, teal)
  + text(originX + unit * 6 + 24, 468, '6', 44, ink, 700);
count += `<line x1="${originX}" y1="570" x2="${originX + unit * max}" y2="570" stroke="#a6b4ae" stroke-width="2"/>`;
for (let n = 0; n <= max; n += 2) count += text(originX + unit * n, 612, n, 28, muted, 400, 'middle');
count += text(1775, 612, '項目', 28, muted)
  + text(60, 719, '機能を削った数ではない。従来の操作は「詳しい操作」に残している。', 34)
  + text(60, 792, '集計：本文のボタン。無効状態を含む。上のタブと下の戻るボタンは除外。', 29, muted)
  + text(60, 852, '操作時間や使いやすさの改善率を示すグラフではない。', 29, muted);
figure('03_menu_count', '農作業メニュー本文の操作数', count);

const titles = ['概要', '課題と整理方針', 'メニューの改善', '種屋・売却の改善', '処理の仕組み', '確認結果と次の課題'];
const seconds = [25, 45, 55, 60, 70, 45];
const deck = titles.map((title, i) => ({ number: i + 1, title, seconds: seconds[i] }));
if (seconds.reduce((a, b) => a + b, 0) !== 300) throw Error('Timing must total 300 seconds');
const cards = media.filter(([s, d]) => d.startsWith('images/')).map(([source, file, description]) =>
  `<figure><a href="${file}"><img src="${file}" alt="${esc(description)}" loading="eager"></a><figcaption><b>${esc(path.basename(file))}</b><br>${esc(description)}</figcaption></figure>`).join('\n');
fs.writeFileSync(path.join(out, 'index.html'), `<!doctype html><html lang="ja"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>水路農業 UI改善・発表準備</title>
<style>body{margin:0;font:16px/1.7 'Yu Gothic UI',Meiryo,sans-serif;letter-spacing:0;color:#263630;background:white}main{max-width:1320px;margin:auto;padding:24px}h1{font-size:30px}h2{font-size:24px}a{color:#126e70}section{border-top:1px solid #ccd8d2;padding-top:20px;margin-top:28px}nav{display:flex;gap:20px;flex-wrap:wrap}.grid{display:grid;grid-template-columns:1fr 1fr;gap:24px}figure{margin:0}img,video{display:block;width:100%;height:auto}figcaption{overflow-wrap:anywhere}table{border-collapse:collapse;width:100%}th,td{padding:10px;text-align:left;border-bottom:1px solid #dbe3df}.note{color:#586561} @media(max-width:850px){main{padding:16px}.grid{grid-template-columns:1fr}h1{font-size:25px}}</style>
<main><h1>操作の目的ごとに情報を分けるUI改善</h1><p>水路農業 / 5分・6枚 / スライドを自分で作るための準備資料</p>
<nav><a href="01_構成と作り方.md">構成と配置</a><a href="02_カンペ.md">発表カンペ</a><a href="03_根拠と質問対策.md">根拠・質問対策</a><a href="04_発表前チェック.md">発表前チェック</a></nav>
<section><h2>話す順番</h2><table><tr><th>枚</th><th>内容</th><th>時間</th></tr>${deck.map(s => `<tr><td>${s.number}</td><td>${s.title}</td><td>${s.seconds}秒</td></tr>`).join('')}<tr><td colspan="2">合計（画像切り替えを含む目安）</td><td>300秒</td></tr></table><p>完成スライドではありません。発表文を全面に貼らず、見出しと画像を中心に組み立てます。</p></section>
<section><h2>図解</h2>${figures.map(f => `<figure><a href="diagrams/${f.name}.png"><img src="diagrams/${f.name}.png" alt="${f.title}"></a><figcaption>${f.title} / <a href="editable/${f.name}.svg">編集用SVG</a></figcaption></figure>`).join('')}</section>
<section><h2>実画面の原画像</h2><p class="note">撮影時期と条件が異なる画像を、同条件の比較として扱わないでください。画像そのものは未加工です。詳しい対応は「根拠・質問対策」にまとめています。</p><div class="grid">${cards}</div></section>
<section><h2>補助動画：過去のHUD操作（14秒）</h2><p>種選択と植え付けの記録です。今回のメニュー・売買改修や最新の種袋の見た目を示す動画ではありません。中間カットあり、等速、無音。本編では省略し、操作連動を質問された場合の補足として使います。</p><video controls preload="metadata" src="video/hud_history_14s.mp4"></video></section></main></html>`);

(async () => {
  const browser = await chromium.launch({ headless: true, channel: 'msedge' });
  const checks = [];
  try {
    const page = await browser.newPage({ viewport: { width: 1920, height: 900 }, deviceScaleFactor: 1 });
    for (const f of figures) {
      await page.goto(pathToFileURL(path.join(out, `editable/${f.name}.svg`)).href);
      await page.evaluate(() => document.fonts.ready);
      const bounds = await page.evaluate(() => [...document.querySelectorAll('text')].map(t => {
        const b = t.getBBox();
        return { text: t.textContent, x: b.x, y: b.y, width: b.width, height: b.height };
      }));
      if (bounds.some(b => b.x < 0 || b.y < 0 || b.x + b.width > 1920 || b.y + b.height > 900))
        throw Error(`Diagram text outside canvas: ${f.name}`);
      for (let a = 0; a < bounds.length; ++a) for (let b = a + 1; b < bounds.length; ++b) {
        const x = bounds[a], y = bounds[b];
        if (x.x < y.x + y.width && y.x < x.x + x.width && x.y < y.y + y.height && y.y < x.y + x.height)
          throw Error(`Diagram text overlaps: ${f.name}: ${x.text} / ${y.text}`);
      }
      await page.screenshot({ path: path.join(out, `diagrams/${f.name}.png`) });
      checks.push({ figure: f.name, labels: bounds.length, bounds: 'PASS' });
    }
    await page.goto(pathToFileURL(path.join(out, 'index.html')).href);
    const links = await page.locator('a').evaluateAll(nodes => nodes.map(a => a.getAttribute('href')));
    for (const link of links) if (!fs.existsSync(path.join(out, link))) throw Error(`Missing local target: ${link}`);
    checks.push({ local_links: links.length, result: 'PASS' });
    await page.waitForFunction(() => [...document.images].every(i => i.complete && i.naturalWidth));
    for (const width of [1440, 900, 375]) {
      await page.setViewportSize({ width, height: 1000 });
      const state = await page.evaluate(() => ({ width: innerWidth, overflow: document.documentElement.scrollWidth > innerWidth,
        images: [...document.images].map(i => ({ width: i.naturalWidth, height: i.naturalHeight })) }));
      if (state.overflow || state.images.some(i => !i.width)) throw Error(`Gallery layout: ${JSON.stringify(state)}`);
      checks.push(state);
    }
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.screenshot({ path: path.join(out, 'review/index.png') });
    const film = page.locator('video');
    await film.scrollIntoViewIfNeeded();
    await film.evaluate(v => v.load());
    await page.waitForFunction(() => document.querySelector('video').readyState >= 2);
    await film.evaluate(v => v.currentTime = 12);
    await page.waitForFunction(() => !document.querySelector('video').seeking);
    await film.evaluate(async v => { v.muted = true; await v.play(); });
    await page.waitForFunction(() => document.querySelector('video').currentTime >= 12.2);
    await film.evaluate(v => { v.pause(); v.controls = false; });
    const duration = await film.evaluate(v => v.duration);
    if (!(duration >= 13.9 && duration <= 14.2)) throw Error(`Unexpected clip duration ${duration}`);
    await film.screenshot({ path: path.join(out, 'review/historical_video.png') });
    checks.push({ historical_video_seconds: duration, metadata_seek_and_playback: 'PASS' });
  } finally { await browser.close(); }
  const final = protectedState();
  if (JSON.stringify(final) !== JSON.stringify(initial)) throw Error('Original game/evidence files were modified');
  const evidence = {
    date: '2026-11-02', purpose: 'Authoring support, not a finished slide deck or submission',
    sources: media.map(([source, destination, note]) => ({ source, destination, sha256: sha(path.join(out, destination)), note })),
    diagrams: figures.map(f => ({ ...f, svg_sha256: sha(path.join(out, `editable/${f.name}.svg`)), png_sha256: sha(path.join(out, `diagrams/${f.name}.png`)) })),
    timing: deck, checks, original_files_unchanged: true,
    test_scope: 'Previous 2026-11-02 build/test/native logs reused; no new game build/test/session in this task',
  };
  fs.writeFileSync(path.join(out, 'evidence.json'), JSON.stringify(evidence, null, 2));
  console.log(`PASS: ${media.length} original media files, ${figures.length} diagrams, 300s, gallery bounds, originals unchanged`);
})().catch(e => { console.error(e); process.exitCode = 1; });
