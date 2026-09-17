#include "ui/pages/PageSettings.h"
#include "app/SettingsManager.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"

namespace studio {

void PageSettings::draw(AppState& state, Viewport& viewport, Toasts& toasts) {
    (void)state;
    ImGui::TextColored(UIStyle::accent, "%s Workstation Settings", icons::kSettings);
    ImGui::TextDisabled("Customize themes, viewport rendering defaults, and export paths");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& settings = SettingsManager::instance().settings();

    // 1. Theme Selection
    ImGui::Text("Interface Theme:");
    if (ImGui::RadioButton("Dark (Default)", settings.theme == "Dark")) {
        settings.theme = "Dark";
        Theme::apply();
        SettingsManager::instance().save();
    }
    ImGui::SameLine(0, 16);
    if (ImGui::RadioButton("Cyber Neon", settings.theme == "Cyber")) {
        settings.theme = "Cyber";
        Theme::apply();
        SettingsManager::instance().save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Viewport Rendering Defaults
    ImGui::Text("Viewport Defaults:");
    bool grid = viewport.showGrid();
    if (ImGui::Checkbox("Show 3D Grid", &grid)) {
        viewport.setGrid(grid);
        settings.showGrid = grid;
    }
    bool axes = viewport.showAxes();
    if (ImGui::Checkbox("Show Coordinate Axes", &axes)) {
        viewport.setAxes(axes);
        settings.showAxes = axes;
    }
    bool floor = viewport.showFloor();
    if (ImGui::Checkbox("Show Floor Plane", &floor)) {
        viewport.setFloor(floor);
        settings.showFloor = floor;
    }
    bool wire = viewport.showWireframe();
    if (ImGui::Checkbox("Wireframe Shading", &wire)) {
        viewport.setWireframe(wire);
        settings.showWireframe = wire;
    }
    bool bones = viewport.showBoneNames();
    if (ImGui::Checkbox("3D Bone Names", &bones)) {
        viewport.setBoneNames(bones);
        settings.showBoneNames = bones;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Application Paths
    ImGui::Text("Storage Paths:");
    ImGui::TextDisabled("App Data: %s", AppPaths::appDataDir().string().c_str());
    ImGui::TextDisabled("Models Directory: %s", AppPaths::defaultModelsDir().string().c_str());
    ImGui::TextDisabled("Characters Directory: %s", AppPaths::defaultCharactersDir().string().c_str());
    ImGui::TextDisabled("Animations Directory: %s", AppPaths::defaultAnimationsDir().string().c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_CHECK " Save Settings", ImVec2(180, 36))) {
        SettingsManager::instance().save();
        toasts.push("Settings saved successfully", ToastKind::Success);
    }
}

} // namespace studio
