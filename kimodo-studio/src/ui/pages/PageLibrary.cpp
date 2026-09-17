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

    // Render list of animation cards
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::BeginGroup();
        {
            ImGui::TextColored(UIStyle::text, "%s %s", icons::kPlay, e.prompt.c_str());
            ImGui::TextDisabled("Frames: %d | FPS: %.0f | Duration: %.2fs | Skeleton: %s",
                                e.frames, e.fps, (e.fps > 0 ? (static_cast<float>(e.frames) / e.fps) : 0.0f),
                                e.skeleton.c_str());

            if (ImGui::Button(ICON_FA_PLAY " Play in Viewport")) {
                Animation anim;
                if (library.loadAnimation(e, anim)) {
                    player.load(anim);
                    toasts.push("Loaded: " + e.prompt, ToastKind::Success);
                } else {
                    toasts.push("Failed to load animation", ToastKind::Error);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TRASH " Delete")) {
                if (library.remove(e.id)) {
                    toasts.push("Deleted animation clip", ToastKind::Info);
                }
            }
        }
        ImGui::EndGroup();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PopID();
    }
}

} // namespace studio
