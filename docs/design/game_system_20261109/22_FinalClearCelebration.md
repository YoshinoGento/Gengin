# 最終大会のクリア演出

## 流れ

30日目の新しい出品が合格したら、短い期待の間からクリア演出を自動開始する。
結果を読んでから「大会クリアへ」を押す旧導線は廃止。3D会場・野菜・木製看板・勲章を共有し、画面を切り替えず結果につなげる。
最初の1秒は野菜を中央に見せ、加速するオリジナルのドラムロールを鳴らす。まだクリア文字・入賞看板・勲章・紙吹雪を出さない。
1秒で72pxの「大会クリア」と4.6秒の祝福音を一度だけ出し、勲章・紙吹雪を同じ時刻から表示する。通常の採点音・合格音は重ねない。
1.7秒から左の観客の吹き出しを表示し、0.10秒後に声を出す。右は左の音声終了から0.32～0.40秒後に返す。種類・大きさ・品質・手入れの記録・満点に合う16組から選び、台詞ごとの実測秒数で表示を決める。実在の人物・芸名・ブランド・キャラクター名や、記録にない味・栽培手法の断定は使わない。
吹き出し内はセリフ2行のみ。「左の観客」「右の観客」の見出しは表示しない。話者の左右は吹き出しの位置と音声の定位で区別する。
2.8～3.7秒で見出しを左上へ収める。紙吹雪と掛け合いの終了後、1秒かけてカメラを結果構図に移し、確定得点と入賞ラインを表示する。合格後の得点count-upは行わない。
結果の表示完了で詳細タブと一覧への操作を有効にする。紙吹雪は従来どおり6秒まで。待たずに「演出を省略」で結果へ進める。
全32台詞をQwen3-TTSの音声に統一した。承認済みの「鍋に入らないぞ！」→「鍋のほうを育ててこい！」は原音を保持し、残り30台詞はその左右2種類の合成音声を参照してローカル生成した。全台詞の生成・実装完了と、発音や勢いの最終試聴は区別する。
台の左右から白、桃色、黄色の花びら160枚と金色の紙32枚を放出する。48枚を手前、144枚を奥へ分散し、world-spaceのDepth Testで野菜との前後関係を保つ。
横方向は空気抵抗で減速し、揺れと回転を加える。重力と抵抗の解析式で落下速度を抑え、各粒の着地時刻に合わせて初速を決める。
着地直前に姿勢を床へ合わせ、着地後0.16秒静止して0.36秒でfadeする。空中の寿命切れや縮小消失は行わない。
通り道は手前・奥に限定して作物と什器の実寸を避ける。任意の物体とのcollision simulationではないため、会場・モデル変更時は検査と再調整が必要。
クラッカーと拍手を模した短いnoise音を、オリジナルの旋律と合成する。掛け声のみ外部の音声合成libraryを制作時に使用し、クレジットを同梱する。ゲーム中はWAVの再生だけで、音声生成や通信をしない。
演出中は「演出を省略」、完了後は「成績一覧へ」。省略すると演出音を止めて同じ画面の完成した結果を見せ、一覧へは進めない。品質・大きさは詳細タブで確認する。成績一覧の見出しは「今シーズンの成績」とし、二度目のクリア発表にしない。

10/20日目、点数不足、未出品、旧形式の非段階制、結果一覧からの再閲覧は対象外。
再確認は既存「展示：最終出品・クリア」から新しく出品する。クリア条件・総合ランク・保存schemaは変更しない。

## 全32台詞とクリア専用展示版

2026-11-17: ユーザーの追加指示により、残り30台詞の延期を取り消し、全台詞の生成と16種類の直接選択を実装した。

- 確認版: generated/exhibitions/contest_cheer_complete_check_20261117/Start_Check.cmd。
- 展示版: generated/exhibitions/contest_clear_gallery_20261117/Start_Check.cmd。
- 両方とも「はじめる」から16項目の一覧を表示する。項目を押すと、対応する野菜・育成記録・成績でクリア演出が始まる。「もう一度見る」は同じ組を再生し、「ほかのセリフを見る」は一覧へ戻る。栽培・出品・時間送りは不要。
- 記録は展示用の合成データ。FarmContestShowcaseSystemが既存V1の品質・大きさ・採点処理を通し、FarmContestCheerSystemの資格に合う比較結果だけを返す。プレイヤーの農場や在庫・セーブを変更しない。
- 音声制作: contest_cheers.json(schema3) → bake_local_cheers.py → publish_local_cheers.py → labels生成 → asset/C++検査 → Release。旧schema2用generator/applyはschema3を拒否する。モデルはQwen3-TTS-12Hz-1.7B-Base、参照2本は承認済みVoiceDesign出力。実在人物の録音は使用しない。
- 原音はgenerated/audio/local_contest_cheersへ保存。公開処理は32本すべての本文・hash・由来を検査してから、無音端の整理、音量調整、左右pan、5msの端fadeを適用する。話速・pitchは加工しない。最大2.371秒、PCM合計5,839,968bytes。吹き出しと応答の間は各WAVの実測長を使用する。
- 配布時はEXE内のspeech manifest SHA256と32WAVのhashを検査し、古いEXEに新音源だけを混ぜる組合せを拒否する。生成環境・モデル・Python/機械学習libraryは配布しない。
- 自動検証: 32音源のasset検査、presentation/audio/exhibition/runtime_view/outcome/season_balanceがPASS。展示24,276レイアウト、全16組・32台詞、再選択・省略・不正設定・採点資格、XAudio2のpause/resume/mute/欠損fallbackを含む。Release/x64は警告・エラー0。Debugは再ビルドしていない。
- 実画面: 新音源入りの独立QAを1280×720で起動。16項目が背景内に収まること、かぼちゃ満点の100/100と同組再生、一覧へ戻ってトマトへ変更、左→右の吹き出しから99/100の結果への遷移を確認した。文字・野菜・3D名札が重ならない観察frameを確認。QAのみ終了しexit0・window消失を確認した。
- 未検証: 全32台詞の発音・勢い・ゲーム内mixの聴感、ネイティブのfocus/skip/通常提出の再検証、他PC/GPU/DPI、実測CPU/GPU/VRAM負荷。機械的検査を自然な声の評価とは扱わない。

試聴用はgenerated/codex_checks/cheer_complete_previewの16個別WAVとAll16.wav。無加工の会場録音ではなく、配布音源を同じ応答間隔で並べた比較用音声である。

2026-11-17 最新の確認・提出用候補は、generated/submissions/clear_20261117_v4/LE3C_28_ヨシノ_ゲント_水路農業.zip。ZIP内のStartGame.cmdから起動する。v4は音声の読みと間を修正した版で、実ゲーム画面の再確認と全台詞の最終試聴は未完了。旧ZIPと起動用フォルダは残している。
v4では14台詞を更新し、残り18台詞と字幕atlasはv3とbyte一致。「はたけ」「ひとたば」「だて」「その土の後の間」の4比較音声はユーザー承認済みで、その音声を保持した。他の区切り・読みも調整したが、全台詞の聴感承認とは区別する。
音声生成用の読み・分割句・無音時間をJSONに分離。句間は0.16～0.24秒、土の後は0.24秒。原音をhash付き履歴へ保管し、生成入力が変わった台詞だけ再生成する。公開処理は無音区間を検査し、完成WAVの実測長から従来の字幕・応答時刻を更新する。ゲーム内の生成・通信・新規状態管理は追加しない。
公式Whisper smallをPC内で使い旧32本と新14本を照合した。認識器自体も誤るため、結果を聴感合格とは扱わない。「煮物」「搬出」は別途ユーザーに試聴を依頼中。最終認識記録はgenerated/codex_checks/cheer_pronunciation_final/asr_report.json、全16組の試聴はgenerated/codex_checks/cheer_pronunciation_final_preview/All16.wav。
単体・音源・表示・同期・XAudio2のテストがPASS、展示27635レイアウト、Release/x64は警告・エラー0。518ファイルのZIP展開hash一致。PCM合計6,330,544bytes、最長3.065秒。v4は未起動のまま保存し、実画面・他PC/GPU/DPI・実測性能は再検証していない。元のSettings20ファイルとv2/v3 ZIPは変更なし。容量確保時にv3の検証用staged/extractedコピーと一時ビルドファイルのみ整理したが、提出ZIP・起動用フォルダは残している。

以下はv3の野菜置換時の記録。
v3では誤って展示していたTestCrop（カブ）3項目を「かぼちゃ：主役」「ニンジン：包丁」「トマト：来賓席」に置換した。丸みの掛け声はかぼちゃ限定、高品質の包丁の掛け声はニンジン、入賞野菜の来賓席はトマトで展示する。台詞にカブ固有の文言はなく、承認済み32音声と応答時間は維持。TurnipStarは録音・由来情報の固定キーとしてのみ残す。通常の栽培・保存・採点仕様は変更しない。
3種類だけで全16組に到達する2304選択、展示24276レイアウト、UI、32音源検査がPASS。Release/x64警告・エラー0、518ファイルの展開hash一致。実画面では更新済み一覧とかぼちゃの左右の吹き出し・99/100点への移行を観察したが、ユーザーのEscape操作で確認を中断。検証プロセスはexit0。ニンジン・トマトの変更項目の実画面再生、全台詞の聴感、他PC/GPU/DPIは未確認。

以下はv2の表示修正時の検証記録。
全16組×左右×3段階の出入り位置で、セリフ2行のみ・見出しなし・枠内配置・行の非重複を検査した。presentation/exhibition/runtime_view/32音源検査がPASS。制限付きビルドでC1902が発生したが、通常環境のRelease/x64 Rebuildは警告・エラー0。展開コピーでトマトの左→右の2行表示と99/100点への遷移、正常終了を確認した。音源とatlasは変更なし。518ファイルのZIP展開・hash一致を検証済み。新しい動画・外部アップロードは行っていない。

## 旧2台詞の声質確認版の履歴

2026-11-17: VOICEVOXの声種・語尾・抑揚を調整した2案は、ユーザーが読み上げ感を理由に不採用とした。
Qwen3-TTS-12Hz-1.7B-VoiceDesignへ「客席から張り上げる」「勢いよく返す」演技を指定した2台詞は、試聴で方向の承認を得た。
当時は公式デモの生成枠が停止したため、ユーザーの選択どおり、この2台詞の確認版を先に完成した。以下はその時点の記録であり、延期・未採用という記述は現在の全32台詞版には適用しない。

- 確認版: generated/exhibitions/contest_cheer_audition_20261117/Start_Check.cmd。「はじめる」→「入賞」→「演出を見る」。「もう一度見る」で同じ2台詞を再生する。
- 音声: 承認済み2台詞以外の30WAVは旧版とbyte一致。不採用の音声は配布しない。原音・指示・hashはtools/audio/cheer_referenceに保存。合成音声であり、実在人物の録音や模倣ではない。
- 制作: contest_cheers.json→必要ならgenerate_contest_cheers.pyで基準音声生成→apply_cheer_audition.pyで承認済み2台詞を反映→asset検査→labels生成→C++テスト→Release。applyは入力を検証し、波形長からheaderを作る。ゲーム内はWAV再生のみ。
- 検証: asset32本、presentation/audio/exhibition/runtime_view/outcome/season_balanceの6テストPASS。繰返し設定の不正型・初期化解除も検査。Release/x64は警告・エラー0。Debug未ビルド。
- 実画面: 独立QAを1280×720で起動。左→右→98/100対85点、同じ組の再生を確認。文字切れ・野菜/名札との重なりは観察したframeにはない。QAだけ閉じ、exit0とwindow消失を確認。
- 保護: 確認版518ファイルのmanifest、Release/QA/配布EXE一致、元Settings20ファイルと以前の518/484/484ファイル版の保全を確認。実行ファイルSHA256は9AA6F214B753E1036480B7F46892FA73B9B4D6F5A7C5927DF04881F3ECB6060C。
- 限界: 試聴方向の承認と、全台詞の発音やゲーム内mixの聴感評価は別。後者は未完了。実機focus/skip/他作物/通常提出、他PC/GPU/DPIは今回は再確認していない。生成済みPerfect_Lの追加1本は未採用。

後半の音声化履歴の固定時間・Nemo全32台詞という記述も旧版の記録。現在の仕様は冒頭と「全32台詞とクリア専用展示版」を優先する。

## 責務と状態

- FarmContestPresentationSystem: fresh submissionからの自動開始、共通タイマー、見出しと結果の表示率、Drumroll/Clear/Silence Cueの一回消費。発表時刻からの紙吹雪local timeを渡す。値のsnapshotを保持し在庫や採点を変更しない。
- FarmRuntimeController: 既存のClose/Reveal要求をSystemへ通知。未完了のCloseを拒否し、表示済みsnapshotの入力判定でスキップの押し抜けを防止。非focus時は進行を止め、未再生Cueを保持する。
- FarmContestCheerSystem: 提出済みの事実と採点から台詞の資格を判定。bounded rotationで直前と同じ組を避ける。PresentationSystemが一度だけ選択して前回IDと表示時間を所有。抽選や毎frameの選び直し、セーブ変更はしない。
- FarmContestShowcaseSystem: 16種類の展示用育成記録を既存の採点処理へ渡し、台詞条件を満たす不変の比較結果を構築する。元農場への参照・変更を持たない。
- FarmExhibitionSystem: clearGallery=trueの場合だけ16種類の選択・再生・戻る状態を所有し、PresentationSystem::BeginPreviewへ資格検査済みの組を渡す。通常プレイの台詞巡回は維持する。旧確認コピーのrepeatFirstCheer設定と3種類の成績プレビューも維持する。UIやSceneに台詞選択の条件を移さない。
- FarmContestDetailView/CheerView/RuntimeUI: Frameを描画し既存Actionを通知するだけ。クリア資格・台詞の条件・日付条件は計算しない。左右どちらか一つの吹き出しと2行の台詞を既存Sprite poolで表示する。見出しを削除した分、背景は132pxから94pxへ縮め、中心位置は維持する。
- FarmContestStageLayout: resultRevealで中央構図から結果構図へカメラを補間。看板と文字はworld transformのまま。
- FarmContestAudioSystem: 5個の演出clipと32個の掛け声clipを所有。演出/speechの2voiceまでに限定。左右panはWAVへ焼き込み、掛け声中は演出音をduckする。非focus時は両voiceを既存Audio::PauseVoiceで保持し、復帰でResumeVoiceする。Silence/終了は自身のvoiceだけ停止。台詞の選択や吹き出しのタイミングを変更しない。
- FarmCelebrationSystem: 固定seedと時刻から最大192枚の位置・回転・色・着地状態を算出。軌道・飛行時間・fadeはconstexprで集約。乱数の共有状態や毎frameの動的確保は持たない。
- FarmCelebrationGeometry/Renderer: 折り目付きの花びらを最大2304頂点に変換、重心のview depth順に整列し1 DrawCall。GPU resourceはRendererが所有する。

Beginで対象なら自動開始する。手動開始APIは撤去。Close/BeginUnsubmittedで破棄する。
過去結果のBegin(replay=true)では設定しない。描画用Frameから保存状態へ逆流させない。

## 安全性と検証

新しいpointerやGridコピー、毎frameの音生成・texture uploadは追加しない。
UI atlasは見出し1個だけ大きいセルを使い、各行の最大高さでpackingする。既存のEXE/atlas契約を再生成する。
既存shadow/color passとconstant buffer更新順序、Descriptor/Barrier/Fence経路を維持する。
花びらは既存Line shaderと雨描画のPSO構成を踏襲する独立batch。alpha blend、depth test有効、depth write無効、両面描画。
Upload vertex bufferは64512bytes、constant bufferは256bytes。実際のcommitted allocationはGPUの配置粒度に従う。新規SRV/texture/compute/UAVは不要。
既存PostDrawのFence完了待ちを毎frameの再利用と終了時の前提とする。フレーム並列化時はbufferのring化が必要。
クリア見出しの影にLabelQuadを1回追加し、得点欄は結果表示率に従う。掛け声表示中は既存Sprite poolから吹き出し1枚と文字2行を追加する。追加のDescriptor/Barrier/Fence経路や毎frameのresource作成はない。UI atlasは815ラベル、3840×2754のまま。未使用となった観客名のIDは保持し、この表示修正ではatlasの再配置をしない。CPU/GPU時間・実VRAM・大量の透明物体交差時の画素単位の正確性は未検証。

2026-11-17 統合版: presentation testは501粒子frame、101箇所の省略、2764368会場頂点、177120比較頂点、4545採点比較をPASS。自動開始条件、結果の遅延表示、見出しと詳細の重なり、同一画面への省略、詳細閲覧後の再発防止を検証。
音声・結果・UI・展示再生・18通りの栽培シーズン回帰testはPASS。Release/x64は警告・エラー0で成功。
独立した確認用コピーで、ユーザーの許可後にかぼちゃ1個を提出。1280x720で「クリア見出し・花吹雪、得点欄なし」から、追加入力なしで同じ会場の98/100点・入賞ライン85点へ進むことを確認。
品質58/60・大きさ40/40の詳細タブ、結果へ戻る操作、成績一覧への遷移を確認。総合ランクS、90/90/98点、合計278点、提出3・未出場0。30日目の結果を再閲覧しても演出は再発せず、確認用コピーだけ正常終了した。
元のSettingsと配布用確認版478ファイルのハッシュは変更なし。配布用は未提出の初期状態を維持し、消費した検証用コピーとは区別する。
確認は離散的な画面取得とクリック操作であり、全フレームの録画や聴感確認ではない。ネイティブでの演出スキップ、不合格・他作物、他PC/GPU/DPI、CPU/GPU時間、音の聴感は未検証。

## 前後分散の修正

2026-11-17 前後分散の修正: 自動検査で48/144枚の前後配分、上下左右の連続性、終端落下速度、3.21～4.26秒の着地、全粒の着地後fade、床・看板・野菜・支柱との間隔を確認。4作物の実メッシュ最大半奥行き1.57692に対して、紙吹雪は絶対Zが1.65を超える通り道に収める。最初の設定はこの実寸検査で失敗したため、前後の通り道を外側へ修正した。
presentation・outcome・runtime_view・exhibitionの回帰testとRelease/x64（警告・エラー0）が成功。移動カメラ下の紙吹雪186852頂点が画面内に収まることも検査。描画は既存の1 DrawCallと最大2304頂点のまま。折り目の深さをSystem/Geometry共通定数にして着地高さとの不一致を防いだ。
同一Releaseの独立QAコピーで既存の読取専用展示再生を使用し、1280x720のかぼちゃ入賞演出を複数回確認。野菜前面を横切る粒、野菜に隠れる奥側、床付近の粒と薄れた粒、その後の消失、同じ画面の98/100点・85点ラインを観察した。QAのみ正常終了（exit0）。今回、通常農場での新規提出・詳細タブ・成績一覧は実機で再検証していない。
連続録画や音の聴感、他GPU/DPI、実測CPU/GPU時間は未確認。particleのlighting/shadow対応と大きさタブのモデル表現は今回変更していない。

## ドラムロールから結果までの修正

2026-11-17: 上の「流れ」を実装。演出の時間とCueはPresentationSystemへ、音のPause/ResumeはAudioSystemへ集約し、View/RendererはFrameを読むだけにした。既存のGame/Scene/採点/保存schemaは変更しない。外部音源は使わず、tools/audio/generate_contest_audio.pyで1秒のdrumroll.wavを生成。既存4音源は前版とbyte単位で同一。

- 自動検証: presentationは121箇所からのスキップ、発表前の非表示、Cueの一回発行、結果完成時の入力解放、スコア固定、既存の全粒子と移動カメラの幾何検査をPASS。
- 実音声API検証: XAudio2でdrumroll→clearの単一voice切替、元clip長を超えるpauseと続きからのresume、skip時の自身のvoiceのみ停止、null/途中load失敗/終了/再入場をPASS。実際の聴感の評価とは区別する。
- 回帰: runtime_view/exhibition/outcome/season_balanceをPASS。18通りの自動栽培シーズンを含む。Release/x64 Buildは警告・エラー0。Debugは再ビルドしていない。
- 確認版: generated/exhibitions/contest_drumroll_20261117/Start_Check.cmd。「はじめる」→「入賞」→「演出を見る」。新しいprepare_contest_check.ps1 -Previewで既存の読取専用展示再生を有効化する。通常の提出・消費をせず何度でも再生可能。既存の通常確認版作成モードは維持。
- 実画面: 別のQAコピーで1280x720のかぼちゃ入賞を複数回再生。クリア見出し・前後の紙吹雪→同じ会場の98/100点・85点ライン、品質58/60・大きさ40/40、結果へ戻っても演出が再発しないことを確認。QAだけ閉じ、exit0・ウィンドウ消失を確認。
- 限界: UI取得の遅延により、1秒以内の発表前とスキップの実機判定は未確定。省略を狙ったクリックは操作到着時に演出が完了しており、メニューへ戻ったため、スキップ成功の証拠として扱わない。これらは上記の自動テストで検証。ネイティブのfocus切替、聴感、通常提出→成績一覧の全導線、他作物/不合格、他PC/GPU/DPIは今回再検証していない。

GPU側は同じ192枚/1 DrawCall、64512bytes VB+256bytes CBのまま。Descriptor/Barrier/Fence・Upload再利用の前提を変更せず、毎frameの音源生成や動的確保を追加しない。5音源のPCM合計341760bytes、既存の正逆再生cacheを含むPCMは683520bytes。実測CPU/GPU時間・VRAMは未測定。大きさタブのモデル統一とparticle lighting/shadow、既存Controllerの責務過多は未対応。

配布版484ファイルのmanifestとEXE/atlas契約を検査。Release/配布/QAのEXE SHA256は7C8BEE2112632530B71F1CD9DDB4D356B63FC8F00A6D116F99CFC91BF01DBFB4で一致。元のproject/Settings20ファイル、以前の自然な紙吹雪版478ファイル、統合版478ファイルは変更なし。QAの農場saveと展示fixturesも同一で、カタログJSONだけ既存起動処理によりキー順・空白が正規化された。配布版は未起動状態を維持する。

## 野菜への掛け声

2026-11-17: 16組の掛け合いを追加。種類限定、大きさ1.5倍以上、品質54/60以上、水分・養分の記録90%以上、満点100点の条件を持たせ、合わない断定は選ばない。不明なサイズ・養分から条件を推測しない。抽選ではなく、条件に合う組を順番に巡回して直前と同じ組を避ける。前回IDは実行中だけ保持し、セーブには追加しない。

例: 大きいかぼちゃ「鍋に入らないぞ！」→「鍋のほうを育ててこい！」、大きな野菜「収穫じゃない、搬出だ！」→「袋じゃない、台車を持ってこい！」、高品質「包丁が緊張してるぞ！」→「今日は切るな、飾っとけ！」。条件はSystem、文言とレイアウトはUIに分離。

- 自動検証:3,072選択ケース、全16組への到達、左右・出入り96レイアウト、141時点からの省略、結果/未出品/再閲覧での残留防止。4種類の実メッシュが左右の吹き出しに重ならないことを確認。既存採点4,545比較、紙吹雪・3D名札の検査もPASS。
- 回帰:runtime_view/exhibition/outcome/season_balance/audioの5テストPASS。Release/x64警告・エラー0。Debug未ビルド。
- 実画面:独立したQA版でかぼちゃを3回再生し、別の台詞への切替、左から右の掛け合い、得点98/100への移行、品質・大きさ・結果への移動を確認。野菜・勲章・3D看板を隠さず、文字切れなし。QAのみ正常終了。
- 確認版:generated/exhibitions/contest_cheers_20261117/Start_Check.cmd。「はじめる」→「入賞」→「演出を見る」。「もう一度見る」で条件に合う別の組に変わる。栽培や出品の消費は不要。
- 保護:元のSettings20ファイル、前のドラムロール確認版484ファイルに変更なし。新確認版484ファイルのmanifestとRelease/QA/配布EXE一致を検査。QAカタログのみ起動時の空白正規化、保存済み農場・展示fixturesは同一。
- 限界:掛け声の音声は未追加。録画・聴感評価・ネイティブの省略/focus切替/他作物/不合格/通常提出から成績一覧までの通し確認、他PC/GPU/DPIは今回未実施。CPU/GPU時間・実VRAMは未測定。

## 掛け声の音声化

2026-11-17: 上の流れを音声付きに更新。VOICEVOX Nemoの異なるstock話者を左/右へ割り当て、既存16組32台詞をWAV化。吹き出し出現の0.16秒後に一度だけ再生し、左右の声を重ねない。panは左-0.35/右+0.35、掛け声中の祝福音は通常の32%へ下げる。セリフ音量は既存の大会音量設定に追従する。

- 文言: tools/audio/contest_cheers.jsonが字幕と音声の共通入力。generate_contest_cheers.pyでWAV/manifest/FarmContestCheerAssets.h、generate_farm_runtime_labels.pyでatlas/headerを再生成する。音声秒数はPCMから実測し、各側の最長台詞と読後/フェードの余白からPresentationSystemの時間を決める。文言は前版と同じで、atlas/headerのbyte一致を確認。
- 配布: Resources/farm/contest/audio/cheersへ32音源・manifest・CREDITS。ゲーム内で音声モデルやエンジンを起動せず、通信もしない。開発用モデル/ランタイムはgenerated/toolsだけに置き、配布から除外する。
- 実行時: AudioSystemの独立したphase/speech voice、同じFrameのvoiceReadyとduckingを使う。連続frameで同じ台詞を再生し直さない。非focusは両voiceをpauseし、復帰時resume。skip/closeは両方停止する。音声が1本でも欠けた場合はspeechだけ全解放し、既存効果音と字幕を残す。
- 自動検証: 32本のhash/本文/ID順/秒数/PCM形式/音量/左右定位/終端fade/mono互換/creditを検査。XAudio2で全32clip、二重再生防止、mute、pause/resume、skip、欠損rollback、別Systemのvoice保護、解放後PCM量を確認。presentation/runtime_view/exhibition(6390layouts)/outcome/season_balance(18seasons)がPASS。Release/x64警告・エラー0、Debug未ビルド。
- 実画面: 独立QAでかぼちゃの2組を再生。発表前の状態、左→右の字幕、野菜と看板を隠さない配置、最後の98/100対85点、正常終了を確認。再生制御テストと画面観察は、声の勢い・発音・mixの聴感評価とは別。聴感、実機focus/skip/全作物/通常提出経路、他PC/GPU/DPIは未評価。
- 確認版: generated/exhibitions/contest_cheer_voices_20261117/Start_Check.cmd。518ファイルのmanifestとRelease/QA/配布EXE一致を確認。元Settings20ファイル、以前の2確認版各484ファイル、既存5効果音は不変。QAはカタログの正規化だけで農場データ不変。

追加PCMは4,387,396bytes。既存backendの正逆cache込みでは8,774,792bytes増加、全大会音声は9,458,312bytes。台詞開始時の既存SourceVoice作成のみで毎frameの合成/decodeはない。既存DX12描画経路・リソース・DrawCall数は変更しない。実測CPU/GPU/VRAM負荷は未計測。

音声: **VOICEVOX Nemo**。[公式利用規約](https://voicevox.hiroshiba.jp/nemo/term/)に従い、creditと音声再利用時の条件を同梱。キャラクターや実在の人物の模倣は行わない。

## 参考と判断

- [任天堂 カービィのすいこみ大作戦 電子説明書・9 ステージクリア](https://www.nintendo.co.jp/data/software/manual/268238.pdf): クリア後に結果を示す順序の参考。
- [Nielsen Norman Group: Progressive Disclosure](https://www.nngroup.com/articles/progressive-disclosure/): 最も重要な達成を先に見せ、詳細は要求に応じて見せる情報階層の参考。

参考は順序・情報量の根拠であり、上記秒数やカメラ構図を指定する規則ではない。秒数は本作の短い展示用演出として設定した。
