# 出品した野菜が主役の審査結果

## 目的と構成

対象は1回の品評会の審査結果。大会全体の最終結果とは別画面。
結果ページは従来の左右ほぼ半分から、野菜側約74%・結果側約26%へ変更する。
入賞/未達、得点、入賞ラインだけを右端に残す。養分・水分などの講評は品質ページ、重さと不足量は大きさページで見る。
採点のしくみは詳細ページから開く。結果に説明を重ねて野菜を隠さない。
害虫減点と対処の案内も品質ページに保持し、情報を削除するだけにしない。

参考: https://note.com/ikyu_design/n/n0e1ad0e56ef9
記事はTooltipとPopoverの役割分離を扱う。今回はホバー必須のUIを追加せず、常時表示と補足の分離という考え方を既存のページ切替に適用した。

## 描画と責務

- FarmContestStageLayout: 寄ったカメラ、約3:1の画面配分、勲章の左に固定する看板のworld position/scale。
- FarmContestDetailView: 結果を9項目以内に整理し、既存タブのRequestを通知。結果ページには2Dの野菜名を出さない。
- RuntimeUI: 小さい結果背景だけを描画。名前の文字と背景Panelは削除。UI atlasや収穫データは変更しない。
- FarmContestRenderer: 既存の木枠モデルと、負のZを表にした専用文字面を同じ固定transformで描画。通常のWVP/depth/照明で遠近・角度・遮蔽が決まる。cameraに正対させたり距離を打ち消したりしない。
- generate_contest_ceremony.py: 同梱NotoSansJP(OFL)で4種類の名前画像を生成。正面UVと3mmの面間隔を保証するname_face.objも生成する。文字を毎frame生成しない。

野菜名は種類名のまま。自由命名はIME入力・文字数上限・収穫記録との紐付け・保存互換性を決める別機能として保留。
実際の大きさ倍率と採点は変えない。cameraで拡大し、品質/大きさ比較ページの共通スケールは維持する。

## 安全性と検証範囲

初版の「world anchorを2Dに投影して固定サイズのPanelを置く」方式はユーザーの意図と異なるため不採用。
看板は台座の布の前に接する固定位置。勲章と別のrigid objectであり、勲章登場時にも追尾しない。
木枠と文字面は独立したObject3d constant bufferを持ち、shadow/color間で書き換えない。木枠のみshadow caster。
2D名札Panel2個を削除し、3D color draw2回とshadow draw1回を追加。既存木枠12三角形と文字面2三角形を使用。
TextureManagerが4枚の720x128名前画像/SRV、ModelManagerがmesh、RendererがObject3d/constant bufferを所有する。
初期化時にtexture fallbackを拒否し、表示は有効な出品作物のみ。選択indexを検査してから配列にアクセスする。
既存PostDrawのFence待機とtexture upload経路を利用し、独自のBarrier/Fenceやrender passは追加しない。
per-frameのtexture生成・動的確保・Gridコピーは追加しない。CPU/GPU時間・DrawCall全体・実VRAMの測定は未実施。
既存Controllerの大きさやRender resource管理の全体監査は今回の変更範囲外。

自動検証: 4作物の実OBJ、サイズ倍率0.1/1/3/8、camera遷移の組合せで921456頂点を投影。
画面端・上部タブ・右欄・3D看板との干渉を検査し、比較ページ177120頂点、4545採点条件、文字寸法/重なり/操作領域/演出順序も検査する。
近寄るcameraで看板幅が165.717pxから243.250pxへ拡大し、視線角度で比率が変わること、文字面が木枠内に収まることを検査。
Assimpによる本番と同じimportで18mesh/45146三角形、法線・UV・index範囲を検査。文字の左右/上下と正面法線も検査する。
上記とfarm_runtime_view_testはPASS。3D看板版のVS18 Release/x64全体Rebuildは警告0・エラー0で成功。Debugは未ビルド。

確認版: generated/exhibitions/contest_3d_sign_20261116/Start_Check.cmd (477ファイル、85.43MiB)。
30日目、かぼちゃ出品予約済みで開始する。全ファイルのmanifest hashとRelease EXEの一致を確認。旧contest_result_ready_20261116は不採用の2D名札を含むため、今回はこちらを使う。
元の農場セーブ・既存の提出ZIPは未変更。別QAコピーは提出確認画面までagentが観測し、ユーザーのテスト後に終了コード0を確認。
ユーザーの「良くなってる」という報告と提供動画の抽出フレームで、かぼちゃの木製名札・勲章・camera演出、98点の結果から一覧・終了農場への遷移を確認した。
Quality/Sizeの実操作、他3作物の実画面は今回未確認。これらの表示データ・geometryは自動検証の範囲。
自動検証は投影と表示データの検査であり、他PC・DPI・長時間プレイ・音声の品質を保証するものではない。
