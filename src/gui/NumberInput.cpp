#include "NumberInput.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

/**
 * @brief Typed, not yet applied text of one number box.
 */
struct PendingInput {
    std::string text;  //!< The typed text.
    double baseValue{}; //!< The applied value when the text was typed. If it changes elsewhere, the pending text is dropped.
};

std::unordered_map<ImGuiID, PendingInput> pendingInputs; //!< Pending text per box ID.

/**
 * @brief Per-box context for the Up/Down callback.
 */
struct StepContext {
    double value{};   //!< The current value, used if the typed text can't be parsed.
    double step{};    //!< The step size for one Up/Down press.
    double min{};     //!< Minimum allowed value.
    double max{};     //!< Maximum allowed value.
    bool isInteger{}; //!< True to format the result as an integer.
};

std::string FormatValue(double value, bool isInteger)
{
    char buffer[64];
    if (isInteger)
    {
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::lround(value)));
    }
    else
    {
        // %g drops trailing zeros: "30", "1.5", "0.05".
        std::snprintf(buffer, sizeof(buffer), "%.6g", value);
    }
    return buffer;
}

bool ParseValue(const char* text, double& value)
{
    char* end{nullptr};
    const double parsed = std::strtod(text, &end);
    if (end == text)
    {
        return false;
    }
    value = parsed;
    return true;
}

int StepCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory)
    {
        return 0;
    }

    const auto* context = static_cast<StepContext*>(data->UserData);

    double current = context->value;
    ParseValue(data->Buf, current);

    double direction = data->EventKey == ImGuiKey_UpArrow ? 1.0 : -1.0;
    if (ImGui::GetIO().KeyShift)
    {
        direction *= 10.0;
    }

    // Snap to the step grid so repeated presses don't accumulate rounding noise.
    double newValue = std::round(current / context->step + direction) * context->step;
    newValue = std::clamp(newValue, context->min, context->max);

    const std::string text = FormatValue(newValue, context->isInteger);
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text.c_str());
    data->SelectAll();

    return 0;
}

/**
 * @brief Draws one row of number boxes plus an Apply button.
 * @return true if any value was applied.
 */
bool NumberRow(const char* id, double* values, int components, double min, double max, double step, bool isInteger, const char* label)
{
    const auto& style = ImGui::GetStyle();

    ImGui::PushID(id);

    // Box width: share the available space with the Apply button, but don't make boxes huge.
    const float applyButtonWidth = ImGui::CalcTextSize("Apply").x + style.FramePadding.x * 2.0f;
    const float available = ImGui::GetContentRegionAvail().x - applyButtonWidth - style.ItemSpacing.x * static_cast<float>(components);
    const float boxWidth = std::clamp(available / static_cast<float>(components),
                                      ImGui::GetFontSize() * 3.0f, ImGui::GetFontSize() * 7.0f);

    std::vector<ImGuiID> boxIds(components);
    bool applyRequested{false};

    for (int i = 0; i < components; ++i)
    {
        ImGui::PushID(i);
        if (i > 0)
        {
            ImGui::SameLine();
        }

        const ImGuiID boxId = ImGui::GetID("##number");
        boxIds[i] = boxId;

        // Drop pending text if the value was changed elsewhere, e.g. by the Reset button.
        auto pending = pendingInputs.find(boxId);
        if (pending != pendingInputs.end() && pending->second.baseValue != values[i])
        {
            pendingInputs.erase(pending);
            pending = pendingInputs.end();
        }

        const bool isPending = pending != pendingInputs.end();

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%s", isPending ? pending->second.text.c_str() : FormatValue(values[i], isInteger).c_str());

        StepContext context{values[i], step, min, max, isInteger};

        if (isPending)
        {
            // Highlight boxes with unapplied input.
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.40f, 0.05f, 0.75f));
        }

        ImGui::SetNextItemWidth(boxWidth);
        ImGui::InputText("##number", buffer, sizeof(buffer),
                         ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CallbackHistory,
                         &StepCallback, &context);

        if (isPending)
        {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemEdited())
        {
            if (buffer == FormatValue(values[i], isInteger))
            {
                // Typed back to the current value, nothing to apply.
                pendingInputs.erase(boxId);
            }
            else
            {
                pendingInputs[boxId] = {buffer, values[i]};
            }
        }

        if (ImGui::IsItemDeactivated())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                pendingInputs.erase(boxId);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            {
                applyRequested = true;
            }
            // Tab or clicking elsewhere: keep the pending text.
        }

        ImGui::PopID();
    }

    bool anyPending{false};
    for (const auto boxId : boxIds)
    {
        anyPending |= pendingInputs.count(boxId) > 0;
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!anyPending);
    if (anyPending)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.40f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.52f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.60f, 0.15f, 1.0f));
    }
    if (ImGui::Button("Apply"))
    {
        applyRequested = true;
    }
    if (anyPending)
    {
        ImGui::PopStyleColor(3);
    }
    ImGui::EndDisabled();

    bool changed{false};
    if (applyRequested)
    {
        for (int i = 0; i < components; ++i)
        {
            auto pending = pendingInputs.find(boxIds[i]);
            if (pending == pendingInputs.end())
            {
                continue;
            }

            double newValue = values[i];
            if (ParseValue(pending->second.text.c_str(), newValue))
            {
                if (isInteger)
                {
                    newValue = std::round(newValue);
                }
                newValue = std::clamp(newValue, min, max);
                if (newValue != values[i])
                {
                    values[i] = newValue;
                    changed = true;
                }
            }

            // Applied, or unparseable text: either way the box shows the current value again.
            pendingInputs.erase(pending);
        }
    }

    ImGui::PopID();

    if (label != nullptr && label[0] != '\0')
    {
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(label);
    }

    return changed;
}

} // namespace

namespace NumberInput {

bool Int(const char* id, int* values, int components, int min, int max, const char* label)
{
    std::vector<double> doubleValues(values, values + components);

    if (!NumberRow(id, doubleValues.data(), components, min, max, 1.0, true, label))
    {
        return false;
    }

    for (int i = 0; i < components; ++i)
    {
        values[i] = static_cast<int>(doubleValues[i]);
    }
    return true;
}

bool Float(const char* id, float* value, float min, float max, const char* label)
{
    const double range = std::max(static_cast<double>(max) - static_cast<double>(min), 1e-6);
    const double step = std::pow(10.0, std::floor(std::log10(range))) / 100.0;

    double doubleValue = *value;
    if (!NumberRow(id, &doubleValue, 1, min, max, step, false, label))
    {
        return false;
    }

    *value = static_cast<float>(doubleValue);
    return true;
}

} // namespace NumberInput
