#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmDateSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmVisualSystem.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "io/Input.h"

bool FarmRuntimeController::OffersCanalLesson(const FarmRuntimeContext& s) const {
    return flow_.GetPhase() == FarmPlayFlow::Phase::Briefing && s.farmDateSystem.GetDay() == 1 &&
        s.farmProgressionSystem.IsContestSeason() && !s.farmDocumentSystem.IsPrepared() &&
        !s.farmDocumentSystem.HasError() && s.farmDocumentSystem.GetActiveDocumentId().empty() &&
        s.farmGrid.GetWidth() == farm::FarmCanalLessonSystem::kWidth &&
        s.farmGrid.GetHeight() == farm::FarmCanalLessonSystem::kHeight;
}

bool FarmRuntimeController::UpdateCanalLesson(const FarmRuntimeContext& s, const Input& input, bool click, farmui::Action hit) {
    using A = farmui::Action;
    using P = farm::FarmCanalLessonSystem::Phase;
    const auto* framework = Framework::GetInstance();
    const auto* clock = framework ? framework->GetFrameClock() : nullptr;
    lesson_->Tick(clock ? clock->GetRealDeltaSeconds() : 0);
    const auto pressed = [&](InputKey key, InputGamepadButton pad) {
        return input.TriggerKey(key) || input.TriggerGamepadButton(pad);
    };
    const int dx = pressed(InputKey::ArrowRight,InputGamepadButton::DPadRight) - pressed(InputKey::ArrowLeft,InputGamepadButton::DPadLeft);
    const int dy = pressed(InputKey::ArrowDown,InputGamepadButton::DPadDown) - pressed(InputKey::ArrowUp,InputGamepadButton::DPadUp);
    if (dx || dy) lesson_->Move(dx,dy);
    A action = click ? hit : A::None;
    if (pressed(InputKey::Escape,InputGamepadButton::B))
        action = lesson_->State() == P::Preview ? A::LessonUndo : A::LessonExit;
    else if (input.TriggerMouseButton(InputMouseButton::Right)) action = A::LessonUndo;
    else if (pressed(InputKey::Enter,InputGamepadButton::A)) {
        switch (lesson_->State()) {
        case P::Choose: action = A::LessonPreview; break;
        case P::Preview: action = lesson_->WillReach() ? A::LessonConnect : A::LessonUndo; break;
        case P::Reached: action = lesson_->Target() == farm::FarmCanalLessonSystem::kFirst ? A::LessonAgain : A::LessonExit; break;
        default: break;
        }
    } else if (click && s.host.IsViewportHovered() && !view_.Covers(pointer_)) {
        Vector3 origin{},direction{}; int index = -1;
        if (s.host.TryBuildViewportRay(origin,direction) && s.farmVisualSystem.TryPickTile(lesson_->Board(),origin,direction,index))
            lesson_->Preview(index);
    }
    switch (action) {
    case A::LessonPreview: lesson_->Preview(lesson_->Selection()); break;
    case A::LessonConnect: lesson_->Connect(); break;
    case A::LessonUndo: lesson_->Cancel(); break;
    case A::LessonAgain: lesson_->TrySecond(); break;
    case A::LessonExit: lesson_.reset(); BeginFarmStart(s); break;
    default: break;
    }
    BuildView(s); frameCaptured_ = true; return true;
}
