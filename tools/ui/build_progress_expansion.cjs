const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {pathToFileURL} = require('node:url');
const Pptx = require('pptxgenjs');
const {chromium} = require('playwright');
const root = path.resolve(__dirname, '../..');
const previous = path.join(root, 'generated/ui_progress_revision_20261103');
const out = path.join(root, 'generated/ui_progress_expanded_20261104');
const W = 1600, H = 900, PX = 120;
const C = {ink:'213B38',teal:'087F7A',muted:'657773',gold:'926719',pale:'EDF5F2',line:'D4DFDA'};
for (const dir of ['assets','preview','review']) fs.mkdirSync(path.join(out,dir),{recursive:true});
const sources = fs.readdirSync(path.join(previous,'assets')).map(name => [name,path.join(previous,'assets',name)]);
sources.push(['game-title.png',path.join(root,'generated/monthly_202610/media/title.png')]);
const hashes = sources.map(([name,source]) => {
  fs.copyFileSync(source,path.join(out,'assets',name));
  return {name,source:path.relative(root,source),sha256:crypto.createHash('sha256').update(fs.readFileSync(source)).digest('hex')};
});
const old = JSON.parse(fs.readFileSync(path.join(previous,'deck.json'),'utf8'));
function slide(title,section,seconds,notes) { return {title,section,seconds,notes,elements:[]}; }
function t(s,value,x,y,w,h,size=32,color=C.ink,bold=false,align='left') { s.elements.push({type:'text',value,x,y,w,h,size,color,bold,align}); }
function rect(s,x,y,w,h,fill) { s.elements.push({type:'rect',x,y,w,h,fill,stroke:fill,thickness:1}); }
function line(s,x,y,w,h=0,color=C.line,arrow=false) { s.elements.push({type:'line',x,y,w,h,color,arrow}); }
function img(s,name,x,y,w,h,crop=null) { s.elements.push({type:'image',name,x,y,w,h,crop}); }
function head(s,sub) { t(s,s.section,64,30,1450,31,22,C.teal,true);t(s,s.title,64,87,1472,72,48,C.ink,true);t(s,sub,64,170,1472,40,27,C.muted); }
function foot(s,text) { line(s,64,848,1472);t(s,text,64,862,1350,27,18,C.muted); }
function replace(s,from,to) { const e=s.elements.find(x=>x.type==='text'&&x.value===from);if(!e)throw Error(`Missing ${from}`);e.value=to; }

const intro = slide('水路農業：土地と水を整え、野菜を育てる','01 ゲーム紹介 / 何を楽しむか',40,
  '目安40秒。水路農業は、土地の高さと水路を整え、野菜を育てる農業ゲームです。水を多く与えるだけではなく、作物に合う水分や育成条件を考えます。育てた野菜は売って資金にするか、残して品評会に出します。大会モードでは10・20・30日目に品質と大きさの審査を受けます。期限を気にせず遊ぶフリー農業モードもあります。こうした複数の行動を選ぶため、今回は操作の入口を整理しました。\n\n補足：左の画像は10月のタイトル内デモです。通常プレイの育成結果ではありません。採点は品質60点と大きさ40点で、合格基準は大会段階により異なります。今回はルール自体を変更していません。');
head(intro,'土地・水路の工夫を、育った野菜の品質と大きさにつなげる');
img(intro,'game-title.png',64,245,860,483.75);
t(intro,'タイトル内のデモ画面',76,741,830,32,21,C.muted);
t(intro,'プレイヤーが考えること',978,252,558,45,31,C.teal,true);
[['土地と水路を整える','高さや接続を考えて給水する'],['作物に合う状態を保つ','水分などの育成条件を調整する'],['売るか、出品するか選ぶ','資金にする／品評会へ残す']].forEach((r,i)=>{t(intro,r[0],978,322+i*124,558,45,31,C.ink,true);t(intro,r[1],978,373+i*124,558,39,25,C.muted);});
rect(intro,64,787,1472,46,C.pale);t(intro,'大会：10・20・30日目に品質と大きさを審査   /   フリー農業：期限を気にせず育成',80,793,1440,36,25,C.teal,true,'center');
foot(intro,'画像：10月制作記録のタイトルデモ。育成・売却・大会は既存機能で、今回のUI改修で追加したものではありません。');

const policy = slide('操作は消さず、見せる順番を変える','02 課題と方針 / なぜ分けるか',30,
  '目安30秒。以前のメニューには、種屋などの行き先と、道具や種の切り替えが同じ階層に並んでいました。そこで、常に使う道具は画面下に、目的を選ぶ入口はメニューに、細かい操作や調整は必要な画面に分けました。アイコンだけでは意味が伝わらない操作には短い言葉を残します。ただし、詳細を開く一手間は増えるので、入口を迷わず見つけられるかを確認する必要があります。');
head(policy,'行き先・設定値・細かい操作が、同じ一覧に混在していた');
const lanes = [
 ['常に使う','道具・種','プレイ中の画面下','アイテム画像＋選択状態'],
 ['目的を選ぶ','種屋・売却・設定','メニューの一覧','短い言葉で入口を示す'],
 ['必要なときに開く','詳細・数値・確認','専用画面','細かい情報はここへ']
];
lanes.forEach((r,i)=>{const x=64+i*500;rect(policy,x,283,472,66,C.pale);t(policy,r[0],x+20,299,432,46,31,C.teal,true);t(policy,r[1],x+20,392,432,55,37,C.ink,true);t(policy,r[2],x+20,472,432,43,28,C.ink);t(policy,r[3],x+20,535,432,42,25,C.muted);if(i<2)line(policy,x+478,427,20,0,C.teal,true);});
line(policy,64,638,1472);
t(policy,'狙い',64,687,155,49,32,C.teal,true);t(policy,'今の目的と関係ない情報を、一度に見せない',230,687,1306,55,36,C.ink,true);
t(policy,'注意点',64,770,155,47,29,C.gold,true);t(policy,'詳細を開く一手間は増える。初見で入口を見つけられるかを確かめる。',230,770,1306,45,28,C.muted);
foot(policy,'方針の説明です。アイコン化だけを解決策にせず、短い名称・分類・詳細画面を使い分けています。');

const evidence = slide('確認した事実と、まだ判断できないこと','補足 A / 確認範囲',0,
  '質問されたときの補足です。動いたことと、初めて触る人が分かりやすいことは別に扱っています。11月2日のテストログでは、取消、二重確定、確認後の在庫変更、保護・出品予約などを確認しています。192は売却画面の状態を組み合わせたレイアウト検査数で、192人のユーザーテストではありません。実機は別コピーのRelease版でマウス操作を確認しました。初見の迷いや所要時間、実際のキーボード・ゲームパッド、別PCの確認は未実施です。\n\n根拠：generated/ui_trade_20261102/test_farm_trade.log、tools/tests/farm_trade_test.cpp、11月2日の比較画像と11月3日の操作動画。今回の資料編集ではゲームを再テストしていません。');
head(evidence,'実装の安全性・実機の動作・初見の使いやすさを分けて扱う');
const ev=[
 ['自動テスト','取消・二重確定・在庫変更を検査','11/2 ログでPASS','192状態の配置・入力範囲も検査'],
 ['実機の確認','メニュー往復・音量・売却を操作','別コピーのRelease版','10個中9個を売り、保護1個が残る'],
 ['使いやすさ','初見で迷わず完了できるか','まだ未測定','所要時間・迷い・誤操作を記録予定']
];
ev.forEach((r,i)=>{const y=266+i*172;rect(evidence,64,y,240,128,i===2?'F8F3E8':C.pale);t(evidence,r[0],84,y+43,200,49,30,i===2?C.gold:C.teal,true);t(evidence,r[1],340,y+14,1140,50,34,C.ink,true);t(evidence,r[2],340,y+75,320,39,25,C.teal,true);t(evidence,r[3],698,y+75,830,39,25,C.muted);line(evidence,340,y+136,1196);});
t(evidence,'未確認：実キーボード・パッド、別PC、長時間プレイへの影響',64,800,1472,36,26,C.gold,true);
foot(evidence,'根拠：11/2のテストログ・比較記録、11/3の実操作動画。192状態はレイアウト検査数で、利用者数ではありません。');

const tradeoffs = slide('変更した範囲と、設計上の注意点','補足 B / 誤解しやすい点',0,
  '質問されたときの補足です。14から6への変化は一覧の本文ボタン数であり、操作時間の改善率ではありません。機能は削除せず詳細画面に残していますが、辿る手間は増えます。今回追加したのは分類、既存設定への入口、売却前の確認です。音量スライダー、価格計算、作物の保護は既存の処理を利用しました。また、今回の確認画面はメニュー経由の売却が対象で、F/Vキーによる即売却は未変更です。全画面の改修や、全ての売却入口の統一が完了したとは説明しません。');
head(tradeoffs,'今回の成果は表示と確認の整理。使いやすさの評価は今後の課題');
const qa=[
 ['機能を減らしたのか？','削除せず「詳しい操作」へ移動。','一覧は減るが、詳細へ進む手間は増える。'],
 ['どれだけ速くなったのか？','14 → 6は本文のボタン数。','操作時間や達成率の改善はまだ未測定。'],
 ['何を今回作ったのか？','分類・設定への入口・売却前の確認。','音量調整、価格、保護ルールは既存処理。'],
 ['すべての画面・操作が対象？','今回はメニュー・設定・売買を整理。','F/V即売却、個別ダイアログなどは残る。']
];
qa.forEach((r,i)=>{const x=64+(i%2)*768,y=276+Math.floor(i/2)*252;t(tradeoffs,r[0],x,y,704,49,33,C.teal,true);line(tradeoffs,x,y+61,704);t(tradeoffs,r[1],x,y+89,704,49,30,C.ink,true);t(tradeoffs,r[2],x,y+153,704,70,26,C.muted);});
foot(tradeoffs,'比較条件：同じ初期状態のメニュー本文。タブ・戻るを除き、無効ボタンを含む。別PCや全季節の検証結果ではありません。');

old[0].seconds=15;old[0].notes='目安15秒。LE3C、28番の吉野絃音です。今回は水路農業のUI改善について、初めにゲームの内容を説明し、その後でメニュー、設定、売却の三つの例を紹介します。\n\n画像はUI確認用の別コピーを11月3日に撮影したものです。通常プレイの育成成果ではありません。';
old[1].seconds=40;old[1].notes='目安40秒。以前は道具や種の切り替えと、種屋などの入口が同じ一覧にありました。変更後は、まず目的を選び、細かい操作を「詳しい操作」で開く構成にしました。本文のボタンは14から6になりましたが、8個の機能を削除したわけではありません。これは表示項目数の比較であり、操作が速くなったという計測結果ではありません。\n\n比較は11月2日の同じ初期状態。無効ボタンを含み、タブと戻るを除きます。変更後の画像は売却入口の改名前です。';
old[2].seconds=30;old[2].notes='目安30秒。設定は、サウンド、カメラ、ゲームモードに分けました。音量を変えるときは「音量を変更」から専用画面へ進みます。既存のスライダーを作り直すのではなく、農場メニューから開けるようにしました。入口は短く具体的な言葉を使い、詳しい調整値は操作するときに見せています。';
old[3].seconds=50;old[3].notes='目安50秒、24秒の動画を含みます。売却では、売る個数と金額、保護して残す個数を確定前に表示します。検証用の10個のうち1個を保護しているため、売れるのは9個です。確定した後も保護した1個が残るところを見てください。\n\nこのページへ進むと無音の動画が自動再生されます。一覧、確認、完了の順に指して説明します。再生できなければ静止画予備版の6ページを使います。11月2日実装の別コピーを11月3日に撮影。待ち時間だけ編集し、操作は等速です。保護のルール自体は既存処理です。';
old[4].seconds=55;old[4].notes='目安55秒。表示した内容と、確定時の処理が食い違わないようにしました。UIは操作を通知し、Controllerが確認用Systemへ渡します。確認画面を開いた時点の個数、金額、保護数などを記録して、確定時の値と比べます。一致すれば売買用Systemが在庫と所持金を変更します。例えば確認中に在庫が変われば、古い見積もりでは売らず、もう一度確認させます。価格計算と保護は既存処理を使い、UIには入れません。\n\n対象はメニュー経由の売却です。F/Vの即売却は未変更。Controller全体の責務整理が完成したという説明ではありません。';
old[5].seconds=40;
old[5].title='実装したことと、発表までの確認予定';
replace(old[5],'改善したことと、次の3日間の予定',old[5].title);
old[5].notes='目安40秒。今回、一覧と詳細を分け、設定への入口と売却前の確認を整理しました。一方で、初めて触る人にとって分かりやすくなったかは、まだ確認できていません。4日に音量変更と売却を試してもらい、迷った箇所を記録する予定です。5日に修正と再確認、画像更新と通し練習を行い、6日は動画と表示を最終確認して発表します。\n\n11月4日時点の予定で、初見確認・再確認・通し練習の実施報告はまだありません。5日の作業後に、このページと原稿を実際の結果へ更新します。未実施項目は完了と話しません。同じ人の再試行には慣れの影響があります。ここで本編を終え、補足A/Bは質問がある場合だけ開きます。';
const deck=[old[0],intro,policy,old[1],old[2],old[3],old[4],old[5],evidence,tradeoffs];
const sections=['制作進捗 / 情報の整理','01 ゲーム紹介 / 何を楽しむか','02 課題と方針 / なぜ分けるか','03 メニュー / ビフォー・アフター','04 設定 / 必要な内容だけを開く','05 売却 / 操作動画','06 プログラム / 売却の安全性','07 結果 / 次の予定'];
deck.forEach((s,i)=>{
  s.elements=s.elements.filter(e=>!(e.type==='text'&&e.x===1450&&e.y===860));
  if(i<8) {const h=s.elements.find(e=>e.type==='text'&&e.value===s.section);h.value=sections[i];s.section=sections[i];}
  t(s,i<8?`${i+1} / 8`:`補足 ${i===8?'A':'B'}`,1438,860,98,27,20,C.muted,false,'right');
});
const added=[intro,policy,evidence,tradeoffs];
const esc=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
function svg(s){return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1600" height="900"><defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 Z" fill="#087F7A"/></marker></defs><rect width="1600" height="900" fill="white"/>${s.elements.map(e=>{
 if(e.type==='text')return `<foreignObject x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}"><div xmlns="http://www.w3.org/1999/xhtml" data-text="1" style="font:${e.bold?'700':'400'} ${e.size}px/1.18 Meiryo,sans-serif;color:#${e.color};text-align:${e.align};white-space:pre-wrap;letter-spacing:0">${esc(e.value)}</div></foreignObject>`;
 if(e.type==='rect')return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="${e.fill==='none'?'none':'#'+e.fill}" stroke="#${e.stroke}" stroke-width="${e.thickness}"/>`;
 if(e.type==='line')return `<line x1="${e.x}" y1="${e.y}" x2="${e.x+e.w}" y2="${e.y+e.h}" stroke="#${e.color}" stroke-width="2" ${e.arrow?'marker-end="url(#a)"':''}/>`;
 const b=fs.readFileSync(path.join(out,'assets',e.name)).toString('base64'),mime=e.name.endsWith('.png')?'png':'jpeg',c=e.crop;
 return c?`<svg x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" viewBox="${c.x} ${c.y} ${c.w} ${c.h}"><image width="${c.iw}" height="${c.ih}" xlink:href="data:image/${mime};base64,${b}"/></svg>`:`<image x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" xlink:href="data:image/${mime};base64,${b}"/>`;
 }).join('')}</svg>`;}
async function pptx(slides,name){
 const p=new Pptx();p.defineLayout({name:'Wide',width:W/PX,height:H/PX});p.layout='Wide';p.author='吉野絃音';p.title='水路農業のUI改善';p.lang='ja-JP';p.theme={headFontFace:'Meiryo',bodyFontFace:'Meiryo',lang:'ja-JP'};
 for(const s of slides){const page=p.addSlide();page.background={color:'FFFFFF'};
  for(const e of s.elements){if(e.x<0||e.y<0||e.x+e.w>W||e.y+e.h>H)throw Error(`Outside ${s.title}`);const g={x:e.x/PX,y:e.y/PX,w:e.w/PX,h:e.h/PX};
   if(e.type==='text')page.addText(e.value,{...g,fontFace:'Meiryo',fontSize:e.size*72/PX,bold:e.bold,color:e.color,margin:0,valign:'top',align:e.align,paraSpaceAfter:0,charSpacing:0});
   else if(e.type==='rect')page.addShape(p.ShapeType.rect,{...g,fill:e.fill==='none'?{color:'FFFFFF',transparency:100}:{color:e.fill},line:{color:e.stroke,width:e.thickness*72/PX}});
   else if(e.type==='line')page.addShape(p.ShapeType.line,{...g,line:{color:e.color,width:1.2,...(e.arrow?{endArrowType:'triangle'}:{})}});
   else {const file=path.join(out,'assets',e.name),c=e.crop;if(c)page.addImage({path:file,x:g.x,y:g.y,w:g.w*c.iw/c.w,h:g.h*c.ih/c.h,sizing:{type:'crop',w:g.w,h:g.h,x:g.w*c.x/c.w,y:g.h*c.y/c.h}});else page.addImage({path:file,...g});}
  }page.addNotes(s.notes);
 }await p.writeFile({fileName:path.join(out,name),compression:true});
}
(async()=>{
 await pptx(deck,'LE3C_28_ヨシノ_ゲント_UI改善_説明追加版.pptx');
 await pptx(added,'追加4枚_ゲーム紹介と補足.pptx');
 fs.writeFileSync(path.join(out,'deck.json'),JSON.stringify(deck,null,2));
 fs.writeFileSync(path.join(out,'発表者ノート.txt'),deck.map((s,i)=>`${i+1}. ${s.title}\n${s.notes}`).join('\n\n'));
 const browser=await chromium.launch({channel:'msedge',headless:true});const checks=[];
 try {const page=await browser.newPage({viewport:{width:W,height:H},deviceScaleFactor:1});
  for(let i=0;i<deck.length;i++){const file=path.join(out,'preview',`slide-${i+1}.svg`);fs.writeFileSync(file,svg(deck[i]));await page.goto(pathToFileURL(file).href);await page.evaluate(()=>document.fonts.ready);const bad=await page.evaluate(()=>[...document.querySelectorAll('[data-text]')].filter(e=>e.scrollWidth>e.parentElement.getBoundingClientRect().width+1||e.scrollHeight>e.parentElement.getBoundingClientRect().height+1).map(e=>e.textContent));if(bad.length)throw Error(`Overflow ${i+1}: ${bad}`);await page.screenshot({path:path.join(out,'preview',`slide-${i+1}.png`)});checks.push({slide:i+1,textFit:'PASS'});}
  const print=path.join(out,'preview/static-print.html');fs.writeFileSync(print,'<!doctype html><meta charset="utf-8"><style>@page{size:13.333333in 7.5in;margin:0}body{margin:0}section{width:13.333333in;height:7.5in;break-after:page}section:last-child{break-after:auto}img{display:block;width:100%;height:100%;object-fit:contain}</style>'+deck.map((_,i)=>`<section><img src="slide-${i+1}.png"></section>`).join(''));
  await page.goto(pathToFileURL(print).href);await page.waitForFunction(()=>[...document.images].every(x=>x.complete&&x.naturalWidth>0));await page.pdf({path:path.join(out,'UI改善_説明追加版_静止画予備.pdf'),printBackground:true,preferCSSPageSize:true});
 } finally {await browser.close();}
 const seconds=deck.reduce((a,s)=>a+s.seconds,0);if(seconds!==300)throw Error(`Timing ${seconds}`);
 fs.writeFileSync(path.join(out,'validation-local.json'),JSON.stringify({mainSlides:8,supplementSlides:2,plannedSeconds:seconds,rehearsalMeasured:false,checks,media:hashes},null,2));
 console.log('PASS: 8 main + 2 supplemental slides, planned300s, text bounds and static PDF.');
})().catch(e=>{console.error(e);process.exitCode=1});
