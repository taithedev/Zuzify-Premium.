#pragma once
#include "imgui.h"

namespace ZuzifyTheme {
inline void Apply() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 18.0f;
    s.ChildRounding = 14.0f;
    s.FrameRounding = 10.0f;
    s.PopupRounding = 12.0f;
    s.ScrollbarRounding = 10.0f;
    s.GrabRounding = 10.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = ImVec2(22, 20);
    s.FramePadding = ImVec2(13, 10);
    s.ItemSpacing = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 12.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = ImVec4(0.94f, 0.94f, 0.98f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.58f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.025f, 0.025f, 0.035f, 0.985f);
    c[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.055f, 0.075f, 0.78f);
    c[ImGuiCol_PopupBg] = ImVec4(0.045f, 0.045f, 0.065f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.30f, 0.20f, 0.55f, 0.22f);
    c[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.09f, 0.12f, 0.92f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.11f, 0.22f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.19f, 0.13f, 0.29f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.39f, 0.18f, 0.70f, 0.92f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.51f, 0.27f, 0.86f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.13f, 0.58f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.30f, 0.16f, 0.50f, 0.70f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.42f, 0.22f, 0.68f, 0.80f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.49f, 0.26f, 0.78f, 0.95f);
    c[ImGuiCol_CheckMark] = ImVec4(0.70f, 0.44f, 1.0f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.62f, 0.34f, 0.90f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.76f, 0.52f, 1.0f, 1.0f);
}
}
