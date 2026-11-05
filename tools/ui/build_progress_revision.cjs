const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {pathToFileURL} = require('node:url');
const Pptx = require('pptxgenjs');
const {chromium} = require('playwright');
const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'generated/ui_progress_revision_20261103');
const src = path.join(root, 'generated/ui_progress_report_20261102/images');
const W=1600, H=900, PX=120;
const C={ink:'213B38',teal:'087F7A',blue:'42658A',muted:'657773',gold:'926719',pale:'EDF5F2',line:'D4DFDA',white:'FFFFFF'};
for(const d of ['assets','preview','review']) fs.mkdirSync(path.join(out,d),{recursive:true});
const media=['03_menu_before.png','04_menu_after.png','07_settings_after.png','08_audio.png','12_sale_after.jpg','14_sale_complete.jpg'];
const hashes=[];
for(const name of media){fs.copyFileSync(path.join(src,name),path.join(out,'assets',name));hashes.push({name,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(src,name))).digest('hex')});}
for(const name of ['current-hud.png','sale-poster.png']) fs.copyFileSync(path.join(out,'video',name),path.join(out,'assets',name));
const deck=[];
function slide(title,section,seconds,notes){const s={title,section,seconds,notes,elements:[]};deck.push(s);return s;}
function t(s,value,x,y,w,h,size=32,color=C.ink,bold=false,align='left'){s.elements.push({type:'text',value,x,y,w,h,size,color,bold,align});}
function rect(s,x,y,w,h,fill,stroke=fill,thickness=1){s.elements.push({type:'rect',x,y,w,h,fill,stroke,thickness});}
function line(s,x,y,w,h=0,color=C.line,arrow=false){s.elements.push({type:'line',x,y,w,h,color,arrow});}
function img(s,name,x,y,w,h,crop=null){s.elements.push({type:'image',name,x,y,w,h,crop});}
function head(s,sub){t(s,s.section,64,30,1450,31,22,C.teal,true);t(s,s.title,64,87,1472,72,48,C.ink,true);t(s,sub,64,170,1472,40,27,C.muted);}
function foot(s,txt){line(s,64,848,1472);t(s,txt,64,862,1350,27,18,C.muted);t(s,`${deck.length} / 6`,1450,860,86,27,20,C.muted,false,'right');}
function badge(s,number,label,x,y,w){rect(s,x,y,w,72,C.pale);t(s,number,x+15,y+10,48,52,34,C.teal,true);t(s,label,x+72,y+18,w-88,44,28,C.ink,true);}

{
const s=slide('水路農業のUI改善','制作進捗 / 情報の整理',25,'目安25秒。LE3C、28番の吉野絃音です。「水路農業」は、土地と水を整えて野菜を育てるゲームです。操作と説明が増えてきたため、必要な情報を必要な場面で見られるように整理しました。今回は、メニュー、設定、売却の三つの例で、何を変えたかを説明します。\n\n画面は11月2日の実装を別コピーで11月3日に撮影。UI確認用のデータであり、通常プレイの育成成果ではありません。');
t(s,s.section,64,32,1472,32,22,C.teal,true);t(s,s.title,64,90,1472,94,62,C.ink,true);t(s,'必要な情報だけを、必要な場面へ',64,187,1472,56,37,C.teal,true);
img(s,'current-hud.png',64,280,1000,562.5);
t(s,'何を見せるか',1120,291,416,46,31,C.ink,true);
[['01','一覧を整理'],['02','詳細を分ける'],['03','確かめて売る']].forEach((r,i)=>{t(s,r[0],1120,369+i*108,65,50,32,C.teal,true);t(s,r[1],1192,369+i*108,344,60,31,C.ink,true);});
t(s,'土地と水を整え、\n野菜を育てるゲーム',1120,712,416,78,27,C.muted);t(s,'LE3C 28  吉野絃音',1120,808,416,30,23,C.muted);
foot(s,'11月2日実装のUIを11月3日に撮影 / 検証用データ / 全画面の改修完了を示すものではありません。');
}
{
const s=slide('一覧には「今すること」を残す','01 メニュー / ビフォー・アフター',50,'目安50秒。以前は道具や種の切り替えと、種屋や収穫物への入口が同じ一覧にありました。変更後は、まず目的を選び、細かい操作だけを「詳しい操作」で開くようにしました。画面下の数字は、本文に並ぶ操作の数です。14項目から6項目にしていますが、機能を8個削除したという意味ではありません。\n\n実際の操作時間が短くなったとはまだ言えません。詳細を開く手間は増えるので、この分け方が分かりやすいかは初めて触る人に確認する必要があります。比較は11月2日の同一初期状態。無効ボタンを含み、タブと戻るボタンは数えません。変更後画像は売却入口の改名前です。');
head(s,'細かい操作は残したまま、最初に見る選択肢を絞る');
t(s,'変更前：目的と細かい操作が混在',64,238,704,44,29,C.muted,true);t(s,'変更後：目的から選ぶ',832,238,704,44,29,C.teal,true);
img(s,media[0],64,298,704,364.78,{x:0,y:0,w:1282,h:664.249,iw:1282,ih:752});
img(s,media[1],832,298,704,364.78,{x:0,y:0,w:1282,h:664.136,iw:1282,ih:752});
rect(s,151,393,270,180,'none',C.gold,3);rect(s,923,417,508,103,'none',C.teal,3);
t(s,'本文の操作',64,754,230,47,31,C.muted);t(s,'14',315,728,115,96,64,C.muted,true);line(s,454,780,99,0,C.teal,true);t(s,'6',594,728,100,96,64,C.teal,true);t(s,'項目',693,767,75,38,26,C.muted);
t(s,'消さずに「詳しい操作」へ',832,755,704,52,34,C.teal,true);
foot(s,'同じ初期状態で比較。本文ボタンのみ集計（無効を含む、タブ・戻るを除く）。操作時間の短縮率ではありません。');
}
{
const s=slide('設定も、入口と調整を分ける','02 設定 / 必要な内容だけを開く',45,'目安45秒。設定は、サウンド、カメラ、ゲームモードに分けました。音量を変えるときは「音量を変更」を選び、右の画面で調整します。細かい調整を最初から全部並べず、その操作をしているときに見せる考え方です。\n\n音量スライダーそのものを今回新しく作ったという意味ではありません。既存の音量設定を農場メニューから開くようにしました。選択肢を隠しすぎると入口が見つからなくなるため、入口の言葉は「設定」だけでなく「音量を変更」と具体的にしています。画像は実画面の一部を拡大表示しています。');
head(s,'サウンド・カメラ・モードを整理。音量は専用画面で調整する');
badge(s,'1','目的を選ぶ',64,235,640);badge(s,'2','値を調整する',832,235,704);
img(s,media[2],60,322.73,640,361.54,{x:129.9948,y:149.86608,w:1022.0104,h:577.0848,iw:1282,ih:752});
line(s,736,531,60,0,C.teal,true);
img(s,media[3],832,332,704,466.19,{x:319.9872,y:254.30384,w:641,h:424.692,iw:1282,ih:752});
t(s,'「音量を変更」が入口',64,773,640,47,33,C.teal,true);
foot(s,'2026年11月2日の実画面を拡大。既存の音量設定への導線を整理。メニューからの往復はマウスで確認済み。');
}
{
const s=slide('売る前に、結果を確認できる','03 売却 / 操作動画',60,'目安60秒（24秒の動画を含む）。ここでは売却の流れを動画で見せます。まず、売る個数と受取額、保護して残す個数を確認します。次に確認画面で確定します。今回の確認データは10個中1個を保護しているため、売れるのは9個です。確定後に保護した1個が残るところまでを見てください。\n\nこのページに進むと、無音の操作動画が自動再生されます。一覧、確認、完了の各場面を指して説明します。再生できない場合は、事前に開いておいた「UI改善_静止画予備版.pdf」の4ページで説明します。\n\n動画は11月2日実装の別コピーを11月3日に撮影。検証用データで、通常の育成成果ではありません。待ち時間だけ編集し、操作自体は等速です。');
head(s,'一覧 → 確認 → 完了。残る作物も、確定前に示す');
img(s,'sale-poster.png',64,250,1000,562.5);
t(s,'今回の確認データ',1120,258,416,40,28,C.muted,true);
[['所持','10個'],['売る','9個'],['保護して残す','1個']].forEach((r,i)=>{t(s,r[0],1120,330+i*124,416,38,27,C.muted);t(s,r[1],1120,371+i*124,416,68,52,i===2?C.gold:C.teal,true);});
t(s,'受取額 1,390G',1120,738,416,53,32,C.ink,true);
foot(s,'実操作の記録 / 11月3日撮影・11月2日実装 / 検証用データ / 無音・操作等速、待ち時間を一部省略。');
}
{
const s=slide('表示した条件と、実行する条件をそろえる','04 プログラム / 売却の安全性',75,'目安75秒。見た目だけでなく、表示した内容と実際の処理が食い違わないようにしました。UIは操作されたことを通知し、Controllerが確認用Systemへ渡します。確認画面を開いた時点の個数、金額、保護数などを記録し、確定時に現在の値と比べます。変化がなければ売買用Systemへ進み、在庫と所持金を変更します。\n\n例えば確認中に在庫が変わった場合、古い見積もりのまま売らず、もう一度内容を確認させます。価格計算と保護ルールは既存の処理を使い、UI側には入れません。動画の保護1個を残す動作は既存の保護ルールで、今回の確認処理とは区別しています。メニューからの売却が対象で、F/Vキーの即売却は今回変更していません。');
head(s,'UIは通知、確認用Systemは照合、売買用Systemは状態変更を担当');
const xs=[64,448,832,1216];const rows=[['UI','操作を通知','在庫は変更しない'],['Controller','要求を渡す','表示と処理をつなぐ'],['確認用System','見積もりを照合','個数・金額・保護数など'],['売買用System','お金・在庫を変更','既存の計算・保護を再利用']];
for(let i=0;i<4;i++){rect(s,xs[i],276,320,222,i===2?C.pale:'F4F7F6');t(s,rows[i][0],xs[i]+18,303,284,40,28,C.teal,true);t(s,rows[i][1],xs[i]+18,365,284,58,30,C.ink,true);t(s,rows[i][2],xs[i]+18,444,284,39,22,C.muted);if(i<3)line(s,xs[i]+327,388,47,0,C.teal,true);}
t(s,'一致',1154,344,60,32,22,C.teal,true);line(s,992,509,0,56,C.gold,true);
rect(s,787,584,470,75,'F8F3E8');t(s,'変化があれば、売らずに再確認',802,607,440,45,27,C.gold,true,'center');
t(s,'例：確認中に在庫が変わった',64,595,650,50,36,C.ink,true);
rect(s,64,715,1472,105,C.pale);t(s,'画面を整理しても、確認と保護の仕組みは省かない。',86,748,1428,53,37,C.teal,true,'center');
foot(s,'FarmSaleReviewSystem::Confirm → FarmEconomySystem / 対象はメニューからの売却。F/Vキーの即売却は変更していません。');
}
{
const s=slide('改善したことと、次の3日間の予定','05 結果 / 次の予定',45,'目安45秒。今回、一覧と詳細を分け、設定への入口と売却前の確認を整理しました。11月6日の発表に向けて、4日に初めて触る人へ音量変更と売却を試してもらい、完了できたか、迷った場所、押し間違いを記録します。5日に見つかった問題を修正し、同じ課題で再確認します。比較画像とカンペも更新し、動画を含めて5分で話せるか通し練習を行います。6日は発表前に動画と表示を最終確認します。項目を減らしただけで終わらず、実際に見つけて操作できるかまで確かめます。\n\n確認用メモ（読み上げない）：発表日は11月6日で確定しています。これは11月3日時点の予定で、初見確認・修正後の再確認・通し練習はまだ実施していません。5日の作業後に、このスライドと読み上げ原稿を実施結果へ更新してください。未実施の項目を完了として話さないようにします。同じ人の再試行には慣れの影響があるため、時間の変化だけでUI改善の効果と断定しません。別PCは用意できる場合に確認し、できなければ未確認と記載します。');
head(s,'今回の改修と、これから行う確認を分ける');
t(s,'実装・動作確認',64,261,704,50,36,C.teal,true);t(s,'11月6日の発表に向けて',832,261,704,50,36,C.gold,true);line(s,64,327,704,0,C.teal);line(s,832,327,704,0,C.gold);
[['一覧と詳細の分離','メニュー・設定・売却を整理'],['確認してから売却','保護した1個が残る動作を確認'],['既存のルールを維持','価格・育成・保存形式は変更しない']].forEach((r,i)=>{t(s,r[0],64,362+i*126,704,48,33,C.ink,true);t(s,r[1],64,417+i*126,704,38,26,C.muted);});
[['11/4：初見プレイの確認','音量変更・売却の迷いと誤操作を記録'],['11/5：修正・再確認・練習','同じ課題で確認し、比較画像を更新'],['11/6：最終確認・発表','動画と表示を確認し、5分で発表']].forEach((r,i)=>{t(s,r[0],832,362+i*126,704,48,33,C.ink,true);t(s,r[1],832,417+i*126,704,38,26,C.muted);});
t(s,'迷いを記録し、修正してから同じ課題で確かめる。',64,770,1472,53,38,C.teal,true,'center');
foot(s,'11月6日発表に向けた予定 / 初見確認・再確認・通し練習は未実施。');
}
const escape=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
function svg(s){return `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="1600" height="900"><defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 Z" fill="#087F7A"/></marker></defs><rect width="1600" height="900" fill="white"/>${s.elements.map(e=>{
 if(e.type==='text')return `<foreignObject x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}"><div xmlns="http://www.w3.org/1999/xhtml" data-text="1" style="font: ${e.bold?'700':'400'} ${e.size}px/1.18 Meiryo,sans-serif;color:#${e.color};text-align:${e.align};white-space:pre-wrap;letter-spacing:0">${escape(e.value)}</div></foreignObject>`;
 if(e.type==='rect')return `<rect x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" fill="${e.fill==='none'?'none':'#'+e.fill}" stroke="#${e.stroke}" stroke-width="${e.thickness}"/>`;
 if(e.type==='line')return `<line x1="${e.x}" y1="${e.y}" x2="${e.x+e.w}" y2="${e.y+e.h}" stroke="#${e.color}" stroke-width="2" ${e.arrow?'marker-end="url(#a)"':''}/>`;
 const b=fs.readFileSync(path.join(out,'assets',e.name)).toString('base64'),mime=e.name.endsWith('.png')?'png':'jpeg';
 const c=e.crop;
 return c?`<svg x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" viewBox="${c.x} ${c.y} ${c.w} ${c.h}"><image width="${c.iw}" height="${c.ih}" xlink:href="data:image/${mime};base64,${b}"/></svg>`:`<image x="${e.x}" y="${e.y}" width="${e.w}" height="${e.h}" xlink:href="data:image/${mime};base64,${b}"/>`;
}).join('')}</svg>`;}
(async()=>{
 const pptx=new Pptx();pptx.defineLayout({name:'Wide',width:W/PX,height:H/PX});pptx.layout='Wide';pptx.author='吉野絃音';pptx.title='水路農業のUI改善';pptx.lang='ja-JP';pptx.theme={headFontFace:'Meiryo',bodyFontFace:'Meiryo',lang:'ja-JP'};
 for(const [i,s] of deck.entries()){
  const p=pptx.addSlide();p.background={color:C.white};
  for(const e of s.elements){if(e.x<0||e.y<0||e.x+e.w>W||e.y+e.h>H)throw Error(`Outside canvas ${i+1}`);const g={x:e.x/PX,y:e.y/PX,w:e.w/PX,h:e.h/PX};
   if(e.type==='text')p.addText(e.value,{...g,fontFace:'Meiryo',fontSize:e.size*72/PX,bold:e.bold,color:e.color,margin:0,valign:'top',align:e.align,paraSpaceAfter:0,breakLine:false,charSpacing:0});
   else if(e.type==='rect')p.addShape(pptx.ShapeType.rect,{...g,fill:e.fill==='none'?{color:'FFFFFF',transparency:100}:{color:e.fill},line:{color:e.stroke,width:e.thickness*72/PX}});
   else if(e.type==='line')p.addShape(pptx.ShapeType.line,{...g,line:{color:e.color,width:1.2,...(e.arrow?{endArrowType:'triangle'}:{})}});
   else{const file=path.join(out,'assets',e.name);if(e.crop){const c=e.crop;p.addImage({path:file,x:g.x,y:g.y,w:g.w*c.iw/c.w,h:g.h*c.ih/c.h,sizing:{type:'crop',w:g.w,h:g.h,x:g.w*c.x/c.w,y:g.h*c.y/c.h}});}else p.addImage({path:file,...g});}
  }
  p.addNotes(s.notes);fs.writeFileSync(path.join(out,'preview',`slide-${i+1}.svg`),svg(s));
 }
 await pptx.writeFile({fileName:path.join(out,'LE3C_28_ヨシノ_ゲント_UI改善_改訂版.pptx'),compression:true});
 fs.writeFileSync(path.join(out,'deck.json'),JSON.stringify(deck,null,2));fs.writeFileSync(path.join(out,'発表者ノート.txt'),deck.map((s,i)=>`${i+1}. ${s.title}\n${s.notes}`).join('\n\n'));
 const browser=await chromium.launch({channel:'msedge',headless:true});const checks=[];
 try{const page=await browser.newPage({viewport:{width:W,height:H},deviceScaleFactor:1});for(let i=0;i<6;i++){await page.goto(pathToFileURL(path.join(out,'preview',`slide-${i+1}.svg`)).href);await page.evaluate(()=>document.fonts.ready);const bad=await page.evaluate(()=>[...document.querySelectorAll('[data-text]')].filter(e=>e.scrollWidth>e.parentElement.getBoundingClientRect().width+1||e.scrollHeight>e.parentElement.getBoundingClientRect().height+1).map(e=>e.textContent));if(bad.length)throw Error(`Overflow ${i+1}: ${bad}`);await page.screenshot({path:path.join(out,'preview',`slide-${i+1}.png`)});checks.push({slide:i+1,textFit:'PASS'});}}finally{await browser.close();}
 fs.writeFileSync(path.join(out,'validation-local.json'),JSON.stringify({slides:6,seconds:deck.reduce((a,s)=>a+s.seconds,0),notes:6,checks,media:hashes,scope:'Local static deck only. Cloud/video evidence is recorded separately.'},null,2));
 console.log('PASS: revised six-slide local static deck. Cloud/video evidence is separate.');
})().catch(e=>{console.error(e);process.exitCode=1});
