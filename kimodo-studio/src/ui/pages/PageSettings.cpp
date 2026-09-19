#include "ui/pages/PageSettings.h"
#include "app/SettingsManager.h"
#include "imgui.h"
#include "rendering/Viewport.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/Toast.h"
#include "utils/AppPaths.h"

namespace studio
{

void PageSettings::Draw(AppState& state, Viewport& viewport, Toasts& toasts)
{
    (void)state;
    ImGui::TextColored(UIStyle::accent, "%s Workstation Settings", icons::kSettings);
    ImGui::TextDisabled("Customize themes, viewport rendering defaults, and export paths");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    auto& settings = SettingsManager::GetInstance().GetSettings();

    // 1. Theme Selection
    ImGui::Text("Interface Theme:");
    if (ImGui::RadioButton("Dark (Default)", settings.theme == "Dark"))
    {
        settings.theme = "Dark";
        Theme::Apply();
        SettingsManager::GetInstance().save();
    }
    ImGui::SameLine(0, 16);
    if (ImGui::RadioButton("Cyber Neon", settings.theme == "Cyber"))
    {
        settings.theme = "Cyber";
        Theme::Apply();
        SettingsManager::GetInstance().save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Viewport Rendering Defaults
    ImGui::Text("Viewport Defaults:");
    bool grid = viewport.ShowGrid();
    if (ImGui::Checkbox("Show 3D Grid", &grid))
    {
        viewport.SetGrid(grid);
        settings.show_grid = grid;
    }
    bool axes = viewport.ShowAxes();
    if (ImGui::Checkbox("Show Coordinate Axes", &axes))
    {
        viewport.SetAxes(axes);
        settings.show_axes = axes;
    }
    bool floor = viewport.ShowFloor();
    if (ImGui::Checkbox("Show Floor Plane", &floor))
    {
        viewport.SetFloor(floor);
        settings.show_floor = floor;
    }
    bool wire = viewport.ShowWireframe();
    if (ImGui::Checkbox("Wireframe Shading", &wire))
    {
        viewport.SetWireframe(wire);
        settings.show_wireframe = wire;
    }
    bool bones = viewport.ShowBoneNames();
    if (ImGui::Checkbox("3D Bone Names", &bones))
    {
        viewport.SetBoneNames(bones);
        settings.show_bone_names = bones;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Application Paths
    ImGui::Text("Storage Paths:");
    ImGui::TextDisabled("App Data: %s", AppPaths::AppDataDir().string().c_str());
    ImGui::TextDisabled("Models Directory: %s", AppPaths::DefaultModelsDir().string().c_str());
    ImGui::TextDisabled("Characters Directory: %s", AppPaths::DefaultCharactersDir().string().c_str());
    ImGui::TextDisabled("Animations Directory: %s", AppPaths::DefaultAnimationsDir().string().c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button(ICON_FA_CHECK " Save Settings", ImVec2(180, 36)))
    {
        SettingsManager::GetInstance().save();
        toasts.Push("Settings saved successfully", ToastKind::Success);
    }
}

} // namespace studio
