#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmSeedShopView.h"
#include "farm/ui/FarmHotbarView.h"
#include "farm/ui/FarmControlGuideView.h"
#include "ui/FieldSurfaceTexture.h"
#include "farm/ui/FarmRuntimeAtlas.h"
#include "farm/render/FarmContestStageLayout.h"
#include "base/Logger.h"
#include <algorithm>
#include <fstream>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace farmui {
bool RuntimeUI::Initialize(SpriteCommon* common) {
    return InitializeIncrementally(common).Complete();
}
InitializationTask RuntimeUI::InitializeIncrementally(SpriteCommon* common) {
    if (!common) co_return false;
    {
        std::ifstream file("Resources/ui/farm_runtime_menu.png", std::ios::binary);
        if (!MatchesRuntimeAtlas(file)) {
            Logger::Error("Farm UI atlas does not match this executable. Rebuild and copy EXE + Resources together.");
            Logger::Error(kAtlasContract);
            co_return false;
        }
    }
    const auto skin=fieldsign::LoadForScene();
    if (!skin.IsValid()) co_return false;
    for (auto& sprite : panels_) {
        if (!sprite.Initialize(common, skin.Index())) co_return false;
        co_await std::suspend_always{};
    }
    // Screen-space atlas: keep the source level instead of building a full mip chain during entry.
    auto* textures = TextureManager::GetInstance();
    const auto atlas = textures->LoadTexture2D("Resources/ui/farm_runtime_menu.png",
        TextureManager::Lifetime::Scene, TextureManager::MipPolicy::PreserveFile);
    if (!atlas.IsValid() || atlas == textures->GetFallback2D()) co_return false;
    co_await std::suspend_always{};
    for (auto& sprite : labels_) {
        if (!sprite.Initialize(common, atlas.Index())) co_return false;
        co_await std::suspend_always{};
    }
    for (auto& sprite : digits_) {
        if (!sprite.Initialize(common, atlas.Index())) co_return false;
        co_await std::suspend_always{};
    }
    for (auto& sprite : radarLines_) {
        if (!sprite.Initialize(common, "Resources/ui/hud_panel_fill.png")) co_return false;
        co_await std::suspend_always{};
    }
    for (auto& sprite : icons_) {
        if (!sprite.Initialize(common, kHUDIconAtlas)) co_return false;
        co_await std::suspend_always{};
    }
    for (std::size_t i=0;i<seedPackets_.size();++i) {
        if (!seedPackets_[i].Initialize(common, i<2 ? "Resources/ui/farm_seed_packets.png" : "Resources/ui/farm_seed_packets_formal.png")) co_return false;
        co_await std::suspend_always{};
    }
    for (std::size_t i=0;i<harvestFigures_.size();++i) {
        if (!harvestFigures_[i].Initialize(common, i%4<2 ? "Resources/ui/farm_harvest_display.png" : "Resources/ui/farm_harvest_formal.png")) co_return false;
        co_await std::suspend_always{};
    }
    if (!names_.Initialize(common, "Resources/ui/hud_panel_fill.png")) co_return false;
    ready_ = true;
    co_return true;
}
void RuntimeUI::PrepareNames(const View& view) {
    if (!ready_ || view.recordNames[0].empty() || cachedNames_ == view.recordNames) return;
    cachedNames_ = view.recordNames;
    namesReady_ = false;
    Gdiplus::GdiplusStartupInput startup{}; ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) return;
    {
        constexpr int width = 900, height = 112;
        DirectX::ScratchImage pixels;
        if (SUCCEEDED(pixels.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1))) {
            Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);
            Gdiplus::Graphics graphics(&bitmap);
            graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
            graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            Gdiplus::Font font(L"Yu Gothic UI", 26, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 234, 166));
            Gdiplus::StringFormat format;
            format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            for (int i = 0; i < 2; ++i) {
                const std::string text = (i == 0 ? "現在の保存名：" : "選択中の保存名：") + view.recordNames[i];
                const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
                if (length <= 0 || length > 256) continue;
                std::wstring wide(length, L'\0');
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), length);
                graphics.DrawString(wide.c_str(), length, &font, Gdiplus::RectF(0, static_cast<float>(i * 56), width, 50), &format, &brush);
            }
            graphics.Flush(Gdiplus::FlushIntentionSync);
            Gdiplus::BitmapData data{}; Gdiplus::Rect rect(0, 0, width, height);
            if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok) {
                const auto* destinationImage = pixels.GetImage(0, 0, 0);
                for (int y = 0; y < height; ++y) {
                    const auto* source = static_cast<const uint8_t*>(data.Scan0) + static_cast<std::ptrdiff_t>(y) * data.Stride;
                    auto* destination = destinationImage->pixels + y * destinationImage->rowPitch;
                    for (int x = 0; x < width; ++x) {
                        destination[x*4] = source[x*4+2]; destination[x*4+1] = source[x*4+1];
                        destination[x*4+2] = source[x*4]; destination[x*4+3] = source[x*4+3];
                    }
                }
                bitmap.UnlockBits(&data);
                const auto texture = TextureManager::GetInstance()->UpdateUiTexture("farm-record-names", pixels);
                if (texture.IsValid()) {
                    names_.SetTexture(texture); names_.SetTextureRect({0, 0}, {width, height});
                    names_.SetPosition({176, 426}); names_.SetSize({width, height});
                    names_.SetColor({1, 1, 1, 1}); namesReady_ = true;
                }
            }
        }
    }
    Gdiplus::GdiplusShutdown(token);
}

void RuntimeUI::Panel(Rect r, Vector4 color, fieldsign::Region uv) {
    color.w *= opacity_;
    if (panelCount_ >= panels_.size()) return;
    auto& sprite = panels_[panelCount_++];
    sprite.SetTextureRect({uv.x,uv.y},{uv.w,uv.h});
    sprite.SetPosition({r.x, r.y}); sprite.SetSize({r.width, r.height});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void RuntimeUI::InfoPanel(Rect r,float alpha) {
    Panel(r,{1,1,1,alpha},fieldsign::Info(r.width,r.height));
}
void RuntimeUI::LabelQuad(Label label, Rect r, Vector4 color) {
    const auto index = static_cast<std::size_t>(label);
    if (index >= kLabels.size()) return;
    const auto& uv = kLabels[index];
    const float scale = (std::min)(1.0f, (std::max)(0.0f, r.width - 20.0f) / uv.width);
    AtlasQuad(uv, {r.x + 10, r.y + (r.height - uv.height * scale) * .5f, uv.width * scale, uv.height * scale}, color);
}
void RuntimeUI::AtlasQuad(const AtlasRect& uv, Rect r, Vector4 color) {
    if (labelCount_ >= labels_.size()) return;
    color.w *= opacity_;
    auto& sprite = labels_[labelCount_++];
    sprite.SetTextureRect({uv.x, uv.y}, {uv.width, uv.height});
    sprite.SetPosition({r.x,r.y}); sprite.SetSize({r.width,r.height});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void RuntimeUI::DrawGuideBoard() {
    constexpr auto board=GuideLayout::board;
    constexpr Vector4 chalkRule{.16f,.29f,.23f,1};
    Panel({0,0,1280,720},{.012f,.024f,.019f,.56f});
    InfoPanel(board);
    IconQuad(HUDIcon::Sprout,{132,114,40,40},{1,1,1,1});
    Panel({132,167,1018,2},chalkRule);
    Panel({358,188,2,236},chalkRule);
    Panel({132,500,1018,2},chalkRule);
}
void RuntimeUI::DrawGuideSign(const Item& item, bool field, bool hover) {
    const bool highlighted=item.selected || hover || item.focused;
    const Vector4 wood=highlighted ? Vector4{1,1,1,1} : Vector4{.88f,.9f,.88f,1};
    Panel(item.rect,wood,fieldsign::Button(item.rect.width,item.rect.height,item.selected,highlighted));
    if (field) {
        const Rect key=GuideLayout::keycap(item.rect);
        Panel(key,{.033f,.059f,.047f,1});
        const bool detail=item.request.action==Action::FieldDetails;
        ValueText(detail ? "Tab" : item.request.action==Action::WalkToggle ? "F4" : "H",{key.x+(detail ? 6 : 10),key.y+2},key.x+key.width,{.93f,.82f,.57f,1},.65f);
    }
    LabelQuad(item.label,GuideLayout::signText(item.rect,field),item.selected ? Vector4{1,.88f,.56f,1} : Vector4{.98f,.96f,.85f,1});
}
void RuntimeUI::ValueText(const std::string& value, Vector2 position, float right, Vector4 color, float scale) {
    color.w *= opacity_;
    // Numeric/status identifiers only; Japanese labels use whole-line atlas regions.
    for (const unsigned char c : value) {
        if (digitCount_ >= digits_.size() || position.x + 24 * scale > right) break;
        const int glyph = (c >= 32 && c <= 126 ? c : '?') - 32;
        auto& sprite = digits_[digitCount_++];
        sprite.SetTextureRect({static_cast<float>((glyph % 40) * 32), kAsciiY + (glyph / 40) * 40.0f}, {24, 36});
        sprite.SetSize({24 * scale, 36 * scale}); sprite.SetPosition(position);
        sprite.SetColor(color); sprite.Update(); sprite.Draw();
        position.x += 14 * scale;
    }
}
void RuntimeUI::IconQuad(HUDIcon icon, Rect rect, Vector4 color) {
    color.w *= opacity_;
    if (!IsValidIcon(icon) || iconCount_ >= icons_.size()) return;
    auto& sprite = icons_[iconCount_++];
    sprite.SetTextureRect(IconUV(icon), {kHUDIconSize, kHUDIconSize});
    sprite.SetPosition({rect.x, rect.y}); sprite.SetSize({rect.width, rect.height});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void RuntimeUI::DrawRadar(const QualityRadar& radar) {
    if (radar.opacity <= 0) return;
    const float savedOpacity = opacity_;
    opacity_ *= std::clamp(radar.opacity, 0.0f, 1.0f);
    const auto transform = [&](Vector2 point) {
        return Vector2{QualityRadar::center.x + radar.offset.x + (point.x - QualityRadar::center.x)*radar.scale,
            QualityRadar::center.y + radar.offset.y + (point.y - QualityRadar::center.y)*radar.scale};
    };
    const auto line = [&](Vector2 a, Vector2 b, float thickness, Vector4 color) {
        a = transform(a); b = transform(b);
        const float dx = b.x-a.x, dy = b.y-a.y;
        const float length = std::hypot(dx,dy);
        if (!std::isfinite(length) || length < 0.01f || radarLineCount_ >= radarLines_.size()) return;
        auto& sprite = radarLines_[radarLineCount_++];
        sprite.SetAnchorPoint({0,0.5f}); sprite.SetPosition(a);
        sprite.SetRotation(std::atan2(dy,dx)); sprite.SetSize({length,thickness});
        color.w *= opacity_;
        sprite.SetColor(color); sprite.Update(); sprite.Draw();
    };
    const Vector4 grid{0.35f,0.43f,0.46f,1}, gold = radar.color;
    for (int ring=1; ring<=4; ++ring) for (std::size_t i=0; i<4; ++i)
        line(QualityRadar::Point(i,ring*0.25f), QualityRadar::Point((i+1)%4,ring*0.25f), 1.5f, grid);
    for (std::size_t i=0; i<4; ++i) {
        line(QualityRadar::center, QualityRadar::Point(i,1), 1.5f, grid);
        const auto next = (i+1)%4;
        // Unknown values leave a gap instead of inventing a zero-valued measurement.
        if (radar.known[i] && radar.known[next])
            line(QualityRadar::Point(i,radar.values[i]), QualityRadar::Point(next,radar.values[next]), 4, gold);
        if (radar.known[i]) {
            const auto point = QualityRadar::Point(i,radar.values[i]);
            line({point.x-4,point.y}, {point.x+4,point.y}, 8, gold);
        }
    }
    for (std::size_t i=0; radar.numberedAxes && i<QualityRadar::axisLabels.size(); ++i) {
        const auto& label = QualityRadar::axisLabels[i];
        const auto position = transform({label.x, label.y});
        ValueText(std::to_string(i+1), position, position.x+label.width);
    }
    opacity_ = savedOpacity;
}
void RuntimeUI::DrawFieldWheelHint(const View& view, Vector2 pointer) {
    if (view.fieldWheelHint == WheelHint::Hidden) return;
    const bool seeds = view.fieldWheelHint == WheelHint::Seeds;
    const auto group = seeds ? Label::WheelSeeds : Label::WheelTools;
    constexpr float iconSize = 24.f, textInset = 20.f, groupScale = .68f, itemScale = .78f;
    const auto textWidth = [=](Label label, float scale) {
        return textInset + kLabels[static_cast<std::size_t>(label)].width * scale;
    };
    const float groupWidth = textWidth(group, groupScale);
    const auto* item = view.wheelItem ? &*view.wheelItem : nullptr;
    Rect box{};
    if (item) {
        const float width = iconSize + (std::max)(groupWidth, textWidth(item->label, itemScale));
        box = PlaceFieldWheelHint(view, pointer, width, kWheelHintRowHeight * 2);
        if (!box.width) item = nullptr;
    }
    if (!item) box = PlaceFieldWheelHint(view, pointer, iconSize + groupWidth);
    if (!box.width) return;
    InfoPanel(box,.96f);
    const auto row = [&](float y, HUDIcon icon, Label label, float width, Vector4 color) {
        IconQuad(icon, {box.x+4, y+3, iconSize, iconSize}, {1,1,1,1});
        LabelQuad(label, {box.x+iconSize, y, width, kWheelHintRowHeight}, color);
    };
    row(box.y, seeds ? HUDIcon::SeedBag : HUDIcon::Hoe, group, groupWidth,
        seeds ? Vector4{.79f, 1.f, .80f, 1.f} : Vector4{1.f, .92f, .71f, 1.f});
    if (item) row(box.y+kWheelHintRowHeight, item->icon, item->label, textWidth(item->label, itemScale), {1,1,1,1});
}

void RuntimeUI::Draw(const View& view, Vector2 pointer) {
    if (!ready_) return;
    panelCount_ = labelCount_ = digitCount_ = iconCount_ = 0;
    radarLineCount_ = 0;
    opacity_ = view.starting ? std::clamp(view.cinematicBars, 0.0f, 1.0f) : 1.0f;
    const Item* tooltip = nullptr;
    const Item* selectedTool = nullptr;
    const Item* focusedSlot = nullptr;
    if (view.compactHUD && view.hotbarVisible) InfoPanel(HUDLayout::tools);
    if (view.audioSettings) {
        Panel({0,0,1280,720},{.01f,.015f,.02f,.75f});
        const auto& box=title::kAudioSettingsPanel;
        InfoPanel({box.position.x,box.position.y,box.size.x,box.size.y});
        for (std::size_t i=0;i<title::kAudioChannelCount;++i) {
            const float y=title::kAudioSliderY[i];
            const int volume=std::clamp(view.audioFrame.volume[i],0,100);
            const float width=title::kAudioSliderWidth*static_cast<float>(volume)*.01f;
            const bool active=view.audioFrame.selected==static_cast<int>(i) || title::kAudioSliderHits[i].Contains(pointer);
            Panel({title::kAudioSliderX,y-3,title::kAudioSliderWidth,6},{.22f,.28f,.27f,1});
            if (width>0) Panel({title::kAudioSliderX,y-3,width,6},{.41f,.76f,.74f,1});
            Panel({title::kAudioSliderX+width-7,y-16,14,32},active ? Vector4{1,.85f,.47f,1} : Vector4{.88f,.93f,.91f,1});
            ValueText(std::to_string(volume)+"%",{800,y+24},924,{.96f,.98f,.96f,1});
        }
        const std::array buttons{title::kAudioSettingsReset,title::kAudioSettingsBack};
        for (std::size_t i=0;i<buttons.size();++i) {
            const auto& button=buttons[i];
            const bool active=view.audioFrame.selected==static_cast<int>(title::kAudioChannelCount+i) || button.Contains(pointer);
            Panel({button.position.x,button.position.y,button.size.x,button.size.y},fieldsign::kWhite,
                fieldsign::Button(button.size.x,button.size.y,false,active));
        }
        if (view.audioFrame.saveFailed) Panel({320,648,640,56},{.15f,.06f,.035f,1});
    } else if (view.starting) {
        const float height = 84.0f * std::clamp(view.cinematicBars, 0.0f, 1.0f);
        Panel({0, 0, 1280, height}, {0.015f, 0.025f, 0.026f, 0.90f});
        Panel({0, 720-height, 1280, height}, {0.015f, 0.025f, 0.026f, 0.90f});
    } else if (view.briefing) {
        InfoPanel({0, 0, 1280, 96},.96f);
        InfoPanel({0, 496, 1280, 224},.97f);
    } else if (view.seedShop || view.harvestDisplay) {
        InfoPanel({24,16,1232,60});
        InfoPanel(view.seedShop ? SeedShopLayout::shopReceipt : SeedShopLayout::receipt);
        InfoPanel(view.seedShop ? SeedShopLayout::shopFooter : SeedShopLayout::footer);
    } else if (view.contestDetail) {
        const auto& frame = view.contestFrame;
        const bool ceremony=frame.page==FarmContestPresentationSystem::Page::Result || frame.unsubmitted;
        InfoPanel({0,0,1280,frame.finalClear ? 46.f+32.f*frame.clearReveal : 78.f},ceremony ? .85f : 1.f);
        InfoPanel({0,648,1280,72},ceremony ? .85f : 1.f);
        const float resultOpacity=frame.resultReveal;
        if (ceremony && resultOpacity>0) {
            const float left=frame.unsubmitted ? 658.f : FarmContestStageLayout::kResultLeft;
            const float height=frame.unsubmitted ? 440.f : 260.f;
            InfoPanel({left,160,1248-left,height},.97f*resultOpacity);
            Panel({left,160,4,height},{.70f,.64f,.40f,resultOpacity});
        }
        const auto& result = frame.comparison;
        const Vector4 neutral{.55f,.72f,.78f,1};
        const Vector4 finalColor = !result.staged ? neutral : result.passed ?
            Vector4{.38f,.8f,.64f,1} : Vector4{.94f,.64f,.3f,1};
        const float blend = frame.verdictReveal;
        const Vector4 accent{neutral.x+(finalColor.x-neutral.x)*blend,
            neutral.y+(finalColor.y-neutral.y)*blend, neutral.z+(finalColor.z-neutral.z)*blend,resultOpacity};
        if (!frame.unsubmitted && resultOpacity>0) {
            const float y=ceremony ? 316.f : 224.f;
            const float x=ceremony ? 944.f : 680.f, width=ceremony ? 288.f : 552.f;
            Panel({x,y,width,4},{.20f,.25f,.24f,resultOpacity});
            Panel({x,y,width*frame.totalReveal*result.total*.01f,4},accent);
            if (result.staged) Panel({x+width*result.target*.01f,y-4,2,12},{.85f,.89f,.87f,resultOpacity});
        }
    } else if (view.contestOutcome) {
        const float reveal = std::clamp(view.outcomeReveal, 0.0f, 1.0f);
        const Vector4 finalAccent = view.outcomePositive ? Vector4{.38f,.8f,.64f,1} : Vector4{.94f,.64f,.3f,1};
        const float verdict = std::clamp(view.outcomeVerdictReveal, 0.0f, 1.0f);
        const Vector4 accent{.55f+(finalAccent.x-.55f)*verdict,
            .72f+(finalAccent.y-.72f)*verdict, .78f+(finalAccent.z-.78f)*verdict, 1};
        Panel({0,0,1280,720},{.015f,.022f,.02f,.82f+.14f*reveal});
        Panel({80,184,1120*reveal,3},accent);
        if (view.seasonOutcome) {
            for (std::size_t i=0; i<view.outcomePoints.size(); ++i) {
                const float y=270+static_cast<float>(i)*86;
                const float rowReveal=std::clamp(view.outcomeRowReveal[i],0.0f,1.0f);
                Panel({80,y,1120,3},{.2f,.25f,.24f,rowReveal});
                const Vector4 rowColor = view.outcomeTargets[i] <= 0 ? Vector4{.55f,.72f,.78f,1} :
                    view.outcomePassed[i] ? Vector4{.38f,.8f,.64f,1} : Vector4{.94f,.64f,.3f,1};
                Panel({80,y,1120*std::clamp(view.outcomePoints[i],0.0f,1.0f)*rowReveal,3},
                    {rowColor.x,rowColor.y,rowColor.z,rowReveal});
                if (view.outcomeTargets[i]>0)
                    Panel({80+1120*std::clamp(view.outcomeTargets[i],0.0f,1.0f),y-4,2,11},{.85f,.89f,.87f,rowReveal});
            }
        }
    } else if (view.controlGuide) {
        DrawGuideBoard();
    } else if (view.modal) {
        Panel({0, 0, 1280, 720}, {0.01f, 0.015f, 0.02f, 0.75f});
        InfoPanel(view.menuShell ? Rect{130,24,1020,688} : Rect{130,32,1020,656});
        if (view.menuShell) {
            Panel({150,112,980,2},{.22f,.29f,.28f,1});
            Panel({150,618,980,2},{.22f,.29f,.28f,1});
        }
    }
    if (view.observation) {
        InfoPanel(View::kObservationTop);
        InfoPanel(View::kObservationBottom);
    }
    if (view.terrain) {
        InfoPanel(View::kTerrainTop);
        InfoPanel(View::kTerrainBottom);
    }
    if (view.fieldActions) InfoPanel(View::kFieldActionsPanel);
    if (view.shortGoal) InfoPanel(HUDLayout::shortGoal);
    if (view.shortGoalDetail) InfoPanel(HUDLayout::shortGoalDetail);
    if (view.waterGuidance) InfoPanel(View::kWaterGuidancePanel);
    if (view.radar.visible) DrawRadar(view.radar);
    if (view.referenceRadar.visible) DrawRadar(view.referenceRadar);
    if (view.harvestDisplay) {
        for (std::size_t i = 0; i < view.harvestFigures.size(); ++i) {
            const int crop = view.harvestFigures[i];
            if (crop < 0 || crop >= farm::kFarmCropTypeCount) continue;
            const auto& target = SeedShopLayout::products[i];
            auto& figure = harvestFigures_[i*farm::kFarmCropTypeCount+crop];
            figure.SetTextureRect({(crop % 2) * 768.0f, 0}, {768, 1024});
            figure.SetPosition({target.x + 15, view.counterSelection == static_cast<int>(i) ? 156.0f : 168.0f});
            figure.SetSize({210, 280}); figure.SetColor({1,1,1,1}); figure.Update(); figure.Draw();
        }
    }
    if (view.cheerBubble.visible && labelCount_ < labels_.size()) {
        const auto& bubble = view.cheerBubble;
        const auto& uv = kCheerBubbleUV[bubble.tailLeft ? 0 : 1];
        auto& sprite = labels_[labelCount_++];
        sprite.SetTextureRect({uv.x,uv.y},{uv.width,uv.height});
        sprite.SetPosition({bubble.rect.x,bubble.rect.y});
        sprite.SetSize({bubble.rect.width,bubble.rect.height});
        sprite.SetColor({1,1,1,std::clamp(bubble.opacity,0.f,1.f)*opacity_});
        sprite.Update(); sprite.Draw();
    }
    const float viewOpacity = opacity_;
    for (std::size_t i = 0; i < (std::min)(view.count, view.items.size()); ++i) {
        const auto& item = view.items[i];
        opacity_ = viewOpacity * std::clamp(item.opacity, 0.0f, 1.0f);
        if (opacity_ <= 0) continue;
        if (item.request.action == Action::HarvestDisplaySelect) continue;
        // The physical packet is the button. Do not paint a menu rectangle over it.
        if (item.request.action == Action::ShopSelect && item.rect.y < 544) {
            const int index = item.request.argument;
            if (index < 0 || index >= farm::kPlayableCropCount) continue;
            const int crop = farm::ToCropSlot(farm::PlayableCrop(index));
            const auto front = SeedShopLayout::PacketFront(index % 2, item.selected);
            // Fixed-camera packet fronts share their layout with the counter targets.
            Panel({front.x + 5, front.y + 5, front.width, front.height}, {0.02f, 0.03f, 0.025f, 0.55f});
            auto& packet = seedPackets_[crop];
            packet.SetTextureRect({(crop % 2) * 768.0f, 0}, {768, 1024});
            packet.SetPosition({front.x, front.y}); packet.SetSize({front.width, front.height});
            packet.SetColor({1, 1, 1, 1}); packet.Update(); packet.Draw();
            continue;
        }
        const bool button = item.request.action != Action::None;
        if (item.label==Label::WalkKeys) {
            Panel({item.rect.x-8,item.rect.y-2,item.rect.width+16,84},{.055f,.085f,.07f,.83f});
        }
        const bool fieldSign=view.compactHUD && (item.request.action==Action::FieldDetails ||
            item.request.action==Action::OpenControlGuide || item.request.action==Action::WalkToggle);
        if (fieldSign || (view.controlGuide && button)) {
            DrawGuideSign(item,fieldSign,item.rect.Contains(pointer));
            continue;
        }
        if (IsValidIcon(item.icon) && !item.leadingIcon) {
            const bool hover = item.rect.Contains(pointer) || item.focused;
            const bool barFocus = view.hotbarVisible && item.focused &&
                (item.request.action == Action::Tool || item.request.action == Action::Crop);
            Panel(item.rect,item.enabled ? fieldsign::kWhite : fieldsign::kDisabled,
                fieldsign::Button(item.rect.width,item.rect.height,item.selected,barFocus || hover));
            const float size = (std::min)(item.rect.width, item.rect.height) - 12;
            IconQuad(item.icon, {item.rect.x + (item.rect.width-size)*0.5f, item.rect.y+5, size, size},
                item.enabled ? Vector4{1,1,1,1} : Vector4{0.40f,0.43f,0.42f,1});
            if (!item.value.empty()) {
                const float scale = (std::min)(0.58f, (item.rect.width-10) / (14*static_cast<float>(item.value.size())+10));
                const float width = (14*static_cast<float>(item.value.size())+10)*scale;
                const float x = item.rect.x+item.rect.width-width-5;
                const float y = item.rect.y+item.rect.height-36*scale-3;
                Panel({x-2,y,width+4,36*scale}, {0.025f,0.03f,0.028f,0.94f});
                ValueText(item.value,{x,y},item.rect.x+item.rect.width-3,{1,1,0.94f,1},scale);
            }
            if (item.request.action == Action::Tool && item.selected) selectedTool = &item;
            if (barFocus) focusedSlot = &item;
            if (item.rect.Contains(pointer)) tooltip = &item;
            continue;
        }
        if (button) {
            Panel(item.rect,item.enabled ? fieldsign::kWhite : fieldsign::kDisabled,
                fieldsign::Button(item.rect.width,item.rect.height,item.enabled && item.selected,
                    item.enabled && (item.focused || item.rect.Contains(pointer))));
            if (view.menuShell && item.enabled && (item.selected || item.focused))
                Panel({item.rect.x,item.rect.y+item.rect.height-3,item.rect.width,3},{.97f,.81f,.40f,1});
        }
        Rect labelRect = item.rect;
        if (item.leadingIcon && IsValidIcon(item.icon)) {
            IconQuad(item.icon,{item.rect.x+10,item.rect.y+(item.rect.height-40)*.5f,40,40},
                item.enabled ? Vector4{1,1,1,1} : Vector4{.4f,.43f,.42f,1});
            labelRect.x+=54; labelRect.width-=54;
            labelRect.y+=(item.rect.height-38)*.5f; labelRect.height=38;
        }
        if (item.label == Label::ShopPlus || item.label == Label::ShopMinus) {
            const float width = kLabels[static_cast<std::size_t>(item.label)].width + 20.0f;
            labelRect.x += (labelRect.width - width) * 0.5f; labelRect.width = width;
        }
        if (!item.value.empty()) labelRect.width = item.valueOffset - 10 - (labelRect.x-item.rect.x);
        const Vector4 darkInk{0.015f, 0.025f, 0.02f, 1};
        if (item.label==Label::FinalClearTitle) {
            auto shadow=labelRect; shadow.x+=2; shadow.y+=3;
            LabelQuad(item.label,shadow,{.05f,.08f,.04f,.9f});
            LabelQuad(item.label,labelRect,{1.f,.87f,.47f,1});
            continue;
        }
        LabelQuad(item.label, labelRect, item.darkInk ? darkInk : !item.enabled ? Vector4{0.42f, 0.46f, 0.45f, 1} :
            item.warning ? Vector4{1.0f, 0.72f, 0.32f, 1} : Vector4{0.96f, 0.98f, 0.96f, 1});
        if (!item.value.empty()) ValueText(item.value, {item.rect.x + item.valueOffset,
            item.rect.y + (item.leadingIcon ? (item.rect.height-36)*.5f : 1)}, item.rect.x + item.rect.width,
            item.darkInk ? darkInk : Vector4{1.0f, 0.87f, 0.50f, 1});
    }
    opacity_ = viewOpacity;
    // The notch points in the direction that transfers focus between the field and its bar.
    const Item* notch = focusedSlot ? focusedSlot : view.hotbarAtFieldEdge ? selectedTool : nullptr;
    if (notch) {
        const float cx = notch->rect.x + notch->rect.width * 0.5f;
        constexpr Vector4 ink{.88f, 1.f, 1.f, 1};
        for (int i = 0; i < 3; ++i) {
            const float half = 2.f + 3.f * static_cast<float>(focusedSlot ? i : 2-i);
            Panel({cx-half, notch->rect.y-8.f+2.f*i, half*2.f, 2.f}, ink);
        }
    }
    const Item* caption = tooltip ? tooltip : focusedSlot ? focusedSlot : selectedTool;
    if (caption) {
        const Label label = tooltip ? HoverCaption(*caption) : caption->label;
        const auto index = static_cast<std::size_t>(label);
        if (index < kLabels.size()) {
            const Rect box = IconCaptionRect(label);
            InfoPanel(box);
            LabelQuad(label,box,{0.97f,0.98f,0.94f,1});
        }
    }
    DrawFieldWheelHint(view, pointer);
    if (!view.recordNames[0].empty() && namesReady_) { names_.Update(); names_.Draw(); }
    if (view.starting && view.cinematicCover > 0) {
        opacity_=1;
        Panel({0,0,1280,720},{.01f,.015f,.012f,std::clamp(view.cinematicCover,0.0f,1.0f)});
    }
}
}
