const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {pathToFileURL} = require('node:url');
const {chromium} = require('playwright');

const root = path.resolve(__dirname, '../..');
const config = JSON.parse(fs.readFileSync(path.join(__dirname, 'progress_easy_cues.json'), 'utf8'));
const source = path.join(root, config.source);
const out = path.join(root, config.output);
const prior = JSON.parse(fs.readFileSync(path.join(source, 'speaker-kit.json'), 'utf8'));
const deck = JSON.parse(fs.readFileSync(path.join(source, 'deck.json'), 'utf8'));
const esc = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
const plain = s => s.replaceAll('**', '');
const rich = s => esc(s).replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
const stamp = n => `${Math.floor(n / 60)}:${String(n % 60).padStart(2, '0')}`;
const hash = p => crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const expected = prior.slides.filter(s => s.seconds > 0).map(s => s.slide);
if (JSON.stringify(config.slides.map(s => s.sourceSlide)) !== JSON.stringify(expected)) throw Error('Main slide order differs from source');
for (const d of ['assets', 'preview']) fs.mkdirSync(path.join(out, d), {recursive: true});
let elapsed = 0;
const slides = config.slides.map((s, i) => {
  const original = prior.slides.find(p => p.slide === s.sourceSlide);
  if (!deck[s.sourceSlide - 1] || deck[s.sourceSlide - 1].hidden) throw Error('Hidden/missing main slide');
  for (const b of s.blocks) for (const line of b.lines) {
    if ((line.match(/\*\*/g) || []).length % 2) throw Error(`Unpaired emphasis: ${line}`);
  }
  const image = `slide-${s.sourceSlide}.png`;
  fs.copyFileSync(path.join(source, 'preview', image), path.join(out, 'assets', image));
  const result = {...s, number: i + 1, seconds: original.seconds, start: elapsed, end: elapsed + original.seconds, image};
  elapsed = result.end;
  return result;
});
if (elapsed !== 300 || slides.length !== 10) throw Error('Expected ten main cues and planned 300 seconds');
const css = `
@page{size:A4;margin:0}
*{box-sizing:border-box}body{margin:0;background:#e7ecea;color:#192e2b;font-family:"BIZ UDPGothic","BIZ UDGothic","Noto Sans JP",Meiryo,sans-serif;letter-spacing:0}
.sheet{width:210mm;min-height:297mm;padding:13mm 15mm 12mm;margin:16px auto;background:white;break-after:page;position:relative}
.sheet:last-child{break-after:auto}.kicker{font-size:13px;color:#52635f;margin:0 0 10px;line-height:1.6}
.top{display:flex;gap:20px;align-items:flex-start;border-bottom:2px solid #19756c;padding-bottom:16px;margin-bottom:14px}
.top img{width:214px;height:121px;object-fit:contain;border:1px solid #d8e1dd}.toptext{flex:1;min-width:0}
.number{font-size:16px;font-weight:700;color:#19756c}.time{font-size:13px;color:#52635f;margin-top:7px}h1{font-size:25px;line-height:1.45;margin:8px 0 0}h2{font-size:22px;line-height:1.5;margin:0 0 12px}
.cue{font-size:20px;font-weight:700;border-left:4px solid #19756c;padding-left:12px;line-height:1.6;margin:0 0 13px}
.direction{font-size:15px;line-height:1.65;background:#f0f3f2;padding:9px 12px;color:#3d504b;margin:0 0 15px}.direction b{font-weight:700;margin-right:8px}
.spoken{font-size:24px;line-height:1.75;margin:0 0 20px}.spoken p{margin:0 0 17px}.spoken p:last-child{margin-bottom:0}.spoken strong{font-weight:700;color:#075c56}
.stage{display:block;font-size:14px;line-height:1.6;font-weight:700;color:#52635f;margin:0 0 4px}.stage:before{content:'操作 / ';color:#52635f}
.bottom{margin-top:22px;border-top:1px solid #cbd7d2;padding-top:12px}.bottom p{font-size:15px;line-height:1.7;margin:0}.foot{font-size:12px;color:#52635f;margin-top:16px}
.quick .entry{border-top:1px solid #cbd7d2;padding:16px 0 13px}.entry h2{font-size:20px;margin:0 0 7px}.entry p{font-size:19px;line-height:1.75;margin:0}.entry .cue-small{font-size:14px;color:#52635f;margin-top:5px}
.qa{border-top:1px solid #cbd7d2;padding:15px 0}.qa h2{font-size:20px;margin:0 0 8px}.qa p{font-size:19px;line-height:1.8;margin:0}.qa .label{color:#19756c;font-weight:700}
.notice{border-left:4px solid #9b6a1e;padding:10px 14px;background:#faf6ec;font-size:18px;line-height:1.8;margin:12px 0 22px}
.check{font-size:19px;line-height:1.8;padding-left:24px}.check li{margin:10px 0}.links a{display:block;color:#075c56;padding:14px 0;font-size:20px;text-decoration-thickness:1px}
.quick .foot{margin-top:10px}.print-only{display:none}
@media print{body{background:white}.sheet{height:297mm;min-height:0;margin:0}.screen-only{display:none}.print-only{display:block}}
@media screen and (max-width:810px){.sheet{width:100%;padding:20px;min-height:0}.top{gap:12px}.top img{width:32%;height:auto}.spoken{font-size:22px}.spoken br{display:none}h1{font-size:23px}.cue{font-size:18px}.number{font-size:15px}}
`;
function document(title, content) {
  return `<!doctype html><html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>${esc(title)}</title><style>${css}</style></head><body>${content}</body></html>`;
}
function section(title, body, klass = '') {
  return `<section class="sheet ${klass}"><p class="kicker">水路農業 / 個人用カンペ / 2026年11月6日発表</p><h1 style="margin-bottom:18px">${esc(title)}</h1>${body}</section>`;
}
const fullPages = slides.map(s => `<section class="sheet main">
<p class="kicker">水路農業 / ${esc(prior.identifier)} / 2026年11月6日発表</p>
<header class="top"><div class="toptext"><div class="number">本編 ${String(s.number).padStart(2,'0')} / 10</div><h1>${esc(s.heading)}</h1><div class="time">目安 ${s.seconds}秒 / 累計 ${stamp(s.start)}-${stamp(s.end)}${s.sourceSlide===7?'（24秒の動画を含む）':''}</div></div><img src="assets/${s.image}" alt="対応するスライド"></header>
<p class="cue">${esc(s.cue)}</p><div class="direction"><b>読む前に</b>${esc(s.before)}</div>
<div class="spoken">${s.blocks.map(b=>`<p>${b.stage?`<span class="stage">${esc(b.stage)}（読まない）</span>`:''}${b.lines.map(rich).join('<br>')}</p>`).join('')}</div>
<div class="bottom"><p><b>次の動き：</b>${esc(s.next)}</p></div>
<div class="foot">大きな本文だけを話す。操作メモは読まない。${s.number===10?'編集画面では11枚目。補足Aは本番原稿から除外。':'時間は予定配分。実際にかかった時間ではありません。'}</div>
</section>`);
const quickPages = [0,5].map((offset,index) => section(`要点だけ見る / ${index+1} / 2`, `<p class="kicker">全文を読む必要がないとき用。本文と同じ順番です。</p>${slides.slice(offset,offset+5).map(s=>`<article class="entry"><h2>${String(s.number).padStart(2,'0')} ${esc(s.heading)}</h2><p>${esc(s.short)}</p><p class="cue-small">${stamp(s.start)}-${stamp(s.end)} / ${s.seconds}秒${s.sourceSlide===7?'・動画込み':''}</p></article>`).join('')}<p class="foot">目安5分。補足Aは含めず、最後は「これからの予定」で終了します。</p>`, 'quick'));
const questions = prior.questions.map(q => ({question:q.q, answer:q.a}));
const supplement = [
  section('補足A / 聞かれたときだけ答える', `<p class="notice">このページは本番5分の原稿には含めません。<br>手元で確認するための資料です。</p><article class="qa"><h2>どこまで確認したか</h2><p>11月2日のログでは、取消・二重確定・確認後の在庫変更などの自動テストが通っています。別コピーのRelease版では、マウスによるメニュー往復・音量変更・売却を確認しています。</p></article><article class="qa"><h2>「192状態」の意味</h2><p>売却画面の配置や入力範囲を調べるための、状態の組み合わせ数です。<span class="label">192人に試してもらった、という意味ではありません。</span></p></article><article class="qa"><h2>まだ判断できないこと</h2><p>初めて遊ぶ人の操作時間や達成率は、まだ測定していません。実キーボード・パッド、別PC、長時間プレイへの影響も、今回の検証範囲外です。</p></article><p class="foot">以前の記録に基づく説明です。今回のカンペ作成で、ゲームの再検証はしていません。</p>`),
  ...[0,4,8].map((offset,i)=>section(`想定質問 / ${i+1} / 3`,questions.slice(offset,offset+(i===2?5:4)).map(q=>`<article class="qa"><h2>Q. ${esc(q.question)}</h2><p>${esc(q.answer)}</p></article>`).join(''))),
  section('本番前の準備 / 読み上げない', `<h2>スライドを進める順番</h2><p class="notice">本編1〜9 → 「これからの予定」で終了。<br>現在のGoogleスライドには補足Aが残っています。通常の順送りでは飛びますが、補足Aを選んで開始すると表示されます。必ず表紙から開始してください。</p><h2>動画が再生できないとき</h2><ol class="check"><li>発表前に、学校で使うPC・アカウントで再生を確認する。</li><li>ローカルの24秒MP4も開いておく。</li><li>再生できなければ、「動画が再生できないため、確認済みの静止画で説明します」と伝える。</li></ol><h2>練習で確認すること</h2><ul class="check"><li>大きな本文だけを読む。灰色の操作メモは読まない。</li><li>動画を含めて通し、5分に収まるか実測する。配分は目安。</li><li>長くなったら要点版に切り替える。早口で詰め込まない。</li><li>12月6日は計画期間の末日。次の発表日とは言わない。</li></ul>`)
];
const documents = [
  {name:'01_読みやすいカンペ', pages:fullPages},
  {name:'02_要点だけのカンペ', pages:quickPages},
  {name:'03_質問用メモと本番前チェック', pages:supplement}
];
for (const d of documents) fs.writeFileSync(path.join(out, d.name+'.html'), document(d.name,d.pages.join('')));
fs.writeFileSync(path.join(out,'00_最初に開く.html'),document('発表用カンペ',section('発表用カンペ', `<p class="notice">本編10ページ。1スライドにつき1ページです。<br>補足Aは読み上げ原稿に入れていません。Googleスライド本体は変更していません。</p><div class="links">${documents.map(d=>`<a href="${d.name}.pdf">${d.name} / PDF</a><a href="${d.name}.html">${d.name} / 画面で読む</a>`).join('')}<a href="読み上げ原稿_編集用.txt">編集用テキスト</a><a href="https://docs.google.com/presentation/d/1AgU2xQB_8ySmiRNc2Pyt2RbAn-v5mzQ0cyTqYNMnwLU/edit">Googleスライド</a></div><p class="foot">24秒の動画を含め、5分の配分で作成。通し練習の実測はまだありません。</p>`)));
const editText = slides.map(s=>`===== 本編 ${s.number} / 10：${s.heading} =====\n目安 ${s.seconds}秒 / 累計 ${stamp(s.start)}-${stamp(s.end)}\n\n${s.blocks.map(b=>(b.stage?`【${b.stage}：この見出しは読まない】\n`:'')+b.lines.map(plain).join('\n')).join('\n\n')}\n\n【次の動き・読まない】${s.next}`).join('\n\n');
fs.writeFileSync(path.join(out,'読み上げ原稿_編集用.txt'),editText);
fs.writeFileSync(path.join(out,'speaker-data.json'),JSON.stringify(slides,null,2));

async function validate(page, name, count, mobile = false) {
  const result = await page.evaluate(() => {
    const sheets = [...document.querySelectorAll('.sheet')];
    const pageLimit = 297 * 96 / 25.4;
    return {
      count: sheets.length,
      brokenImages: [...document.images].filter(i=>!i.complete||!i.naturalWidth).length,
      horizontal: [...document.querySelectorAll('.sheet *')].filter(e=>e.scrollWidth>e.clientWidth+2&&e.clientWidth>0).map(e=>e.tagName+':'+e.textContent.slice(0,50)),
      pages: sheets.map((s,i)=>({page:i+1,height:s.getBoundingClientRect().height,limit:pageLimit})),
      documentOverflow:document.documentElement.scrollWidth>window.innerWidth
    };
  });
  if (result.count!==count||result.brokenImages||result.horizontal.length||result.documentOverflow||(!mobile&&result.pages.some(s=>s.height>s.limit+1))) throw Error(`${name}: ${JSON.stringify(result)}`);
  return result;
}
(async()=>{
  const browser = await chromium.launch({channel:'msedge',headless:true});
  const checks = [];
  try {
    const page = await browser.newPage({viewport:{width:1100,height:1250},deviceScaleFactor:1});
    for (const d of documents) {
      await page.goto(pathToFileURL(path.join(out,d.name+'.html')).href);
      await page.evaluate(()=>document.fonts.ready);
      await page.waitForFunction(()=>[...document.images].every(i=>i.complete&&i.naturalWidth>0));
      checks.push({document:d.name,desktop:await validate(page,d.name,d.pages.length)});
      await page.pdf({path:path.join(out,d.name+'.pdf'),printBackground:true,preferCSSPageSize:true});
      for (let i=0;i<d.pages.length;i++) await page.locator('.sheet').nth(i).screenshot({path:path.join(out,'preview',`${d.name}-${i+1}.png`)});
    }
    await page.setViewportSize({width:390,height:844});
    await page.goto(pathToFileURL(path.join(out,documents[0].name+'.html')).href);
    await page.evaluate(()=>document.fonts.ready);
    checks.push({mobile:await validate(page,'main mobile',10,true)});
    await page.screenshot({path:path.join(out,'preview','mobile-main.png')});
  } finally {await browser.close();}
  const spokenChars=slides.reduce((n,s)=>n+s.blocks.reduce((m,b)=>m+b.lines.map(plain).join('').length,0),0);
  fs.writeFileSync(path.join(out,'validation.json'),JSON.stringify({
    sourceDeck:config.source, sourceDeckSha256:hash(path.join(source,'deck.json')),
    sourceMapping:slides.map(s=>({cue:s.number,physicalSlide:s.sourceSlide})),
    plannedSeconds:elapsed,videoSeconds:24,rehearsalMeasured:false,spokenCharacters:spokenChars,
    googleDeckModified:false,supplementIncludedInMain:false,checks
  },null,2));
  console.log(JSON.stringify({result:'PASS',output:out,mainPages:10,quickPages:2,referencePages:supplement.length,spokenCharacters:spokenChars,plannedSeconds:elapsed},null,2));
})().catch(e=>{console.error(e);process.exitCode=1;});
