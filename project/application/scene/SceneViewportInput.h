#pragma once
#include "math/Struct.h"

namespace scene {
struct ViewportPointerInput {
    Vector2 position{};
    bool valid = false;
    bool pressed = false;
    bool held = false;
    bool focused = false;
};

// Optional capability for scenes accepting virtual-screen pointer notifications from the editor.
class ViewportInputTarget {
public:
    virtual ~ViewportInputTarget() = default;
    virtual void SetViewportPointerInput(const ViewportPointerInput& input) noexcept = 0;
};
}
