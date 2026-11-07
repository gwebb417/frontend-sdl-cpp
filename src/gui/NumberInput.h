#pragma once

/**
 * @brief Type-in number boxes with an Apply button, used for all numeric settings.
 *
 * - Typed values are NOT applied while typing, on Tab or when clicking elsewhere. They stay in
 *   the box as "pending" (highlighted) until Enter is pressed in one of the boxes or the Apply
 *   button next to them is clicked. This allows e.g. typing width, Tab, height, then Enter.
 * - Enter or Apply applies all pending boxes of the same setting at once.
 * - While editing, Up/Down step the typed value (Shift: 10x). Like typing, this needs Enter/Apply.
 * - ESC cancels the pending value of the box being edited.
 * - Applied values are clamped to [min, max].
 * - If the value is changed elsewhere (e.g. the Reset button), pending input is discarded.
 *
 * All functions return true if a value was applied (changed).
 */
namespace NumberInput {

/**
 * @brief Integer input box(es) with one Apply button.
 * @param id Unique ImGui ID (e.g. "##window_size").
 * @param values Pointer to @a components integers.
 * @param components Number of values, e.g. 2 for width/height.
 * @param min Minimum value.
 * @param max Maximum value.
 * @param label Optional visible label drawn after the Apply button.
 * @return true if any value was changed.
 */
bool Int(const char* id, int* values, int components, int min, int max, const char* label = nullptr);

/**
 * @brief Float input box with Apply button.
 * The Up/Down step is roughly 1/100th of the range, e.g. 0.01 for 0..2, 0.1 for 0..10, 1 for 1..240.
 * @param id Unique ImGui ID (e.g. "##display_duration").
 * @param value The value to edit.
 * @param min Minimum value.
 * @param max Maximum value.
 * @param label Optional visible label drawn after the Apply button.
 * @return true if the value was changed.
 */
bool Float(const char* id, float* value, float min, float max, const char* label = nullptr);

} // namespace NumberInput
