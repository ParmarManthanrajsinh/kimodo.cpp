# KIMODO STUDIO — NVIDIA-GRADE UI/UX REDESIGN

You are working on Kimodo Studio, a native C++23 desktop application built with:
- Dear ImGui
- Raylib
- Kimodo.cpp inference backend

The current UI is functional but visually looks like a prototype.
Redesign the entire application UI so it feels like a polished,
professional NVIDIA-grade technical/creative application.

IMPORTANT:
- Do NOT rewrite the application architecture unnecessarily.
- Do NOT replace Dear ImGui.
- Do NOT replace Raylib.
- Do NOT change the inference/generation backend.
- Do NOT break existing functionality.
- Focus on UI/UX, layout, styling, interaction, viewport presentation,
  panels, navigation, typography, spacing, and visual hierarchy.
- Preserve existing application functionality.
- Build reusable UI components instead of duplicating styling code.

============================================================
1. DESIGN DIRECTION
============================================================

The visual language should be inspired by professional NVIDIA tooling,
Omniverse, RTX creative applications, Unreal Editor, Blender, and
modern engineering software.

The result should feel:
- Premium
- Technical
- Dense but readable
- Professional
- GPU-oriented
- Modern
- Minimal
- Precise
- Production-ready

Avoid:
- Generic default ImGui appearance
- Excessive rounded cards
- Excessive gradients
- Huge empty spaces
- Mobile-app style UI
- Gaming RGB aesthetics
- Excessive neon green
- Giant buttons
- Decorative UI that provides no information
- Excessive animations
- Cartoon-like visuals

Use NVIDIA green as an ACCENT, not as the primary background.

The application should immediately communicate:
"Professional GPU-powered motion generation and animation software."

============================================================
2. OVERALL APPLICATION STRUCTURE
============================================================

Replace the current simple layout with a professional editor-style
workspace.

Use this general structure:

+---------------------------------------------------------------+
| TOP APPLICATION BAR                                           |
+----------+---------------------------------------+------------+
|          |                                       |            |
| NAV      |                                       | INSPECTOR  |
| SIDEBAR  |            MAIN VIEWPORT               | / CONTEXT  |
|          |                                       | PANEL      |
|          |                                       |            |
|          |                                       |            |
+----------+---------------------------------------+------------+
| TIMELINE / TRANSPORT / STATUS BAR                             |
+---------------------------------------------------------------+

The UI should feel like an actual animation workstation.

Major areas:

1. Top bar
2. Left navigation
3. Main viewport
4. Right inspector/context panel
5. Bottom timeline
6. Status bar

Panels should have clear boundaries without looking like many
independent floating windows.

============================================================
3. TOP APPLICATION BAR
============================================================

Create a professional top application bar.

Left:
- Kimodo Studio logo/wordmark
- Current workspace/project name
- Optional project state indicator

Center:
- Workspace tabs if appropriate:
  Generate
  Library
  Retarget
  Export

Right:
- GPU status
- VRAM indicator
- Current model
- Settings button
- Window controls if controlled by the application

GPU status should look like:

GPU ● RTX 4060
VRAM 5.2 / 8 GB

Use a small green indicator when the GPU is available.

Do not make GPU information visually dominant.

============================================================
4. LEFT NAVIGATION
============================================================

Replace the current primitive vertical buttons with a professional
compact navigation rail/sidebar.

Sections:

WORKSPACE
- Home
- Generate
- Library
- Retarget
- Export

ASSETS
- Models
- Animations
- Presets

SYSTEM
- Settings

Each navigation item should have:
- Icon
- Label
- Active state
- Hover state
- Keyboard shortcut where useful

Use subtle active highlighting.

Example:

[icon] Generate
[icon] Library
[icon] Retarget
[icon] Export

The selected item should have:
- subtle dark elevated background
- thin NVIDIA-green accent indicator on the left
- brighter text
- subtle icon emphasis

Do NOT use large blue Windows/ImGui-looking buttons.

============================================================
5. ICON SYSTEM
============================================================

Do not use text characters as fake icons.

Create a consistent icon system.

Use either:
- existing icon font already available in the project
- a lightweight icon font
- simple custom vector icons using ImGui drawing

Icons should be:
- monochrome
- thin
- consistent
- approximately 16px

Suggested icons:
Home
Play
Spark/Generate
Cube/Model
Film/Animation
Retarget
Export
Settings
Folder
Search
Filter
GPU
Warning
Success
Pause
Stop
Previous
Next
Loop
Camera
Timeline
Download
Upload

Do not introduce a large external UI framework.

============================================================
6. COLOR SYSTEM
============================================================

Create a centralized Kimodo Studio UI theme.

Define colors in one place.

Base:

Application background:
very dark neutral charcoal

Panel background:
slightly lighter charcoal

Secondary panel:
dark graphite

Viewport:
near-black blue/gray

Borders:
subtle gray

Primary text:
light gray / near white

Secondary text:
muted gray

Disabled:
dark gray

Accent:
NVIDIA green

Warning:
amber

Error:
red

Success:
green

IMPORTANT:
Do not make every element green.

Use green for:
- active states
- primary actions
- GPU status
- progress
- selected controls
- important confirmations

Use neutral colors for most controls.

============================================================
7. TYPOGRAPHY
============================================================

Improve typography significantly.

Use a clean modern sans-serif font if the project already supports
custom font loading.

Recommended:
- Inter
- Segoe UI
- Noto Sans
- system sans-serif fallback

Create font hierarchy:

Application title:
16–18px

Section heading:
13–15px

Normal UI:
12–13px

Metadata:
10–11px

Timeline:
11–12px

Avoid oversized typography.

Use font weight hierarchy through:
- brighter text
- spacing
- size
rather than excessive bold text.

============================================================
8. SPACING SYSTEM
============================================================

Create a consistent spacing system.

Use approximately:

4px  = micro spacing
8px  = normal spacing
12px = control spacing
16px = panel spacing
24px = section spacing

Avoid random padding values throughout the code.

Create helper functions for:
- section spacing
- panel padding
- control spacing
- separators

============================================================
9. BUTTON DESIGN
============================================================

Replace default ImGui blue buttons.

Create custom button styles:

Primary:
dark graphite background
NVIDIA green hover/active accent

Secondary:
dark neutral background
subtle border

Danger:
dark neutral with red hover

Tool button:
square compact icon-only button

Buttons should be compact and professional.

Example:

[  Generate Motion  ]

not:

[         GENERATE         ]

Primary actions should visually stand out.

============================================================
10. INPUT CONTROLS
============================================================

Restyle:
- ComboBox
- Slider
- InputText
- Checkbox
- RadioButton
- DragFloat
- Tabs

Use:
- dark graphite backgrounds
- subtle borders
- small corner radius
- clean hover states
- NVIDIA-green active states

Sliders should look like professional editor controls.

Do not use giant rounded sliders.

============================================================
11. MAIN VIEWPORT
============================================================

The viewport is the most important part of the application.

Make it feel like a professional 3D editor.

The current viewport contains too much empty space.

Add a subtle viewport overlay.

Top-left:

VIEWPORT

Animation name
Frame 042 / 120
FPS 30

Top-right:

Perspective
[Camera]
[Grid]
[Axes]

Bottom-left:

XYZ axis gizmo

Bottom-right:

small viewport statistics

Example:

Vertices    0
Joints      30
FPS         60

Keep overlays subtle.

============================================================
12. VIEWPORT BACKGROUND
============================================================

Improve the Raylib viewport presentation.

Use:
- dark neutral background
- subtle grid
- axis lines
- soft visual contrast

Grid should not dominate the viewport.

Use different visual strength for:
- major grid lines
- minor grid lines
- axes

Add optional:
- grid toggle
- axes toggle
- floor toggle
- camera reset

Do not make the viewport look like an empty debugging scene.

============================================================
13. CHARACTER / SKELETON PRESENTATION
============================================================

Improve skeleton rendering.

Skeleton should be visually readable.

Use:
- joints represented by small spheres/circles
- bones represented by clean lines/cylinders
- selected joint highlighting
- subtle motion visualization

Selected joints can use NVIDIA green.

Normal joints should remain neutral.

Avoid overly bright skeleton rendering.

============================================================
14. VIEWPORT TOOLBAR
============================================================

Add a compact floating toolbar near the top of the viewport.

Example:

[Select] [Move] [Rotate] [Scale] | [Grid] [Axes] | [Camera]

Use small icon buttons.

Show tooltip on hover.

Do not use large buttons.

============================================================
15. LIBRARY PAGE
============================================================

Redesign the Library page completely.

Instead of the current right-side list, make the Library a proper
asset browser.

Layout:

---------------------------------------------------------
| Animations                              Search [____] |
---------------------------------------------------------
| Filters | Asset cards/grid                           |
---------------------------------------------------------

Each animation should appear as a professional asset card.

Card should contain:
- thumbnail
- animation name
- duration
- frame count
- FPS
- creation date
- tags
- status

Example:

+--------------------------+
|                          |
|       THUMBNAIL          |
|                          |
+--------------------------+
| A person eating an apple |
| 4.0 sec • 120 frames     |
| 30 FPS                   |
|                          |
| [Open] [More]            |
+--------------------------+

Use 2–4 columns depending on window size.

Provide:
- Grid view
- List view
- Search
- Sort
- Filter

============================================================
16. GENERATE PAGE
============================================================

Make Generate the primary workflow.

Structure:

+------------------------------------------------------------+
| Generate Motion                                            |
|                                                            |
| Prompt                                                     |
| [                                                    ]     |
| [                                                    ]     |
|                                                            |
| Model        [Model Name ▼]                               |
| Duration     [4.0 sec]                                    |
| FPS          [30]                                         |
| Seed         [Auto]                                       |
|                                                            |
|                  [ Generate Motion ]                       |
+------------------------------------------------------------+

Show advanced settings in a collapsible section.

Advanced:
- Seed
- Guidance
- Steps
- Motion parameters
- Device
- Precision
- Output format

Do not expose advanced settings by default.

============================================================
17. GENERATION PROGRESS
============================================================

During generation, provide a professional progress experience.

Example:

Generating Motion

████████████████░░░░ 78%

Step 18 / 24

GPU: RTX 4060
VRAM: 5.4 / 8 GB

Estimated remaining: 2.3 sec

Do not freeze the UI.

Allow:
[Cancel]

============================================================
18. MODEL MANAGER
============================================================

Redesign Models page as a professional model manager.

Show:

MODEL LIBRARY

Search
Filter
Sort

Each model:

Model Name
Version
Format
Size
License
Installed status

Example:

Kimodo Motion v1
GGUF
4.2 GB
License: Apache-2.0
Installed

[Use] [Update] [Details]

License information must remain visible.

Do not label models "royalty-free" unless the actual license
explicitly supports that terminology.

============================================================
19. RETARGET PAGE
============================================================

Keep retargeting focused on GENERIC HUMANOID / BLENDER workflow.

Do NOT build a custom UE5 Manny retargeting interface.

Do NOT expose fake Unreal skeleton mapping.

The page should communicate:

RETARGET MOTION

Source:
[Kimodo Humanoid ▼]

Target:
[Blender Generic ▼]

Mapping:
[Auto Map]

Preview:
[Preview]

Apply:
[Apply]

Show mapping health:

24 / 24 joints mapped
No missing required joints

Use green status for successful mapping.

============================================================
20. UNREAL WORKFLOW
============================================================

Unreal should be represented as an EXPORT WORKFLOW.

Create an Unreal Engine workflow panel:

UNREAL ENGINE

Kimodo Studio exports a generic humanoid BVH.
Use Unreal's IK Rig and IK Retargeter to transfer the motion
to Manny or another target skeleton.

[ Export BVH ]

Then show a compact workflow:

1. Generate motion
2. Export BVH
3. Import BVH into Unreal
4. Create/use Source IK Rig
5. Retarget using IK Retargeter
6. Apply to Manny

Do not implement custom Manny retargeting.

Do not make the application pretend it is performing Unreal's
IK Retargeter internally.

============================================================
21. EXPORT PAGE
============================================================

Create a dedicated Export page.

Header:

EXPORT ANIMATION

Format:

[ BVH ▼ ]

Preset:

[ Generic Humanoid ▼ ]

Settings:

FPS              [30]
Scale            [1.0]
Root Motion      [✓]
Rotation Order   [XYZ ▼]

Output:

[________________________]
[ Browse ]

Preview:

Frames: 120
Duration: 4.0 sec
Joints: 30
FPS: 30

[ Export Animation ]

BVH should be the main Unreal-oriented export format.

============================================================
22. TIMELINE
============================================================

Add a professional animation timeline at the bottom.

Layout:

+------------------------------------------------------------+
| ◀ | ▶ | ▶ | LOOP |  Frame 42 / 120        00:01.40       |
+------------------------------------------------------------+
|                                                            |
| 0    10    20    30    40    50    60    70              |
| -------------------|--------------------------------------|
|                    ▲                                       |
+------------------------------------------------------------+

Features:
- Play
- Pause
- Stop
- Previous frame
- Next frame
- Loop
- Current frame
- Time
- FPS
- Timeline scrubber

Make the timeline compact when unused.

Allow expanding/collapsing it.

============================================================
23. TRANSPORT CONTROLS
============================================================

Use professional compact transport controls.

[|<] [<] [Play] [>] [>|] [Loop]

Current:

Frame 042 / 120

Time:

00:01.40 / 00:04.00

Playback speed:

1.0x

============================================================
24. RIGHT INSPECTOR PANEL
============================================================

Create a context-sensitive inspector.

When animation selected:

ANIMATION

Name
Duration
Frames
FPS
Source
Model
Created

When joint selected:

JOINT

Name
Parent
Position
Rotation
Length

When export selected:

EXPORT

Format
Preset
Scale
Root Motion

The right panel should change depending on context.

============================================================
25. STATUS BAR
============================================================

Create a professional bottom status bar.

Left:

Ready

Center:

GPU: RTX 4060
Backend: Vulkan
FPS: 60

Right:

Kimodo Studio 0.1.0

Avoid making the status bar visually heavy.

============================================================
26. SETTINGS
============================================================

Organize settings into categories:

GENERAL
- Theme
- UI scale
- Autosave
- Startup behavior

GPU
- Device
- Backend
- VRAM usage
- Performance

ANIMATION
- Default FPS
- Default duration
- Playback settings

EXPORT
- Default format
- Default directory
- BVH settings

APPEARANCE
- UI scale
- Viewport grid
- Viewport axes
- Timeline

ABOUT
- Version
- Build
- License
- Credits

============================================================
27. RESPONSIVE LAYOUT
============================================================

The application must work at:

1280x720
1920x1080
2560x1440
3840x2160

Do not hardcode panel widths that break on smaller displays.

Use proportional/responsive sizing where appropriate.

At small resolutions:
- collapse navigation labels
- reduce inspector width
- reduce timeline height
- preserve viewport

============================================================
28. IMGUI IMPLEMENTATION
============================================================

Create reusable UI helpers.

Examples:

DrawPanel()
DrawSectionHeader()
DrawToolbar()
DrawIconButton()
DrawPrimaryButton()
DrawSecondaryButton()
DrawStatusBadge()
DrawPropertyRow()
DrawSearchBar()
DrawAssetCard()
DrawTimeline()
DrawInspector()
DrawViewportOverlay()

Centralize theme configuration:

ApplyKimodoTheme()

Centralize dimensions:

UIStyle
{
    panelPadding
    sectionSpacing
    controlHeight
    toolbarHeight
    sidebarWidth
    inspectorWidth
    timelineHeight
    borderRadius
}

Do not scatter magic numbers throughout the code.

============================================================
29. IMGUI STYLE
============================================================

Configure:

ImGuiStyle:
- WindowRounding: small
- ChildRounding: small
- FrameRounding: small
- PopupRounding: small
- ScrollbarRounding: small

Avoid completely square old-school ImGui styling.

But also avoid excessive rounded UI.

Use subtle 3–5px rounding.

Configure:
- WindowPadding
- FramePadding
- ItemSpacing
- ItemInnerSpacing
- IndentSpacing

for a professional dense editor feel.

============================================================
30. PANELS
============================================================

Use subtle borders.

Avoid:
thick outlines
bright borders
heavy shadows
huge cards

Panels should be separated mainly by:
- background value
- thin border
- spacing

============================================================
31. HOVER EFFECTS
============================================================

Use subtle interaction feedback.

Hover:
slightly brighter background

Active:
slightly brighter + accent

Selected:
accent indicator + brighter text

Disabled:
muted

Do not animate every UI element.

============================================================
32. MICRO ANIMATIONS
============================================================

Use animation sparingly.

Allowed:
- subtle progress transitions
- fade between workspace states
- button state transition
- generation progress
- panel expansion

Avoid:
- bouncing
- scaling buttons
- excessive glowing
- flashy transitions

The application should feel like professional engineering software.

============================================================
33. TOOLTIPS
============================================================

Every icon-only button must have a tooltip.

Examples:

Reset Camera (R)
Play Animation (Space)
Export BVH
Toggle Grid
Toggle Axes
Settings

Tooltips should appear after a short hover delay.

============================================================
34. KEYBOARD SHORTCUTS
============================================================

Implement or preserve useful shortcuts.

R:
Reset camera

Space:
Play/Pause

F:
Frame selected

Left/Right:
Previous/Next frame

Home:
First frame

End:
Last frame

Ctrl+S:
Save

Ctrl+E:
Export

Do not conflict with existing application shortcuts.

============================================================
35. EMPTY STATES
============================================================

Never show an empty panel with nothing inside.

Create professional empty states.

Example:

NO ANIMATION SELECTED

Select an animation from the Library
to begin previewing.

[Open Library]

For no models:

NO MODELS INSTALLED

Install a compatible Kimodo model
to generate motion.

============================================================
36. ERROR STATES
============================================================

Errors should be clear and professional.

Example:

Generation Failed

Unable to initialize Vulkan device.

Reason:
Insufficient GPU memory.

[Retry] [Open Settings]

Do not dump raw technical errors into the primary UI.

Provide expandable technical details.

============================================================
37. SUCCESS STATES
============================================================

Use subtle success indicators.

Example:

✓ Animation generated successfully

120 frames • 4.0 sec

[Open in Library]

Do not use giant notification popups.

============================================================
38. CONTEXT MENUS
============================================================

Use professional right-click menus for assets.

Animation:

Open
Rename
Duplicate
Retarget
Export BVH
Delete

Model:

Use
Details
Open Folder
Remove

============================================================
39. SEARCH
============================================================

Create a consistent search component.

Example:

🔍 Search animations...

Use:
- keyboard focus
- clear button
- filtering
- live results

Search fields should not look like default ImGui InputText.

============================================================
40. LIBRARY THUMBNAILS
============================================================

Generate useful animation thumbnails from the existing Raylib
viewport renderer if practical.

Thumbnail should show:
- skeleton
- floor/grid
- animation pose

Do not introduce an expensive rendering pipeline solely for thumbnails.

Cache thumbnails where possible.

============================================================
41. PERFORMANCE
============================================================

UI redesign must NOT significantly reduce performance.

Important:
- avoid allocations every frame
- cache fonts
- cache icons
- cache generated thumbnails
- avoid rebuilding large UI structures unnecessarily
- avoid expensive rendering in ImGui
- avoid unnecessary Raylib render targets
- avoid excessive draw calls

Target:
60 FPS UI at 1080p and 1440p.

============================================================
42. ACCESSIBILITY / READABILITY
============================================================

Ensure:
- sufficient text contrast
- readable font sizes
- clear selected states
- tooltips for ambiguous controls
- keyboard navigation where practical

Do not rely solely on color to communicate state.

============================================================
43. NVIDIA VISUAL IDENTITY
============================================================

The application may be visually inspired by NVIDIA's professional
software ecosystem.

However:
- do not copy NVIDIA logos
- do not falsely claim the application is made by NVIDIA
- do not use NVIDIA trademarks as the application logo
- do not reproduce proprietary UI assets

Use the design language:
dark graphite + restrained green accent + technical density.

Kimodo Studio must remain its own product identity.

============================================================
44. REMOVE PROTOTYPE FEEL
============================================================

Specifically fix the problems visible in the current UI:

CURRENT PROBLEM:
Large empty viewport.

FIX:
Use a structured editor workspace and useful viewport overlays.

CURRENT PROBLEM:
Primitive left buttons.

FIX:
Professional navigation rail with icons and active indicators.

CURRENT PROBLEM:
Library is a tiny right-side list.

FIX:
Dedicated asset browser with cards/list view.

CURRENT PROBLEM:
Default blue ImGui controls.

FIX:
Custom dark graphite controls with restrained green accent.

CURRENT PROBLEM:
No visual hierarchy.

FIX:
Use section headers, spacing, typography, panel hierarchy.

CURRENT PROBLEM:
Timeline missing.

FIX:
Add professional bottom animation timeline.

CURRENT PROBLEM:
GPU status is tiny and disconnected.

FIX:
Integrate GPU status into the top/status bar.

CURRENT PROBLEM:
UI looks like a debug application.

FIX:
Create consistent spacing, typography, controls, icons,
panel structure, and interaction states.

============================================================
45. DO NOT OVERENGINEER
============================================================

Do not rewrite every subsystem.

Prefer incremental UI refactoring.

First create:
1. Theme system
2. Layout system
3. Navigation
4. Viewport chrome
5. Timeline
6. Inspector
7. Library
8. Generate UI
9. Export UI
10. Settings UI

Then polish.

============================================================
46. CODE QUALITY
============================================================

Keep UI code modular.

Prefer files such as:

ui/
    Theme.h
    Theme.cpp
    Layout.h
    Layout.cpp
    Components.h
    Components.cpp
    Navigation.h
    Navigation.cpp
    ViewportUI.h
    ViewportUI.cpp
    TimelineUI.h
    TimelineUI.cpp
    LibraryUI.h
    LibraryUI.cpp
    InspectorUI.h
    InspectorUI.cpp

Adapt to the existing project architecture instead of blindly creating
duplicate systems.

Use existing project conventions where appropriate.

============================================================
47. IMPORTANT FUNCTIONALITY RULE
============================================================

The UI redesign must not break:

- animation generation
- model management
- animation library
- animation playback
- retargeting
- Blender workflow
- BVH export
- GLB export if currently supported
- settings
- Vulkan/GPU status
- packaging

Do not remove working functionality merely to simplify the UI.

============================================================
48. FINAL QUALITY BAR
============================================================

After implementation, run the application and inspect it at:

1280x720
1920x1080
2560x1440

Check:

- no overlapping panels
- no clipped text
- no broken controls
- no layout jumps
- no excessive empty space
- no accidental scrollbars
- no unreadable text
- no default blue ImGui controls
- no inconsistent spacing
- no excessive green
- no giant buttons
- no visual clutter

The application should look like a professional desktop DCC /
AI animation tool rather than a student project.

============================================================
49. FINAL UX TARGET
============================================================

The final experience should communicate:

"Kimodo Studio is a serious AI-powered motion-generation workstation."

The visual hierarchy should be:

1. Animation / viewport
2. Primary workflow
3. Timeline
4. Context/inspector
5. Navigation
6. System information

The UI should be dark, precise, dense, elegant, and technically
sophisticated.

Do not simply make the current UI darker.

Actually redesign the information architecture and workspace.

============================================================
50. ACCEPTANCE CRITERIA
============================================================

The redesign is complete only when:

[ ] Professional dark editor workspace
[ ] NVIDIA-inspired visual language
[ ] Consistent custom ImGui theme
[ ] Professional navigation
[ ] Professional top bar
[ ] GPU status integrated
[ ] Improved Raylib viewport
[ ] Viewport toolbar
[ ] Viewport overlays
[ ] Animation timeline
[ ] Professional transport controls
[ ] Context-sensitive inspector
[ ] Proper animation library
[ ] Search/filter/sort
[ ] Professional Generate page
[ ] Professional Model Manager
[ ] Blender generic retarget workflow
[ ] Unreal workflow presented as BVH export
[ ] Dedicated Export page
[ ] Professional Settings page
[ ] Responsive layout
[ ] Tooltips
[ ] Keyboard shortcuts
[ ] Empty states
[ ] Error states
[ ] Success states
[ ] No default ImGui blue styling
[ ] No excessive NVIDIA green
[ ] No Unity-specific UI or workflow
[ ] No custom UE Manny retargeter UI
[ ] Existing functionality preserved
[ ] No significant performance regression

Finally, build the project, launch Kimodo Studio, and visually inspect
the result.

If something still looks like default ImGui, refactor it.

The goal is not "technically functional UI".

The goal is:

POLISHED.
PROFESSIONAL.
NVIDIA-GRADE.
PRODUCTION-READY.
