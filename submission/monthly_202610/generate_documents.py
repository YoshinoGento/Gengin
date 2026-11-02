"""Readable, source-backed Japanese portfolio and technical explanation."""
from pathlib import Path
import json
import math
import re
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.colors import HexColor, Color, white
from pypdf import PdfReader
import pypdfium2 as pdfium
from PIL import Image, ImageDraw
from prepare_monthly import OUT, ACCOUNT, VERIFY, write_json

DEST = OUT / "documents"
MEDIA = OUT / "media"
WIDTH, HEIGHT = 842, 595
INK = HexColor("#183638")
TEAL = HexColor("#216D70")
GREEN = HexColor("#547C49")
GOLD = HexColor("#B4903C")
MUTED = HexColor("#50666B")
PALE = HexColor("#EDF3F2")
FONT = "Meiryo"
BOLD = "MeiryoBold"
pdfmetrics.registerFont(TTFont(FONT, "C:/Windows/Fonts/meiryo.ttc", subfontIndex=0))
pdfmetrics.registerFont(TTFont(BOLD, "C:/Windows/Fonts/meiryob.ttc", subfontIndex=0))


def wrap(text, width, size=13, font=FONT):
    lines = []
    for paragraph in text.split("\n"):
        current = ""
        for ch in re.findall(r"[A-Za-z0-9_./％%]+|.", paragraph):
            if current and pdfmetrics.stringWidth(current + ch, font, size) > width:
                if ch[0] in "。、，．）］」』！？：；" and len(current) > 1:
                    lines.append(current[:-1])
                    current = current[-1] + ch
                else:
                    lines.append(current)
                    current = ch
            else:
                current += ch
        lines.append(current)
    return lines


class Document:
    def __init__(self, kind, total):
        self.path = DEST / f"{ACCOUNT}_{kind}.pdf"
        self.c = canvas.Canvas(str(self.path), pagesize=(WIDTH, HEIGHT), pageCompression=1)
        self.c.setTitle(f"水路農業｜2026年10月 {kind}")
        self.c.setAuthor("吉野絃音")
        self.kind, self.total, self.page = kind, total, 0
        self.bounds = []

    def rect(self, x, y, w, h, color):
        self.c.setFillColor(color)
        self.c.rect(x, HEIGHT-y-h, w, h, fill=1, stroke=0)

    def text(self, text, x, y, size=13, font=FONT, color=INK):
        w = pdfmetrics.stringWidth(text, font, size)
        assert x >= 0 and x+w <= WIDTH+.1, (self.page, text, x, w)
        assert y >= 0 and y+size <= HEIGHT, (self.page, text, y)
        self.c.setFillColor(color)
        self.c.setFont(font, size)
        self.c.drawString(x, HEIGHT-y-size, text)
        self.bounds.append([self.page, x, y, w, size, text])

    def para(self, text, x, y, w, size=13, leading=21, color=INK):
        for line in wrap(text, w, size):
            self.text(line, x, y, size, color=color)
            y += leading
        return y

    def page_start(self, title, subtitle, source=""):
        if self.page:
            self.c.showPage()
        self.page += 1
        self.rect(0, 0, WIDTH, HEIGHT, white)
        self.rect(36, 31, 32, 4, TEAL)
        self.text("SUIRO NOGYO / 2026.10", 79, 23, 10, BOLD, TEAL)
        self.text(title, 36, 52, 27, BOLD)
        self.para(subtitle, 37, 94, 768, 12, 18, MUTED)
        self.rect(36, 552, 770, 1, HexColor("#CCD9D7"))
        self.text(f"吉野絃音｜{self.kind}", 36, 565, 9, color=MUTED)
        self.text(f"{self.page:02} / {self.total:02}", 754, 565, 9, color=MUTED)
        if source:
            self.para(source, 37, 526, 765, 8.2, 11, MUTED)

    def shot(self, name, x, y, w, caption=None):
        path = MEDIA / f"{name}.png"
        with Image.open(path) as im:
            h = w * im.height / im.width
        self.c.drawImage(str(path), x, HEIGHT-y-h, width=w, height=h)
        if caption:
            self.para(caption, x, y+h+7, w, 9.5, 14, MUTED)
        return h

    def block(self, label, body, x, y, w=230):
        self.text(label, x, y, 15, BOLD, TEAL)
        return self.para(body, x, y+29, w, 12.3, 21)

    def arrow(self, x1, y1, x2, y2, color=TEAL):
        c = self.c
        c.setStrokeColor(color)
        c.setLineWidth(1.7)
        c.line(x1, HEIGHT-y1, x2, HEIGHT-y2)
        angle = math.atan2(y2-y1, x2-x1)
        for a in (-.45, .45):
            c.line(x2, HEIGHT-y2, x2-8*math.cos(angle+a), HEIGHT-(y2-8*math.sin(angle+a)))

    def node(self, x, y, w, heading, sub, color=TEAL):
        self.rect(x, y, w, 83, PALE)
        self.rect(x, y, 4, 83, color)
        self.text(heading, x+13, y+12, 14, BOLD, color)
        self.para(sub, x+13, y+39, w-25, 11, 16)

    def finish(self):
        assert self.page == self.total
        self.c.save()
        reader = PdfReader(str(self.path))
        assert len(reader.pages) == self.total
        assert all(len(page.extract_text().strip()) > 150 for page in reader.pages)
        write_json(VERIFY / f"{self.kind}_bounds.json", self.bounds)
        return self.path


def portfolio():
    d = Document("ポートフォリオ", 6)
    d.page_start("水路農業", "土地の高さと水の流れを整え、野菜を育てて品評会に挑む農業ゲームです。")
    d.shot("title", 36, 134, 530, "10月の通常版から撮影したタイトル画面")
    d.block("制作について", "個人制作\nC++ / HLSL / DirectX 12\nWindows x64", 592, 140, 207)
    d.block("今月取り組んだこと", "遊び始めるまでの演出、農場UI、品評会の結果表示を中心に改善しました。", 592, 270, 207)
    d.para("日本工学院専門学校　ゲームクリエイター科4年制\nLE3C 28　吉野絃音", 37, 462, 740, 13, 22)
    d.para("描画・入力などの学校提供基盤を使用し、農業の仕組みと画面表現を制作しています。", 37, 512, 765, 10, 15)

    d.page_start("10月の制作内容", "既存の農業システムを土台に、遊び方と結果が伝わる画面へ改善しました。")
    rows = [
        ("タイトル・背景", "池への反射、草の揺れ、クリック位置に反応する水滴と波紋を追加しました。"),
        ("画面遷移・導入", "波紋で画面をつなぎ、農場の状態に応じた開始演出を加えました。"),
        ("操作画面", "道具・種を画像で整理し、畑の状態と次の操作を分けて表示しました。"),
        ("品評会", "段階的な入賞目標、立体的な審査演出、結果の詳しい比較を追加しました。"),
        ("内部の整理", "道具ごとの判定を分離し、プレビューと実行で操作計画を共有しました。"),
    ]
    for i, (title, body) in enumerate(rows):
        y = 143+i*64
        d.text(f"0{i+1}", 39, y, 19, BOLD, GREEN)
        d.text(title, 86, y+1, 15, BOLD)
        d.para(body, 260, y, 538, 13, 20)
        d.rect(85, y+51, 717, .6, HexColor("#D7E1DF"))
    d.para("水路接続・作物の育成・品質評価・購入販売は既存の機能です。今月の新規成果とは分けて説明しています。", 37, 479, 765, 11.5, 18, MUTED)

    d.page_start("水のある風景から、農場へ", "タイトルの池と画面遷移をつなぎ、ゲームの題材が伝わる導入にしました。")
    d.shot("title", 36, 140, 500)
    d.block("風景の一体感", "池に風景を映し、雲と草に共通の風を使いました。昼夜変化のある既存タイトルに、水辺らしい動きを重ねました。", 563, 140)
    d.block("操作への反応", "クリックした位置を水滴の演出に反映し、波紋から次の画面が見えるようにしました。", 563, 300)
    d.shot("arrival", 36, 430, 145)
    d.para("新しい農場では、草や道具に寄るカメラから畑全体へ移ります。水路がある農場では、実際の接続を見せる案内に切り替えています。", 201, 436, 590, 12, 21)

    d.page_start("文章中心のUIを、選びやすい操作欄へ", "情報をすべて常時表示するのではなく、操作と状態に合わせて見せるよう整理しました。")
    d.shot("hud_before", 36, 142, 374, "改善前：10月16日の画面。操作説明と状態表示が画面の各所に分散していました。")
    d.shot("hud", 432, 142, 374, "改善後：道具・種を下部にまとめ、畑の状態と次の操作を左右に分けました。")
    d.block("見た目を対応させる", "種袋や作物の画像を操作欄にも使い、何を選ぶのかを対応づけました。", 37, 416, 235)
    d.block("必要な情報を残す", "残数や選択枠を表示し、画像だけでは分からない状態を補いました。", 302, 416, 235)
    d.block("次にすることを示す", "選択中の畑に合わせて、耕す・種を買うなどの案内を切り替えました。", 568, 416, 235)

    d.page_start("失敗を、次の育成につなげる", "結果の点数だけで終わらせず、どこを見直せばよいか確認できるようにしました。")
    d.shot("failed", 36, 139, 490)
    d.block("審査の区切り", "作物を展示台に置き、カメラと結果札で入賞・入賞ならずを伝えました。未出品は別の結果として扱います。", 559, 140)
    d.block("詳細は別の画面へ", "結果画面には不足点と主な見直し項目をまとめ、品質のレーダー図と大きさの比較はタブに分けました。", 559, 303)
    d.shot("quality", 36, 428, 150)
    d.shot("size", 199, 428, 150)
    d.para("品評会の画面は、同じ共通実装を使う確認用デモで撮影しました。提出する実行版は通常版です。", 372, 447, 423, 10.5, 17, MUTED)

    d.page_start("制作で意識したことと、今後の課題", "見た目の追加だけでなく、操作・状態変更・描画の役割も整理しました。")
    d.block("自分の制作範囲", "農業のルール、道具操作、画面遷移、UI、開始・審査演出を制作しています。描画・入力の基盤には学校提供コードを使用しています。", 37, 143, 356)
    d.block("制作支援・外部素材", "調査・実装補助・検証・資料整理に生成AIを使用しています。環境音とフォント、外部ライブラリの出所・利用条件を同梱のクレジットにまとめました。", 445, 143, 356)
    d.block("確認した範囲", "Release版でタイトル、開始演出、農場UI、購入の流れを確認しました。品評会は入賞・不入賞・未出品の表示を確認しています。", 37, 304, 356)
    d.block("次に改善すること", "初見での操作確認、Sessionに集まった処理の分割、描画負荷の計測を進めます。別PC・別GPUでの動作確認はまだ行えていません。", 445, 304, 356)
    d.rect(36, 468, 770, 43, PALE)
    d.para("「何を操作するか」と「なぜその結果になったか」が伝わることを、次の改善でも重視します。", 51, 479, 738, 12, 20)
    return d.finish()


def technical():
    d = Document("プログラム説明資料", 8)
    d.page_start("水路農業｜プログラム説明資料", "2026年10月の変更を中心に、実装の目的・処理の流れ・制約をまとめました。",
                 "主な参照：application/Game.cpp / scene/GamePlayScene.cpp / gameplay/GamePlaySessionSystem.cpp")
    d.node(36, 147, 176, "Game / Scene", "起動と進行の統括")
    d.node(248, 147, 176, "System", "状態変更・判定・記録")
    d.node(460, 147, 160, "View", "表示用データ・UI")
    d.node(656, 147, 150, "Renderer", "Mesh・描画命令")
    d.arrow(215, 188, 244, 188)
    d.arrow(426, 188, 455, 188)
    d.arrow(623, 188, 651, 188)
    d.block("目的", "画面ごとの処理と農業の状態変更が混ざらないよう、SceneからSystemへ役割を移しました。UIは表示と入力通知を担当します。", 37, 276, 357)
    d.block("現在の制約", "GamePlaySessionSystemには多くの処理が残っています。責務分離が完了したとは考えず、今後も用途ごとの分割を進めます。", 445, 276, 357)
    d.para("既存の土台：水路接続、育成履歴、品質評価、購入販売、保存。\n今月の主な変更：タイトル表現、遷移・段階ロード、UI、開始演出、品評会、道具判定の整理。", 37, 443, 765, 12, 21)

    d.page_start("池の反射と、風に連動する背景", "風景が別々に動いて見えないよう、共通の時間・風の状態を描画へ渡しました。",
                 "参照：title/TitleFarmRenderer.cpp / TitlePresentationSystem.cpp / TitleCloudMotionSystem.h / shader/TitlePond.PS.hlsl")
    d.shot("title", 36, 141, 466)
    d.block("水面の見せ方", "Planar Reflectionで風景を別の描画先へ描き、水面Shaderで反射画像を参照します。波の変化で参照位置をずらし、水面の揺らぎを表現しました。", 539, 143, 263)
    d.block("共通の風", "風向きを滑らかに変化させ、雲のUV移動と草の揺れへ渡します。草はInstancingでまとめて描画しています。", 539, 320, 263)
    d.para("反射用の描画が増えるため、追加の描画負荷が発生します。現時点ではGPU時間の比較計測はしていません。\n見た目を作るShader表現であり、水の流体シミュレーションやTeardownと同じ描画方式ではありません。", 37, 456, 765, 11.5, 20)

    d.page_start("波紋を止めにくくする、段階ロード", "シーンの初期化を一度に行うと波紋が停止して見えたため、処理を複数フレームへ分けました。",
                 "参照：scene/SceneTransitionSystem.h / SceneLoadSystem.h / SceneManager.cpp / engine/base/InitializationTask.h")
    nodes = [("覆う", "入力を止める"), ("表示確認", "覆ったフレームを提示"),
             ("初期化", "小さな処理単位で進む"), ("開く", "次の画面を見せる")]
    for i, (title, sub) in enumerate(nodes):
        x = 36+i*198
        d.node(x, 151, 176, title, sub)
        if i < 3:
            d.arrow(x+178, 192, x+194, 192)
    d.block("なぜ分割したか", "旧画面を覆ったことを確認してから切り替えます。読み込み中も演出の時間を更新し、初期化の待ち時間を水面の動きでつなぎました。", 37, 286, 355)
    d.block("保証できない点", "1フレーム4msを目安に、最大64ステップまで進めます。ただしファイルのデコードやドライバー呼び出しは分割できず、4ms以内を保証する仕組みではありません。", 446, 286, 355)
    d.rect(36, 466, 770, 43, PALE)
    d.para("別スレッドによる完全な非同期ロードではなく、メインループ上で初期化を少しずつ進める方式です。", 50, 477, 742, 11.5, 20)

    d.page_start("入力と状態変更を分け、操作計画を共有", "見た目だけでなく、プレビューと実行が異なる結果にならない構造を重視しました。",
                 "参照：farm/ui/FarmHotbarView.h / system/FarmToolBehavior.h / FarmToolActionSystem.cpp / tools/ui/farm_item_icons.py")
    d.node(36, 143, 225, "UIからの通知", "道具・対象の選択要求")
    d.node(308, 143, 225, "Behaviorで計画", "条件・対象・必要資源を判定")
    d.node(580, 143, 225, "Systemで確定", "畑・所持数・Undo履歴を更新")
    d.arrow(264, 183, 302, 183)
    d.arrow(536, 183, 574, 183)
    d.shot("hud", 36, 264, 396)
    d.block("表示を整理した理由", "Hotbarには道具と種、残数、選択枠をまとめました。種袋・作物に対応した画像を使い、詳しい説明は必要な場面へ分けました。", 470, 268, 332)
    d.para("同じ操作計画をプレビューと実行で使い、状態変更はSystem側に残しました。UIから直接農地を変更しない方針です。", 470, 429, 332, 11.5, 20)

    d.page_start("実際の農場に合わせた開始演出", "水路や水源がない農場でも、導入が短く終わらないように分岐を設けました。",
                 "参照：farm/system/FarmRuntimeController.cpp / FarmStartPresentationSystem.h / FarmArrivalPresentation.h")
    d.shot("arrival", 36, 141, 458)
    d.block("空の5×4農地", "約14.4秒。草と道具に寄った視点から、農場全体へカメラを移します。", 536, 141, 266)
    d.block("有効な水路がある農場", "約6.5秒。実際の水源と接続した水路を案内します。", 536, 264, 266)
    d.block("案内できる経路がない場合", "約3秒の俯瞰に切り替えます。存在しない水路を案内しないようにしました。", 536, 374, 266)
    d.para("開始演出はスキップできます。演出の状態とカメラの進行はSystemが持ち、Sceneは進行を統括します。", 37, 466, 458, 11.5, 20)

    d.page_start("段階解放と、変えない採点ルール", "新規大会の目標を段階的に上げ、解放された作物・地形を使う流れにしました。",
                 "参照：farm/system/FarmStageAccessSystem.h / FarmContestJudgeSystem.h")
    headers = [(48,"期間"),(211,"追加される作物"),(447,"高さの上限"),(646,"審査日 / 目標")]
    d.rect(36, 145, 770, 39, TEAL)
    for x, text in headers:
        d.text(text, x, 155, 13, BOLD, white)
    for i, values in enumerate([("1〜10日", "ニンジン", "H1", "10日 / 65点"),
                                ("11〜20日", "トマト", "H2", "20日 / 75点"),
                                ("21〜30日", "かぼちゃ", "H2", "30日 / 85点")]):
        y = 184+i*52
        if i % 2 == 0:
            d.rect(36, y, 770, 52, PALE)
        for (x, _), value in zip(headers, values):
            d.text(value, x, y+15, 14)
    d.block("採点の整合性", "既存の品質60点＋大きさ40点は維持しました。各寄与点を丸めてから加算し、表示した内訳と合計が一致するようにしています。", 37, 369, 357)
    d.block("保存との互換性", "段階解放は新規大会の設定として保存します。過去のセーブを読み込んだときに、従来使えた機能を突然制限しないようにしています。", 445, 369, 357)

    d.page_start("結果を要約し、比較は詳細画面へ", "結果・品質・大きさを分け、数値の意味を誤解しない表示にしました。",
                 "参照：system/FarmContestPresentationSystem.h / ui/FarmContestDetailView.h / FarmContestOutcomeView.h")
    d.shot("quality", 36, 140, 374)
    d.shot("size", 432, 140, 374)
    d.block("品質の比較", "成熟・水分・地形・養分を表示します。レーダー図の外周は100％で、各軸に個別の合格ラインがあるわけではありません。", 37, 374, 357)
    d.block("大きさの比較", "同じ尺度のモデルと換算重量を並べます。参考サイズの前提を表示し、品質不足を重さだけで補えると誤解させないようにしました。", 445, 374, 357)
    d.para("gはゲーム内の換算値です。画像は共通の品評会処理を使う確認用デモで撮影しています。", 37, 501, 765, 9.5, 15, MUTED)

    d.page_start("立体的な審査演出と、残っている課題", "作物を主役にしつつ、結果の判定と描画を分けて実装しました。",
                 "参照：render/FarmContestRenderer.cpp / FarmContestStageLayout.h / tools/title/contest_voxel.py")
    d.shot("passed", 36, 140, 390)
    d.block("ボクセルメッシュ", "作物の形をオフラインでボクセル化し、外側の面をまとめたメッシュを生成しました。約4秒のカメラ・結果札・報酬の演出を時間で制御します。", 463, 140, 339)
    d.block("描画と安全性", "既存のShadow Mapと影・カラー描画を使い、共通の空の描画で雲を動かしています。演出中に毎フレームモデルを作り直す方式にはしていません。", 463, 298, 339)
    d.para("確認：通常版の起動・農場UI・購入、共通演出の3種類の結果表示。提出ソースのReleaseビルドも別フォルダで確認しました。\n課題：Sessionの責務整理、GPU負荷の実測、初見の操作テスト、別PC・別GPUでの確認。破壊物理・実流体は未実装です。", 37, 433, 765, 11.5, 21)
    return d.finish()


def verify(paths):
    report = []
    for path in paths:
        pdf = pdfium.PdfDocument(path)
        renders = OUT / "document_review" / path.stem
        renders.mkdir(parents=True, exist_ok=True)
        thumbs = []
        for i in range(len(pdf)):
            page = pdf[i]
            image = page.render(scale=1.5).to_pil()
            image.save(renders / f"page_{i+1:02}.png")
            thumb = image.copy()
            thumb.thumbnail((632, 447))
            thumbs.append(thumb)
        contact = Image.new("RGB", (1264, 447*math.ceil(len(thumbs)/2)), "#D4DCDA")
        for i, im in enumerate(thumbs):
            contact.paste(im, ((i%2)*632, (i//2)*447))
        contact.save(renders / "contact.png")
        report.append({"file": path.name, "pages": len(pdf), "text_extraction": "PASS", "visual_review": "pending"})
    write_json(VERIFY / "documents_report.json", report)


if __name__ == "__main__":
    DEST.mkdir(parents=True, exist_ok=True)
    verify([portfolio(), technical()])
    print("Generated portfolio (6 pages) and technical document (8 pages)")
