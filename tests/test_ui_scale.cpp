// The in-game menu's UI scale and its software cursor (spartanport_ui_scale.h), on a real ImGui frame.
#include <cassert>
#include <cfloat>
#include <cstdio>
#include "imgui.h"
#include "spartanport_ui_scale.h"

// Width of what the frame draws on top of everything (the software cursor), 0 when nothing.
static float CursorWidth(float display_height) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // as the Vulkan backend: no Build() needed
    io.Fonts->AddFontDefault();
    io.DisplaySize = ImVec2(display_height * 16.0f / 9.0f, display_height);
    io.DeltaTime = 1.0f / 60.0f;
    float base_scale = 1.0f;
    SpartanUiScale::Apply(ImGui::GetStyle(), base_scale, SpartanUiScale::ForHeight(display_height));
    assert(base_scale == SpartanUiScale::ForHeight(display_height));
    io.MouseDrawCursor = true;
    io.AddMousePosEvent(200.0f, 200.0f);
    ImGui::NewFrame();
    ImGui::Begin("menu");
    ImGui::Text("settings");
    ImGui::End();
    ImGui::Render();
    float min_x = FLT_MAX, max_x = -FLT_MAX;
    for (const ImDrawVert& v : ImGui::GetForegroundDrawList()->VtxBuffer) {
        min_x = v.pos.x < min_x ? v.pos.x : min_x;
        max_x = v.pos.x > max_x ? v.pos.x : max_x;
    }
    ImGui::DestroyContext();
    return max_x > min_x ? max_x - min_x : 0.0f;
}

int main() {
    assert(SpartanUiScale::ForHeight(1080.0f) == 1.0f);
    assert(SpartanUiScale::ForHeight(2160.0f) == 2.0f);
    assert(SpartanUiScale::ForHeight(768.0f) == 0.75f && SpartanUiScale::ForHeight(480.0f) == 0.75f);

    // The cursor's own scale follows the UI scale (ScaleAllSizes alone made it 0 below 1080p).
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();
    float base_scale = 1.0f;
    SpartanUiScale::Apply(style, base_scale, 0.75f);
    assert(style.MouseCursorScale == 0.75f && style.FontScaleMain == 0.75f);
    SpartanUiScale::Apply(style, base_scale, 1.5f); // a window resized later
    assert(style.MouseCursorScale == 1.5f && base_scale == 1.5f);
    ImGui::DestroyContext();

    // The menu's cursor is drawn on the frame at every display height: the 1360x768 stream, 720p,
    // 1080p and 4K (the arrow is 12 px wide at scale 1).
    for (float height : {720.0f, 768.0f, 1080.0f, 2160.0f}) {
        const float width = CursorWidth(height);
        std::printf("cursor at %.0fp: %.1f px wide\n", height, width);
        assert(width >= 12.0f * SpartanUiScale::ForHeight(height));
    }
    std::puts("PASS: UI scale");
    return 0;
}
