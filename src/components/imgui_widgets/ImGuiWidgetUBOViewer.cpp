#include "Quarkolor.h"

#include "ImGuiWidget.h"

void DrawMat4(
    const char* label,
    glm::mat4& matrix,
    bool editable = false
) {
    ImGui::PushID(label);
    ImGui::Text("%s:", label);

    if (ImGui::BeginTable(
            "MatrixTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
        )) {
        for (int row = 0; row < 4; row++) {
            ImGui::TableNextRow();
            for (int col = 0; col < 4; col++) {
                ImGui::TableSetColumnIndex(col);
                ImGui::PushID(row * 4 + col);

                if (editable) {
                    float value = matrix[col][row];
                    if (ImGui::InputFloat("##v", &value, 0.0f, 0.0f, "%.3f")) {
                        matrix[col][row] = value;
                    }
                } else {
                    ImGui::Text("%.3f", matrix[col][row]);
                }

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    ImGui::PopID();
}

void ImGuiWidgetUBOViewer::Draw(const Quarkolor* engine, ColorSpace colorSpace) {
    const VQBuffer& currentUBO
        = engine->_engineUBOStatic[engine->_currentFrame];

    Quarkolor::EngineUBOStatic* ubo
        = reinterpret_cast<Quarkolor::EngineUBOStatic*>(
            currentUBO.bufferAddress
        );

    DrawMat4("view", ubo->view);
    DrawMat4("proj", ubo->proj);
    ImGui::Text("timeSinceStartSeconds: %f", ubo->timeSinceStartSeconds);
    ImGui::Text("sinWave: %f", ubo->sinWave);
}
