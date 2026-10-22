// spartanport: the in-game menu's UI scale (spartanport_overlay.cpp), kept apart so it can be tested.
#pragma once

#include <algorithm>
#include <cmath>
#include "imgui.h"

namespace SpartanUiScale {

// The scale follows the display height (1080p = 1), never below 0.75.
inline float ForHeight(float height) {
    return std::max(height / 1080.0f, 0.75f);
}

// Moves the style from the scale it has (base_scale) to `scale`. ScaleAllSizes truncates the software
// cursor's scale to a whole number: below 1080p (0.75 on a 1360x768 stream) it became 0 and the menu's
// cursor (io.MouseDrawCursor) was drawn with no size. It follows the UI scale itself instead.
inline void Apply(ImGuiStyle& style, float& base_scale, float scale) {
    if (std::abs(scale - base_scale) <= 0.01f) {
        return;
    }
    style.ScaleAllSizes(scale / base_scale);
    style.FontScaleMain = scale;
    style.MouseCursorScale = scale;
    base_scale = scale;
}

} // namespace SpartanUiScale
