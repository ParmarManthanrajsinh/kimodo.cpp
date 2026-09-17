#include "ui/pages/PageLibrary.h"
#include "animation/AnimationPlayer.h"
#include "imgui.h"
#include "library/AnimationLibrary.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"

namespace studio {

void PageLibrary::draw(AppState& state, AnimationLibrary& library,
                       AnimationPlayer& player, Toasts& toasts) {
    (void)state;
    ImGui::TextColored(UIStyle::accent, "%s Animation Library", icons::kFolder);
    ImGui::TextDisabled("Browse, load, and manage saved motion animations");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const auto& entries = library.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("Library is empty. Generate motion in the Generate page to save clips here.");
        return;
    }

    ImGui::TextDisabled("Total clips: %llu", static_cast<unsigned long long>(entries.size()));
    ImGui::Spacing();

    auto trimPrompt = [](const std::string& str) -> std::string {
        std::string s = str;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
            s.pop_back();
        }
        return s;
    };

    // Render list of animation cards
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.24f, 0.32f, 0.60f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));

        std::string cardId = "##ClipCard_" + std::to_string(i);
        if (ImGui::BeginChild(cardId.c_str(), ImVec2(0, 96), true)) {
            std::string cleanPrompt = trimPrompt(e.prompt);
            ImGui::TextColored(UIStyle::text, "%s %s", icons::kPlay, cleanPrompt.c_str());
            ImGui::TextDisabled("Frames: %d | FPS: %.0f | Duration: %.2fs | Skeleton: %s",
                                e.frames, e.fps, (e.fps > 0 ? (static_cast<float>(e.frames) / e.fps) : 0.0f),
                                e.skeleton.c_str());
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_PLAY " Play in Viewport", ImVec2(140, 24))) {
                Animation anim;
                if (library.loadAnimation(e, anim)) {
                    player.load(anim);
                    toasts.push("Loaded: " + cleanPrompt, ToastKind::Success);
                } else {
                    toasts.push("Failed to load animation", ToastKind::Error);
                }
            }
            ImGui::SameLine(0, 8);
            if (ImGui::Button(ICON_FA_TRASH " Delete", ImVec2(90, 24))) {
                if (library.remove(e.id)) {
                    toasts.push("Deleted animation clip", ToastKind::Info);
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::Spacing();

        ImGui::PopID();
    }
}

} // namespace studio
