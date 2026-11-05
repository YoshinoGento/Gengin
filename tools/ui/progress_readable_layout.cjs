module.exports = function makeReadable(prior, data) {
 const C={ink:'213B38',teal:'087F7A',muted:'657773',gold:'926719',pale:'EDF5F2',line:'D4DFDA'};
 const text=(s,value,x,y,w,h,size=32,color=C.ink,bold=false,align='left')=>s.elements.push({type:'text',value,x,y,w,h,size,color,bold,align});
 const rect=(s,x,y,w,h,fill=C.pale)=>s.elements.push({type:'rect',x,y,w,h,fill,stroke:fill,thickness:1});
 const line=(s,x,y,w,color=C.line,arrow=false)=>s.elements.push({type:'line',x,y,w,h:0,color,arrow});
 const image=(s,name,x,y,w,h,crop=null)=>s.elements.push({type:'image',name,x,y,w,h,crop});
 const page=(title,section)=>{const s={title,section,elements:[]};text(s,section,64,30,1472,34,22,C.teal,true);text(s,title,64,87,1472,80,48,C.ink,true);return s;};
 const footer=(s,value)=>{line(s,64,848,1472);text(s,value,64,862,1340,27,18,C.muted);};
 const clone=s=>JSON.parse(JSON.stringify(s));

 const cover={title:data.title,section:'制作進捗 / 水路農業',elements:[]};
 text(cover,'水路農業 / UI改善',96,125,1408,60,36,C.teal,true);
 text(cover,'目的別メニューと\n売却前の確認',96,285,1408,200,72,C.ink,true);
 line(cover,96,567,1408);
 text(cover,data.identifier,96,626,1408,55,32,C.ink,true);
 text(cover,'2026年11月6日',96,699,1408,48,28,C.muted);

 const game=page('土地と水を整え、野菜を育てる','ゲーム紹介 / 水路農業');
 image(game,'game-title.png',64,236,1000,562.5);
 [['整える','土地の高さと水路'],['育てる','作物に合う水分'],['選ぶ','売却か、品評会への出品']].forEach((v,i)=>{
  const y=268+i*165; text(game,v[0],1112,y,424,55,40,C.teal,true);text(game,v[1],1112,y+64,424,65,29);
 });
 footer(game,'タイトル内のデモ画面。育成・売却・大会は既存機能。今回の改修対象はUIです。');

 const problem=page('目的と細かい操作が、同じ一覧に混在','課題 / 変更前');
 const oldImage=prior[3].elements.find(e=>e.type==='image');
 image(problem,oldImage.name,64,259,930,482,oldImage.crop);
 [['行き先','種屋・収穫物'],['道具の選択','クワ・じょうろ'],['個別の操作','種の切り替え・売却']].forEach((v,i)=>{
  const y=285+i*145;text(problem,v[0],1050,y,486,48,34,C.gold,true);text(problem,v[1],1050,y+56,486,48,30);
 });
 footer(problem,'変更前のメニュー。次のページでは、操作の種類に合わせた配置方針を示します。');

 const policy=page('操作は消さず、置き場所を分ける','改善方針 / 情報の整理');
 [['常に使う','道具・種','プレイ中の画面下'],['目的を選ぶ','種屋・売却・設定','メニューの一覧'],['必要なときに開く','詳細・数値・確認','専用画面']].forEach((v,i)=>{
  const x=64+i*500;rect(policy,x,280,472,68);text(policy,v[0],x+20,294,432,48,32,C.teal,true);
  text(policy,v[1],x+20,405,432,60,37,C.ink,true);text(policy,v[2],x+20,501,432,50,31);
 });
 line(policy,64,648,1472);
 text(policy,'詳細を開く一手間は増えるため、入口の見つけやすさを確認する。',64,700,1472,70,31,C.gold,true);
 footer(policy,'アイコンだけに置き換えず、意味が曖昧になる操作には短い言葉を残しています。');

 const menu=clone(prior[3]),settings=clone(prior[4]),video=clone(prior[5]);
 const flow=page('見積もりを照合してから、在庫を変更する','プログラム / 通常の売却');
 [['UI','操作を通知'],['Controller','要求を仲介'],['確認用System','見積もりを照合'],['売買用System','在庫・所持金を変更']].forEach((v,i)=>{
  const x=64+i*384;rect(flow,x,315,320,233);text(flow,v[0],x+18,344,284,50,31,C.teal,true);text(flow,v[1],x+18,440,284,80,30,C.ink,true);
  if(i<3)line(flow,x+330,425,43,C.teal,true);
 });
 text(flow,'UIは表示と入力通知。実際の状態変更はSystemが担当する。',64,677,1472,70,36,C.teal,true);
 footer(flow,'メニューからの売却が対象。価格計算・保護ルールは既存処理を再利用しています。');

 const changed=page('確認中に条件が変わったら、売らない','プログラム / 状態が変わった場合');
 [['確認画面を開く','個数・金額・保護数を記録'],['確定するとき','現在の値と照合'],['一致しない場合','古い見積もりでは確定しない']].forEach((v,i)=>{
  const x=64+i*500;rect(changed,x,292,472,220,i===2?'F8F3E8':C.pale);text(changed,v[0],x+24,323,424,58,36,i===2?C.gold:C.teal,true);text(changed,v[1],x+24,420,424,78,29);
  if(i<2)line(changed,x+478,395,16,C.teal,true);
 });
 text(changed,'売却しない → 新しい内容で再確認',64,642,1472,84,47,C.teal,true,'center');
 footer(changed,'在庫を変更するケースは自動テストで確認。通常プレイでの発生頻度を示すものではありません。');

 const evidence=clone(prior[7]);evidence.hidden=true;
 const schedule=page('これからの予定：UI全体の統一と確認','次の1か月 / 11月7日〜12月6日の予定案');
 text(schedule,'前半は画面の統一、後半は操作の確認と記録',64,183,1472,60,34,C.teal,true);
 text(schedule,'期間',64,284,240,42,25,C.muted,true);text(schedule,'取り組むこと',330,284,700,42,25,C.muted,true);text(schedule,'11月7日 → 12月6日',1116,284,420,42,23,C.muted,true);
 const start=Date.parse(data.plan.start+'T00:00:00Z'),day=86400000,cell=14;
 data.tasks.forEach((r,i)=>{
  const y=348+i*62;const md=d=>{const a=d.split('-');return `${Number(a[1])}/${Number(a[2])}`;};
  text(schedule,`${md(r.start)}-${md(r.end)}`,64,y,246,44,30);text(schedule,r.task,330,y,744,44,32,C.ink,true);
  for(let d=0;d<30;d++)rect(schedule,1116+d*cell,y+6,cell-1,28,C.pale);
  const first=(Date.parse(r.start+'T00:00:00Z')-start)/day,count=(Date.parse(r.end+'T00:00:00Z')-start)/day-first+1;
  rect(schedule,1116+first*cell,y+6,count*cell-1,28,r.id==='B'?C.gold:C.teal);line(schedule,64,y+52,1472);
 });
 text(schedule,'次回発表日は未確定。11/15・22・29に予定と実績を見直す。',64,799,1472,42,28,C.gold,true);
 footer(schedule,'個人作業の予定案。1目盛＝1日。日数・優先度・完了条件は別紙「日別工程表」に記載。');

 const deck=[cover,game,problem,policy,menu,settings,video,flow,changed,evidence,schedule];
 const original=data.slides;const take=(i,slide,seconds,changes={})=>({...original[i-1],slide,seconds,...changes});
 data.slides=[
  take(1,1,10,{cue:'氏名 → テーマ',script:'LE3C、28番の吉野絃音です。水路農業の「目的別メニューと売却前の確認」について発表します。',short:'水路農業の、目的別メニューと売却前の確認について説明します。'}),
  take(2,2,30,{script:'水路農業は、土地の高さと水路を整えて、野菜を育てるゲームです。作物に合う水分などの育成条件を考え、収穫した野菜を売るか、品評会に出すかを選びます。品評会では品質と大きさが審査されます。今回は、こうした行動を選ぶためのUIを改善しました。',operation:'右の三項目を順に説明。大会の日程とフリー農業の細かい説明は質問時に回す。画像はタイトル内のデモで、通常プレイの育成結果ではない。'}),
  {slide:3,seconds:20,cue:'変更前の一覧 → 操作の種類が混在',script:'変更前は、種屋などの行き先と、道具や種の切り替え、売却などが同じ一覧に並んでいました。目的の違う操作が一度に見えることを課題として、整理を進めました。',operation:'左の変更前画像と右の分類を対応させる。初見の操作時間を測定済みとは言わない。',short:'行き先、道具、細かい操作が同じ一覧に混在していたため、整理しました。'},
  take(3,4,25,{script:'常に使う道具は画面下に、目的を選ぶ入口はメニューに、詳しい情報は専用画面に分けました。すべてをアイコンにはせず、意味が曖昧になる操作には短い言葉を残しています。詳細を開く一手間は増えるため、入口の見つけやすさは今後確認します。'}),
  take(4,5,35),take(5,6,25),
  take(6,7,45,{operation:original[5].operation.replace('50秒','45秒')}),
  take(7,8,35,{cue:'UI通知 → 仲介 → 見積照合 → 状態変更',script:'プログラムでは、表示と状態変更の役割を分けました。UIは操作を通知し、Controllerが要求を仲介します。確認用Systemは、表示した見積もりと確定時の値を照合します。一致した場合だけ、売買用Systemが在庫と所持金を変更します。UIに価格計算や在庫変更の処理は持たせていません。',operation:'図を左から右に説明。今回は正常な流れだけを説明し、条件が変わる場合は次のページへ。対象はメニュー売却で、F/V即売却は未変更。',short:'UIは通知、確認用Systemは照合、売買用Systemは在庫と所持金の変更を担当します。'}),
  {slide:9,seconds:35,cue:'確認時と確定時が違う → 売らずに再確認',script:'例えば、確認画面を開いた後に在庫が変わると、表示した内容のまま売ってよいとは限りません。そこで、確定時に個数、金額、保護数などを照合します。違いがあれば売却せず、新しい内容でもう一度確認する流れにしました。このケースは自動テストで確認していますが、通常操作での発生頻度を測ったものではありません。',operation:'処理の流れと例外を混ぜず、このページでは再確認する理由に絞る。具体的な件数や金額を架空の実測結果として追加しない。',short:'確認後に条件が変わったら売却せず、新しい内容で再確認します。'},
  take(8,10,0),take(9,11,40,{operation:original[8].operation.replaceAll('8枚目','10枚目')})
 ];
 const map={1:1,2:2,3:4,4:5,5:6,6:7,7:8,8:10,9:11};
 for(const q of data.questions)q.page=q.page.replace(/\b[1-9]\b/g,n=>map[n]);
 data.questions.find(q=>q.q.startsWith('確認中に')).page='9 / 10';
 for(const a of data.audit){a.finding=a.finding.replace(/([1-9])枚目/g,(_,n)=>`${map[n]}枚目`).replace('40秒枠','30秒枠');}
 data.audit.find(a=>a.rule.includes('ガント')).finding+='投影する最終頁は期間・作業・バーに絞り、工数・優先度・完了条件は手元の日別工程表に残す。';
 let n=0;
 for(const s of deck){
  if(s!==cover){
   const captions=s.elements.filter(e=>e.type==='text'&&e.y>=850&&e.x<1400&&!/^\d+ \/ \d+$/.test(e.value)&&e.value!=='補足A');
   s.referenceNote=captions.map(e=>e.value).join('\n');
   s.elements=s.elements.filter(e=>!captions.includes(e));
  }
  s.elements=s.elements.filter(e=>!(e.type==='text'&&/^\d+ \/ \d+$/.test(e.value)));
  const section=s.elements.find(e=>e.type==='text'&&e.y===30);if(section)section.value=section.value.replace(/^\d+ /,'');
  if(!s.hidden){n++;text(s,`${n} / 10`,1418,860,118,27,20,C.muted,false,'right');}
 }
 return deck;
};
