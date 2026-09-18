#include "ui/pages/PageSettings.h"
#include "app/SettingsManager.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"

namespace studio {

void SPageSettings::Draw(FAppState& state, FViewport& viewport, SToasts& toasts) {
    (void)state;
    ImGui::TextColored(FUIStyle::accent, "%s Workstation Settings", icons::kSettings);
    ImGui::TextDisabled("Customize themes, viewport rendering defaults, and export paths");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& settings = FSettingsManager::GetInstance().GetSettings();

    // 1. Theme Selection
    ImGui::Text("Interface Theme:");
    if (ImGui::RadioButton("Dark (Default)", settings.theme == "Dark")) {
        settings.theme = "Dark";
        FTheme::apply();
        FSettingsManager::GetInstance().save();
    }
    ImGui::SameLine(0, 16);
    if (ImGui::RadioButton("Cyber Neon", settings.theme == "Cyber")) {
        settings.theme = "Cyber";
        FTheme::apply();
        FSettingsManager::GetInstance().save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Viewport Rendering Defaults
    ImGui::Text("Viewport Defaults:");
    bool grid = viewport.ShowGrid();
    if (ImGui::Checkbox("Show 3D Grid", &grid)) {
        viewport.SetGrid(grid);
        settings.showGrid = grid;
    }
    bool axes = viewport.ShowAxes();
    if (ImGui::Checkbox("Show Coordinate Axes", &axes)) {
        viewport.SetAxes(axes);
        settings.showAxes = axes;
    }
    bool floor = viewport.ShowFloor();
    if (ImGui::Checkbox("Show Floor Plane", &floor)) {
        viewport.SetFloor(floor);
        settings.showFloor = floor;
    }
    bool wire = viewport.ShowWireframe();
    if (ImGui::Checkbox("Wireframe Shading", &wire)) {
        viewport.SetWireframe(wire);
        settings.showWireframe = wire;
    }
    bool bones = viewport.ShowBoneNames();
    if (ImGui::Checkbox("3D Bone Names", &bones)) {
        viewport.SetBoneNames(bones);
        settings.showBoneNames = bones;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Application Paths
    ImGui::Text("Storage Paths:");
    ImGui::TextDisabled("App Data: %s", FAppPaths::appDataDir().string().c_str());
    ImGui::TextDisabled("Models Directory: %s", FAppPaths::defaultModelsDir().string().c_str());
    ImGui::TextDisabled("Characters Directory: %s", FAppPaths::defaultCharactersDir().string().c_str());
    ImGui::TextDisabled("Animations Directory: %s", FAppPaths::defaultAnimationsDir().string().c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_CHECK " Save Settings", ImVec2(180, 36))) {
        FSettingsManager::GetInstance().save();
        toasts.Push("Settings saved successfully", EToastKind::Success);
    }
}

} // namespace studio
