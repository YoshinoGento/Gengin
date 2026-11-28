from pathlib import Path
import json
import hashlib
from tempfile import NamedTemporaryFile
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
labels = json.loads(r'''[["Menu","農場メニュー"],["Farm","農作業・売買"],["Terrain","地形・用水路"],["Records","セーブ・ロード"],["Observe","観察ノート"],["Settings","時間・カメラ"],["Resume","農場に戻る"],["Hoe","クワを選ぶ"],["Water","じょうろを選ぶ"],["Seed","種を選ぶ"],["Harvest","収穫道具を選ぶ"],["Turnip","カブ"],["Carrot","ニンジン"],["Buy","選んだ種を１個買う"],["Sell","選んだ作物を売る"],["SellAll","収穫物をすべて売る"],["Raise","土地を高くする"],["Lower","土地を低くする"],["Canal","用水路を設置・撤去"],["Source","水源を設置・撤去"],["Path","用水路をつなげる"],["RemovePath","用水路をまとめて撤去"],["Confirm","この変更を確定"],["Cancel","キャンセル"],["Undo","元に戻す"],["Redo","やり直す"],["Save","現在の農場を上書き保存"],["SaveCopy","新しい記録として保存"],["Load","選択した記録を開く"],["Previous","前の記録"],["Next","次の記録"],["Restart","農場を最初からやり直す"],["ConfirmDestructive","未保存の変更は失われます"],["Accept","変更を破棄して続行"],["Success","操作しました"],["Failure","現在の状態では実行できません"],["Saved","保存しました"],["Loaded","読み込みました"],["NoSaves","保存された記録はありません"],["PinA","選択マスを観察Ａに登録"],["PinB","選択マスを観察Ｂに登録"],["Start","生育比較を開始"],["Stop","比較を終了"],["Clear","比較記録をリセット"],["Pause","時間を止める"],["Play","時間を進める"],["Speed1","通常速度"],["Speed2","２倍速"],["Speed4","４倍速"],["Follow","プレイヤーを追う"],["Overview","農場を見渡す"],["Inventory","種の所持数 ／ 収穫物の個数 ／ 販売予定額"],["Money","所持金 ／ 選択中の種の価格"],["Tile","選択マス ／ 高さ ／ 水分％ ／ 成長％"],["Quality","成熟度 ／ 水分バランス ／ 地形適性"],["Score","品質点 ／ 収穫時の予想単価"],["Compare","観察Ａ ／ 観察Ｂ ／ 計測時間（秒）"],["WaterStats","接続水路数 ／ 給水対象マス数"],["Record","選択中の記録番号 ／ 保存件数"],["Day","日付 ／ 時間倍率"],["Paused","一時停止中"],["Preview","地形変更プレビュー"],["NoCrop","選択マスに作物はありません"],["Running","生育を計測中"],["NotRunning","計測していません"],["CompareIssue","同じ作物・高さ・成長度の２マスが必要です"],["Dirty","未保存の変更があります"],["Clean","保存済み"],["Ready","農場の記録"],["ClearLocked","目標達成済み"],["PathActive","用水路の配置"],["RemoveActive","用水路の撤去"],["Changed","変更マス数"],["Close","閉じる"],["SaveTime","保存日時"],["Selected","選択中"],["Exit","ゲームを終了"],["PauseCapture","時間・移動は停止中"],["Status","水分％ ／ 成長％"]]''')
labels.append(["LayoutLibrary", "配置だけを保存・再現"])
labels.extend([
    ["NextTool", "次の道具へ"], ["SwitchHoe", "クワに持ち替え"],
    ["SwitchWater", "じょうろに変更"], ["SwitchSeed", "種に持ち替え"],
    ["SwitchHarvest", "収穫道具に変更"], ["BuyAndReturn", "１個買って畑へ"],
])
labels.extend([
    ["ApplyHoe", "このマスを耕す"], ["ApplyWater", "水やりする"],
    ["ApplySeed", "種を植える"], ["ApplyHarvest", "収穫する"],
    ["OpenSeedShop", "種の購入へ"], ["ActionReady", "実行できます"],
    ["ActionNoTile", "マスが未選択"], ["ActionWaterFull", "水分は満タン"],
    ["ActionNoSeed", "種がありません"], ["ActionGrowing", "まだ成長中です"],
    ["ActionOtherTool", "別の道具を選択"],
])
labels.extend([
    ["TrialStart", "農場体験を始める"], ["TrialComplete", "目標金額を達成しました"],
    ["TrialGoal", "今回の目標：作物を売って所持金を増やす"],
    ["TrialMoney", "所持金 ／ 目標金額"], ["TrialDay", "農場の日付"],
    ["TrialGrow", "種を買い、耕した畑で作物を育てる"],
    ["TrialWater", "水不足と水過多を避け、生育を整える"],
    ["TrialSell", "育った作物を収穫し、売って目標へ"],
    ["TrialBegin", "この農場でプレイする"], ["TrialReview", "達成した農場を見る"],
    ["TrialResultNote", "収穫・販売の目標達成"],
    ["TrialRemaining", "未販売の作物 ／ 販売予定額"],
    ["TrialGoalOnly", "体験版の金額目標（コンテストとは別）"],
])
labels.extend([
    ["ObserveField", "畑を見ながら観察"], ["ObserveExit", "農作業に戻る"],
    ["PickA", "Ａを選び直す"], ["PickB", "Ｂを選び直す"],
    ["JumpA", "Ａのマスを選択"], ["JumpB", "Ｂのマスを選択"],
    ["PickingA", "観察Ａのマスを選択中"], ["PickingB", "観察Ｂのマスを選択中"],
    ["ObservationA", "観察Ａ"], ["ObservationB", "観察Ｂ"],
    ["Moisture", "水分"], ["Growth", "成長"], ["ReadySeconds", "収穫秒"],
    ["Elapsed", "計測時間"], ["ObservationEmpty", "未登録"],
    ["ObservationIdle", "比較の準備"], ["ObservationStopped", "計測終了・結果を保持"],
    ["ObservationCompleted", "両方が収穫可能・計測完了"],
    ["ObservationInvalid", "畑の変更により計測無効"],
    ["IssuePickTwo", "観察ＡとＢを登録してください"],
    ["IssueSame", "ＡとＢに別のマスを選んでください"],
    ["IssueNotGrowing", "両方に成長途中の作物が必要です"],
    ["IssueCrop", "同じ種類の作物を選んでください"],
    ["IssueHeight", "同じ高さのマスを選んでください"],
    ["IssueGrowth", "開始時の成長度が揃っていません"],
    ["IssueData", "畑の値が無効です・再登録してください"],
    ["CompareReady", "比較を開始できます"],
    ["KeepResults", "表示は計測終了時の値です"],
])
font_path = ROOT / "project/Resources/fonts/NotoSansJP-VF.ttf"
font = ImageFont.truetype(str(font_path), 26)
font.set_variation_by_axes([500])
labels.extend([
    ["SoilCare", "土・養分・適正水分"], ["Compost", "堆肥を混ぜる（無料）"],
    ["SoilNutrients", "土の養分（０～１００）"], ["NutrientTarget", "この作物の養分目安"],
    ["NutrientUse", "成長０～１００％で使う養分"], ["NutrientQuality", "育成中の養分充足度"],
    ["SoilHelp", "不足すると品質が低下。植える前に堆肥で準備"],
    ["SoilGrowing", "生育中：収穫後に堆肥を混ぜられます"],
    ["SoilFull", "養分は十分です。種を植えられます"],
    ["SoilTill", "まず土のマスを選び、クワで耕してください"],
    ["SoilReady", "堆肥を混ぜて養分を１００にできます"],
    ["SoilApplied", "堆肥を混ぜました。養分１００"],
    ["SoilSelectedCrop", "選択マスの作物（未植付けは選択中の種）"],
])
cell_w, cell_h = 640, 42
labels.extend([
    ["QualityOpen", "作物の品質を見る"],
    ["QualityCurrent", "選択マスの品質予測"], ["QualityHarvest", "直前の収穫結果"],
    ["QualityNoHarvest", "まだ収穫記録がありません"],
    ["QualityMaturity", "１　成熟度"], ["QualityWater", "２　水分バランス"],
    ["QualityTerrain", "３　地形適性"], ["QualityNutrient", "４　養分充足度"],
    ["QualityScale", "外周１００・１目盛２５"],
    ["QualityEstimate", "収穫前の予測です。生育状況で変化します"],
    ["QualityRecorded", "収穫時の記録です。不明な値は -- 表示"],
    ["QualityFinalPrice", "収穫時の品質点 ／ 確定単価"],
])
labels.extend([
    ["HintMaturity", "成熟度：成長１００％まで待って収穫"],
    ["HintMaturityNext", "次回：成長１００％まで育てて収穫"],
    ["HintWater", "水分：育成中の水不足と水過多を避ける"],
    ["HintTerrain", "地形：次回は植付け前に作物向けの高さへ"],
    ["HintNutrients", "養分：次回は植付け前に堆肥を混ぜる"],
    ["HintBalanced", "記録された各項目は良好です"],
    ["HintUnknown", "不明な項目あり。評価は参考値です"],
    ["HintPartialRecord", "収穫記録：一部の項目は不明です"],
    ["HintPartialEstimate", "品質予測：一部の項目は不明です"],
])
labels.extend([
    ["PathNonStraight", "縦・横に延長。曲がり角で止める"],
    ["PathBlocked", "追加できないマス"],
    ["ConfirmCandidates", "表示中の候補を確定"],
    ["PreviewStale", "畑が変わりました。取消して再配置"],
    ["RaiseBrush", "上げる：左ドラッグでマスを追加"],
    ["LowerBrush", "下げる：左ドラッグでマスを追加"],
    ["SizeForecast", "サイズ予測（基準比）"],
    ["SizeRecorded", "収穫サイズ（基準比）"],
])
labels.extend([
    ["HarvestInventory", "収穫物の保管庫"],
    ["HarvestColumns", "個数 ／ サイズ ／ 品質点 ／ 単価"],
    ["HarvestEmpty", "保管中の収穫記録はありません"],
    ["HarvestUnknown", "詳細不明の収穫物"],
    ["HarvestSaleNotice", "売った収穫物は保管庫からなくなります"],
    ["HarvestPrevious", "前のページ"], ["HarvestNext", "次のページ"],
    ["HarvestCount", "収穫記録数 ／ 上限"],
    ["HarvestFull", "保管庫が満杯"],
    ["HarvestLegacyNotice", "旧記録の個別品質・サイズは不明です"],
    ["HarvestPageNumber", "ページ"],
])
columns = 6
labels.extend([
    ["HarvestProtection", "売却保護"],
    ["HarvestProtect", "保護する"],
    ["HarvestProtected", "保護中"],
    ["ProtectedCropCount", "保護中の個数"],
    ["HarvestProtectionNotice", "保護中は売却対象外。再度選ぶと解除"],
    ["ProtectionCommitted", "保護を変更しました。操作履歴を確定"],
])
labels.extend([
    ["ContestReserve", "この１個を出品予約"],
    ["ContestCancel", "出品予約中・取消"],
    ["ContestEligibility", "出品予約の対象外"],
    ["HarvestDay", "収穫日"],
    ["HarvestDayUnknown", "収穫日不明"],
    ["ContestTurnip", "出品予約：カブ"],
    ["ContestCarrot", "出品予約：ニンジン"],
    ["ContestNone", "出品予約：なし"],
    ["ContestNotice", "予約は１個。取消後も保護は続きます"],
    ["ContestPreview", "予約した野菜の審査プレビュー"],
    ["ContestPreviewTitle", "審査プレビュー（仮ルール）"],
    ["ContestRecordedQuality", "記録済みの品質点"],
    ["ContestQualityPoints", "品質による得点"],
    ["ContestSizePoints", "大きさによる得点"],
    ["ContestTotal", "合計点（試算）"],
    ["ContestSizeRange", "大きさの採点範囲：０点～満点"],
    ["ContestRounding", "各得点を四捨五入して合計。範囲外は上下限を適用"],
    ["ContestPreviewNotice", "試算のみ：提出・消費・所持金の変更はしません"],
    ["ContestNotReserved", "保管庫で１個を保護し、出品予約してください"],
    ["ContestInvalid", "記録または採点設定が不正なため試算できません"],
    ["ContestChanged", "出品予約を変更。操作履歴を確定"],
])
labels.extend([
    ["ContestPeriod", "大会の対象期間"], ["EntryBackToScore", "採点の試算へ"],
    ["EntryContestDay", "対象の大会日"],
    ["EntryPeriodRange", "対象となる収穫日（両端を含む）"],
    ["EntryDaysRemaining", "大会までの日数（０は当日）"],
    ["EntryEligible", "対象期間内です（仮判定・提出前）"],
    ["EntryInvalidDay", "農場の日付が不正なため判定できません"],
    ["EntrySeasonEnded", "３０日を過ぎたため対象の大会はありません"],
    ["EntryUnknownDay", "収穫日不明：新しく収穫した野菜を予約"],
    ["EntryFutureDay", "収穫日が未来のため対象外です"],
    ["EntryOutsidePeriod", "対象期間外：この期間の野菜に予約を変更"],
    ["EntryPeriodRule", "仮ルール：１０日ごとの収穫物が対象"],
])
labels.extend([
    ["SubmitContest", "予約した１個を提出"],
    ["ContestSubmitted", "この大会は提出済み"],
    ["ContestDayOnly", "提出は大会当日のみ"],
    ["ContestCannotSubmit", "予約品を確認してください"],
    ["ContestResults", "大会の提出結果"],
    ["ContestSubmissionNotice", "提出で１個消費。結果は通常セーブに保存"],
    ["ContestSubmitConfirm", "１個を提出します。消費後は元に戻せません"],
    ["ContestSubmitAccept", "１個を提出する"],
    ["ContestResultColumns", "大会日 ／ 品質点 ／ 大きさ点 ／ 合計"],
    ["ContestNotSubmitted", "未提出"],
    ["ContestResultHarvest", "収穫日 ／ 収穫サイズ"],
    ["ContestResultNotice", "記録済みの得点です。順位・賞金は未実装"],
    ["ContestUpcoming", "開催前"],
    ["ContestOpen", "受付中"],
    ["ContestEntered", "提出済み"],
    ["ContestUnknown", "確認不可"],
    ["ContestMissed", "未出場"],
    ["ContestSeasonFinal", "３０日間の大会結果"],
    ["ContestAggregate", "提出数 ／ 未出場数 ／ 総得点"],
    ["SeasonMode", "３０日間の大会モード"],
    ["TrialMode", "５４０Ｇの体験版モード"],
    ["SeasonGoal", "土地と水を整え、育てた野菜を大会に提出"],
    ["SeasonPrepare", "１０・２０・３０日目に予約した１個を提出"],
    ["SeasonEndRule", "３０日目の提出か期限終了で結果を確認"],
    ["ModeConfirm", "進行ルールを切替。農場は維持、操作履歴は確定"],
    ["ModeAccept", "このモードに切り替える"],
    ["SeasonReview", "終了した農場を見る"],
    ["SeasonEnded", "大会終了"],
    ["SeasonRating", "大会評価（仮）"],
    ["SeasonUnrated", "大会評価：出場なし"],
    ["SeasonNextRating", "次の評価まで"],
    ["SeasonRatingRules", "３大会合計の評価基準"],
])
labels.extend([
    ["ContestDayTitle", "今日は野菜の大会です"],
    ["ContestDayStopped", "時間を止めました。予約した１個を提出できます"],
    ["ContestDayDeadline", "当日を過ぎると提出できません。自動提出はしません"],
    ["ContestDayReview", "出品画面へ（停止を維持）"],
    ["ContestDayPrepare", "畑で準備（停止を維持）"],
    ["ContestDayResume", "案内を閉じて時間を進める"],
])
labels.extend([
    ["IntakeOn", "給水 入"], ["IntakeOff", "給水 切"],
    ["IntakeClosed", "この畑の自動給水：切"],
    ["SupplyAvailable", "給水可能（実際の給水量とは異なります）"],
    ["SupplyRetained", "残り水で給水可能（水源からは切断）"],
    ["SupplyWaiting", "用水路への通水待ち"],
    ["SupplyDry", "用水路の水が不足：水源と接続を確認"],
    ["SupplyNone", "給水経路なし：用水路と高さを確認"],
    ["WaterAdviceUnknown", "水分状態を確認できません"],
    ["WaterAdviceTill", "耕してから給水できます"],
    ["WaterAdvicePlant", "種を植えてから生育を確認"],
    ["WaterAdviceHarvest", "収穫できます：追加の水やりは不要"],
    ["WaterAdviceWater", "水不足：じょうろで水やり"],
    ["WaterAdviceSupply", "水不足：給水後の水分量を確認"],
    ["WaterAdviceClose", "水過多：自動給水を切る"],
    ["WaterAdviceAvoid", "水過多：追加の水やりを控える"],
    ["WaterAdviceMonitor", "水分は適量：状態を見ながら育てる"],
    ["SpeedCompact1", "1倍"], ["SpeedCompact2", "2倍"], ["SpeedCompact4", "4倍"],
])
labels.extend([
    ["ShopTitle", "農業用品店"], ["ShopMoney", "所持金"],
    ["ShopTurnip", "カブの種"], ["ShopCarrot", "ニンジンの種"],
    ["ShopOwned", "所持数"], ["ShopUnitPrice", "単価"],
    ["ShopQuantity", "購入数"], ["ShopTotal", "合計"],
    ["ShopMinus", "－"], ["ShopPlus", "＋"],
    ["ShopChoose", "商品と個数を選択"], ["ShopReview", "購入内容を確認"],
    ["ShopConfirm", "右の内容で購入しますか？"], ["ShopPay", "この内容で購入"],
    ["ShopPurchased", "選んだ種を購入しました"],
    ["ShopChanged", "所持金などが変わりました。もう一度確認"],
    ["ShopNoMoney", "所持金が足りません"], ["ShopUnavailable", "この数量では購入できません"],
])
labels.extend([
    ["HarvestDisplay", "収穫物の展示台"], ["HarvestList", "一覧で確認"],
    ["DisplayPoints", "品質／大きさ"], ["DisplayQuantity", "所持数"],
    ["DisplayEstimate", "大会予想点"], ["DisplayReview", "取り置いた作物の出品確認へ"],
    ["DisplayArtNote", "絵は種類の見本です。品質・大きさは数値で比較"],
    ["DisplayReserved", "取り置き"], ["DisplayEligible", "対象期間内"],
    ["DisplayWrongPeriod", "対象期間外"], ["DisplayUnknownDay", "収穫日が不明"],
    ["DisplayNoContest", "大会は終了しました"],
    ["DisplayProtectFirst", "先に売却保護してください"],
])
labels.extend([
    ["Tomato", "トマト"], ["Pumpkin", "かぼちゃ"],
    ["OptimalWater", "この作物の適正水分"],
    ["ShopTomato", "トマトの種"], ["ShopPumpkin", "かぼちゃの種"],
    ["ContestTomato", "出品予約：トマト"], ["ContestPumpkin", "出品予約：かぼちゃ"],
    ["ShopPrevious", "前の種へ"], ["ShopNext", "次の種へ"],
])
labels.extend([
    ["FreeMode", "フリー農業モード"],
    ["FreeGoal", "土地と水を整え、自分のペースで野菜を育てる"],
    ["FreePrepare", "収穫・売買・地形づくりを自由に続けられます"],
    ["FreeEndRule", "日数・金額による終了なし。大会日の自動停止なし"],
])
labels.extend([
    ["SupplyAvailableShort", "給水中"], ["SupplyRetainedShort", "貯水あり"],
    ["SupplyWaitingShort", "給水待ち"], ["SupplyDryShort", "水源が空"],
    ["SupplyNoneShort", "水路未接続"], ["IntakeClosedShort", "給水停止中"],
    ["WaterAdviceUnknownShort", "水分を確認してください"],
    ["WaterAdviceTillShort", "まず耕しましょう"], ["WaterAdvicePlantShort", "種を植えられます"],
    ["WaterAdviceHarvestShort", "収穫できます"], ["WaterAdviceWaterShort", "水やりが必要です"],
    ["WaterAdviceSupplyShort", "給水経路を確認"], ["WaterAdviceCloseShort", "過湿：給水を止める"],
    ["WaterAdviceAvoidShort", "過湿：水やりを控える"], ["WaterAdviceMonitorShort", "水分は適量です"],
])
labels.extend([
    ["StartSource", "水源"], ["StartCanal", "水路"], ["StartFarm", "農場"],
    ["StartSkip", "スキップ"],
    ["ReturnTitle", "タイトルへ戻る"],
    ["ReturnTitleConfirm", "タイトルへ戻ります。未保存の変更は失われます"],
])
labels.extend([
    ["StageGuide", "大会の目標・解放"], ["StageUnlockDay", "解放日"],
    ["StageLocked", "この種はまだ解放されていません"],
    ["StageCurrent", "現在の段階"], ["StageHeight", "地形の上限"],
    ["StageThreshold", "合格点"], ["StagePassed", "入賞"], ["StageFailed", "入賞ならず"],
    ["StageShortfall", "あと必要な点数"], ["StageGuideColumns", "解放日 / 大会日 / 合格点 / 高さ上限"],
    ["StageBasicTools", "水源・水路・給水調整・堆肥は初日から使えます"],
    ["StageCropChoice", "表は新たに解放される種。以前の作物も出品可"],
    ["StageDateUnlock", "入賞・欠場に関係なく、日付で次の段階へ進みます"],
    ["StageLegacy", "この農場は段階解放の対象外です"],
])
labels.extend([
    ["ContestDetailTitle", "品評会・審査結果"], ["ContestActual", "出品した野菜"],
    ["ContestIdeal", "入賞の一例：品質１００点の場合"],
    ["ContestNoReference", "この記録には比較基準がありません"],
    ["ContestGameWeight", "ゲーム内換算重量"],
    ["ContestIdealNote", "右は採点上の一例。唯一の正解ではありません"],
    ["ContestWeightNote", "換算重量は大きさ倍率から算出した表示値です"],
    ["ContestRadarLegend", "１ 成熟　２ 水分　３ 地形　４ 栄養（外周１００％）"],
    ["ContestAxesNote", "各軸に合格条件はなく、品質と大きさの合計で審査"],
    ["ContestJudging", "審査結果を確認中"],
    ["ContestMoreGrams", "同じ品質での不足重量"],
    ["ContestImproveQuality", "大きさだけでは届きません。品質の改善が必要"],
    ["ContestTargetMet", "今回の入賞基準を満たしました"],
    ["ContestDetailBack", "結果一覧へ"], ["ContestReveal", "結果を表示"],
    ["ContestDetails", "詳しく見る"],
    ["ContestEventDay", "開催日"],
])
labels.extend([
    ["OutcomeMissed", "出品がないまま、大会が終了しました"],
    ["OutcomeNoScore", "未出品のため、採点はありません"],
    ["OutcomeReservation", "取り置きだけでは出品されません"],
    ["OutcomeSubmitHint", "大会当日に出品を確定してください"],
    ["OutcomeKeepFarm", "農場はそのまま、次の大会へ進めます"],
    ["OutcomeContinue", "農場を続ける"],
    ["OutcomeFinal", "３０日間の品評会を終えて"],
    ["OutcomeFinalPassed", "今シーズンの成績"],
    ["OutcomeFinalFailed", "挑戦終了：最終大会は目標点未満"],
    ["OutcomeFinalMissed", "挑戦終了：最終大会に未出品"],
    ["OutcomeFinalLegacy", "最終大会：出品済み"],
    ["OutcomeNoRank", "出品がないため、総合評価はありません"],
    ["OutcomeRankNote", "総合ランクは３大会の合計点で決まります"],
    ["OutcomeRetry", "結果を保存して再挑戦"],
    ["OutcomeRetryTitle", "結果を残して、新しい農場を始めますか？"],
    ["OutcomeRetryCopy", "今回の農場と結果を、別の記録に保存します"],
    ["OutcomeRetryNew", "新しい農場は１日目から。元の記録は残ります"],
    ["OutcomeSaveFailed", "保存・開始に失敗しました。農場は未変更です"],
    ["OutcomeReviewHint", "詳しい審査結果から、次の育て方を確認できます"],
    ["OutcomeBack", "シーズン結果へ"],
    ["OutcomeRank", "総合ランク"], ["OutcomePoints", "得点／合格点"],
])
labels.extend([
    ["ContestTabResult", "結果"], ["ContestTabQuality", "品質"], ["ContestTabSize", "大きさ"],
    ["ContestExplanation", "採点のしくみ"], ["ContestSummaryBack", "結果に戻る"],
    ["ContestScoreShort", "得点"], ["ContestTargetShort", "入賞ライン"],
    ["ContestGapShort", "入賞まであと"], ["ContestPassedShort", "入賞ラインを達成しました"],
    ["ContestLegacyShort", "この記録は合否判定なし"],
    ["ContestHintTitle", "次の育成のヒント"], ["ContestQualityNeeded", "重さだけでなく品質も改善する"],
    ["ContestReviewMaturity", "見直す項目：成熟"], ["ContestReviewWater", "見直す項目：水分"],
    ["ContestReviewTerrain", "見直す項目：地形"], ["ContestReviewNutrients", "見直す項目：養分"],
    ["ContestHintMaturity", "十分に育ってから収穫する"],
    ["ContestHintWater", "給水を調整して適量を保つ"],
    ["ContestHintTerrain", "作物に合う土地の高さを選ぶ"],
    ["ContestHintNutrients", "植付け前に堆肥を混ぜる"],
    ["ContestHintBalanced", "今回の育て方を次にも生かす"],
    ["ContestHintUnknown", "育成記録がなく、助言できません"],
    ["ContestPartialShort", "記録のある項目だけで評価"],
    ["ContestQualityScoreShort", "品質の得点"], ["ContestSizeScoreShort", "大きさの得点"],
    ["ContestAxisMaturity", "成熟"], ["ContestAxisWater", "水分"],
    ["ContestAxisTerrain", "地形"], ["ContestAxisNutrients", "養分"],
    ["ContestRadarScaleShort", "外周は１００％"],
    ["ContestAxesShort", "各項目に合格ラインはありません"],
    ["ContestExampleShort", "入賞する大きさの一例"],
    ["ContestExampleCondition", "品質が満点の場合"],
    ["ContestWeightShort", "換算重量"],
    ["ContestMoreShort", "今の品質なら入賞まで"],
    ["ContestRuleShort", "品質６０点＋大きさ４０点で採点"],
    ["ContestWeightRuleShort", "重さはゲーム内の換算値です"],
])
labels.extend([
    ["ExhibitionTitle", "大会演出の確認"], ["ExhibitionEntry", "大会演出の確認"],
    ["ExhibitionGoal", "品評会の結果を選ぶ"],
    ["ExhibitionRecorded", "育成操作なしで、３種類の結果を再生できます"],
    ["ExhibitionWatch", "演出を見る"], ["ExhibitionAgain", "もう一度見る"],
    ["ExhibitionHome", "デモメニューへ"],
    ["ExhibitionMissing", "デモ用データを読み込めません"],
    ["ClearGalleryTitle", "クリア演出コレクション"],
    ["ClearGalleryHome", "ほかのセリフを見る"],
    ["GalleryPerfect", "かぼちゃ：満点"],
    ["GalleryPumpkinPot", "かぼちゃ：鍋"],
    ["GalleryPumpkinFestival", "かぼちゃ：収穫祭"],
    ["GalleryCarrotCurry", "ニンジン：カレー"],
    ["GalleryCarrotBundle", "ニンジン：団体戦"],
    ["GalleryTomatoCenter", "トマト：センター"],
    ["GalleryTomatoLunch", "トマト：弁当箱"],
    ["GalleryPumpkinStar", "かぼちゃ：主役"],
    ["GalleryLargeDelivery", "かぼちゃ：搬出"],
    ["GalleryLargeShop", "ニンジン：売り場"],
    ["GalleryQualityMuseum", "トマト：美術館"],
    ["GalleryQualityKnife", "ニンジン：包丁"],
    ["GalleryNutrientSoil", "ニンジン：土"],
    ["GalleryWaterCare", "かぼちゃ：水加減"],
    ["GalleryPortrait", "トマト：記念写真"],
    ["GalleryGuests", "トマト：来賓席"],
])
labels.extend([
    ["MenuFarm", "農作業"], ["MenuTerrain", "水路・地形"], ["MenuRecords", "セーブ"],
    ["MenuObserve", "育成比較"], ["MenuDetails", "詳しい操作"], ["MenuSummary", "一覧に戻る"],
    ["MenuWork", "農場で行うこと"], ["MenuTerrainWork", "土地と水の流れを整える"],
    ["MenuSaveHeading", "現在の農場を保存"], ["MenuLoadHeading", "保存データを選ぶ"],
    ["MenuCompareHeading", "２つの畑の育ち方を比べる"],
    ["MenuCompareA", "比較する畑Ａ"], ["MenuCompareB", "比較する畑Ｂ"],
    ["MenuSoundHeading", "サウンド"], ["MenuCameraHeading", "カメラ"], ["MenuModeHeading", "ゲームモード"],
    ["MenuAudioOpen", "音量を変更"], ["MenuAudioTitle", "音量設定"],
    ["MenuEnvironment", "環境音"], ["MenuWaterAudio", "水音"], ["MenuContestAudio", "審査演出"],
    ["MenuAudioReset", "初期値に戻す"], ["MenuAudioBack", "戻る"], ["MenuAudioFailed", "音量を保存できません"],
    ["MenuSeedCount", "選択中の種の数"], ["MenuModeSeason", "３０日間の大会"],
    ["MenuModeTrial", "目標金額に挑戦"], ["MenuModeFree", "フリー農業"],
    ["MenuSettings", "設定"],
])
labels.extend([
    ["TradeOpen", "収穫物・売却"], ["TradeTitle", "収穫物の売却"],
    ["TradeDetails", "保管・大会の詳細"], ["TradeBack", "売却画面に戻る"],
    ["TradeAll", "すべての作物"], ["TradeOwned", "所持数"],
    ["TradeCount", "売る個数"], ["TradeValue", "受取額"],
    ["TradeRemaining", "売却後の所持金"], ["ShopRemaining", "購入後の残金"],
    ["TradeProtected", "保護して残す個数"], ["TradeReview", "売却内容を確認"],
    ["TradeConfirm", "この内容で売却"], ["TradeQuestion", "選択した作物を売却しますか？"],
    ["TradeProtectionNote", "保護中の作物は売却されません"],
    ["TradeEmpty", "売却できる作物がありません"], ["TradeSold", "売却しました"],
    ["TradeChanged", "所持品が変わりました。もう一度確認してください"],
    ["TradeUnavailable", "現在は売却できません"],
])
labels.extend([
    ["PestWarning", "害虫の予兆：畑を確認"],
    ["PestActive", "害虫発生：畑を確認"],
    ["PestCure", "虫取り網で駆除"],
    ["PestWarningDetail", "明日から品質低下"],
    ["PestActiveDetail", "放置で品質低下"],
    ["PestPenalty", "害虫による品質減点"],
    ["PestAdvice", "予兆のうちに虫取り網で駆除"],
    ["PestFocusOne", "害虫の畑へ（1マス）"],
    ["PestFocusTwo", "害虫の畑へ（2マス）"],
    ["PestFocusThree", "害虫の畑へ（3マス）"],
    ["PestCureOne", "駆除する（残り1マス）"],
    ["PestCureTwo", "駆除する（残り2マス）"],
    ["PestCureThree", "駆除する（残り3マス）"],
    ["PestSpreadWarning", "明日から被害・放置で拡散"],
    ["PestSpreadActive", "隣へ拡散・早めに駆除"],
])
labels.extend([
    ["GoalSubmit", "次：大会へ提出する"],
    ["GoalProtectReserve", "次：出品候補を保護・予約"],
    ["GoalReserve", "次：出品作物を予約する"],
    ["GoalReserved", "大会の出品予約済み"],
    ["GoalSoil", "次：作業する畑を選ぶ"],
    ["GoalTill", "次：畑を耕す"],
    ["GoalPlant", "次：種を植える"],
    ["GoalChooseCrop", "次：別の種を選ぶ"],
    ["GoalBuy", "次：種を買う"],
    ["GoalSell", "次：収穫物を売る"],
    ["GoalFunds", "種を買う資金が不足"],
    ["GoalCheckWater", "次：給水の状態を確認"],
    ["GoalWater", "次：水やりをする"],
    ["GoalReduceWater", "水やりを控える"],
    ["GoalGrow", "作物を育成中"],
    ["GoalHarvest", "次：作物を収穫する"],
    ["GoalFull", "収穫物の保管庫が満杯"],
    ["GoalWhyContest", "右の数字は大会の開催日"],
    ["GoalWhySeeds", "植えられる種を確保する"],
    ["GoalWhyWater", "水分を適量に近づける"],
    ["GoalWhyHarvest", "保管庫の空きを確認する"],
    ["GoalWhyFunds", "種の在庫と所持金を確認"],
    ["GoalResumeGrowth", "次：時間を進める"],
    ["GoalStopIrrigation", "次：自動給水を切る"],
])
labels.extend([
    ["TimeAdvance", "変化があるまで時間送り"],
    ["TimeRunning", "時間送り中"], ["TimeCancel", "時間送りを取り消す"],
    ["TimeHold", "停止中：再開は再生ボタン"],
    ["TimeHarvest", "停止：収穫できる畑あり"],
    ["TimeDry", "停止：水分が不足"], ["TimeExcess", "停止：水分が過剰"],
    ["TimePest", "停止：害虫の予兆・被害"], ["TimeContest", "停止：大会の開催日"],
    ["TimeFinished", "停止：シーズン終了"], ["TimeNoTarget", "停止：待つ目標がありません"],
    ["TimeBlocked", "停止：画面・操作状態の変更"],
    ["TimeBudget", "停止：時間送りの上限"], ["TimeInvalid", "停止：農場の状態を確認"],
])
labels.extend([
    ["TimeWeather", "停止：天気予報・天候の変化"],
    ["WeatherForecast", "明日は雨：畑の水分と取水を確認"],
    ["WeatherRain", "雨：土に給水中。取水の重複に注意"],
    ["WeatherClear", "晴れ：雨からの給水なし"],
])
labels.extend([
    ["Development", "農場ランク・土地拡張"],
    ["DevelopmentRank", "農場ランク"],
    ["DevelopmentArea", "農地マス数"],
    ["DevelopmentSales", "累計売上"],
    ["DevelopmentNext", "次のランクまで"],
    ["DevelopmentMax", "最高ランクに到達"],
    ["DevelopmentRule", "売上と進行に応じ、資金で農地を拡張"],
    ["DevelopmentExpand", "５マス拡張の確認へ"],
    ["DevelopmentCustom", "この形の農場は拡張対象外"],
    ["DevelopmentFull", "農地は最大まで拡張済み"],
    ["DevelopmentLocked", "未解放、または現在は操作できません"],
    ["DevelopmentDone", "農地が５マス広がりました"],
])
labels.extend([
    ["CropBook", "この農場の収穫記録"],
    ["Repellent", "虫よけ植物"],
    ["RepellentTarget", "選択マス"],
    ["RepellentPlacePreview", "設置後の保護範囲（中央が設置先）"],
    ["RepellentRemovePreview", "撤去後の保護範囲（中央が撤去先）"],
    ["RepellentOutside", "農地外"],
    ["RepellentWater", "水路・水源"],
    ["RepellentCrop", "作物"],
    ["RepellentSoil", "空き地"],
    ["RepellentProtected", "保護あり"],
    ["RepellentUnprotected", "保護なし"],
    ["RepellentRule", "範囲：上下左右１マス"],
    ["RepellentCost", "費用なし・農地１マスを使用"],
    ["RepellentCureRule", "発生済みの虫は虫取り網で対処"],
    ["RepellentPrevention", "新しい害虫の発生を防ぐ"],
    ["RepellentDisabled", "このモードでは害虫は発生しない"],
    ["RepellentPlace", "このマスに設置"],
    ["RepellentRemove", "この植物を撤去"],
    ["RepellentEmptyOnly", "作物・水路のあるマスは設置不可"],
    ["BookCollected", "収穫した種類"],
    ["BookQuality", "最高品質"],
    ["BookWeight", "最大重量"],
    ["BookCollectGoal", "目標：３種類すべてを収穫"],
    ["BookImprove", "３種類達成：次は自己記録の更新"],
    ["BookIndependent", "品質と重量は別々の最高記録。未記録は --"],
    ["Recovery", "栽培の再開支援"],
    ["RecoveryOffer", "支援内容：ニンジンの種１個"],
    ["RecoveryRule", "種・栽培中の作物・収穫在庫・購入資金がないとき"],
    ["RecoveryEligible", "支援を受け取れます"],
    ["RecoveryClaim", "種を１個受け取る"],
    ["RecoveryDaily", "今日の支援は受取済み"],
    ["RecoveryTomorrow", "畑に戻って時間を進め、翌日に再確認できます"],
    ["RecoverySeeds", "使える種を持っています"],
    ["RecoveryGrowing", "農地に作物が残っています"],
    ["RecoveryCare", "残っている作物を手入れして収穫してください"],
    ["RecoveryHarvest", "収穫した野菜が残っています"],
    ["RecoveryProtected", "保護・出品予約中も対象。売却は自分で選べます"],
    ["RecoveryBuy", "解放済みの種を購入できる資金があります"],
    ["RecoveryNext", "畑に戻り、耕したマスに持っている種をまきましょう"],
    ["RecoveryNoCash", "１日１回。お金・作物・大会結果は変更しません"],
    ["RecoveryBlocked", "今は支援を利用できません"],
    ["RecoveryDone", "ニンジンの種を１個受け取りました"],
    ["RecoveryHelp", "種がないとき：再開支援"],
    ["PreviousTile", "前のマス"],
    ["NextTile", "次のマス"],
    ["DevelopmentSalesGoal", "累計売上 / 必要売上"],
    ["DevelopmentDay", "解放日"],
    ["DevelopmentDayReady", "解放済み（日付）"],
    ["DevelopmentNoDay", "この農場には日付の制限はありません"],
    ["DevelopmentCost", "拡張費"],
    ["DevelopmentReserve", "残しておく種２個分の資金"],
    ["DevelopmentWaitDay", "解放日以降に拡張できます。入賞は不要です"],
    ["DevelopmentNeedSales", "必要売上まで、収穫した野菜を売りましょう"],
    ["DevelopmentNeedFunds", "拡張費と種２個分の資金を用意しましょう"],
    ["DevelopmentConfirm", "資金を使って農地を広げますか？"],
    ["DevelopmentAfter", "拡張後の所持金"],
    ["DevelopmentKeep", "今ある作物・水路はそのまま残ります"],
    ["DevelopmentChanged", "状態が変わりました。条件を再確認してください"],
    ["DevelopmentPay", "支払って５マス広げる"],
])

labels.extend([
    ["Experience", "システム体験"], ["ExperienceCrops", "栽培と売買"],
    ["ExperienceEnvironment", "水と環境"], ["ExperienceSeason", "大会と成長"],
    ["ExperienceSupport", "記録と支援"], ["ExperienceBegin", "この体験を始める"],
    ["ExperienceOperation", "体験の操作を開く"], ["ExperienceFree", "フリーモードの体験用農場"],
    ["ExperienceSeedTool", "種まき道具を選ぶ"], ["ExperienceHarvestTool", "収穫道具を選ぶ"],
    ["ExperiencePestFocus", "害虫の畑を選ぶ"],
    ["ExperienceContest", "大会ルールの体験用農場"], ["ExperienceConfirm", "準備済みの農場へ切り替えますか？"],
    ["ExperienceDiscard", "今の未保存の変更は失われます"],
    ["ExperienceKeep", "保存済みの農場には上書きしません"],
    ["ExperienceMissing", "体験データを開けません。現在の農場を保持しました"],
    ["ExperiencePlant", "種まき・栽培"], ["ExperiencePlantGoal", "３種類の種から野菜を育てる"],
    ["ExperienceHarvest", "収穫"], ["ExperienceHarvestGoal", "成熟した３種類の野菜を収穫する"],
    ["ExperienceSoil", "土づくり・品質"], ["ExperienceSoilGoal", "種まき前に土の栄養を補給する"],
    ["ExperienceTrade", "売買・保護"], ["ExperienceTradeGoal", "保護した野菜を残して、在庫を売却する"],
    ["ExperienceWater", "水源・水路・高低差"], ["ExperienceWaterGoal", "水路と高さを工夫し、畑に水を届ける"],
    ["ExperienceRain", "雨・給水"], ["ExperienceRainGoal", "雨の日のトマトを水カビから守る"],
    ["ExperiencePests", "害虫・駆除"], ["ExperiencePestsGoal", "発生した害虫を駆除して、被害を抑える"],
    ["ExperienceRepellent", "防虫・被害の広がり"], ["ExperienceRepellentGoal", "防虫植物で新しい害虫の発生を防ぐ"],
    ["ExperienceTime", "時間送り・次の目標"], ["ExperienceTimeGoal", "収穫や出来事までの待ち時間を短くする"],
    ["ExperienceJudging", "品評会・出品予約"], ["ExperienceJudgingGoal", "用意した野菜を選び、審査に出す"],
    ["ExperienceResults", "審査結果・講評"], ["ExperienceResultsGoal", "品質・大きさの結果と次の目標を見る"],
    ["ExperienceDevelopment", "解放・土地拡張"], ["ExperienceDevelopmentGoal", "作物と水路を残して、農地を５マス広げる"],
    ["ExperienceBook", "作物図鑑"], ["ExperienceBookGoal", "収穫した野菜の自己ベストを調べる"],
    ["ExperienceCompare", "生育比較"], ["ExperienceCompareGoal", "水分が違う２つの畑で育ち方を比べる"],
    ["ExperienceRecovery", "資金・種切れの救済"], ["ExperienceRecoveryGoal", "支援の種を受け取り、栽培を再開する"],
    ["ExperienceRecords", "保存・配置の記録"], ["ExperienceRecordsGoal", "新しい記録として保存し、再び開く"],
    ["ExperienceIntroduction", "水路と土を整え、よい野菜を育てるゲームです"],
    ["ExperiencePlantStep", "まず：種を選び、耕したマスにまく"],
    ["ExperiencePlantWatch", "注目：芽の見た目と成長率"],
    ["ExperienceHarvestStep", "まず：収穫道具で育った野菜を収穫"],
    ["ExperienceHarvestWatch", "注目：収穫物の数・品質・重さ"],
    ["ExperienceSoilStep", "まず：選択中の畑に堆肥を混ぜる"],
    ["ExperienceSoilWatch", "注目：土の養分。品質は栽培して確認"],
    ["ExperienceTradeStep", "まず：収穫物一覧で売却を選ぶ"],
    ["ExperienceTradeWatch", "注目：保護した野菜と売却後の所持金"],
    ["ExperienceWaterStep", "まず：水路・地形で給水経路を確認"],
    ["ExperienceWaterWatch", "注目：水の届くマスと畑の水分"],
    ["ExperienceRainStep", "まず：左のトマトの給水を切る"],
    ["ExperienceRainWatch", "注目：左は回復、右は放置で発病"],
    ["ExperiencePestsStep", "まず：害虫の畑を選び、駆除する"],
    ["ExperiencePestsWatch", "注目：害虫の印。受けた被害は残る"],
    ["ExperienceRepellentStep", "まず：作物の隣の空きマスに植える"],
    ["ExperienceRepellentWatch", "注目：上下左右を保護。駆除とは別"],
    ["ExperienceTimeStep", "まず：変化があるまで時間送り"],
    ["ExperienceTimeWatch", "注目：自動で止まった理由と次の目標"],
    ["ExperienceJudgingStep", "まず：出品内容を確認して審査へ"],
    ["ExperienceJudgingWatch", "注目：品質と大きさによる得点"],
    ["ExperienceResultsStep", "まず：結果・品質・大きさを切り替え"],
    ["ExperienceResultsWatch", "注目：入賞までの差と改善する項目"],
    ["ExperienceDevelopmentStep", "まず：拡張の費用を確認して購入"],
    ["ExperienceDevelopmentWatch", "注目：増えた農地と残りの所持金"],
    ["ExperienceBookStep", "まず：作物図鑑を開く"],
    ["ExperienceBookWatch", "注目：種類ごとの最高品質・最大重量"],
    ["ExperienceCompareStep", "まず：２つの畑をＡ・Ｂに登録し開始"],
    ["ExperienceCompareWatch", "注目：時間経過による成長の差"],
    ["ExperienceRecoveryStep", "まず：再開支援で種を１個受け取る"],
    ["ExperienceRecoveryWatch", "注目：ニンジンの種が増え、栽培可能"],
    ["ExperienceRecordsStep", "まず：新しい記録として保存"],
    ["ExperienceRecordsWatch", "注目：読み込むと畑や所持品が戻る"],
])
labels.extend([
    ["CampaignTab", "大会・クリア"],
    ["CampaignStart", "大会を初日から始める"],
    ["CampaignExhibit", "展示：クリア直前から"],
    ["FinalSubmission", "展示：最終出品・クリア"],
    ["FinalFailure", "展示：目標点未満"],
    ["FinalMissed", "展示：未出品で終了"],
    ["CampaignGoal", "目標：３０日目の品評会で８５点以上"],
    ["CampaignStep", "まず：畑を耕し、堆肥と種を準備"],
    ["CampaignWatch", "１０・２０日目も出品し、最後を目指す"],
    ["CampaignFresh", "新規大会：１日目・３００Ｇから"],
    ["FinalSubmissionGoal", "最終出品からクリアまでを体験"],
    ["FinalSubmissionStep", "まず：出品内容を確認し、提出する"],
    ["FinalSubmissionWatch", "展示用の作物で、通常と同じ審査"],
    ["FinalFailureGoal", "点数が足りない場合の結果を確認"],
    ["FinalFailureStep", "まず：予約済みの作物を提出する"],
    ["FinalFailureWatch", "クリアと区別し、再挑戦へ進める"],
    ["FinalMissedGoal", "提出せず期限を過ぎた場合を確認"],
    ["FinalMissedStep", "開始すると大会の終了結果を表示"],
    ["FinalMissedWatch", "農場を見るか、再挑戦を選べる"],
])
labels.extend([
    ["FinalClearTitle", "大会クリア"], ["FinalClearBegin", "大会クリアへ"],
    ["FinalClearSkip", "演出を省略"], ["FinalClearContinue", "成績一覧へ"],
])
labels.extend([
    ["CheerAudienceLeft", "左の観客"], ["CheerAudienceRight", "右の観客"],
])
cheer_data = json.loads((ROOT / "tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
assert cheer_data["schemaVersion"] == 3
for pair in cheer_data["pairs"]:
    for side in ("L", "R"):
        assert len(pair[side]) == 2
        labels.extend([f"Cheer{pair['id']}{side}{i}", line] for i, line in enumerate(pair[side]))
labels.extend([
    ["RainCanClose", "雨：この株は取水を止めても育つ"],
    ["RainOnly", "雨：この株は雨だけで水分が足りる"],
    ["MoldWarning", "水カビ予兆：取水を切る"], ["MoldWait", "水カビ予兆：乾き待ち"],
    ["MoldSick", "水カビ発病：取水を切る"], ["MoldSickClosed", "水カビ発病：乾き待ち"],
    ["MoldRecovery", "水カビ：回復中"], ["TimeMold", "水カビ：畑を確認"],
    ["Fungicide", "防カビ剤"], ["FungicideInfo", "１株の水カビを20秒予防。発病後は使えません"],
    ["FungicideBought", "購入しました。畑で株を選んで散布できます"],
    ["FungicideApply", "この株に防カビ剤"], ["FungicideActive", "水カビ予防中"],
    ["FungicideSick", "発病後は散布できません"], ["FungicidePick", "生育中の株を選択"],
])
labels.extend([
    ["MainPurpose", "水路で畑に水を届け、育てた野菜を品評会に出そう。"],
    ["MainFirstPlot", "まずは畑を１マス選び、クワで耕してみよう。"],
    ["MainEnter", "畑へ"], ["MainTill", "クワを選び、右下で耕す"],
    ["MainBuy", "右の店で種を１つ買う"], ["MainPlant", "種を選び、右下で植える"],
    ["MainWater", "じょうろで水をやる"], ["MainHarvest", "かごを選んで収穫する"],
])
labels.extend([
    ["LessonBegin", "水路を体験して畑へ"], ["LessonIntro", "まずは水路を１マスつないでみよう。"],
    ["LessonTitle", "水路の練習畑"], ["LessonMoisture", "トマトの水分"],
    ["LessonChoose", "水路の間の空きマスを選んでみよう。"],
    ["LessonSecond", "右のトマトにも水を届けてみよう。"],
    ["LessonMiss", "そこには置けません。空きマスを選ぼう。"],
    ["LessonYes", "仮置き：この水路ならトマトに届きます。"],
    ["LessonNo", "仮置き：ここではトマトに届きません。"],
    ["LessonFlow", "水路がつながりました。水が流れています。"],
    ["LessonReached", "水が届いて、トマトの水分が増えました。"],
    ["LessonPreview", "選んだマスに仮置き"], ["LessonConnect", "ここにつなぐ"],
    ["LessonUndo", "選び直す"], ["LessonAgain", "別の株にも挑戦"],
    ["LessonExit", "練習を終えて畑へ"],
    ["GapTitle", "畑に水を届けよう"],
    ["GapSource", "青い水が水源。水路から隣の畑に水がしみこむ。"],
    ["GapHint", "水源と水路の間の土をクリックしよう。"],
    ["GapMiss", "そこではなく、水源のすぐ隣の枠を選ぼう。"],
    ["GapDig", "枠の中に水路を掘る"], ["GapSkip", "今は掘らずに遊ぶ"],
    ["GapDone", "水路がつながった！　水の行き先を見てみよう。"],
    ["FieldDetails", "畑の詳しい表示"], ["FieldSimple", "シンプル表示に戻す"],
])
labels.extend([
    ["ForecastGain", "＋ 給水が強まる畑"], ["ForecastLoss", "－ 給水が弱まる畑"],
    ["ForecastStrength", "水源からの給水力（目安）"],
    ["ForecastHover", "畑にカーソルを重ねて給水力を比較"],
    ["ForecastEmpty", "耕した畑が比較の対象です"],
    ["ForecastClosed", "この畑は取水を停止しています"],
])
labels.extend([["WheelTools", "ホイール：道具"], ["WheelSeeds", "ホイール：種"]])
labels.extend([
    ["WheelHoe", "クワ"], ["WheelWater", "じょうろ"], ["WheelBag", "種袋"], ["WheelBasket", "収穫かご"],
    ["WheelCandidateCarrot", "候補：ニンジン"], ["WheelCandidateTomato", "候補：トマト"],
    ["WheelCandidatePumpkin", "候補：かぼちゃ"],
    ["GuideOpen", "操作  H"], ["GuideDetail", "詳細表示"],
    ["GuideTitle", "畑の操作"], ["GuideMouse", "マウス"], ["GuideKeys", "キーボード"],
    ["GuideBoth", "キー＋マウス"], ["GuidePad", "パッド"],
    ["GuideWork", "作業する"], ["GuideAim", "マスを選ぶ"], ["GuideEquip", "道具を選ぶ"],
    ["GuideBag", "種袋を持つ／戻す"], ["GuideSeeds", "種を選ぶ"],
    ["GuideClick", "畑を左クリック"], ["GuideWheel", "畑の上でホイール"],
    ["GuideRight", "畑を短く右クリック"], ["GuideBagWheel", "種袋に持ち替えてホイール"],
    ["GuideArrows", "矢印キー"], ["GuideQE", "Q で前へ、E で次へ"], ["GuideEnter", "Enter"],
    ["GuideKeySeeds", "C → 左右で選ぶ → Enter"],
    ["GuideMixedWork", "Q / E で道具を選び、畑をクリック"],
    ["GuideMixedSeeds", "C → 畑の上でホイール → 畑をクリック"],
    ["GuideMenu", "農場メニュー"], ["GuidePadMenu", "畑で B"],
    ["GuidePadChoose", "メニューの選択"], ["GuidePadButtons", "十字キーで選ぶ、A で決定、B で戻る"],
    ["GuideFirstWater", "最初の水路案内"], ["GuidePadWater", "A で掘る、B でスキップ"],
    ["GuidePadScope", "通常の農作業はマウスかキーボードで"],
    ["GuideFooter", "上下で切替　Esc / B で閉じる"],
    ["HoverHoe", "クリック：クワを持つ"], ["HoverWater", "クリック：じょうろを持つ"],
    ["HoverBag", "クリック：種を選ぶ"], ["HoverBasket", "クリック：収穫道具を持つ"],
    ["HoverCarrot", "クリック：ニンジンの種を持つ"], ["HoverTomato", "クリック：トマトの種を持つ"],
    ["HoverPumpkin", "クリック：かぼちゃの種を持つ"],
    ["GuidePlaqueHelp", "操作"], ["GuidePlaqueSimple", "簡易表示"],
    ["WalkEnter", "畑に降りる"], ["WalkLeave", "見渡す"],
    ["WalkKeys", "WASD / 矢印で歩く　Enter を押しながら引く"],
    ["WalkMouse", "左ドラッグで鍬を引く　右で歩く"],
    ["PondCut", "右の土手から、鍬を畑へ引く"],
])
clear_font = ImageFont.truetype(str(font_path), 72)
clear_font.set_variation_by_axes([700])
def label_font(name):
    return clear_font if name == "FinalClearTitle" else font

goal_debug_labels = labels[next(i for i, row in enumerate(labels) if row[0] == "PestWarning"):]
assert len({name for name, _ in labels}) == len(labels)
atlas_width = cell_w * columns
measure = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
rects = []
x = y = 0
row_height = cell_h
# Variable-width shelves retain stable label IDs without allocating a second texture.
for name, text in labels:
    width = int(measure.textlength(text, font=label_font(name))) + 4
    label_height = 88 if name == "FinalClearTitle" else 38
    assert width <= cell_w, (name, width)
    if x + width > atlas_width:
        x, y = 0, y + row_height
        row_height = cell_h
    row_height = max(row_height, label_height+4)
    rects.append((x, y, width, label_height))
    x += width + 4
ascii_y = y + row_height
bubble_y = ascii_y + 128
height = bubble_y + 140
assert height <= 4096, "Split the label atlas before adding more rows"
atlas = Image.new("RGBA", (atlas_width, height))
draw = ImageDraw.Draw(atlas)
for (name, text), (x, y, _, _) in zip(labels, rects):
    draw.text((x + 2, y + 4), text, fill="white", font=label_font(name), anchor="lt")
for (name, text), (x, y, width, height_) in zip(labels, rects):
    left, top, right, bottom = draw.textbbox((x + 2, y + 4), text, font=label_font(name), anchor="lt")
    assert x <= left <= right <= x + width and y <= top <= bottom <= y + height_, name
    assert atlas.getchannel("A").crop((x, y, x + width, y + height_)).getbbox(), name
ascii_font = ImageFont.truetype(str(font_path), 24)
ascii_font.set_variation_by_axes([500])
for i in range(95):
    x, y = (i % 40) * 32, ascii_y + (i // 40) * 40
    # A shared baseline keeps decimal points and punctuation below the digits.
    draw.text((x, y + 28), chr(32 + i), font=ascii_font, fill="white", anchor="ls")
# Authored UI masks share the existing atlas and descriptor. Tails point to offscreen spectators.
for side in range(2):
    x = side * 360
    left = x + (16 if side == 0 else 0)
    outline, fill = (32, 51, 44, 255), (249, 250, 245, 255)
    draw.rounded_rectangle((left, bubble_y, left + 335, bubble_y + 131), radius=6, fill=fill, outline=outline, width=3)
    if side == 0:
        draw.polygon([(x+18,bubble_y+80),(x,bubble_y+111),(x+18,bubble_y+104)], fill=outline)
        draw.polygon([(x+19,bubble_y+85),(x+7,bubble_y+105),(x+19,bubble_y+101)], fill=fill)
    else:
        draw.polygon([(x+333,bubble_y+80),(x+351,bubble_y+111),(x+333,bubble_y+104)], fill=outline)
        draw.polygon([(x+332,bubble_y+85),(x+344,bubble_y+105),(x+332,bubble_y+101)], fill=fill)
# The title also uses these surfaces, without loading the full farm text atlas.
def board_skin(size, state="normal"):
    w, h = size
    selected = state == "selected"
    plank = Image.new("RGBA", size)
    pen = ImageDraw.Draw(plank)
    if state == "quiet":
        pen.polygon([(3,0),(w-4,0),(w-1,3),(w-1,h-4),(w-4,h-1),(3,h-1),(0,h-4),(0,3)], fill="#233830")
        pen.line((4,1,w-5,1),fill="#61806b")
        pen.line((4,h-2,w-5,h-2),fill="#172c28")
        pen.rectangle((1,5,2,h-6),fill="#739882")
        return plank
    pen.polygon([(3,0),(w-4,0),(w-1,3),(w-1,h-4),(w-4,h-1),(3,h-1),(0,h-4),(0,3)], fill="#422f24")
    pen.rectangle((2,3,w-3,h-4), fill="#986d44")
    pen.rectangle((4,2,w-5,4), fill="#cca06b")
    pen.rectangle((4,h-5,w-5,h-4), fill="#6c4b32")
    for gx in range(16,w-12,37):
        pen.line((gx,3,gx+8,3), fill="#b68352")
    inset = 6 if w > 200 else 4
    face = {"info":"#233830","normal":"#30473c","selected":"#504832","focus":"#395954"}
    pen.rectangle((inset,inset,w-inset-1,h-inset-1), fill=face[state])
    pen.line((inset,inset,w-inset-1,inset), fill="#1c302a")
    pen.line((inset,h-inset-1,w-inset-1,h-inset-1), fill="#496455")
    for px in (3,w-6):
        for py in (5,h-8):
            pen.rectangle((px,py,px+1,py+2), fill="#533e30")
            pen.point((px,py), fill="#ead2a1")
    if selected or state == "focus":
        pen.rectangle((inset+1,inset+1,w-inset-2,h-inset-2), outline="#eac06b" if selected else "#9dd3c8", width=2)
        pen.rectangle((inset+2,9,inset+4,h-10), fill="#edc369" if selected else "#9dd3c8")
    return plank

skin = Image.new("RGBA",(1024,512))
parts = []
def put(name, pos, size, state):
    sx, sy = pos
    assert sx+size[0] < skin.width and sy+size[1] < skin.height
    skin.paste(board_skin(size,state),pos)
    parts.append((name, (*pos,*size)))

put("Board",(2,2),(544,258),"info")
put("Info",(552,2),(264,60),"quiet")
put("Strip",(2,264),(624,44),"quiet")
for i,state in enumerate(("normal","focus","selected")):
    y=312+i*64
    put("Slot"+state.title(),(2,y),(60,60),state)
    put("Sign"+state.title(),(66,y),(172,44),state)
    put("Wide"+state.title(),(242,y),(456,48),state)
ImageDraw.Draw(skin).rectangle((990,490,1006,506),fill="white")
parts.append(("Fill",(995,495,4,4)))
skin.save(ROOT / "project/Resources/ui/field_surfaces.png")
skin_header = "#pragma once\nnamespace fieldsign {\nstruct Region { float x, y, w, h; };\n"
for name, rect in parts:
    skin_header += f"inline constexpr Region k{name} = {{" + ", ".join(f"{v}.0f" for v in rect) + "};\n"
(ROOT / "project/application/ui/FieldSurfaceAtlas.h").write_text(skin_header+"}\n", encoding="utf-8")
asset = ROOT / "project/Resources/ui/farm_runtime_menu.png"
asset.parent.mkdir(parents=True, exist_ok=True)
with NamedTemporaryFile(dir=asset.parent, suffix=".png", delete=False) as temporary:
    staged_asset = Path(temporary.name)
try:
    atlas.save(staged_asset)
    staged_asset.replace(asset)
finally:
    staged_asset.unlink(missing_ok=True)
atlas_bytes = asset.read_bytes()
fingerprint = 14695981039346656037
for byte in atlas_bytes:
    fingerprint = ((fingerprint ^ byte) * 1099511628211) & 0xffffffffffffffff
contract = "FarmRuntimeAtlas:sha256:" + hashlib.sha256(atlas_bytes).hexdigest()
header = "#pragma once\n#include <array>\n#include <cstdint>\n#include <string_view>\nnamespace farmui {\n"
header += f'inline constexpr char kAtlasContract[] = "{contract}";\n'
header += f"inline constexpr std::uint64_t kAtlasFingerprint = {fingerprint}ULL;\n"
header += f"inline constexpr std::uint64_t kAtlasByteCount = {len(atlas_bytes)}ULL;\n"
header += "enum class Label {\n"
header += "".join("    " + name + ",\n" for name, _ in labels) + "};\n"
header += "struct AtlasRect { float x, y, width, height; };\n"
header += f"inline constexpr std::array<AtlasRect, 2> kCheerBubbleUV = {{{{{{0, {bubble_y}, 352, 132}}, {{360, {bubble_y}, 352, 132}}}}}};\n"
header += f"inline constexpr std::array<AtlasRect, {len(rects)}> kLabels = {{{{\n"
header += "".join("    {" + ", ".join(str(v) + ".0f" for v in r) + "},\n" for r in rects) + "}};\n"
header += f"inline constexpr float kAtlasWidth = {atlas.width}.0f;\n"
header += f"inline constexpr float kAsciiY = {ascii_y}.0f;\n"
header += "#ifdef _DEBUG\n"
header += f"inline constexpr std::array<std::u8string_view, {len(goal_debug_labels)}> kGoalDebugText = {{{{\n"
header += "".join(f"    u8{json.dumps(label, ensure_ascii=False)},\n" for _, label in goal_debug_labels)
header += "}};\n#endif\n}\n"
(ROOT / "project/application/farm/ui/FarmRuntimeLabels.h").write_text(header, encoding="utf-8")
print(asset, atlas.size, len(labels))
