#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmSeedShopView.h"
#include <algorithm>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

namespace farmui {
bool RuntimeUI::Initialize(SpriteCommon* common) {
    return InitializeIncrementally(common).Complete();
}
InitializationTask RuntimeUI::InitializeIncrementally(SpriteCommon* common) {
    if (!common) co_return false;
    for (auto& sprite : panels_) {
        if (!sprite.Initialize(common, "Resources/ui/hud_panel_fill.png")) co_return false;
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

void RuntimeUI::Panel(Rect r, Vector4 color) {
    color.w *= opacity_;
    if (panelCount_ >= panels_.size()) return;
    auto& sprite = panels_[panelCount_++];
    sprite.SetPosition({r.x, r.y}); sprite.SetSize({r.width, r.height});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
}
void RuntimeUI::LabelQuad(Label label, Rect r, Vector4 color) {
    color.w *= opacity_;
    const auto index = static_cast<std::size_t>(label);
    if (index >= kLabels.size() || labelCount_ >= labels_.size()) return;
    const auto& uv = kLabels[index];
    const float scale = (std::min)(1.0f, (std::max)(0.0f, r.width - 20.0f) / uv.width);
    auto& sprite = labels_[labelCount_++];
    sprite.SetTextureRect({uv.x, uv.y}, {uv.width, uv.height});
    sprite.SetPosition({r.x + 10, r.y + (r.height - uv.height * scale) * 0.5f});
    sprite.SetSize({uv.width * scale, uv.height * scale});
    sprite.SetColor(color); sprite.Update(); sprite.Draw();
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
void RuntimeUI::Draw(const View& view, Vector2 pointer) {
    if (!ready_) return;
    panelCount_ = labelCount_ = digitCount_ = iconCount_ = 0;
    radarLineCount_ = 0;
    opacity_ = view.starting ? std::clamp(view.cinematicBars, 0.0f, 1.0f) : 1.0f;
    const Item* tooltip = nullptr;
    const Item* selectedTool = nullptr;
    if (view.compactHUD && view.hotbarVisible) Panel(HUDLayout::tools, {0.025f, 0.035f, 0.032f, 0.93f});
    if (view.starting) {
        const float height = 84.0f * std::clamp(view.cinematicBars, 0.0f, 1.0f);
        Panel({0, 0, 1280, height}, {0.015f, 0.025f, 0.026f, 0.90f});
        Panel({0, 720-height, 1280, height}, {0.015f, 0.025f, 0.026f, 0.90f});
    } else if (view.briefing) {
        Panel({0, 0, 1280, 96}, {0.035f, 0.055f, 0.06f, 0.94f});
        Panel({0, 496, 1280, 224}, {0.035f, 0.055f, 0.06f, 0.97f});
    } else if (view.seedShop || view.harvestDisplay) {
        Panel({24, 16, 1232, 60}, {0.035f, 0.055f, 0.06f, 0.94f});
        Panel(SeedShopLayout::receipt, {0.035f, 0.055f, 0.06f, 0.97f});
        Panel(SeedShopLayout::footer, {0.035f, 0.055f, 0.06f, 0.97f});
    } else if (view.contestDetail) {
        const auto& frame = view.contestFrame;
        const bool ceremony=frame.page==FarmContestPresentationSystem::Page::Result || frame.unsubmitted;
        Panel({0,0,1280,78},{.025f,.03f,.035f,ceremony ? .78f : 1.f});
        Panel({0,648,1280,72},{.025f,.03f,.035f,ceremony ? .78f : 1.f});
        if (ceremony) {
            Panel({658,160,590,440},{.035f,.065f,.055f,.92f});
            Panel({658,160,4,440},{.70f,.64f,.40f,1});
        }
        const auto& result = frame.comparison;
        const Vector4 neutral{.55f,.72f,.78f,1};
        const Vector4 finalColor = !result.staged ? neutral : result.passed ?
            Vector4{.38f,.8f,.64f,1} : Vector4{.94f,.64f,.3f,1};
        const float blend = frame.verdictReveal;
        const Vector4 accent{neutral.x+(finalColor.x-neutral.x)*blend,
            neutral.y+(finalColor.y-neutral.y)*blend, neutral.z+(finalColor.z-neutral.z)*blend,1};
        if (!frame.unsubmitted) {
            const float y=ceremony ? 292.f : 224.f;
            Panel({680,y,552,4},{.20f,.25f,.24f,1});
            Panel({680,y,552*frame.totalReveal*result.total*.01f,4},accent);
            if (result.staged) Panel({680+552*result.target*.01f,y-4,2,12},{.85f,.89f,.87f,1});
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
    } else if (view.modal) {
        Panel({0, 0, 1280, 720}, {0.01f, 0.015f, 0.02f, 0.75f});
        Panel({130, 32, 1020, 656}, {0.055f, 0.075f, 0.072f, 0.99f});
    }
    if (view.observation) {
        Panel(View::kObservationTop, {0.035f, 0.055f, 0.06f, 0.96f});
        Panel(View::kObservationBottom, {0.035f, 0.055f, 0.06f, 0.96f});
    }
    if (view.terrain) {
        Panel(View::kTerrainTop, {0.035f, 0.055f, 0.06f, 0.96f});
        Panel(View::kTerrainBottom, {0.035f, 0.055f, 0.06f, 0.96f});
    }
    if (view.fieldActions) Panel(View::kFieldActionsPanel, {0.035f, 0.055f, 0.06f, 0.96f});
    if (view.waterGuidance) Panel(View::kWaterGuidancePanel, {0.035f, 0.055f, 0.06f, 0.96f});
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
        if (IsValidIcon(item.icon)) {
            const bool hover = item.rect.Contains(pointer) || item.focused;
            const Vector4 border = item.selected ? Vector4{0.96f, 0.78f, 0.35f, 1} :
                hover ? Vector4{0.68f, 0.89f, 0.85f, 1} : Vector4{0.23f, 0.30f, 0.28f, 1};
            Panel(item.rect, border);
            Panel({item.rect.x + 3, item.rect.y + 3, item.rect.width - 6, item.rect.height - 6},
                item.enabled ? Vector4{0.07f, 0.10f, 0.09f, 0.98f} : Vector4{0.045f, 0.05f, 0.05f, 1});
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
            if (hover) tooltip = &item;
            continue;
        }
        if (button) {
            Vector4 color = item.enabled ? Vector4{0.16f, 0.23f, 0.22f, 1} : Vector4{0.09f, 0.11f, 0.11f, 1};
            if (item.selected) color = {0.40f, 0.32f, 0.10f, 1};
            if (item.enabled && (item.focused || item.rect.Contains(pointer))) color = {0.25f, 0.40f, 0.36f, 1};
            Panel(item.rect, color);
        }
        Rect labelRect = item.rect;
        if (item.label == Label::ShopPlus || item.label == Label::ShopMinus) {
            const float width = kLabels[static_cast<std::size_t>(item.label)].width + 20.0f;
            labelRect.x += (labelRect.width - width) * 0.5f; labelRect.width = width;
        }
        if (!item.value.empty()) labelRect.width = item.valueOffset - 10;
        const Vector4 darkInk{0.015f, 0.025f, 0.02f, 1};
        LabelQuad(item.label, labelRect, item.darkInk ? darkInk : !item.enabled ? Vector4{0.42f, 0.46f, 0.45f, 1} :
            item.warning ? Vector4{1.0f, 0.72f, 0.32f, 1} : Vector4{0.96f, 0.98f, 0.96f, 1});
        if (!item.value.empty()) ValueText(item.value, {item.rect.x + item.valueOffset, item.rect.y + 1}, item.rect.x + item.rect.width,
            item.darkInk ? darkInk : Vector4{1.0f, 0.87f, 0.50f, 1});
    }
    opacity_ = viewOpacity;
    const Item* caption = tooltip ? tooltip : selectedTool;
    if (caption) {
        const auto index = static_cast<std::size_t>(caption->label);
        if (index < kLabels.size()) {
            const Rect box = IconCaptionRect(*caption);
            Panel(box, {0.018f,0.026f,0.024f,0.98f});
            LabelQuad(caption->label,box,{0.97f,0.98f,0.94f,1});
        }
    }
    if (!view.recordNames[0].empty() && namesReady_) { names_.Update(); names_.Draw(); }
    if (view.starting && view.cinematicCover > 0) {
        opacity_=1;
        Panel({0,0,1280,720},{.01f,.015f,.012f,std::clamp(view.cinematicCover,0.0f,1.0f)});
    }
}
}
