const fs = require('node:fs');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const Pptx = require('pptxgenjs');
const {chromium} = require('playwright');
const root = path.resolve(__dirname, '../..');
const prior = path.join(root, 'generated/ui_progress_expanded_20261104');
const out = path.join(root, 'generated/ui_speaker_kit_plan_last_20261104');
const data = JSON.parse(fs.readFileSync(path.join(__dirname, 'progress_speaker_kit.json'), 'utf8'));
const deck = JSON.parse(fs.readFileSync(path.join(prior, 'deck.json'), 'utf8'));
const C = {ink:'213B38',teal:'087F7A',muted:'657773',gold:'926719',pale:'EDF5F2',line:'D4DFDA'};
const W=1600, H=900, PX=120;
for (const d of ['assets','preview','review','video']) fs.mkdirSync(path.join(out,d), {recursive:true});
for (const f of fs.readdirSync(path.join(prior,'assets'))) fs.copyFileSync(path.join(prior,'assets',f),path.join(out,'assets',f));
fs.copyFileSync(path.join(root,'generated/ui_progress_revision_20261103/video/LE3C_28_ヨシノ_ゲント_UI改善_売却操作.mp4'),path.join(out,'video/sale-demo.mp4'));
const esc = s => String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const time = s => `${Math.floor(s/60)}:${String(s%60).padStart(2,'0')}`;
const dayMs = 86400000;
const planStart = Date.parse(data.plan.start + 'T00:00:00Z');
const offset = value => (Date.parse(value + 'T00:00:00Z') - planStart) / dayMs;
const shortDate = value => {const d=new Date(value+'T00:00:00Z');return `${d.getUTCMonth()+1}/${d.getUTCDate()}`;};
const dateRange = r => `${shortDate(r.start)}-${shortDate(r.end)}`;
const planDays = offset(data.plan.end) + 1;
for(const r of data.tasks){
 if(!Number.isInteger(offset(r.start))||offset(r.start)<0||offset(r.end)>=planDays||offset(r.end)<offset(r.start)||!Number.isInteger(r.effortDays)||r.effortDays<1||r.effortDays>offset(r.end)-offset(r.start)+1)throw Error(`Invalid task dates: ${r.id}`);
}
const workDays=data.tasks.filter(r=>r.id!=='B').reduce((n,r)=>n+r.effortDays,0);
const bufferDays=data.tasks.find(r=>r.id==='B').effortDays;
function t(s,value,x,y,w,h,size=28,color=C.ink,bold=false,align='left'){s.elements.push({type:'text',value,x,y,w,h,size,color,bold,align});}
function rect(s,x,y,w,h,fill){s.elements.push({type:'rect',x,y,w,h,fill,stroke:fill,thickness:1});}
function line(s,x,y,w){s.elements.push({type:'line',x,y,w,h:0,color:C.line,arrow:false});}
function replace(s,old,value){const e=s.elements.find(e=>e.type==='text'&&e.value===old);if(!e)throw Error(`Missing text: ${old}`);e.value=value;return e;}
const cover=deck[0];
replace(cover,cover.title,data.title).size=58;cover.title=data.title;
replace(cover,'必要な情報だけを、必要な場面へ',data.subtitle).size=35;
replace(cover,'LE3C 28  吉野絃音',data.identifier).size=21;
const schedule={title:'次の1か月で、UI全体を整える',section:'07 次の計画 / 日単位の工程表',seconds:40,notes:'',elements:[]};
t(schedule,schedule.section,64,30,1472,31,22,C.teal,true);
t(schedule,schedule.title,64,87,1472,72,48,C.ink,true);
t(schedule,'今回：一覧・詳細・売却確認を整理  /  次回：画面間の統一と初見の確認',64,170,1472,42,30,C.teal,true);
rect(schedule,64,235,1472,63,C.pale);
t(schedule,`11/7-12/6の予定案   作業日目安：${workDays}日＋予備${bufferDays}日   S：必須 / A：改善`,84,251,1432,43,27,C.ink,true);
const cols=[64,122,312,390,705];
const widths=[52,185,72,305,395];
['優先','期間','日数','作業','完了条件'].forEach((v,i)=>t(schedule,v,cols[i],322,widths[i],37,24,C.muted,true));
const gx=1116,cell=420/planDays;
t(schedule,'日単位の実施期間（1目盛＝1日）',gx,307,420,25,20,C.muted);
for(const d of [0,6,12,18,24]){
 const date=new Date(planStart+d*dayMs);
 t(schedule,`${date.getUTCMonth()+1}/${date.getUTCDate()}`,gx+d*cell,336,80,28,20,C.muted);
}
line(schedule,64,373,1472);
data.tasks.forEach((r,i)=>{
 const y=390+i*56;
 t(schedule,r.priority,cols[0]+8,y,widths[0]-8,40,25,r.priority==='S'?C.teal:C.gold,true);
 t(schedule,dateRange(r),cols[1],y,widths[1],40,24,C.ink);
 t(schedule,`${r.effortDays}日`,cols[2],y,widths[2],40,24,C.ink);
 t(schedule,r.task,cols[3],y,widths[3],40,25,C.ink,true);
 t(schedule,r.criterion,cols[4],y,widths[4],40,24,C.ink);
 for(let d=0;d<planDays;d++)rect(schedule,gx+d*cell,y+4,cell-1,28,C.pale);
 rect(schedule,gx+offset(r.start)*cell,y+4,(offset(r.end)-offset(r.start)+1)*cell-1,28,r.id==='B'?C.gold:C.teal);
 line(schedule,64,y+51,1472);
});
t(schedule,'次回発表日は未確定。11/15・22・29に進捗を見直し、正式日程に合わせて調整する。',64,791,1472,40,26,C.gold,true);
line(schedule,64,848,1472);
t(schedule,'自分で進める予定案。日数は取り組む日の目安で、終日作業の想定ではない。初見の効果は未測定。',64,862,1350,27,18,C.muted);
t(schedule,'8 / 8',1438,860,98,27,20,C.muted,false,'right');
const supplement=deck[8];supplement.hidden=true;
deck.splice(7,3,supplement,schedule);
let elapsed=0;
for(const s of data.slides){s.start=elapsed;s.end=elapsed+s.seconds;elapsed=s.end;
 const timing=s.seconds?`目安 ${s.seconds}秒 / 累計 ${time(s.start)}-${time(s.end)}`:'補足用 / 本編5分には含めない';
 deck[s.slide-1].notes=`【${timing}】\n\n【話す】\n${s.script}\n\n【操作・注意：読まない】\n${s.operation}\n\n【短くする場合】\n${s.short}`;
}
if(elapsed!==300)throw Error(`Timing: ${elapsed}`);
const mainSlides=data.slides.filter(s=>s.seconds>0);
function svg(s){return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1600" height="900"><defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 Z" fill="#087F7A"/></marker></defs><rect width="1600" height="900" fill="white"/>${s.elements.map(e=>{
 if(e.type==='text')return `<foreignObject x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}"><div xmlns="http://www.w3.org/1999/xhtml" data-text="" style="font:${e.bold?'700':'400'} ${e.size}px/1.18 Meiryo,sans-serif;color:#${e.color};text-align:${e.align};white-space:pre-wrap;letter-spacing:0">${esc(e.value)}</div></foreignObject>`;
 if(e.type==='rect')return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="${e.fill==='none'?'none':'#'+e.fill}" stroke="#${e.stroke}" stroke-width="${e.thickness}"/>`;
 if(e.type==='line')return `<line x1="${e.x}" y1="${e.y}" x2="${e.x+e.w}" y2="${e.y+e.h}" stroke="#${e.color}" stroke-width="2" ${e.arrow?'marker-end="url(#a)"':''}/>`;
 const b=fs.readFileSync(path.join(out,'assets',e.name)).toString('base64'),mime=e.name.endsWith('.png')?'png':'jpeg',c=e.crop;
 return c?`<svg x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" viewBox="${c.x} ${c.y} ${c.w} ${c.h}"><image width="${c.iw}" height="${c.ih}" xlink:href="data:image/${mime};base64,${b}"/></svg>`:`<image x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" xlink:href="data:image/${mime};base64,${b}"/>`;
 }).join('')}</svg>`;}
async function pptx(slides,name){
 const p=new Pptx();p.defineLayout({name:'Wide',width:W/PX,height:H/PX});p.layout='Wide';p.author='吉野絃音';p.title=data.title;p.lang='ja-JP';p.theme={headFontFace:'Meiryo',bodyFontFace:'Meiryo',lang:'ja-JP'};
 for(const s of slides){const page=p.addSlide();page.hidden=!!s.hidden;page.background={color:'FFFFFF'};
  for(const e of s.elements){const g={x:e.x/PX,y:e.y/PX,w:e.w/PX,h:e.h/PX};
   if(e.type==='text')page.addText(e.value,{...g,fontFace:'Meiryo',fontSize:e.size*72/PX,bold:e.bold,color:e.color,margin:0,valign:'top',align:e.align,paraSpaceAfter:0,charSpacing:0});
   else if(e.type==='rect')page.addShape(p.ShapeType.rect,{...g,fill:e.fill==='none'?{color:'FFFFFF',transparency:100}:{color:e.fill},line:{color:e.stroke,width:e.thickness*72/PX}});
   else if(e.type==='line')page.addShape(p.ShapeType.line,{...g,line:{color:e.color,width:1.2,...(e.arrow?{endArrowType:'triangle'}:{})}});
   else {const file=path.join(out,'assets',e.name),c=e.crop;if(c)page.addImage({path:file,x:g.x,y:g.y,w:g.w*c.iw/c.w,h:g.h*c.ih/c.h,sizing:{type:'crop',w:g.w,h:g.h,x:g.w*c.x/c.w,y:g.h*c.y/c.h}});else page.addImage({path:file,...g});}
  }page.addNotes(s.notes);
 }await p.writeFile({fileName:path.join(out,name),compression:true});
}
const css=`@page{size:A4;margin:0}*{box-sizing:border-box}body{margin:0;color:#213b38;background:#e5eae8;font-family:Meiryo,sans-serif;letter-spacing:0}.sheet{width:210mm;height:297mm;padding:13mm 15mm;background:white;margin:12px auto;break-after:page;overflow:hidden;position:relative}.sheet:last-child{break-after:auto}h1{font-size:23px;line-height:1.4;margin:0 0 8px}h2{font-size:19px;line-height:1.45;margin:0 0 8px}h3{font-size:16px;margin:0 0 7px}.meta{font-size:12px;color:#657773;margin-bottom:16px}.cue{border-top:2px solid #087f7a;padding:12px 0;margin-top:12px}.cue p{font-size:17px;line-height:1.65;margin:7px 0}.cue strong{color:#087f7a}.memo{font-size:12px!important;color:#536761;line-height:1.65!important}.talk{height:455px;border-top:2px solid #087f7a;padding-top:14px}.talk .heading{display:flex;gap:16px;align-items:center}.talk img{width:150px;height:85px;object-fit:contain;border:1px solid #dae3de}.talk .script{font-size:16px;line-height:1.85;white-space:pre-line;margin:12px 0}.ops{background:#f0f4f2;padding:9px 12px;font-size:12px;line-height:1.65}.small{font-size:12px}.qa{padding:8px 0;border-top:1px solid #d4dfda}.qa h2{font-size:17px}.qa p{font-size:14px;line-height:1.65;margin:5px 0}.audit{padding:9px 0;border-top:1px solid #d4dfda}.audit h2{font-size:16px}.audit p{font-size:13px;line-height:1.65;margin:4px 0}ul{padding-left:22px;font-size:14px;line-height:1.9}.footer{position:absolute;bottom:9mm;left:15mm;right:15mm;font-size:11px;color:#657773;border-top:1px solid #ddd;padding-top:5px}.warn{background:#f8f3e8;padding:10px;font-size:13px;line-height:1.7}.links a{display:block;padding:12px;color:#087f7a}.schedule{width:100%;border-collapse:collapse;font-size:12px}.schedule th,.schedule td{border-bottom:1px solid #d4dfda;padding:8px;text-align:left}@media print{body{background:white}.sheet{margin:0}}`;
function sheet(title,body,index){return `<section class="sheet"><h1>${esc(title)}</h1><div class="meta">水路農業 / ${esc(data.identifier)} / 11月6日発表 / 11月4日時点</div>${body}<div class="footer">個人用カンペ。操作メモは読まない / ${index}</div></section>`;}
function document(title,sheets){return `<!doctype html><html lang="ja"><meta charset="utf-8"><title>${esc(title)}</title><style>${css}</style><body>${sheets.join('')}</body></html>`;}
const cueSheets=[0,4].map((start,i)=>sheet('本番用カンペ / 要点だけを見る',mainSlides.slice(start,start+4).map(s=>`<article class="cue"><h2>${s.slide}. ${esc(deck[s.slide-1].title)}</h2><div class="meta">${time(s.start)}-${time(s.end)} / ${s.seconds}秒</div><p><strong>${esc(s.cue)}</strong></p><p>${esc(s.short)}</p><p class="memo">${esc(s.slide===9?'日程は予定案。12/6を次回発表日と断定しない。9枚目で締める。8枚目の補足は通常表示しない。':s.slide===6?'24秒の動画込み。開始を目視して、画面の変化に合わせて説明する。':s.operation)}</p></article>`).join(''),`${i+1}/2`));
const scriptSheets=[0,2,4,6].map((start,i)=>sheet('読み上げ原稿 / 話す部分と操作を分ける',mainSlides.slice(start,start+2).map(s=>`<article class="talk"><div class="heading"><img src="preview/slide-${s.slide}.png" alt="${s.slide}枚目"><div><h2>${s.slide}. ${esc(deck[s.slide-1].title)}</h2><div class="meta">${time(s.start)}-${time(s.end)} / ${s.seconds}秒</div></div></div><p class="script">${esc(s.script)}</p><div class="ops"><strong>操作・注意（読まない）</strong><br>${esc(s.operation)}</div></article>`).join(''),`${i+1}/4`));
const reviewSheets=[];
for(let i=0;i<2;i++)reviewSheets.push(sheet('想定質問 / 聞かれた項目だけ答える',data.questions.slice(i*7,i*7+7).map(r=>`<article class="qa"><h2>${esc(r.q)}</h2><p>${esc(r.a)}</p><div class="small">参照スライド：${esc(r.page)}</div></article>`).join(''),`${i+1}/5`));
for(let i=0;i<2;i++)reviewSheets.push(sheet('学校資料との照合',data.audit.slice(i*6,i*6+6).map(r=>`<article class="audit"><h2>${esc(r.rule)} / ${esc(r.status)}</h2><p>${esc(r.finding)}</p><div class="small">資料：スクリーンショット 2026-11-04 ${esc(r.source)}</div></article>`).join(''),`${i+3}/5`));
reviewSheets.push(sheet('本番前の確認と、月間計画の伝え方',`<div class="warn">重要：現状のDrive動画は閲覧側で未確認。学校資料はGIF直接配置／YouTube限定公開を推奨しています。共有設定・公開投稿は変更していません。</div><ul><li>表紙を映して、クラス・番号・氏名を述べる。</li><li>9枚目は今回の実績ではなく、次の約1か月の予定として説明する。</li><li>同じ人の再試行は慣れがある。少人数の結果を全利用者に一般化しない。</li><li>発表用のアカウント・PCで、スライドと動画の閲覧・再生を確認する。</li><li>ローカルMP4と静止画PDFを開いておく。PDFだけでは動作の説明を代替できない。</li><li>動画を含めて2回通し、5分に収まるか測る。8枚目の補足Aは通常のスライドショーでは飛ばし、最後は9枚目の予定で締める。</li></ul><h2>「日数」は、作業する日の目安</h2><p class="small">11/7〜12/6の30日間に、作業日20日と予備2日を配分する予定案です。1日8時間などの終日作業を想定していません。授業や他課題の状況、協力者の日程に合わせて調整します。</p><h2>次回発表日はまだ未確定</h2><p class="small">12/6は計画期間の末日です。発表日が決まれば、比較記録・資料作成・予備日を発表前に配置し直します。現時点で「12/6に発表します」とは言いません。</p><h2>計画がずれたとき</h2><p class="small">11/15・22・29に見直します。「予定していた内容」「実際にできた内容」「残った問題」「次の作業日」を記録します。金額や在庫の不整合、操作不能を優先し、見た目の追加調整は後に回します。</p><h2 style="margin-top:20px">学校資料の確認範囲</h2><p class="small">提供された19画像を確認しました。重複・部分画像を含み、主に学校資料2〜19頁の内容です。未提供の頁、リンク先の全編、正式な採点結果までは確認していません。相互評価は録画公開後に最後まで見てから行います。</p>`, '5/5'));
const scheduleSheets=[
 sheet('日別工程表 / 次の1か月のUI改善',`<div class="warn">${esc(data.plan.basis)}<br>計画期間：2026年11月7日〜12月6日。作業日目安${workDays}日＋予備${bufferDays}日。Sは必須、Aは改善です。</div><table class="schedule"><thead><tr><th>優先</th><th>期間</th><th>作業</th><th>日数</th><th>完了条件</th></tr></thead><tbody>${data.tasks.map(r=>`<tr><td>${r.priority}</td><td style="white-space:nowrap">${dateRange(r)}</td><td>${esc(r.task)}</td><td style="white-space:nowrap">${r.effortDays}日</td><td>${esc(r.criterion)}</td></tr>`).join('')}</tbody></table><h2 style="margin-top:24px">計画の進め方</h2><ul><li>11/15・22・29に予定と実績を比較し、次の具体日を更新する。</li><li>初見確認の協力者とパッドは未確保。用意できない場合は未検証と明示する。</li><li>在庫・金額・保存・戻る操作の不具合を優先し、見た目の追加調整は後に回す。</li><li>全ゲームの完成計画ではなく、UIの統一と確認に範囲を絞る。</li><li>今回の11/6発表の前には、別途カンペ練習と動画の再生確認を行う。</li></ul><p class="small">次回はこの予定表に対する実績を示します。12/6を次回発表日と断定しません。</p>`, '1/2'),
 sheet('日別工程表 / 作業内容と確認方法',data.tasks.map(r=>`<article class="audit"><h2>${esc(r.id)} ${dateRange(r)} / ${esc(r.task)}</h2><p>${esc(r.detail)}</p></article>`).join(''), '2/2')
];
const documents=[['01_本番用カンペ',cueSheets],['02_読み上げ原稿',scriptSheets],['03_想定質問と学校資料チェック',reviewSheets],['04_日別工程表',scheduleSheets]];
for(const [name,pages] of documents)fs.writeFileSync(path.join(out,name+'.html'),document(name,pages));
fs.writeFileSync(path.join(out,'カンペ_編集用.txt'),data.slides.map(s=>`${s.slide}. ${deck[s.slide-1].title}\n${deck[s.slide-1].notes}`).join('\n\n'));
fs.writeFileSync(path.join(out,'deck.json'),JSON.stringify(deck,null,2));
fs.writeFileSync(path.join(out,'speaker-kit.json'),JSON.stringify(data,null,2));
fs.writeFileSync(path.join(out,'00_開く.html'),document('発表準備キット',[sheet('発表準備キット / 予定を最後に配置',`<div class="links">${documents.map(([n])=>`<a href="${n}.pdf">${n}.pdf</a>`).join('')}<a href="カンペ_編集用.txt">カンペの編集用テキスト</a><a href="https://docs.google.com/presentation/d/1AgU2xQB_8ySmiRNc2Pyt2RbAn-v5mzQ0cyTqYNMnwLU/edit">Googleスライド</a></div><h2>使う順番</h2><ul><li>練習は「02 読み上げ原稿」。操作メモは声に出さない。</li><li>本番は「01 本番用カンペ」。累計時間を目安にする。</li><li>質問されたら「03」の該当項目だけ見る。旧補足Bの回答もここに残す。</li><li>9枚目の予定で締める。8枚目の補足Aは通常表示しない。</li><li>「04」は11/7〜12/6の予定案。毎週、予定と実績を分けて更新する。</li></ul><div class="warn">工程は日単位、発表の持ち時間は5分です。本編8枚の時間配分は実測ではありません。24秒動画を含めて練習してください。</div>`, '案内')]));
fs.writeFileSync(path.join(out,'README.txt'),'学校資料と照合した個人用カンペ。予定を最後に配置した版。00_開く.htmlから参照。\n最後の9枚目は11/7〜12/6の予定案。次回発表日は未確定。作業日20日＋予備2日、終日稼働の想定ではない。\n本編8枚＋通常非表示の補足A1枚、全9枚。補足Bは外し、内容は想定質問に残す。予定300秒。実測なし。話す部分と操作・注意は区別。\n予定は毎週見直し、実績と残課題を記録。ゲーム全体の完成計画ではない。\n動画は既存Drive動画を維持。権限変更・公開投稿なし。第三者の閲覧は未確認。\nローカルPPTX/PDFは静止画版。動画本体はvideo/sale-demo.mp4。\n旧キットはui_speaker_kit_month_plan_20261104に保存。\n');
(async()=>{
 await pptx(deck,`${data.identifier}_${data.title}.pptx`);
 const browser=await chromium.launch({channel:'msedge',headless:true});const checks=[];
 try{
  const page=await browser.newPage({viewport:{width:W,height:H},deviceScaleFactor:1});
  for(let i=0;i<deck.length;i++){
   const f=path.join(out,'preview',`slide-${i+1}.svg`);fs.writeFileSync(f,svg(deck[i]));await page.goto(pathToFileURL(f).href);await page.evaluate(()=>document.fonts.ready);
   const expected=deck[i].elements.filter(e=>e.type==='text').length;
   if(await page.locator('[data-text]').count()!==expected||await page.locator('parsererror').count())throw Error(`Slide ${i+1} failed to parse`);
   const overflow=await page.evaluate(()=>[...document.querySelectorAll('[data-text]')].filter(e=>e.scrollWidth>e.parentElement.getBoundingClientRect().width+1||e.scrollHeight>e.parentElement.getBoundingClientRect().height+1).map(e=>e.textContent));
   if(overflow.length)throw Error(`Slide ${i+1} overflow ${overflow}`);
   await page.screenshot({path:path.join(out,'preview',`slide-${i+1}.png`)});checks.push({slide:i+1,textBounds:'PASS'});
  }
  for(const [name,pages] of documents){await page.goto(pathToFileURL(path.join(out,name+'.html')).href);await page.evaluate(()=>document.fonts.ready);await page.waitForFunction(()=>[...document.images].every(i=>i.complete&&i.naturalWidth>0));
   const bad=await page.evaluate(()=>[...document.querySelectorAll('.sheet')].map((s,i)=>({page:i+1,bad:[...s.children].filter(e=>!e.classList.contains('footer')&&e.getBoundingClientRect().bottom>s.querySelector('.footer').getBoundingClientRect().top-4).map(e=>e.tagName)})).filter(r=>r.bad.length));
   if(bad.length)throw Error(`${name} exceeds page: ${JSON.stringify(bad)}`);
   await page.pdf({path:path.join(out,name+'.pdf'),printBackground:true,preferCSSPageSize:true});
   for(let i=0;i<pages.length;i++)await page.locator('.sheet').nth(i).screenshot({path:path.join(out,'preview',`${name}-${i+1}.png`)});
   checks.push({document:name,pages:pages.length,bounds:'PASS'});
  }
  const print=path.join(out,'preview/static-print.html');fs.writeFileSync(print,'<!doctype html><meta charset="utf-8"><style>@page{size:13.333333in 7.5in;margin:0}body{margin:0}section{width:13.333333in;height:7.5in;break-after:page}section:last-child{break-after:auto}img{display:block;width:100%;height:100%}</style>'+deck.map((_,i)=>`<section><img src="slide-${i+1}.png"></section>`).join(''));
  await page.goto(pathToFileURL(print).href);await page.waitForFunction(()=>[...document.images].every(i=>i.complete&&i.naturalWidth>0));await page.pdf({path:path.join(out,'発表用_静止画予備.pdf'),printBackground:true,preferCSSPageSize:true});
 }finally{await browser.close();}
 fs.writeFileSync(path.join(out,'validation-local.json'),JSON.stringify({plan:data.plan,planDays,workDays,bufferDays,plannedSeconds:300,rehearsalMeasured:false,mainScriptCharacters:mainSlides.reduce((n,s)=>n+s.script.replace(/\s/g,'').length,0),checks},null,2));
 console.log('PASS: speaker kit PDFs, text bounds, 9 slides (1 hidden), 300-second planned timing.');
})().catch(e=>{console.error(e);process.exitCode=1;});
