# 02 / 現行実装との差分

2026-11-09のコード確認。**実装あり = コードと接続を確認した範囲。今回の実機検証済み、または品質完成という意味ではない。**

## 水・土地・育成

| ID | 項目 | 状態 | 今できること / 残ること | 根拠 |
|---|---|---|---|---|
| GS01 | 造成・水路・水源 | 実装あり | 高さ変更、配置、取消、Undo/Redo。農地拡張は別 | E01 |
| GS02 | 水路の通水・貯水・畑給水 | 実装あり | 有限貯水から給水。雨、洪水、地表排水ではない | E02 |
| GS03 | 事前プレビュー | 実装あり | 一時Gridで変更候補と水・成長予測、確定/取消 | E01/E02 |
| GS04 | 3作物の栽培と収穫 | 実装あり | 育成条件とモデル変化、1回収穫 | E03 |
| GS05 | 水不足・過多と品質/サイズ | 実装あり | 履歴で評価。枯死や病気はない | E03/E04 |
| GS06 | 養分・堆肥 | 部分実装 | 養分消費と植付け前の補充。堆肥の購入/費用/種類なし | E04 |
| GS07 | 水の状態を理解する表示 | 部分実装・要確認 | 選択畑の案内/予測/流れの表示あり。原案のNormal/Water/Crop ImpactのRelease一式は完成確認できず | E02/E05 |
| GS08 | 通常の土の湛水・排水 | 未実装 | 土壌水分と地表水深を別に持つ設計が必要 | E02/E06 |
| GS09 | 水源の栓・可変流量 | 未実装 | 今の畑ごとのirrigationEnabledは取水ON/OFFであり水源の調節ではない | E02/E06 |
| GS10 | トマトの連続収穫 | 未実装・採否未決定 | 再成長、養分、寿命、大会記録を設計する必要あり | E03 |
| GS11 | 2×2巨大かぼちゃ | 未実装・採否未決定 | サイズ倍率とは別に、占有マスと操作単位が必要 | E03/E06 |

## 進行・経済・大会

| ID | 項目 | 状態 | 今できること / 残ること | 根拠 |
|---|---|---|---|---|
| GS12 | 日付と1/2/4倍速 | 実装あり | 通常更新、大会日停止。自動待ち時間送りとは別 | E07 |
| GS13 | 安全な時間送り | Systemのみ | 停止条件・バッチ処理とテストあり。製品コード内で通常呼出しなし | E08 |
| GS14 | 売買・保管・保護・出品予約 | 実装あり | メニュー売却確認あり。F/V直接売却の統一は別課題 | E09 |
| GS15 | 作物・高さの段階解放 | 実装あり | 新規大会で1/11/21日の段階制。既存農場への再制限はしない | E10 |
| GS16 | 10/20/30日の大会 | 実装あり | 対象期間、提出、消費、結果、未出品・不入賞・入賞 | E11 |
| GS17 | 結果の比較・再挑戦導線 | 実装あり・要検証 | 品質/大きさ/不足と結果演出。初見の理解や通常30日プレイは別検証 | E11 |
| GS18 | 農場ランクの成長 | 未実装 | HUD値は1固定。大会S～Dを流用して済ませない | E07 |
| GS19 | ランク連動の農地拡張 | 未実装 | 通常は5×4初期化。TileのLocked/Unlocked状態なし | E06/E07 |
| GS20 | 他者順位・賞金 | 未実装・採否未決定 | 入賞表示はあるが、対戦相手や順位計算、賞金経済はない | E11 |
| GS21 | 総売上・虫被害等を含む最終評価 | 部分実装 | 大会合計S～Dはある。農場全体の複合評価はない | E11 |
| GS22 | 保存・ロード・配置保存 | 実装あり | 通常schema18と配置保存。将来の虫・天候状態はまだ持たない | E12 |
| GS23 | 状況に応じた次の短期目標 | 部分実装 | 開始目標・給水助言・段階ガイドあり。栽培→販売→出品を追う達成管理は未確認 | E05/E13 |

## 障害・寄り道・その他

| ID | 項目 | 状態 | 今できること / 残ること | 根拠 |
|---|---|---|---|---|
| GS24 | 害虫の発生・警告・拡散・被害 | 未実装（元案） | 元案33/34節に存在。現行Tileに虫/被害履歴なし | E06/E14 |
| GS25 | 虫取り網による駆除 | 名前のみ | 4道具しか通常選択できず、Editor経路もBugNetを拒否 | E14 |
| GS26 | 虫除け植物 | 未実装（元案） | 効果範囲・植える場所・種の経済が必要 | E06/E14 |
| GS27 | 台風・災害 | 未実装（元案の候補） | 天球の風・昼夜表現は農場被害のシミュレーションではない | E06/E07 |
| GS28 | 雨・乾燥予報と対策 | 未実装（今回の提案） | 現行給水に雨/補給変動を足す案。詳細は04 | E02/E07 |
| GS29 | 複数ステージ・自動生成 | 未実装・必須ではない | 配置保存と別。目標や攻略条件の違うシナリオは確認できない | E12 |
| GS30 | 作物図鑑・記録更新・トロフィー | 部分実装 / 拡張案 | 個別収穫記録と展示はある。収集達成・報酬・長期図鑑はない | E09/E11 |
| GS31 | 道具強化・スキルツリー | 未実装・追加候補 | 学校の例であり、本作の必須仕様ではない | E03/E10 |
| GS32 | オンライン・ランキング共有 | 未実装・今回は対象外 | 今のオフライン栽培の完成を優先 | 調査範囲内に接続経路なし |
| GS33 | 資金枯渇時の救済 | 未実装・新提案 | 残金だけで破産を判断しない。種/作物/売れる在庫まで確認が必要 | E09 |
| GS34 | 作物枯死・病気・火災・盗難・宇宙人 | 未実装・今は採用しない | 宇宙人は元案54節の候補。他は元案確定仕様と扱わない | E06、元案54節 |

## コードの根拠

パスはこの文書からの相対リンク。代表関数まで併記し、古い検証記録だけを根拠にしない。

- **E01** [FarmToolActionSystem.cpp](../../../project/application/farm/system/FarmToolActionSystem.cpp)、[FarmIrrigationPreviewSystem.cpp](../../../project/application/farm/system/FarmIrrigationPreviewSystem.cpp): 本Gridの操作と一時Gridの分離。
- **E02** [FarmIrrigationSystem.cpp](../../../project/application/farm/system/FarmIrrigationSystem.cpp): `Rebuild`、`UpdateWater`。水量と土壌への給水。[FarmVisualSystem.cpp](../../../project/application/farm/system/FarmVisualSystem.cpp): upstreamに基づく表示。
- **E03** [FarmToolBehavior.cpp](../../../project/application/farm/system/FarmToolBehavior.cpp): `SeedBehavior`、`HarvestBehavior`。[FarmGrowthSystem.cpp](../../../project/application/farm/system/FarmGrowthSystem.cpp): `EvaluateMoisture`、`Update`。[FarmRules.h](../../../project/application/farm/data/FarmRules.h): 作物別値。
- **E04** [FarmSoilSystem.h](../../../project/application/farm/system/FarmSoilSystem.h): `CanCompost`、`Grow`。[FarmCropQualitySystem.cpp](../../../project/application/farm/system/FarmCropQualitySystem.cpp): `Evaluate`。[FarmCropSizeSystem.h](../../../project/application/farm/system/FarmCropSizeSystem.h): 育成履歴からの相対サイズ。
- **E05** [FarmGrowthSystem.cpp](../../../project/application/farm/system/FarmGrowthSystem.cpp): `AnalyzeWater`。[FarmRuntimeController.cpp](../../../project/application/farm/system/FarmRuntimeController.cpp)、[FarmMenuView.h](../../../project/application/farm/ui/FarmMenuView.h): 表示・操作経路。
- **E06** [FarmTypes.h](../../../project/application/farm/core/FarmTypes.h): 実際にFarmGridが使用する`FarmTile`と`FarmCropCareHistory`。古い同名の`FarmTile.h`を現行状態定義と取り違えない。
- **E07** [GamePlaySessionSystem.cpp](../../../project/application/gameplay/GamePlaySessionSystem.cpp): `Initialize(5, 4)`、`FixedUpdate`、`viewData.rank = 1`。[FarmDateSystem.cpp](../../../project/application/farm/system/FarmDateSystem.cpp)。
- **E08** [FarmTimeAdvanceSystem.h](../../../project/application/farm/system/FarmTimeAdvanceSystem.h): `Start`、`AdvanceBatch`。[farm_time_advance_test.cpp](../../../tools/tests/farm_time_advance_test.cpp)。`project`のC++検索で定義以外の呼出しなし。テスト存在はUI接続の証拠ではない。
- **E09** [FarmEconomySystem.cpp](../../../project/application/farm/system/FarmEconomySystem.cpp)、[FarmSaleReviewSystem.h](../../../project/application/farm/system/FarmSaleReviewSystem.h)、[FarmRuntimeMenu.cpp](../../../project/application/farm/system/FarmRuntimeMenu.cpp)。
- **E10** [FarmStageAccessSystem.h](../../../project/application/farm/system/FarmStageAccessSystem.h): `kFarmStageRulesV1`。[FarmProgressionSystem.cpp](../../../project/application/farm/system/FarmProgressionSystem.cpp): `Initialize`、`SetMode`。購入/植付け/造成/プレビューへBind。
- **E11** [FarmContestEntrySystem.h](../../../project/application/farm/system/FarmContestEntrySystem.h)、[FarmContestSubmissionSystem.h](../../../project/application/farm/system/FarmContestSubmissionSystem.h)、[FarmContestJudgeSystem.h](../../../project/application/farm/system/FarmContestJudgeSystem.h)、[FarmContestGoalSystem.h](../../../project/application/farm/system/FarmContestGoalSystem.h)、[FarmContestSeasonSystem.h](../../../project/application/farm/system/FarmContestSeasonSystem.h)、[FarmContestPresentationSystem.h](../../../project/application/farm/system/FarmContestPresentationSystem.h)。
- **E12** [FarmDocumentSystem.cpp](../../../project/application/farm/system/FarmDocumentSystem.cpp): schema18。[FarmLayoutSystem.cpp](../../../project/application/farm/system/FarmLayoutSystem.cpp): 配置保存とロード。
- **E13** [FarmStartPresentationSystem.h](../../../project/application/farm/system/FarmStartPresentationSystem.h)、[FarmQualityView.h](../../../project/application/farm/ui/FarmQualityView.h): 開始目標と段階ガイド。
- **E14** [FarmToolSystem.cpp](../../../project/application/farm/system/FarmToolSystem.cpp): `kSelectableToolCount = 4`。[FarmToolBehavior.cpp](../../../project/application/farm/system/FarmToolBehavior.cpp): `FindFarmToolBehavior`はBugNet未対応。[GamePlaySessionEditor.cpp](../../../project/application/gameplay/GamePlaySessionEditor.cpp): BugNet選択拒否。

## 未実装判定の範囲

当該状態定義・更新ループ・System一覧・入力経路・保存・関連テストを調べた範囲での判定。提出フォルダや古いコピーにある名前を現在の実装とは扱わない。ゲームコード全体の安全性・DirectX12監査を実施したという意味でもない。
