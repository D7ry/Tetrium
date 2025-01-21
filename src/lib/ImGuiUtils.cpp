#include "ImGuiUtils.h"

#include "imgui.h"
#include "imgui_internal.h"

void ImGuiU::DrawCenteredText(const char* text, const ImVec4& windowBackground)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBackground);
    if (ImGui::Begin(
            "Fullscreen",
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoSavedSettings
        )) {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        ImVec2 centerPos((screenSize.x - textSize.x) * 0.5f, (screenSize.y - textSize.y) * 0.5f);

        ImGui::SetCursorPos(centerPos);
        ImGui::Text("%s", text);
    }
    ImGui::PopStyleColor();
    ImGui::End();
}

void ImGuiU::DrawCursor(const ImGuiTexture& cursorTexture)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 mousePos = io.MousePos;
    ImVec2 cursorSize(cursorTexture.width * 2, cursorTexture.height * 2);
    ImVec2 cursorPos(mousePos.x, mousePos.y);
    drawList->AddImage(
        (ImTextureID)cursorTexture.id,
        cursorPos,
        ImVec2(cursorPos.x + cursorSize.x, cursorPos.y + cursorSize.y)
    );
}

void ImGuiU::DrawFootNote(const char* footnoteText)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize = io.DisplaySize;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 textSize = ImGui::CalcTextSize(footnoteText);
    ImVec2 padding(10.0f, 5.0f);
    ImVec2 pos(
        windowSize.x - textSize.x - padding.x * 2, windowSize.y - textSize.y - padding.y * 2
    );

    drawList->AddRectFilled(pos, ImVec2(windowSize.x, windowSize.y), IM_COL32(0, 0, 0, 200));
    drawList->AddText(
        ImVec2(pos.x + padding.x, pos.y + padding.y), IM_COL32(255, 255, 255, 255), footnoteText
    );
}

static void renderFrameGradient(
    ImVec2 p_min,
    ImVec2 p_max,
    ImVec4 col_left,
    ImVec4 col_right,
    bool borders,
    float rounding
)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    // Convert ImVec4 to ImU32 color format (from [0,1] range to [0,255] range)
    ImU32 col_left_u32 = ImColor(col_left.x, col_left.y, col_left.z, col_left.w);
    ImU32 col_right_u32 = ImColor(col_right.x, col_right.y, col_right.z, col_right.w);

    // Add a filled rectangle with gradient from left to right
    window->DrawList->AddRectFilledMultiColor(
        p_min, p_max, col_left_u32, col_right_u32, col_right_u32, col_left_u32
    );

    const float border_size = g.Style.FrameBorderSize;
    if (borders && border_size > 0.0f) {
        window->DrawList->AddRect(
            p_min + ImVec2(1, 1),
            p_max + ImVec2(1, 1),
            ImGui::GetColorU32(ImGuiCol_BorderShadow),
            rounding,
            0,
            border_size
        );
        window->DrawList->AddRect(
            p_min, p_max, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, border_size
        );
    }
}


bool ImGuiU::GradientSliderFloat(
    const char* label,
    float* p_data,
    float v_min,
    float v_max,
    const ImVec4& left_color,
    const ImVec4& right_color,
    const char* format,
    ImGuiSliderFlags flags
)
{
    float* p_min = &v_min;
    float* p_max = &v_max;
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(
        window->DC.CursorPos,
        window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f)
    );
    const ImRect total_bb(
        frame_bb.Min,
        frame_bb.Max
            + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f)
    );

    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(ImGuiDataType_Float)->PrintFmt;

    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.InFlags);
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active) {
        // Tabbing or CTRL-clicking on Slider turns it into an input box
        const bool clicked = hovered && IsMouseClicked(0, ImGuiInputFlags_None, id);
        const bool make_active = (clicked || g.NavActivateId == id);
        if (make_active && clicked)
            SetKeyOwner(ImGuiKey_MouseLeft, id);
        if (make_active && temp_input_allowed)
            if ((clicked && g.IO.KeyCtrl)
                || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput)))
                temp_input_is_active = true;

        if (make_active && !temp_input_is_active) {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        }
    }

    if (temp_input_is_active) {
        // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
        const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;
        return TempInputScalar(
            frame_bb,
            id,
            label,
            ImGuiDataType_Float,
            p_data,
            format,
            is_clamp_input ? p_min : NULL,
            is_clamp_input ? p_max : NULL
        );
    }

    // Draw frame
    const ImU32 frame_col = GetColorU32(
        g.ActiveId == id ? ImGuiCol_FrameBgActive
        : hovered        ? ImGuiCol_FrameBgHovered
                         : ImGuiCol_FrameBg
    );
    RenderNavHighlight(frame_bb, id);
    renderFrameGradient(
        frame_bb.Min, frame_bb.Max, left_color, right_color, true, g.Style.FrameRounding
    );
    //RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(
        frame_bb, id, ImGuiDataType_Float, p_data, p_min, p_max, format, flags, &grab_bb
    );
    if (value_changed)
        MarkItemEdited(id);

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
        window->DrawList->AddRectFilled(
            grab_bb.Min,
            grab_bb.Max,
            GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab),
            style.GrabRounding
        );

    // Display value using user-provided display format so user can add prefix/suffix/decorations to
    // the value.
    char value_buf[64];
    const char* value_buf_end
        = value_buf
          + DataTypeFormatString(
              value_buf, IM_ARRAYSIZE(value_buf), ImGuiDataType_Float, p_data, format
          );
    if (g.LogEnabled)
        LogSetNextTextDecoration("{", "}");
    RenderTextClipped(
        frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f)
    );

    if (label_size.x > 0.0f)
        RenderText(
            ImVec2(
                frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y
            ),
            label
        );

    IMGUI_TEST_ENGINE_ITEM_INFO(
        id,
        label,
        g.LastItemData.StatusFlags | (temp_input_allowed ? ImGuiItemStatusFlags_Inputable : 0)
    );
    return value_changed;
}