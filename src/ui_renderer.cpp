#include "ui_renderer.h"
#include "gw2_logic.h"
#include "version.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gw2qh {

namespace {

const ImVec4 kColorGold   = ImVec4(0.95f, 0.78f, 0.32f, 1.0f);
const ImVec4 kColorGreen  = ImVec4(0.35f, 0.88f, 0.48f, 1.0f);
const ImVec4 kColorMuted  = ImVec4(0.68f, 0.70f, 0.75f, 1.0f);
const ImVec4 kColorSubtle = ImVec4(0.58f, 0.64f, 0.78f, 1.0f);
const ImVec4 kColorWarn   = ImVec4(0.96f, 0.58f, 0.25f, 1.0f);
const ImVec4 kColorError  = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
const ImVec4 kColorCyan   = ImVec4(0.42f, 0.80f, 0.96f, 1.0f);
const ImVec4 kColorPurple = ImVec4(0.76f, 0.56f, 0.96f, 1.0f);

void CopyToBuffer(char* dst, size_t dstSize, const std::string& src) {
    if (dstSize == 0) return;
    std::strncpy(dst, src.c_str(), dstSize - 1);
    dst[dstSize - 1] = '\0';
}

//-----------------------------------------------------------------------------
// Theme Scope & Crisp Vector Icons (zero external font dependencies)
//-----------------------------------------------------------------------------

const ImVec4 kColorDoneDim = ImVec4(0.38f, 0.56f, 0.43f, 0.85f);

struct ThemeScope {
    int colCount = 0;
    int varCount = 0;

    explicit ThemeScope(bool isHud) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, isHud ? 7.0f : 8.0f); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 5.0f); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isHud ? ImVec2(10.0f, 8.0f) : ImVec2(12.0f, 10.0f)); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.0f, 4.0f)); ++varCount;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, isHud ? ImVec2(6.0f, 4.0f) : ImVec2(8.0f, 6.0f)); ++varCount;

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.96f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.08f, 0.11f, isHud ? 0.0f : 0.72f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.09f, 0.11f, 0.15f, 0.97f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.28f, 0.36f, 0.85f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.24f, 0.28f, 0.36f, 0.65f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.07f, 0.09f, 0.12f, 0.98f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.11f, 0.14f, 0.20f, 0.98f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.16f, 0.22f, 0.95f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.22f, 0.30f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.22f, 0.27f, 0.36f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.21f, 0.28f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42f, 0.33f, 0.13f, 0.95f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.56f, 0.43f, 0.15f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.95f, 0.78f, 0.32f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.68f, 0.26f, 0.95f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.96f, 0.80f, 0.35f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.13f, 0.16f, 0.22f, 0.95f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.45f, 0.35f, 0.14f, 0.95f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.32f, 0.26f, 0.12f, 1.0f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.24f, 0.32f, 0.90f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.40f, 0.32f, 0.13f, 0.90f)); ++colCount;
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.50f, 0.39f, 0.15f, 0.95f)); ++colCount;
    }

    ~ThemeScope() {
        ImGui::PopStyleColor(colCount);
        ImGui::PopStyleVar(varCount);
    }

    ThemeScope(const ThemeScope&) = delete;
    ThemeScope& operator=(const ThemeScope&) = delete;
};

enum class StatusGlyph {
    Incomplete,
    Completed,
    CompletedDim,
    LocalCompleted,
    ActiveStep,
    Warning
};

void DrawStatusGlyphAt(StatusGlyph glyph, ImVec2 tl, ImVec2 br, ImVec2 c, float size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    switch (glyph) {
        case StatusGlyph::Completed:
        case StatusGlyph::CompletedDim:
        case StatusGlyph::LocalCompleted: {
            ImU32 boxCol = IM_COL32(52, 168, 83, 220);
            ImU32 chkCol = IM_COL32(255, 255, 255, 255);
            if (glyph == StatusGlyph::CompletedDim) {
                boxCol = IM_COL32(44, 118, 66, 130);
                chkCol = IM_COL32(205, 235, 210, 175);
            } else if (glyph == StatusGlyph::LocalCompleted) {
                boxCol = IM_COL32(58, 138, 148, 185);
                chkCol = IM_COL32(245, 215, 130, 235);
            }
            dl->AddRectFilled(tl, br, boxCol, 3.0f);
            ImVec2 pts[3] = {
                ImVec2(tl.x + size * 0.22f, tl.y + size * 0.52f),
                ImVec2(tl.x + size * 0.44f, tl.y + size * 0.74f),
                ImVec2(tl.x + size * 0.80f, tl.y + size * 0.28f)
            };
            dl->AddPolyline(pts, 3, chkCol, false, 1.7f);
            break;
        }
        case StatusGlyph::ActiveStep: {
            dl->AddTriangleFilled(
                ImVec2(tl.x + size * 0.20f, tl.y + size * 0.16f),
                ImVec2(tl.x + size * 0.20f, tl.y + size * 0.84f),
                ImVec2(tl.x + size * 0.86f, c.y),
                IM_COL32(245, 200, 82, 255));
            break;
        }
        case StatusGlyph::Warning: {
            dl->AddQuadFilled(
                ImVec2(c.x, tl.y),
                ImVec2(br.x, c.y),
                ImVec2(c.x, br.y),
                ImVec2(tl.x, c.y),
                IM_COL32(242, 185, 58, 235));
            dl->AddLine(ImVec2(c.x, tl.y + 3.0f), ImVec2(c.x, br.y - 5.0f), IM_COL32(25, 25, 25, 255), 1.8f);
            dl->AddCircleFilled(ImVec2(c.x, br.y - 2.6f), 1.1f, IM_COL32(25, 25, 25, 255));
            break;
        }
        case StatusGlyph::Incomplete:
        default: {
            dl->AddCircle(c, size * 0.36f, IM_COL32(150, 158, 174, 195), 12, 1.4f);
            break;
        }
    }
}

void DrawStatusGlyph(StatusGlyph glyph, float size = 13.0f) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetTextLineHeight();
    float yOff  = std::max(0.0f, (lineH - size) * 0.5f);
    ImVec2 tl(p.x, p.y + yOff);
    ImVec2 br(p.x + size, p.y + yOff + size);
    ImVec2 c(p.x + size * 0.5f, p.y + yOff + size * 0.5f);
    DrawStatusGlyphAt(glyph, tl, br, c, size);

    ImGui::Dummy(ImVec2(size, lineH));
    ImGui::SameLine(0.0f, 5.0f);
}

bool DrawClickableStatusGlyph(const char* id, StatusGlyph glyph, const char* tooltip, float size = 11.5f) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetTextLineHeight();
    float hitW  = std::max(size + 3.0f, 14.0f);
    float yOff  = std::max(0.0f, (lineH - size) * 0.5f);
    float xOff  = (hitW - size) * 0.5f;
    ImVec2 tl(p.x + xOff, p.y + yOff);
    ImVec2 br(tl.x + size, tl.y + size);
    ImVec2 c(tl.x + size * 0.5f, tl.y + size * 0.5f);

    bool clicked = ImGui::InvisibleButton(id, ImVec2(hitW, lineH));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        ImGui::GetWindowDrawList()->AddCircle(c, size * 0.52f, IM_COL32(245, 200, 82, 220), 14, 1.3f);
    }
    DrawStatusGlyphAt(glyph, tl, br, c, size);
    if (hovered && tooltip && tooltip[0] != '\0') {
        ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::SameLine(0.0f, 5.0f);
    return clicked;
}

void DrawMapPin(float size = 14.0f) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetTextLineHeight();
    float yOff  = std::max(0.0f, (lineH - size) * 0.5f);
    ImVec2 c(p.x + size * 0.5f, p.y + yOff + size * 0.38f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float r = size * 0.30f;
    ImVec2 tip(p.x + size * 0.5f, p.y + yOff + size * 0.94f);
    ImU32 pinCol = IM_COL32(68, 226, 118, 245);
    dl->AddCircleFilled(c, r, pinCol, 14);
    dl->AddTriangleFilled(ImVec2(c.x - r * 0.85f, c.y + r * 0.25f),
                          ImVec2(c.x + r * 0.85f, c.y + r * 0.25f),
                          tip, pinCol);
    dl->AddCircleFilled(c, r * 0.40f, IM_COL32(18, 26, 22, 245), 10);

    ImGui::Dummy(ImVec2(size, lineH));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Active in current map");
    }
    ImGui::SameLine(0.0f, 4.0f);
}

void DrawFallbackEntryEmblem(EntryKind kind, float size) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineH = ImGui::GetTextLineHeight();
    float yOff  = std::max(0.0f, (lineH - size) * 0.5f);
    ImVec2 c(p.x + size * 0.5f, p.y + yOff + size * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (kind == EntryKind::Quest) {
        // Golden/Green GW2 Story Star (4-pointed star)
        float rOuter = size * 0.48f;
        float rInner = size * 0.18f;
        ImU32 col = IM_COL32(105, 225, 125, 245);
        dl->AddTriangleFilled(ImVec2(c.x, c.y - rOuter), ImVec2(c.x + rInner, c.y), ImVec2(c.x - rInner, c.y), col);
        dl->AddTriangleFilled(ImVec2(c.x, c.y + rOuter), ImVec2(c.x + rInner, c.y), ImVec2(c.x - rInner, c.y), col);
        dl->AddTriangleFilled(ImVec2(c.x - rOuter, c.y), ImVec2(c.x, c.y - rInner), ImVec2(c.x, c.y + rInner), col);
        dl->AddTriangleFilled(ImVec2(c.x + rOuter, c.y), ImVec2(c.x, c.y - rInner), ImVec2(c.x, c.y + rInner), col);
    } else {
        // Golden Achievement Shield/Hexagon
        float r = size * 0.44f;
        ImVec2 hex[6] = {
            ImVec2(c.x, c.y - r),
            ImVec2(c.x + r * 0.86f, c.y - r * 0.45f),
            ImVec2(c.x + r * 0.86f, c.y + r * 0.45f),
            ImVec2(c.x, c.y + r),
            ImVec2(c.x - r * 0.86f, c.y + r * 0.45f),
            ImVec2(c.x - r * 0.86f, c.y - r * 0.45f)
        };
        dl->AddConvexPolyFilled(hex, 6, IM_COL32(235, 188, 66, 220));
        dl->AddPolyline(hex, 6, IM_COL32(255, 232, 150, 255), true, 1.2f);
    }

    ImGui::Dummy(ImVec2(size, lineH));
    ImGui::SameLine(0.0f, 5.0f);
}

enum class IconBtnKind {
    ChevronRight,
    ChevronDown,
    ChevronUp,
    Sync,
    SettingsSliders,
    HudEye,
    EditPencil,
    RemoveX,
    ExternalWiki,
    WaypointDiamond,
    ChainNodes,
    ArrowNext,
    PinPlus
};

bool DrawIconButton(const char* id, IconBtnKind kind, const char* tooltip, bool highlight = false, bool spinning = false) {
    float lineH = ImGui::GetTextLineHeight();
    float boxSz = std::max(kind == IconBtnKind::WaypointDiamond ? 18.0f : 16.0f, lineH);
    ImVec2 p    = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(boxSz, boxSz));
    bool hovered = ImGui::IsItemHovered();
    bool active  = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 tl = p;
    ImVec2 br(p.x + boxSz, p.y + boxSz);
    ImVec2 c(p.x + boxSz * 0.5f, p.y + boxSz * 0.5f);

    if (hovered || active || highlight) {
        ImU32 bg = active ? IM_COL32(255, 255, 255, 55)
                          : (hovered ? IM_COL32(255, 255, 255, 35) : IM_COL32(245, 200, 82, 28));
        dl->AddRectFilled(tl, br, bg, 3.0f);
    }

    ImU32 fg = hovered ? IM_COL32(255, 235, 165, 255)
                       : (highlight ? IM_COL32(245, 200, 82, 245) : IM_COL32(190, 198, 212, 225));

    switch (kind) {
        case IconBtnKind::ChevronRight: {
            float r = 3.8f;
            dl->AddTriangleFilled(ImVec2(c.x - r * 0.6f, c.y - r),
                                  ImVec2(c.x - r * 0.6f, c.y + r),
                                  ImVec2(c.x + r * 0.9f, c.y), fg);
            break;
        }
        case IconBtnKind::ChevronDown: {
            float r = 3.8f;
            dl->AddTriangleFilled(ImVec2(c.x - r, c.y - r * 0.5f),
                                  ImVec2(c.x + r, c.y - r * 0.5f),
                                  ImVec2(c.x, c.y + r * 0.9f), fg);
            break;
        }
        case IconBtnKind::ChevronUp: {
            float r = 3.8f;
            dl->AddTriangleFilled(ImVec2(c.x - r, c.y + r * 0.5f),
                                  ImVec2(c.x + r, c.y + r * 0.5f),
                                  ImVec2(c.x, c.y - r * 0.9f), fg);
            break;
        }
        case IconBtnKind::Sync: {
            float r = boxSz * 0.30f;
            float baseAngle = spinning ? static_cast<float>(ImGui::GetTime() * 5.0) : 0.4f;
            dl->PathArcTo(c, r, baseAngle, baseAngle + 4.6f, 14);
            dl->PathStroke(spinning ? IM_COL32(245, 200, 82, 255) : fg, false, 1.6f);
            float endA = baseAngle + 4.6f;
            ImVec2 tip(c.x + std::cos(endA) * r, c.y + std::sin(endA) * r);
            dl->AddCircleFilled(tip, 2.1f, spinning ? IM_COL32(245, 200, 82, 255) : fg);
            break;
        }
        case IconBtnKind::SettingsSliders: {
            float x0 = tl.x + 3.0f, x1 = br.x - 3.0f;
            float y1 = c.y - 4.0f, y2 = c.y, y3 = c.y + 4.0f;
            dl->AddLine(ImVec2(x0, y1), ImVec2(x1, y1), fg, 1.4f);
            dl->AddLine(ImVec2(x0, y2), ImVec2(x1, y2), fg, 1.4f);
            dl->AddLine(ImVec2(x0, y3), ImVec2(x1, y3), fg, 1.4f);
            dl->AddCircleFilled(ImVec2(x0 + 3.0f, y1), 2.0f, fg);
            dl->AddCircleFilled(ImVec2(x1 - 3.0f, y2), 2.0f, fg);
            dl->AddCircleFilled(ImVec2(x0 + 5.0f, y3), 2.0f, fg);
            break;
        }
        case IconBtnKind::HudEye: {
            float rx = boxSz * 0.36f;
            float ry = boxSz * 0.22f;
            ImVec2 pts[6] = {
                ImVec2(c.x - rx, c.y),
                ImVec2(c.x - rx * 0.45f, c.y - ry),
                ImVec2(c.x + rx * 0.45f, c.y - ry),
                ImVec2(c.x + rx, c.y),
                ImVec2(c.x + rx * 0.45f, c.y + ry),
                ImVec2(c.x - rx * 0.45f, c.y + ry)
            };
            dl->AddPolyline(pts, 6, fg, true, 1.4f);
            dl->AddCircleFilled(c, 2.1f, fg, 10);
            if (!highlight) {
                dl->AddLine(ImVec2(tl.x + 3.0f, br.y - 3.0f), ImVec2(br.x - 3.0f, tl.y + 3.0f), fg, 1.5f);
            }
            break;
        }
        case IconBtnKind::EditPencil: {
            ImVec2 p0(tl.x + 3.5f, br.y - 3.5f);
            ImVec2 p1(br.x - 3.5f, tl.y + 3.5f);
            dl->AddLine(p0, p1, fg, 1.8f);
            dl->AddTriangleFilled(p0, ImVec2(p0.x + 3.2f, p0.y), ImVec2(p0.x, p0.y - 3.2f), fg);
            break;
        }
        case IconBtnKind::RemoveX: {
            ImU32 xCol = hovered ? IM_COL32(245, 95, 95, 255) : IM_COL32(195, 130, 130, 225);
            float pad = 4.0f;
            dl->AddLine(ImVec2(tl.x + pad, tl.y + pad), ImVec2(br.x - pad, br.y - pad), xCol, 1.7f);
            dl->AddLine(ImVec2(tl.x + pad, br.y - pad), ImVec2(br.x - pad, tl.y + pad), xCol, 1.7f);
            break;
        }
        case IconBtnKind::ExternalWiki: {
            ImVec2 b0(tl.x + 3.0f, tl.y + 5.0f), b1(br.x - 5.0f, br.y - 3.0f);
            dl->AddRect(b0, b1, fg, 1.5f, 0, 1.3f);
            ImVec2 a0(c.x - 1.0f, c.y + 1.0f), a1(br.x - 2.5f, tl.y + 2.5f);
            dl->AddLine(a0, a1, fg, 1.5f);
            dl->AddLine(a1, ImVec2(a1.x - 4.0f, a1.y), fg, 1.5f);
            dl->AddLine(a1, ImVec2(a1.x, a1.y + 4.0f), fg, 1.5f);
            break;
        }
        case IconBtnKind::WaypointDiamond: {
            float r = boxSz * 0.34f;
            ImU32 wpOuter = hovered ? IM_COL32(150, 240, 255, 255) : IM_COL32(82, 208, 250, 245);
            dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y),
                              ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), wpOuter);
            float ri = r * 0.42f;
            dl->AddQuadFilled(ImVec2(c.x, c.y - ri), ImVec2(c.x + ri, c.y),
                              ImVec2(c.x, c.y + ri), ImVec2(c.x - ri, c.y), IM_COL32(245, 255, 255, 255));
            break;
        }
        case IconBtnKind::ChainNodes: {
            ImVec2 n1(tl.x + 3.5f, c.y);
            ImVec2 n2(c.x, tl.y + 4.0f);
            ImVec2 n3(br.x - 3.5f, c.y + 2.5f);
            dl->AddLine(n1, n2, fg, 1.4f);
            dl->AddLine(n2, n3, fg, 1.4f);
            dl->AddCircleFilled(n1, 2.0f, fg);
            dl->AddCircleFilled(n2, 2.0f, fg);
            dl->AddCircleFilled(n3, 2.0f, fg);
            break;
        }
        case IconBtnKind::ArrowNext: {
            ImU32 arrowCol = hovered ? IM_COL32(120, 255, 155, 255) : IM_COL32(88, 225, 122, 245);
            float x0 = tl.x + 3.0f, x1 = br.x - 3.0f;
            dl->AddLine(ImVec2(x0, c.y), ImVec2(x1 - 2.0f, c.y), arrowCol, 1.8f);
            dl->AddTriangleFilled(ImVec2(x1 - 4.5f, c.y - 3.8f),
                                  ImVec2(x1 - 4.5f, c.y + 3.8f),
                                  ImVec2(x1 + 0.5f, c.y), arrowCol);
            break;
        }
        case IconBtnKind::PinPlus: {
            float r = boxSz * 0.27f;
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), fg, 1.7f);
            dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), fg, 1.7f);
            break;
        }
    }

    if (hovered && tooltip && tooltip[0] != '\0') {
        ImGui::SetTooltip("%s", tooltip);
    }
    return clicked;
}

// Progress bar with a dark track, subtle top sheen, optional segment ticks,
// and an outlined label so numbers remain legible across filled/empty regions.
void DrawReadableProgressBar(float fraction, float width, float height, const char* label, bool completed, int segmentTarget = 0) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1(p0.x + width, p0.y + height);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p0, p1, IM_COL32(12, 14, 19, 235), 3.0f);

    float f = std::clamp(fraction, 0.0f, 1.0f);
    if (f > 0.0f) {
        ImU32 fillCol  = completed ? IM_COL32(44, 158, 78, 255) : IM_COL32(204, 150, 42, 255);
        ImU32 sheenCol = completed ? IM_COL32(175, 255, 200, 90) : IM_COL32(255, 238, 175, 95);
        float fillW    = std::max(3.0f, (width - 2.0f) * f);
        ImVec2 fillTL(p0.x + 1.0f, p0.y + 1.0f);
        ImVec2 fillBR(p0.x + 1.0f + fillW, p1.y - 1.0f);
        dl->AddRectFilled(fillTL, fillBR, fillCol, 2.5f);
        dl->AddLine(ImVec2(fillTL.x + 1.0f, fillTL.y + 0.5f),
                    ImVec2(fillBR.x - 1.0f, fillTL.y + 0.5f),
                    sheenCol, 1.0f);
    }

    if (segmentTarget > 1 && segmentTarget <= 25 && width > 40.0f) {
        for (int s = 1; s < segmentTarget; ++s) {
            float sx = p0.x + (width * static_cast<float>(s)) / static_cast<float>(segmentTarget);
            dl->AddLine(ImVec2(sx, p0.y + 1.5f), ImVec2(sx, p1.y - 1.5f), IM_COL32(8, 10, 14, 150), 1.0f);
        }
    }

    dl->AddRect(p0, p1, IM_COL32(88, 96, 114, 230), 3.0f, 0, 1.0f);

    ImVec2 ts = ImGui::CalcTextSize(label);
    ImVec2 tp(p0.x + (width - ts.x) * 0.5f, p0.y + (height - ts.y) * 0.5f);
    ImU32 outline = IM_COL32(0, 0, 0, 255);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            dl->AddText(ImVec2(tp.x + dx, tp.y + dy), outline, label);
        }
    }
    dl->AddText(tp, IM_COL32(255, 255, 255, 255), label);

    ImGui::Dummy(ImVec2(width, height));
}

const char* KindBadge(EntryKind kind) {
    switch (kind) {
        case EntryKind::Achievement: return "ACH";
        case EntryKind::Quest:       return "STORY";
    }
    return "GOAL";
}

ImVec4 KindBadgeColor(EntryKind kind) {
    switch (kind) {
        case EntryKind::Achievement: return kColorGold;
        case EntryKind::Quest:       return kColorGreen;
    }
    return kColorMuted;
}

void RenderStepGuideTooltip(const EvaluatedBitView& bit) {
    if (bit.area.empty() && bit.guideNotes.empty() && bit.waypointName.empty()) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(360.0f);
    ImGui::TextColored(kColorGold, "%s", bit.label.c_str());
    if (!bit.area.empty()) {
        ImGui::TextColored(kColorCyan, "Area: %s", bit.area.c_str());
    }
    if (!bit.waypointName.empty()) {
        if (!bit.waypointChatLink.empty()) {
            ImGui::TextColored(kColorGreen, "Waypoint: %s %s", bit.waypointName.c_str(), bit.waypointChatLink.c_str());
        } else {
            ImGui::TextColored(kColorGreen, "Waypoint: %s", bit.waypointName.c_str());
        }
    }
    if (!bit.guideNotes.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted(bit.guideNotes.c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void RenderChainRoadmapTooltip(const EvaluatedChainContext& chain) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(380.0f);

    if (!chain.questlineTitle.empty()) {
        ImGui::TextColored(kColorGold, "%s", chain.questlineTitle.c_str());
    } else if (!chain.parentTitle.empty()) {
        ImGui::TextColored(kColorGold, "%s", chain.parentTitle.c_str());
    }
    if (chain.hasParent && chain.parentMax > 0) {
        ImGui::TextColored(kColorCyan, "%s (%d / %d)",
                           chain.parentTitle.c_str(), chain.parentCurrent, chain.parentMax);
    }
    if (!chain.transitionHint.empty()) {
        ImGui::TextColored(kColorWarn, "Turn-in: %s", chain.transitionHint.c_str());
    }
    if (chain.hasNext) {
        ImGui::TextColored(kColorGreen, "Next Stage: %s", chain.nextTitle.c_str());
    }

    if (!chain.roadmapNodes.empty()) {
        ImGui::Separator();
        for (const auto& node : chain.roadmapNodes) {
            StatusGlyph g = node.isCurrent
                ? StatusGlyph::ActiveStep
                : (node.completed ? StatusGlyph::Completed : StatusGlyph::Incomplete);
            DrawStatusGlyph(g, 12.0f);
            ImVec4 col = node.isCurrent ? kColorGold : (node.completed ? kColorGreen : kColorMuted);
            if (node.max > 1) {
                ImGui::TextColored(col, "%s (%d/%d)", node.title.c_str(), node.current, node.max);
            } else {
                ImGui::TextColored(col, "%s", node.title.c_str());
            }
        }
    }

    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

struct AreaBitGroup {
    std::string area;
    std::vector<const EvaluatedBitView*> items;
};

template <typename PinFn, typename ToggleBitFn>
void RenderHudStepItem(const EvaluatedBitView& bit,
                       bool showAreaSuffix,
                       bool interactive,
                       float viewBottom,
                       int& clippedStepsBelow,
                       const TrackerService& service,
                       PinFn&& pinSubCollection,
                       ToggleBitFn&& toggleLocalBit,
                       std::string& lastCopiedWp) {
    ImGui::PushID(bit.index);
    if (ImGui::GetCursorScreenPos().y > viewBottom - 4.0f) {
        ++clippedStepsBelow;
    }

    bool hasWpBtn  = interactive && !bit.completed && !bit.waypointChatLink.empty();
    bool hasPinBtn = interactive && !bit.completed && bit.childAchievementId > 0 &&
                     !service.IsPinned(EntryKind::Achievement, bit.childAchievementId);
    float boxSz      = std::max(18.0f, ImGui::GetTextLineHeight());
    float btnReserve = (hasWpBtn ? (boxSz + 10.0f) : 0.0f) + (hasPinBtn ? (boxSz + 6.0f) : 0.0f);
    float startX     = ImGui::GetCursorPosX();
    float regionMaxX = startX + ImGui::GetContentRegionAvail().x - 8.0f;
    float wrapX      = std::max(startX + 110.0f, regionMaxX - btnReserve);

    ImGui::PushTextWrapPos(wrapX);
    if (interactive && !bit.apiCompleted) {
        if (bit.localCompleted) {
            if (DrawClickableStatusGlyph("##LocChk", StatusGlyph::LocalCompleted,
                                         "Marked done locally (waiting for API) - click to unmark", 11.5f)) {
                toggleLocalBit(bit.index);
            }
            ImGui::TextColored(ImVec4(0.55f, 0.80f, 0.84f, 0.90f), "%s", bit.label.c_str());
        } else {
            if (DrawClickableStatusGlyph("##IncChk", StatusGlyph::Incomplete,
                                         "Click to mark done locally", 11.5f)) {
                toggleLocalBit(bit.index);
            }
            ImGui::TextColored(kColorMuted, "%s", bit.label.c_str());
        }
    } else if (bit.completed) {
        DrawStatusGlyph(bit.localCompleted ? StatusGlyph::LocalCompleted : StatusGlyph::CompletedDim, 11.5f);
        ImGui::TextColored(kColorDoneDim, "%s", bit.label.c_str());
    } else {
        DrawStatusGlyph(StatusGlyph::Incomplete, 11.5f);
        ImGui::TextColored(kColorMuted, "%s", bit.label.c_str());
    }

    bool hoveredStep = ImGui::IsItemHovered();

    if (!bit.completed && showAreaSuffix && !bit.area.empty()) {
        ImGui::SameLine(0.0f, 5.0f);
        ImGui::TextColored(kColorCyan, "(%s)", bit.area.c_str());
        hoveredStep = hoveredStep || ImGui::IsItemHovered();
    }
    ImGui::PopTextWrapPos();

    if (hoveredStep) {
        RenderStepGuideTooltip(bit);
    }

    if (hasWpBtn) {
        ImGui::SameLine(0.0f, 7.0f);
        std::string wpTip = (bit.waypointName.empty() ? "Copy Waypoint " : ("Copy " + bit.waypointName + " ")) +
                            bit.waypointChatLink;
        if (DrawIconButton("##StepWp", IconBtnKind::WaypointDiamond, wpTip.c_str())) {
            ImGui::SetClipboardText(bit.waypointChatLink.c_str());
            lastCopiedWp = bit.waypointChatLink;
        }
    }
    if (hasPinBtn) {
        ImGui::SameLine(0.0f, 4.0f);
        if (DrawIconButton("##StepPinChild", IconBtnKind::PinPlus, "Pin sub-collection")) {
            pinSubCollection(bit.childAchievementId, bit.label);
        }
    }

    ImGui::PopID();
}

} // namespace

UiRenderer::UiRenderer(TrackerService& service,
                       IconTextureLookupFn iconLookup,
                       OpenExternalUrlFn openUrl)
    : service_(service),
      iconLookup_(std::move(iconLookup)),
      openUrl_(std::move(openUrl)) {}

void UiRenderer::DrawIconIfReady(const std::string& iconUrl, float sizePx) {
    if (!iconLookup_ || iconUrl.empty()) return;
    void* srv = iconLookup_(iconUrl);
    if (srv) {
        ImGui::Image(srv, ImVec2(sizePx, sizePx));
        ImGui::SameLine(0.0f, 5.0f);
    }
}

void UiRenderer::RenderWindows(const LiveGameState& gameState) {
    UiSnapshot snap = service_.GetUiSnapshot();

    if (!apiKeyBufInitialized_ && !snap.settings.apiKey.empty()) {
        CopyToBuffer(apiKeyBuf_, sizeof(apiKeyBuf_), snap.settings.apiKey);
        apiKeyBufInitialized_ = true;
    }

    bool pushedFont = false;
    if (gameState.uiFont) {
        ImGui::PushFont(gameState.uiFont);
        pushedFont = true;
    }

    if (snap.settings.showHud) {
        bool shouldHide =
            (snap.settings.hideOutsideGameplay && !gameState.isGameplay) ||
            (snap.settings.hideInCombat && gameState.isInCombat);
        if (!shouldHide) {
            RenderHud(snap, gameState);
        }
    }

    if (snap.settings.showManager) {
        RenderManager(snap);
    }

    if (pushedFont) {
        ImGui::PopFont();
    }
}

void UiRenderer::RenderNexusOptions() {
    UiSnapshot snap = service_.GetUiSnapshot();

    ImGui::TextColored(kColorGold, "GW2 Quest Helper v%s", GW2QH_VERSION_STR);
    ImGui::Separator();

    if (snap.tokenValid) {
        ImGui::TextColored(kColorGreen, "Connected Account: %s",
                           snap.accountName.empty() ? "Verified" : snap.accountName.c_str());
    } else {
        ImGui::TextColored(kColorWarn, "API Key Not Configured / Invalid");
    }

    if (ImGui::Button("Open Quest Manager (ALT+SHIFT+Q)")) {
        service_.SetShowManager(true);
    }
    ImGui::SameLine();
    bool showHud = snap.settings.showHud;
    if (ImGui::Checkbox("Show In-Game HUD (ALT+SHIFT+H)", &showHud)) {
        service_.SetShowHud(showHud);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sync Now")) {
        service_.TriggerSyncNow();
    }

    AddonSettings updated = snap.settings;
    bool changed = false;
    changed |= ImGui::Checkbox("Lock HUD Position", &updated.lockHud);
    if (updated.lockHud) {
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Click-Through When Locked", &updated.clickThroughWhenLock);
    }
    changed |= ImGui::Checkbox("Auto-Advance Questline on Completion", &updated.autoAdvanceQuestline);
    changed |= ImGui::Checkbox("Prioritize Current Map Goals", &updated.prioritizeCurrentMap);
    changed |= ImGui::Checkbox("Toast Alerts on Progress", &updated.showProgressAlerts);
    changed |= ImGui::Checkbox("Hide HUD in Combat", &updated.hideInCombat);
    changed |= ImGui::Checkbox("Hide HUD on Loading / Char Select", &updated.hideOutsideGameplay);
    if (changed) {
        service_.UpdateDisplaySettings(updated);
    }
}

void UiRenderer::RenderHud(const UiSnapshot& snap, const LiveGameState& /*gameState*/) {
    bool anyTwoCol = false;
    for (const auto& ev : snap.evaluatedEntries) {
        if (snap.settings.hideCompletedEntries && ev.completed) continue;
        if (snap.settings.filterCurrentMapOnly && !ev.matchesCurrentMap) continue;
        if (ev.entry.expanded && !ev.bits.empty()) {
            int visBits = 0;
            for (const auto& b : ev.bits) {
                if (!(b.completed && snap.settings.hideCompletedBits)) ++visBits;
            }
            if (visBits > 8) {
                anyTwoCol = true;
                break;
            }
        }
    }

    float bodyW   = anyTwoCol ? 452.0f : 356.0f;
    float minWinW = bodyW + 20.0f;

    ThemeScope theme(true);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(minWinW, 0.0f),
        ImVec2(560.0f, snap.settings.hudMaxHeight));
    ImGui::SetNextWindowBgAlpha(snap.settings.hudBgAlpha);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (snap.settings.lockHud) {
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        if (snap.settings.clickThroughWhenLock) {
            flags |= ImGuiWindowFlags_NoInputs;
        }
    }

    bool open = snap.settings.showHud;
    bool* openPtr = snap.settings.lockHud ? nullptr : &open;
    bool interactive = !(snap.settings.lockHud && snap.settings.clickThroughWhenLock);

    if (ImGui::Begin("Quest Helper##GW2QHHud", openPtr, flags)) {
        // Pinned Top Bar: Character | Map ... [Sync Icon] [Manage Icon]
        if (!snap.activeCharacter.empty()) {
            ImGui::TextColored(kColorGold, "%s", snap.activeCharacter.c_str());
            if (!snap.activeMapName.empty()) {
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::TextColored(kColorMuted, "|  %s", snap.activeMapName.c_str());
            }
        } else {
            ImGui::TextColored(kColorGold, "Quest Helper");
        }

        if (interactive) {
            float rightEdge = ImGui::GetWindowContentRegionMax().x;
            ImGui::SameLine(std::max(180.0f, rightEdge - 42.0f));

            std::string syncTip = "Sync GW2 Account";
            if (snap.syncInProgress) {
                syncTip = "Syncing account data...";
            } else if (snap.secondsSinceLastSync >= 0) {
                int mins = snap.secondsSinceLastSync / 60;
                syncTip = (mins == 0)
                    ? ("Synced " + std::to_string(snap.secondsSinceLastSync) + "s ago (click to refresh)")
                    : ("Synced " + std::to_string(mins) + "m ago (click to refresh)");
            }
            if (DrawIconButton("##HdrSync", IconBtnKind::Sync, syncTip.c_str(), false, snap.syncInProgress)) {
                service_.TriggerSyncNow();
            }
            ImGui::SameLine(0.0f, 4.0f);
            if (DrawIconButton("##HdrManage", IconBtnKind::SettingsSliders, "Manage Goals & Settings (ALT+SHIFT+Q)", snap.settings.showManager)) {
                service_.ToggleShowManager();
            }
        }

        if (!lastCopiedWp_.empty()) {
            ImGui::Spacing();
            DrawIconButton("##WpCopiedBadge", IconBtnKind::WaypointDiamond, "Copied to clipboard", true);
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(kColorGreen, "%s", lastCopiedWp_.c_str());
            ImGui::Spacing();
        }

        ImGui::Separator();

        float maxBodyH  = std::max(80.0f, snap.settings.hudMaxHeight - 58.0f);
        float childH    = (hudBodyContentHeight_ > 1.0f) ? std::min(maxBodyH, hudBodyContentHeight_) : maxBodyH;

        if (ImGui::BeginChild("##HudGoalsBody", ImVec2(bodyW, childH), false, ImGuiWindowFlags_None)) {
            float viewBottom = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;
            float availW     = std::max(260.0f, ImGui::GetContentRegionAvail().x - 4.0f);
            int clippedStepsBelow = 0;

            if (!snap.tokenValid && snap.settings.apiKey.empty()) {
                ImGui::PushTextWrapPos(availW);
                ImGui::TextColored(kColorMuted, "Click the top-right sliders icon to set your GW2 API key.");
                ImGui::PopTextWrapPos();
            }

            if (snap.evaluatedEntries.empty()) {
                ImGui::TextColored(kColorMuted, "No goals pinned (ALT+SHIFT+Q).");
            }

            int visibleCount = 0;
            for (size_t i = 0; i < snap.evaluatedEntries.size(); ++i) {
                const auto& ev = snap.evaluatedEntries[i];
                if (snap.settings.hideCompletedEntries && ev.completed) continue;
                if (snap.settings.filterCurrentMapOnly && !ev.matchesCurrentMap) continue;
                ++visibleCount;

                ImGui::PushID(static_cast<int>(ev.originalIndex));

                // 1. Expand/Collapse Chevron
                if (!ev.bits.empty() && interactive) {
                    if (DrawIconButton("##ExpChevron",
                                       ev.entry.expanded ? IconBtnKind::ChevronDown : IconBtnKind::ChevronRight,
                                       ev.entry.expanded ? "Collapse steps" : "Expand steps")) {
                        service_.SetEntryExpanded(ev.originalIndex, !ev.entry.expanded);
                    }
                    ImGui::SameLine(0.0f, 3.0f);
                }

                // 2. Entry Icon (GW2 icon texture when available, else vector emblem)
                void* texSrv = (iconLookup_ && !ev.iconUrl.empty()) ? iconLookup_(ev.iconUrl) : nullptr;
                if (texSrv) {
                    ImGui::Image(texSrv, ImVec2(18.0f, 18.0f));
                    ImGui::SameLine(0.0f, 5.0f);
                } else {
                    DrawFallbackEntryEmblem(ev.entry.kind, 17.0f);
                }

                // 3. Inline Active Map Pin + Clean Title
                if (ev.matchesCurrentMap) {
                    DrawMapPin(13.0f);
                }

                if (ev.completed) {
                    ImGui::TextColored(kColorGreen, "%s", ev.title.c_str());
                } else {
                    ImGui::Text("%s", ev.title.c_str());
                }

                if (!ev.subtitle.empty() && ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(340.0f);
                    if (ev.matchesCurrentMap) {
                        ImGui::TextColored(kColorGreen, "Active in Current Map");
                    }
                    ImGui::TextUnformatted(ev.subtitle.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }

                // 4. Compact Action Icons beside the Title: [Chain Nodes] [Wiki Link]
                bool hasChainUi = ev.chain.hasParent || !ev.chain.questlineTitle.empty() ||
                                  ev.chain.hasNext || !ev.chain.roadmapNodes.empty();
                if (interactive && hasChainUi) {
                    ImGui::SameLine(0.0f, 5.0f);
                    if (DrawIconButton("##BtnChain", IconBtnKind::ChainNodes, nullptr, ev.entry.showChain)) {
                        service_.SetEntryShowChain(ev.originalIndex, !ev.entry.showChain);
                    }
                    if (ImGui::IsItemHovered()) {
                        RenderChainRoadmapTooltip(ev.chain);
                    }
                }
                if (interactive && !ev.wikiUrl.empty() && openUrl_) {
                    ImGui::SameLine(0.0f, 3.0f);
                    if (DrawIconButton("##BtnWiki", IconBtnKind::ExternalWiki, "Open GW2 Wiki Guide")) {
                        openUrl_(ev.wikiUrl);
                    }
                }

                // 5. Subtle 1-Line Parent / Active Story Step Subtitle
                if (ev.entry.kind == EntryKind::Quest) {
                    if (!ev.completed && !ev.subtitle.empty()) {
                        ImGui::Indent(6.0f);
                        DrawStatusGlyph(StatusGlyph::ActiveStep, 11.0f);
                        ImGui::PushTextWrapPos(availW - 8.0f);
                        ImGui::TextColored(kColorMuted, "%s", ev.subtitle.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::Unindent(6.0f);
                    }
                } else if (ev.chain.hasParent || (ev.chain.stageIndex > 0 && ev.chain.stageCount > 0)) {
                    std::string crumb;
                    if (!ev.chain.parentTitle.empty()) {
                        crumb = ev.chain.parentTitle;
                        if (ev.chain.parentMax > 0) {
                            crumb += " (" + std::to_string(ev.chain.parentCurrent) + "/" +
                                     std::to_string(ev.chain.parentMax) + ")";
                        }
                    } else if (!ev.chain.questlineTitle.empty()) {
                        crumb = ev.chain.questlineTitle;
                    }
                    if (ev.chain.stageIndex > 0 && ev.chain.stageCount > 0) {
                        if (!crumb.empty()) crumb += "  |  ";
                        crumb += std::to_string(ev.chain.stageIndex) + "/" + std::to_string(ev.chain.stageCount);
                    }
                    if (!crumb.empty()) {
                        ImGui::TextColored(kColorSubtle, "   %s", crumb.c_str());
                        if (ImGui::IsItemHovered()) {
                            RenderChainRoadmapTooltip(ev.chain);
                        }
                    }
                }

                // 6. Collapsible Stage-Level Roadmap Drawer
                if (hasChainUi && ev.entry.showChain) {
                    ImGui::Indent(12.0f);
                    if (ev.chain.hasParent && ev.chain.parentId > 0 &&
                        !service_.IsPinned(ev.chain.parentKind, ev.chain.parentId) && interactive) {
                        ImGui::TextColored(kColorSubtle, "%s", ev.chain.parentTitle.c_str());
                        ImGui::SameLine(0.0f, 4.0f);
                        if (DrawIconButton("##PinParent", IconBtnKind::PinPlus, "Also pin parent collection")) {
                            service_.PinEntry(ev.chain.parentKind, ev.chain.parentId, ev.chain.parentTitle);
                        }
                    }

                    for (size_t nIdx = 0; nIdx < ev.chain.roadmapNodes.size(); ++nIdx) {
                        const auto& node = ev.chain.roadmapNodes[nIdx];
                        ImGui::PushID(static_cast<int>(nIdx) + 9000);

                        StatusGlyph g = node.isCurrent
                            ? StatusGlyph::ActiveStep
                            : (node.completed ? StatusGlyph::Completed : StatusGlyph::Incomplete);
                        DrawStatusGlyph(g, 12.0f);

                        ImVec4 col = node.isCurrent ? kColorGold : (node.completed ? kColorGreen : kColorMuted);
                        if (node.max > 1) {
                            ImGui::TextColored(col, "%s (%d/%d)", node.title.c_str(), node.current, node.max);
                        } else {
                            ImGui::TextColored(col, "%s", node.title.c_str());
                        }

                        if (interactive && node.canJump && !node.isCurrent && !node.completed && node.id > 0 &&
                            !service_.IsPinned(node.kind, node.id)) {
                            ImGui::SameLine(0.0f, 5.0f);
                            std::string jumpTip = "Switch tracker to '" + node.title + "'";
                            if (DrawIconButton("##JumpStage", IconBtnKind::ArrowNext, jumpTip.c_str())) {
                                service_.ReplaceEntryWithNext(ev.originalIndex, node.kind, node.id, node.title);
                                ImGui::PopID();
                                break;
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::Unindent(12.0f);
                }

                // 7. Segmented Progress Bar (full content width)
                {
                    float fraction = static_cast<float>(ev.current) / static_cast<float>(std::max(1, ev.target));
                    fraction = std::clamp(fraction, 0.0f, 1.0f);

                    char overlayBuf[64];
                    std::snprintf(overlayBuf, sizeof(overlayBuf), "%d / %d", ev.current, ev.target);
                    DrawReadableProgressBar(fraction, availW, 18.0f, overlayBuf, ev.completed, ev.target);
                }

                // 8. "Up Next" Strip When Completed (Achievement Stages Only)
                if (ev.completed && ev.chain.hasNext && ev.chain.nextId > 0) {
                    if (!ev.chain.transitionHint.empty()) {
                        ImGui::PushTextWrapPos(availW);
                        ImGui::TextColored(kColorWarn, "   %s", ev.chain.transitionHint.c_str());
                        ImGui::PopTextWrapPos();
                    }
                    DrawStatusGlyph(StatusGlyph::ActiveStep, 12.0f);
                    ImGui::TextColored(kColorGold, "%s", ev.chain.nextTitle.c_str());
                    if (interactive) {
                        ImGui::SameLine(0.0f, 6.0f);
                        std::string nextTip = "Continue to '" + ev.chain.nextTitle + "'";
                        if (DrawIconButton("##AdvNext", IconBtnKind::ArrowNext, nextTip.c_str(), true)) {
                            service_.ReplaceEntryWithNext(ev.originalIndex, ev.chain.nextKind, ev.chain.nextId, ev.chain.nextTitle);
                            ImGui::PopID();
                            break;
                        }
                        if (!service_.IsPinned(ev.chain.nextKind, ev.chain.nextId)) {
                            ImGui::SameLine(0.0f, 3.0f);
                            if (DrawIconButton("##PinNextBoth", IconBtnKind::PinPlus, "Also pin next stage")) {
                                service_.PinEntry(ev.chain.nextKind, ev.chain.nextId, ev.chain.nextTitle);
                            }
                        }
                    }
                }

                if (!ev.entry.note.empty()) {
                    ImGui::PushTextWrapPos(availW);
                    ImGui::TextColored(kColorGold, "   %s", ev.entry.note.c_str());
                    ImGui::PopTextWrapPos();
                }

                // 9. Collection / Achievement Checklist Steps:
                //    - Group consecutive steps sharing the same area under compact cyan sub-headers
                //    - Switch to 2-column grid when > 8 visible steps
                //    - Dim completed steps that remain visible
                if (!ev.bits.empty() && ev.entry.expanded) {
                    std::vector<const EvaluatedBitView*> visibleBits;
                    visibleBits.reserve(ev.bits.size());
                    int hiddenDone = 0;
                    for (const auto& bit : ev.bits) {
                        if (bit.completed && snap.settings.hideCompletedBits) {
                            ++hiddenDone;
                        } else {
                            visibleBits.push_back(&bit);
                        }
                    }

                    std::vector<AreaBitGroup> groups;
                    for (const auto* bit : visibleBits) {
                        if (groups.empty() || groups.back().area != bit->area) {
                            groups.push_back(AreaBitGroup{bit->area, {bit}});
                        } else {
                            groups.back().items.push_back(bit);
                        }
                    }

                    bool useTwoCols = (visibleBits.size() > 8);
                    auto pinSubFn = [this](int childAchId, const std::string& label) {
                        service_.PinEntry(EntryKind::Achievement, childAchId, label);
                    };
                    auto toggleBitFn = [this, origIdx = ev.originalIndex](int bitIdx) {
                        service_.ToggleLocalBit(origIdx, bitIdx);
                    };

                    ImGui::Indent(8.0f);
                    float stepAreaW = std::max(220.0f, availW - 10.0f);
                    for (size_t gIdx = 0; gIdx < groups.size(); ++gIdx) {
                        const auto& grp = groups[gIdx];
                        bool hasGroupHeader = !grp.area.empty();
                        if (hasGroupHeader) {
                            if (gIdx > 0) ImGui::Spacing();
                            if (ImGui::GetCursorScreenPos().y > viewBottom - 4.0f) {
                                ++clippedStepsBelow;
                            }
                            ImGui::TextColored(kColorCyan, "%s", grp.area.c_str());
                        }

                        if (useTwoCols && grp.items.size() >= 2) {
                            ImGui::PushID(static_cast<int>(gIdx) + 5000);
                            if (ImGui::BeginTable("##StepCols", 2, ImGuiTableFlags_SizingStretchSame, ImVec2(stepAreaW, 0.0f))) {
                                for (const auto* bit : grp.items) {
                                    ImGui::TableNextColumn();
                                    RenderHudStepItem(*bit, !hasGroupHeader, interactive,
                                                      viewBottom, clippedStepsBelow, service_, pinSubFn, toggleBitFn, lastCopiedWp_);
                                }
                                ImGui::EndTable();
                            }
                            ImGui::PopID();
                        } else {
                            for (const auto* bit : grp.items) {
                                RenderHudStepItem(*bit, !hasGroupHeader, interactive,
                                                  viewBottom, clippedStepsBelow, service_, pinSubFn, toggleBitFn, lastCopiedWp_);
                            }
                        }
                    }

                    bool showHiddenLine = (hiddenDone > 0 && !ev.completed);
                    bool showLocalFooter = showHiddenLine || (ev.localCheckedCount > 0);
                    if (showHiddenLine) {
                        if (ev.localCheckedCount > 0) {
                            ImGui::TextColored(kColorDoneDim, "   +%d completed hidden (%d local)", hiddenDone, ev.localCheckedCount);
                        } else {
                            ImGui::TextColored(kColorDoneDim, "   +%d completed hidden", hiddenDone);
                        }
                    } else if (ev.localCheckedCount > 0) {
                        ImGui::TextColored(ImVec4(0.55f, 0.80f, 0.84f, 0.90f),
                                           "   %d marked locally (pending API sync)", ev.localCheckedCount);
                    }
                    if (interactive && showLocalFooter && ev.localCheckedCount > 0) {
                        ImGui::SameLine(0.0f, 6.0f);
                        if (ImGui::SmallButton("Undo")) {
                            service_.UndoLastLocalBit(ev.originalIndex);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Undo last locally marked step");
                        }
                    }
                    ImGui::Unindent(8.0f);
                }

                if (i + 1 < snap.evaluatedEntries.size()) {
                    ImGui::Spacing();
                }
                ImGui::PopID();
            }

            if (visibleCount == 0 && !snap.evaluatedEntries.empty()) {
                if (snap.settings.filterCurrentMapOnly && snap.activeMapName.empty()) {
                    ImGui::TextColored(kColorMuted, "Resolving current map...");
                } else {
                    ImGui::TextColored(kColorMuted, "No goals match active HUD filters.");
                }
            }

            ImGui::Dummy(ImVec2(1.0f, 4.0f));
            hudBodyContentHeight_ = ImGui::GetCursorPosY() + 2.0f;

            // Smooth bottom fade + "+N more" pill when scrolled above bottom
            float scrollY = ImGui::GetScrollY();
            float scrollMaxY = ImGui::GetScrollMaxY();
            if (scrollMaxY > 1.0f && scrollY < scrollMaxY - 1.0f) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 wp = ImGui::GetWindowPos();
                ImVec2 ws = ImGui::GetWindowSize();
                float fadeRight = wp.x + ws.x - 14.0f;
                dl->AddRectFilledMultiColor(
                    ImVec2(wp.x, wp.y + ws.y - 26.0f),
                    ImVec2(fadeRight, wp.y + ws.y),
                    IM_COL32(14, 17, 23, 0),
                    IM_COL32(14, 17, 23, 0),
                    IM_COL32(14, 17, 23, 230),
                    IM_COL32(14, 17, 23, 230));

                char moreBuf[32];
                if (clippedStepsBelow > 0) {
                    std::snprintf(moreBuf, sizeof(moreBuf), "+%d more", clippedStepsBelow);
                } else {
                    std::snprintf(moreBuf, sizeof(moreBuf), "v more");
                }
                ImVec2 ts = ImGui::CalcTextSize(moreBuf);
                float pillW = ts.x + 14.0f;
                float pillH = ts.y + 4.0f;
                ImVec2 pillTL(fadeRight - pillW - 4.0f, wp.y + ws.y - pillH - 2.0f);
                ImVec2 pillBR(fadeRight - 4.0f, wp.y + ws.y - 2.0f);
                dl->AddRectFilled(pillTL, pillBR, IM_COL32(28, 34, 46, 235), 4.0f);
                dl->AddRect(pillTL, pillBR, IM_COL32(204, 150, 42, 185), 4.0f, 0, 1.0f);
                dl->AddText(ImVec2(pillTL.x + 7.0f, pillTL.y + 2.0f), IM_COL32(245, 200, 82, 255), moreBuf);
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    if (!snap.settings.lockHud && open != snap.settings.showHud) {
        service_.SetShowHud(open);
    }
}

void UiRenderer::RenderManager(const UiSnapshot& snap) {
    ThemeScope theme(false);
    ImGui::SetNextWindowSize(ImVec2(690.0f, 540.0f), ImGuiCond_FirstUseEver);
    bool open = snap.settings.showManager;

    if (ImGui::Begin("Quest Helper - Manager##GW2QHManager", &open)) {
        if (snap.tokenValid) {
            ImGui::TextColored(kColorGreen, "Account: %s",
                               snap.accountName.empty() ? "Connected" : snap.accountName.c_str());
        } else {
            ImGui::TextColored(kColorWarn, "Account: Not Connected (%s)",
                               snap.tokenError.empty() ? "No API Key" : snap.tokenError.c_str());
        }
        ImGui::SameLine();
        ImGui::TextColored(kColorMuted, "(v%s)", GW2QH_VERSION_STR);

        float rightEdge = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(std::max(260.0f, rightEdge - 46.0f));
        std::string syncTip = snap.syncInProgress ? "Syncing account data..." : "Sync GW2 Account";
        if (DrawIconButton("##MgrSync", IconBtnKind::Sync, syncTip.c_str(), false, snap.syncInProgress)) {
            service_.TriggerSyncNow();
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (DrawIconButton("##MgrHudEye", IconBtnKind::HudEye,
                           snap.settings.showHud ? "Hide In-Game HUD" : "Show In-Game HUD",
                           snap.settings.showHud)) {
            service_.ToggleShowHud();
        }

        ImGui::TextColored(kColorMuted, "%s", snap.lastStatusMessage.c_str());
        ImGui::Separator();

        if (ImGui::BeginTabBar("GW2QHTabs")) {
            if (ImGui::BeginTabItem("Search & Pin")) {
                RenderTabSearch(snap);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("In-Progress Achievements")) {
                RenderTabInProgress(snap);
                ImGui::EndTabItem();
            }
            std::string pinnedLabel = "Pinned Goals (" + std::to_string(snap.settings.tracked.size()) + ")###PinnedTab";
            if (ImGui::BeginTabItem(pinnedLabel.c_str())) {
                RenderTabPinned(snap);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("API Key & Settings")) {
                RenderTabSettings(snap);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (open != snap.settings.showManager) {
        service_.SetShowManager(open);
    }
}

void UiRenderer::RenderTabSearch(const UiSnapshot& snap) {
    searchKindIdx_ = (searchKindIdx_ == 1) ? 1 : 0;
    EntryKind selectedKind = (searchKindIdx_ == 1) ? EntryKind::Quest : EntryKind::Achievement;

    // Segmented pill toggle
    bool isAch = (searchKindIdx_ == 0);
    ImGui::PushStyleColor(ImGuiCol_Button, isAch ? ImVec4(0.48f, 0.37f, 0.14f, 0.95f) : ImVec4(0.14f, 0.17f, 0.23f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Text, isAch ? ImVec4(1.0f, 0.95f, 0.80f, 1.0f) : kColorMuted);
    if (ImGui::Button("Collections & Achievements")) {
        searchKindIdx_ = 0;
        selectedKind = EntryKind::Achievement;
    }
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0.0f, 6.0f);

    bool isStory = (searchKindIdx_ == 1);
    ImGui::PushStyleColor(ImGuiCol_Button, isStory ? ImVec4(0.18f, 0.42f, 0.25f, 0.95f) : ImVec4(0.14f, 0.17f, 0.23f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Text, isStory ? ImVec4(0.86f, 1.0f, 0.90f, 1.0f) : kColorMuted);
    if (ImGui::Button("Storylines")) {
        searchKindIdx_ = 1;
        selectedKind = EntryKind::Quest;
    }
    ImGui::PopStyleColor(2);

    float availW = ImGui::GetContentRegionAvail().x;
    bool hasSearchText = (searchBuf_[0] != '\0');
    ImGui::SetNextItemWidth(std::max(200.0f, availW - (hasSearchText ? 112.0f : 84.0f)));
    bool submit = ImGui::InputTextWithHint(
        "##SearchQuery",
        "Search by name, numeric ID, or [&ChatLink]...",
        searchBuf_,
        sizeof(searchBuf_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (hasSearchText) {
        ImGui::SameLine(0.0f, 4.0f);
        if (DrawIconButton("##ClearSearch", IconBtnKind::RemoveX, "Clear search text")) {
            searchBuf_[0] = '\0';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Search", ImVec2(76.0f, 0.0f)) || submit) {
        service_.RequestSearch(selectedKind, searchBuf_);
    }

    if (snap.searchInProgress) {
        ImGui::TextColored(kColorGold, "Searching...");
    } else if (!snap.searchStatus.empty()) {
        ImGui::TextColored(kColorMuted, "%s", snap.searchStatus.c_str());
    }

    ImGui::Separator();

    if (ImGui::BeginChild("SearchResultsScroll", ImVec2(0, 0), true)) {
        if (snap.searchResults.empty() && !snap.searchInProgress) {
            ImGui::Spacing();
            ImGui::Indent(10.0f);
            ImGui::TextColored(kColorGold, "Tips:");
            ImGui::TextColored(kColorMuted, "  - Search achievement or collection names (e.g. 'Skyscale Eggs', 'Vision', 'Aurora').");
            ImGui::TextColored(kColorMuted, "  - Paste an in-game [&ChatLink] or numeric achievement ID directly.");
            ImGui::TextColored(kColorMuted, "  - Switch to 'Storylines' above to find Personal Story / Living World chapters.");
            ImGui::Unindent(10.0f);
        }

        float rowWrapW = std::max(260.0f, ImGui::GetContentRegionAvail().x - 16.0f);
        for (size_t i = 0; i < snap.searchResults.size(); ++i) {
            const auto& r = snap.searchResults[i];
            ImGui::PushID(static_cast<int>(i));

            bool alreadyPinned = service_.IsPinned(r.kind, r.id);
            if (alreadyPinned) {
                DrawStatusGlyph(StatusGlyph::Completed, 14.0f);
            } else if (DrawIconButton("##SearchPinBtn", IconBtnKind::PinPlus, "Pin to HUD")) {
                service_.PinEntry(r.kind, r.id, r.name);
            }
            if (!alreadyPinned) ImGui::SameLine(0.0f, 5.0f);

            if (iconLookup_ && !r.iconUrl.empty() && iconLookup_(r.iconUrl)) {
                ImGui::Image(iconLookup_(r.iconUrl), ImVec2(20.0f, 20.0f));
                ImGui::SameLine(0.0f, 6.0f);
            } else {
                DrawFallbackEntryEmblem(r.kind, 18.0f);
            }

            ImGui::TextColored(KindBadgeColor(r.kind), "%s", KindBadge(r.kind));
            ImGui::SameLine();
            ImGui::Text("%s", r.name.c_str());

            if (r.hasProgress && r.max > 0) {
                ImGui::SameLine(0.0f, 10.0f);
                char barBuf[32];
                std::snprintf(barBuf, sizeof(barBuf), "%d / %d", r.current, r.max);
                float frac = static_cast<float>(r.current) / static_cast<float>(std::max(1, r.max));
                DrawReadableProgressBar(frac, 110.0f, 16.0f, barBuf, r.current >= r.max, r.max);
            }

            if (!r.subtitle.empty()) {
                ImGui::Indent(24.0f);
                ImGui::PushTextWrapPos(rowWrapW);
                ImGui::TextColored(kColorMuted, "%s", r.subtitle.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Unindent(24.0f);
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void UiRenderer::RenderTabInProgress(const UiSnapshot& snap) {
    if (!requestedInitialInProg_ && snap.tokenValid) {
        requestedInitialInProg_ = true;
        service_.RequestLoadInProgressAchievements();
    }

    if (!snap.tokenValid) {
        ImGui::TextColored(kColorWarn, "Connect your GW2 API key (with 'progression' scope) to browse started achievements.");
        return;
    }

    if (ImGui::Button("Refresh In-Progress List")) {
        service_.RequestLoadInProgressAchievements();
    }
    ImGui::SameLine();
    bool hasFilterText = (inProgressFilter_[0] != '\0');
    ImGui::SetNextItemWidth(hasFilterText ? 236.0f : 260.0f);
    ImGui::InputTextWithHint("##InProgFilter", "Filter by name or requirement...", inProgressFilter_, sizeof(inProgressFilter_));
    if (hasFilterText) {
        ImGui::SameLine(0.0f, 4.0f);
        if (DrawIconButton("##ClearInProg", IconBtnKind::RemoveX, "Clear filter text")) {
            inProgressFilter_[0] = '\0';
        }
    }

    if (snap.inProgressLoading) {
        ImGui::SameLine();
        ImGui::TextColored(kColorGold, "Loading achievements...");
    }

    ImGui::Separator();

    if (ImGui::BeginChild("InProgScroll", ImVec2(0, 0), true)) {
        std::string filterStr = Trim(inProgressFilter_);
        float rowWrapW = std::max(260.0f, ImGui::GetContentRegionAvail().x - 16.0f);
        int shown = 0;
        for (size_t i = 0; i < snap.inProgressAchievements.size(); ++i) {
            const auto& r = snap.inProgressAchievements[i];
            if (!filterStr.empty() &&
                !ContainsCaseInsensitive(r.name, filterStr) &&
                !ContainsCaseInsensitive(r.subtitle, filterStr)) {
                continue;
            }
            ++shown;
            ImGui::PushID(static_cast<int>(i));

            bool alreadyPinned = service_.IsPinned(EntryKind::Achievement, r.id);
            if (alreadyPinned) {
                DrawStatusGlyph(StatusGlyph::Completed, 14.0f);
            } else if (DrawIconButton("##InProgPinBtn", IconBtnKind::PinPlus, "Pin to HUD")) {
                service_.PinEntry(EntryKind::Achievement, r.id, r.name);
            }
            if (!alreadyPinned) ImGui::SameLine(0.0f, 5.0f);

            if (iconLookup_ && !r.iconUrl.empty() && iconLookup_(r.iconUrl)) {
                ImGui::Image(iconLookup_(r.iconUrl), ImVec2(20.0f, 20.0f));
                ImGui::SameLine(0.0f, 6.0f);
            } else {
                DrawFallbackEntryEmblem(EntryKind::Achievement, 18.0f);
            }

            char barBuf[32];
            std::snprintf(barBuf, sizeof(barBuf), "%d / %d", r.current, std::max(1, r.max));
            float frac = static_cast<float>(r.current) / static_cast<float>(std::max(1, r.max));
            DrawReadableProgressBar(frac, 110.0f, 16.0f, barBuf, r.current >= r.max, r.max);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::Text("%s (#%d)", r.name.c_str(), r.id);

            if (!r.subtitle.empty()) {
                ImGui::Indent(24.0f);
                ImGui::PushTextWrapPos(rowWrapW);
                ImGui::TextColored(kColorMuted, "%s", r.subtitle.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Unindent(24.0f);
            }
            ImGui::Separator();
            ImGui::PopID();
        }
        if (shown == 0 && !snap.inProgressLoading) {
            ImGui::Spacing();
            ImGui::TextColored(kColorMuted, "No matching in-progress achievements found.");
        }
    }
    ImGui::EndChild();
}

void UiRenderer::RenderTabPinned(const UiSnapshot& snap) {
    if (snap.settings.tracked.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kColorMuted, "Nothing pinned yet. Add collections, achievements, or storylines from 'Search & Pin'.");
        return;
    }

    std::vector<const EvaluatedEntryView*> byOriginal(snap.settings.tracked.size(), nullptr);
    for (const auto& ev : snap.evaluatedEntries) {
        if (ev.originalIndex < byOriginal.size()) {
            byOriginal[ev.originalIndex] = &ev;
        }
    }

    if (ImGui::BeginChild("PinnedListScroll", ImVec2(0, 0), true)) {
        for (size_t i = 0; i < byOriginal.size(); ++i) {
            const auto* evPtr = byOriginal[i];
            if (!evPtr) continue;
            const auto& ev = *evPtr;

            ImGui::PushID(static_cast<int>(i));

            if (DrawIconButton("##PinUp", IconBtnKind::ChevronUp, "Move Up") && i > 0) {
                service_.MoveEntry(i, -1);
            }
            ImGui::SameLine(0.0f, 3.0f);
            if (DrawIconButton("##PinDown", IconBtnKind::ChevronDown, "Move Down") && i + 1 < byOriginal.size()) {
                service_.MoveEntry(i, 1);
            }
            ImGui::SameLine(0.0f, 6.0f);

            if (iconLookup_ && !ev.iconUrl.empty() && iconLookup_(ev.iconUrl)) {
                ImGui::Image(iconLookup_(ev.iconUrl), ImVec2(20.0f, 20.0f));
                ImGui::SameLine(0.0f, 6.0f);
            } else {
                DrawFallbackEntryEmblem(ev.entry.kind, 18.0f);
            }

            ImGui::TextColored(ev.completed ? kColorGreen : kColorGold, "%s", ev.title.c_str());
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextColored(kColorMuted, "(#%d)", ev.entry.id);

            ImGui::SameLine(0.0f, 10.0f);
            char barBuf[32];
            std::snprintf(barBuf, sizeof(barBuf), "%d / %d", ev.current, std::max(1, ev.target));
            float frac = static_cast<float>(ev.current) / static_cast<float>(std::max(1, ev.target));
            DrawReadableProgressBar(frac, 110.0f, 16.0f, barBuf, ev.completed, ev.target);

            if (!ev.wikiUrl.empty() && openUrl_) {
                ImGui::SameLine(0.0f, 8.0f);
                if (DrawIconButton("##PinnedWikiBtn", IconBtnKind::ExternalWiki, "Open GW2 Wiki Guide")) {
                    openUrl_(ev.wikiUrl);
                }
            }
            ImGui::SameLine(0.0f, 4.0f);
            if (DrawIconButton("##PinEdit", IconBtnKind::EditPencil, "Edit custom note", editingPinnedIndex_ == static_cast<int>(i))) {
                if (editingPinnedIndex_ == static_cast<int>(i)) {
                    editingPinnedIndex_ = -1;
                } else {
                    editingPinnedIndex_ = static_cast<int>(i);
                    CopyToBuffer(editNoteBuf_, sizeof(editNoteBuf_), ev.entry.note);
                }
            }
            ImGui::SameLine(0.0f, 4.0f);
            if (DrawIconButton("##PinUnpin", IconBtnKind::RemoveX, "Unpin goal")) {
                if (editingPinnedIndex_ == static_cast<int>(i)) editingPinnedIndex_ = -1;
                service_.UnpinEntry(i);
                ImGui::PopID();
                break;
            }

            if (!ev.entry.note.empty() && editingPinnedIndex_ != static_cast<int>(i)) {
                ImGui::Indent(26.0f);
                ImGui::TextColored(kColorMuted, "Note: %s", ev.entry.note.c_str());
                ImGui::Unindent(26.0f);
            }

            if (editingPinnedIndex_ == static_cast<int>(i)) {
                ImGui::Indent(26.0f);
                ImGui::SetNextItemWidth(340.0f);
                ImGui::InputTextWithHint("##PinnedNoteInput", "Custom note (include map name for green map pin)", editNoteBuf_, sizeof(editNoteBuf_));
                ImGui::SameLine();
                if (ImGui::SmallButton("Save")) {
                    service_.UpdateEntryNote(i, Trim(editNoteBuf_));
                    editingPinnedIndex_ = -1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Cancel")) {
                    editingPinnedIndex_ = -1;
                }
                ImGui::Unindent(26.0f);
            }

            ImGui::Separator();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void UiRenderer::RenderTabSettings(const UiSnapshot& snap) {
    ImGui::TextColored(kColorGold, "Guild Wars 2 API Key");
    ImGui::TextColored(kColorMuted, "Create a key at account.arena.net/applications with scopes:");
    ImGui::TextColored(kColorCyan, "account, progression, unlocks, characters");

    ImGuiInputTextFlags pwFlags = showApiKeyPlain_ ? 0 : ImGuiInputTextFlags_Password;
    ImGui::SetNextItemWidth(420.0f);
    ImGui::InputText("##ApiKeyInput", apiKeyBuf_, sizeof(apiKeyBuf_), pwFlags);
    ImGui::SameLine();
    ImGui::Checkbox("Show", &showApiKeyPlain_);

    if (ImGui::Button("Save & Verify Key")) {
        service_.SetApiKey(apiKeyBuf_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload from .env")) {
        if (service_.ReloadApiKeyFromEnv()) {
            UiSnapshot refreshed = service_.GetUiSnapshot();
            CopyToBuffer(apiKeyBuf_, sizeof(apiKeyBuf_), refreshed.settings.apiKey);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Key")) {
        apiKeyBuf_[0] = '\0';
        service_.SetApiKey("");
    }

    ImGui::TextColored(kColorMuted, ".env path: %s", service_.GetEnvFilePath().c_str());

    if (!snap.permissions.empty()) {
        ImGui::Spacing();
        ImGui::Text("Active Scopes:");
        for (const char* req : {"account", "progression", "unlocks", "characters"}) {
            ImGui::SameLine();
            bool ok = std::find(snap.permissions.begin(), snap.permissions.end(), req) != snap.permissions.end();
            DrawStatusGlyph(ok ? StatusGlyph::Completed : StatusGlyph::Warning, 12.0f);
            ImGui::TextColored(ok ? kColorGreen : kColorWarn, "%s", req);
        }
    } else if (!snap.tokenError.empty()) {
        ImGui::TextColored(kColorError, "Error: %s", snap.tokenError.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(kColorGold, "Questline Progression, HUD & Alert Preferences");

    AddonSettings updated = snap.settings;
    bool changed = false;

    changed |= ImGui::Checkbox("Auto-Advance to Next Stage in Questline When Completed", &updated.autoAdvanceQuestline);
    ImGui::TextColored(kColorMuted, "   (When off, shows an arrow button to advance once a quest finishes.)");
    changed |= ImGui::Checkbox("Show In-Game HUD Overlay (ALT+SHIFT+H)", &updated.showHud);
    changed |= ImGui::Checkbox("Lock HUD Window (Borderless & Immovable)", &updated.lockHud);
    if (updated.lockHud) {
        ImGui::Indent(18.0f);
        changed |= ImGui::Checkbox("Click-Through When Locked (Pass mouse clicks to GW2)", &updated.clickThroughWhenLock);
        ImGui::Unindent(18.0f);
    }
    changed |= ImGui::Checkbox("Prioritize Goals Matching Current Map", &updated.prioritizeCurrentMap);
    changed |= ImGui::Checkbox("Only Show Goals Matching Current Map on HUD", &updated.filterCurrentMapOnly);
    changed |= ImGui::Checkbox("Show Nexus Toast Alerts When Progress Increases", &updated.showProgressAlerts);
    changed |= ImGui::Checkbox("Hide HUD While in Combat", &updated.hideInCombat);
    changed |= ImGui::Checkbox("Hide HUD on Loading Screens / Character Select", &updated.hideOutsideGameplay);
    changed |= ImGui::Checkbox("Hide Completed Goals from HUD", &updated.hideCompletedEntries);
    changed |= ImGui::Checkbox("Hide Completed Collection Steps on HUD", &updated.hideCompletedBits);

    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderFloat("HUD Max Height (before scroll)", &updated.hudMaxHeight, 180.0f, 800.0f, "%.0f px");

    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderFloat("HUD Background Opacity", &updated.hudBgAlpha, 0.15f, 1.0f, "%.2f");

    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderInt("Auto-Sync Interval (seconds)", &updated.refreshIntervalSec, 30, 600, "%d sec");

    if (changed) {
        service_.UpdateDisplaySettings(updated);
    }
}

} // namespace gw2qh
