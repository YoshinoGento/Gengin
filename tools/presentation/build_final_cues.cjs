const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');
const {pathToFileURL} = require('node:url');
const {chromium} = require('playwright');
const root = path.resolve(__dirname, '../..');
const programmer = process.argv.includes('--programmer');
const profile = programmer ? 'programmer' : 'readaloud';
const out = path.join(root, `generated/ui_${profile}_cues_20261105`);
const esc = s => s.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const rich = s => esc(s).replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
const time = n => `${Math.floor(n/60)}:${String(n%60).padStart(2,'0')}`;
const css = `@page{size:A4;margin:0}*{box-sizing:border-box}body{margin:0;background:#e8eded;color:#172c2a;font-family:"BIZ UDPGothic","Yu Gothic","Meiryo",sans-serif;letter-spacing:0}.page{width:210mm;height:297mm;padding:15mm 16mm 14mm;background:white;position:relative;break-after:page;overflow:hidden}.page:last-child{break-after:auto}.top{display:flex;justify-content:space-between;font-size:15px;color:#536561;padding-bottom:12px;border-bottom:2px solid #d8e2de}.count{color:#087f7a;font-weight:700}.heading{font-size:28px;line-height:1.5;margin:20px 0 10px}.cue{font-size:19px;font-weight:700;color:#087f7a;line-height:1.6;margin-bottom:14px}.action{font-size:15px;color:#536561;background:#f1f5f3;padding:11px 14px;line-height:1.65;margin-bottom:20px}.block{margin:0 0 17px}.spoken{font-size:23px;line-height:1.8;margin:0}.stage{font-size:14px;font-weight:700;color:#526963;margin-bottom:3px}.next{position:absolute;bottom:15mm;left:16mm;right:16mm;border-top:1px solid #d8e2de;padding-top:12px;font-size:15px;line-height:1.6;color:#536561}.mini{font-size:16px;color:#536561;line-height:1.7}.quick .heading{margin:14px 0;font-size:26px}.item{padding:13px 0;border-bottom:1px solid #d8e2de;break-inside:avoid}.item h2{font-size:18px;margin:0 0 8px}.item p{font-size:19px;line-height:1.6;margin:0}.item small{display:block;font-size:13px;color:#536561;margin-top:6px}.quick .notice{font-size:15px;background:#f1f5f3;padding:12px;line-height:1.7}.home{max-width:850px;margin:40px auto;padding:28px;background:white;line-height:1.8}.home a{color:#087f7a}@media print{body{background:white}}`;

(async()=>{
 await fs.mkdir(path.join(out,'preview'),{recursive:true});
 const data=JSON.parse(await fs.readFile(path.join(__dirname,profile+'-cues.json'),'utf8'));
 const durations=programmer ? [8,18,18,23,22,17,23,52,45,42,32] : [10,23,18,25,25,18,24,55,42,28,32];
 let cumulative=0;
 data.forEach((s,i)=>{s.number=i+1;s.seconds=durations[i];s.start=cumulative;cumulative+=s.seconds;s.end=cumulative;delete s.image;});
 for(const s of data)for(const b of s.blocks)for(const line of b.lines){if(rich(line).includes('**'))throw new Error('Unpaired emphasis on slide '+s.sourceSlide);}
 if(cumulative!==300 || data.map(s=>s.sourceSlide).join(',')!=='1,2,3,4,5,6,7,8,9,10,12')throw new Error('Invalid timing/order');
 const page=s=>`<section class="page"><div class="top"><span class="count">本番 ${s.number} / 11</span><span>編集画面 ${s.sourceSlide}枚目</span><span>目安 ${time(s.start)}～${time(s.end)}</span></div><h1 class="heading">${esc(s.heading)}</h1><div class="cue">${esc(s.cue)}</div><div class="action">操作メモ・読まない：${esc(s.before)}</div><article>${s.blocks.map(b=>`<div class="block">${b.stage?`<div class="stage">操作メモ（読み上げない）：${esc(b.stage)}</div>`:''}<p class="spoken">${b.lines.map(rich).join('<br>')}</p></div>`).join('')}</article><div class="next">${esc(s.next)}</div></section>`;
 const wrap=(title,body)=>`<!doctype html><html lang="ja"><meta charset="utf-8"><title>${title}</title><style>${css}</style>${body}</html>`;
 await fs.writeFile(path.join(out,'01_本番用カンペ.html'),wrap('本番用カンペ',data.map(page).join('')));
 const quickPage=(items,n)=>`<section class="page quick"><div class="top"><span class="count">要点版 ${n} / 2</span><span>2026年11月6日 / 約5分の目安</span></div><h1 class="heading">${n===1?'前半：何を、どう変えたか':'後半：実演・仕組み・今後'}</h1>${items.map(s=>`<div class="item"><h2>${s.number}. ${esc(s.heading)} <small>編集${s.sourceSlide}枚目 / 終了目安 ${time(s.end)}</small></h2><p>${esc(s.short)}</p>${s.sourceSlide===8?'<small>24秒の動画を含む。確認画面と売却後の残数に合わせて話す。</small>':''}</div>`).join('')}${n===2?'<p class="notice">編集11枚目の「補足A」は本番では読まない。<br>最後は編集12枚目の予定を映したまま終了する。</p>':''}</section>`;
 await fs.writeFile(path.join(out,'02_要点だけ.html'),wrap('要点カンペ',(quickPage(data.slice(0,6),1)+quickPage(data.slice(6),2)).replace('確認画面と売却後の残数に合わせて話す。','動画をご覧ください、と言って再生。再生中は待ち、終了後に結果を話す。')));
 const preflight=`<section class="page"><div class="top"><span class="count">手元用 / 読み上げない</span></div><h1 class="heading">本番前の確認</h1><div class="mini"><p>1. 表紙からスライドショーを開始する。補足Aを直接開いて開始しない。</p><p>2. 7枚目がアイコン化の比較、8枚目が売却動画であることを確認する。</p><p>3. 動画が再生できるか確認し、音量を調整する。予備MP4も事前に開く。</p><p>4. このカンペの黒い本文だけを話す。灰色の操作メモ・段階名は読まない。</p><p>5. 発表全体を一度計測する。5分は配分案で、実際の話す速さは未測定。</p></div><h2 class="heading">動画が再生されないとき</h2><p class="spoken">「動画が再生できないため、<br>確認済みの静止画で説明します。<br>10個中9個を売り、<br>保護した1個が残る流れです。」</p><h2 class="heading">時間が足りないとき</h2><div class="mini"><p>残りのページは「要点だけ」の文章に切り替える。プログラムの図や日程表を一項目ずつ読み上げず、役割分担と前半・後半の方針に絞る。</p><p>数値は表示項目数の比較。操作時間や使いやすさの改善率として説明しない。アイコン比較は異なる時期・状態の実画面で、背景の変化はUI改善の比較対象ではない。</p></div></section>`;
 await fs.writeFile(path.join(out,'03_本番前チェック.html'),wrap('本番前チェック',preflight));
 await fs.writeFile(path.join(out,'本番用原稿_編集用.txt'),data.map(s=>`【本番${s.number} / 編集${s.sourceSlide}枚目】${s.heading}\n目安 ${time(s.start)}～${time(s.end)}\n\n${s.blocks.map(b=>(b.stage?`［${b.stage}：読まない］\n`:'')+b.lines.join('\n').replaceAll('**','')).join('\n\n')}\n\n［次へ：${s.next}］`).join('\n\n--------------------\n\n'));
 await fs.writeFile(path.join(out,'00_ここから開く.html'),wrap('本番用カンペ','<main class="home"><h1>本番用カンペ</h1><p>11月6日の発表用。追加した比較ページを含む通常11枚に対応しています。編集11枚目の補足Aは読みません。</p><p><a href="01_本番用カンペ.pdf">本番用PDF：1スライド1ページ・大きい文字</a></p><p><a href="02_要点だけ.pdf">要点PDF：2ページ</a></p><p><a href="03_本番前チェック.pdf">本番前チェックと動画トラブル時の一言</a></p><p><a href="本番用原稿_編集用.txt">編集用原稿</a></p><p>約5分は予定配分です。8枚目の24秒動画を含みます。<br>Googleスライドの以前のノートには追加前の累計時間が残るため、今回はこのカンペの時刻を使ってください。</p></main>'));
 await fs.writeFile(path.join(out,'speaker-data.json'),JSON.stringify(data,null,2));
 const browser=await chromium.launch({executablePath:'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
 const validations=[];
 try{
  const p=await browser.newPage({viewport:{width:794,height:1123}});
  for(const name of ['01_本番用カンペ','02_要点だけ','03_本番前チェック']){
   await p.goto(pathToFileURL(path.join(out,name+'.html')).href);await p.evaluate(()=>document.fonts.ready);
   const pages=p.locator('.page');const count=await pages.count();
   for(let i=0;i<count;i++){
    const issues=await pages.nth(i).evaluate(el=>{const r=el.getBoundingClientRect();const issues=[];for(const n of el.querySelectorAll('h1,h2,p,.cue,.action,.next,.item,.notice')){const b=n.getBoundingClientRect();if(b.right>r.right+1||b.bottom>r.bottom+1||n.scrollWidth>n.clientWidth+1)issues.push(n.textContent.slice(0,60));} const article=el.querySelector('article');const next=el.querySelector('.next');if(article&&next&&article.getBoundingClientRect().bottom>next.getBoundingClientRect().top-10)issues.push('Body overlaps next instruction');return issues;});
    if(issues.length)throw new Error(name+' '+(i+1)+JSON.stringify(issues));
    await pages.nth(i).screenshot({path:path.join(out,'preview',`${name}_${i+1}.png`)});
   }
   await p.pdf({path:path.join(out,name+'.pdf'),printBackground:true,preferCSSPageSize:true});validations.push({name,pages:count,bounds:'PASS'});
   if(programmer && name==='01_本番用カンペ')await p.pdf({path:path.join(out,'04_9ページだけ.pdf'),pageRanges:'9',printBackground:true,preferCSSPageSize:true});
   if(programmer && name==='01_本番用カンペ')await p.pdf({path:path.join(out,'05_10ページだけ.pdf'),pageRanges:'10',printBackground:true,preferCSSPageSize:true});
   if(programmer && name==='01_本番用カンペ')await p.pdf({path:path.join(out,'06_6ページだけ.pdf'),pageRanges:'6',printBackground:true,preferCSSPageSize:true});
  }
 }finally{await browser.close();}
 const source=await fs.readFile(path.join(root,'generated/ui_icon_comparison_20261105/review/google-final.pptx'));
 await fs.writeFile(path.join(out,'validation.json'),JSON.stringify({sourceExportSha256:crypto.createHash('sha256').update(source).digest('hex'),sourcePhysicalSlides:12,spokenPhysicalOrder:data.map(s=>s.sourceSlide),plannedSeconds:cumulative,videoSeconds:24,spokenCharacters:data.flatMap(s=>s.blocks.flatMap(b=>b.lines)).join('').replaceAll('**','').length,validations,notVerified:['actual rehearsal','classroom media access'],googleNotesChanged:false},null,2));
 console.log(out);
})().catch(e=>{console.error(e);process.exitCode=1});
