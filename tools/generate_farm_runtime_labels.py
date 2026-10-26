from pathlib import Path
import json
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
cell_w, cell_h = 640, 44
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
columns = 5
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
    ["ShopTitle", "種屋"], ["ShopMoney", "所持金"],
    ["ShopTurnip", "カブの種"], ["ShopCarrot", "ニンジンの種"],
    ["ShopOwned", "持っている種"], ["ShopUnitPrice", "単価"],
    ["ShopQuantity", "購入数"], ["ShopTotal", "合計"],
    ["ShopMinus", "－"], ["ShopPlus", "＋"],
    ["ShopChoose", "購入する種と個数を選択"], ["ShopReview", "購入内容を確認"],
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
    ["OutcomeFinalPassed", "最終大会：入賞"],
    ["OutcomeFinalFailed", "最終大会：入賞ならず"],
    ["OutcomeFinalMissed", "最終大会：未出品"],
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
ascii_y = ((len(labels) + columns - 1) // columns) * cell_h
height = ascii_y + 128
assert height <= 4096, "Split the label atlas before adding more rows"
atlas = Image.new("RGBA", (cell_w * columns, height))
draw = ImageDraw.Draw(atlas)
rects = []
for i, (name, text) in enumerate(labels):
    x, y = (i % columns) * cell_w, (i // columns) * cell_h
    width = int(draw.textlength(text, font=font)) + 4
    assert width <= cell_w, (name, width)
    draw.text((x + 2, y + 4), text, fill="white", font=font, anchor="lt")
    rects.append((x, y, width, 38))
ascii_font = ImageFont.truetype(str(font_path), 24)
ascii_font.set_variation_by_axes([500])
for i in range(95):
    x, y = (i % 40) * 32, ascii_y + (i // 40) * 40
    # A shared baseline keeps decimal points and punctuation below the digits.
    draw.text((x, y + 28), chr(32 + i), font=ascii_font, fill="white", anchor="ls")
asset = ROOT / "project/Resources/ui/farm_runtime_menu.png"
asset.parent.mkdir(parents=True, exist_ok=True)
atlas.save(asset)
header = "#pragma once\n#include <array>\nnamespace farmui {\nenum class Label {\n"
header += "".join("    " + name + ",\n" for name, _ in labels) + "};\n"
header += "struct AtlasRect { float x, y, width, height; };\n"
header += f"inline constexpr std::array<AtlasRect, {len(rects)}> kLabels = {{{{\n"
header += "".join("    {" + ", ".join(str(v) + ".0f" for v in r) + "},\n" for r in rects) + "}};\n"
header += f"inline constexpr float kAtlasWidth = {atlas.width}.0f;\n"
header += f"inline constexpr float kAsciiY = {ascii_y}.0f;\n}}\n"
(ROOT / "project/application/farm/ui/FarmRuntimeLabels.h").write_text(header, encoding="utf-8")
print(asset, atlas.size, len(labels))
