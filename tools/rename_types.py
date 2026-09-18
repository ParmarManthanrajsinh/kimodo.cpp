"""Phase 3: rename project types to UE-style prefixes (E/F/I/S) in code regions only.
Skips string literals, char literals, and comments. Include paths untouched (filenames kept).
"""
import re
import pathlib

RENAMES = {
    # Interface first
    "AnimationExporter": "IAnimationExporter",
    # F data structs / aliases
    "AppState": "FAppState", "UserSettings": "FUserSettings", "Animation": "FAnimation",
    "LibraryEntry": "FLibraryEntry", "ModelEntry": "FModelEntry", "CharacterEntry": "FCharacterEntry",
    "CharacterBone": "FCharacterBone", "CharacterSubmesh": "FCharacterSubmesh",
    "CharacterValidationReport": "FCharacterValidationReport",
    "ExportOptions": "FExportOptions", "ExportPreset": "FExportPreset",
    "RetargetReport": "FRetargetReport", "Toast": "FToast",
    "GenerationParams": "FGenerationParams", "MotionResult": "FMotionResult",
    "SkeletonProfile": "FSkeletonProfile", "Soma30Spec": "FSoma30Spec",
    "SomaPresentationSpec": "FSomaPresentationSpec",
    "SkinVertex": "FSkinVertex", "SkinningData": "FSkinningData", "DebugPose": "FDebugPose",
    "UIStyle": "FUIStyle", "Theme": "FTheme",
    "BoneMap": "FBoneMap", "CharacterBoneMap": "FCharacterBoneMap",
    # F service classes
    "Application": "FApplication", "Viewport": "FViewport", "UIManager": "FUIManager",
    "AnimationPlayer": "FAnimationPlayer", "AnimationLibrary": "FAnimationLibrary",
    "CharacterLibrary": "FCharacterLibrary", "CharacterAsset": "FCharacterAsset",
    "ModelManager": "FModelManager", "KimodoEngine": "FKimodoEngine",
    "KimodoAdapter": "FKimodoAdapter", "SettingsManager": "FSettingsManager",
    "Logger": "FLogger", "AppPaths": "FAppPaths", "FileHash": "FFileHash",
    "FileDialog": "FFileDialog", "BVHParser": "FBVHParser", "CharacterLoader": "FCharacterLoader",
    "CharacterMapper": "FCharacterMapper", "Retargeter": "FRetargeter",
    "SomaPresentation": "FSomaPresentation", "Skeleton": "FSkeleton",
    "GridRenderer": "FGridRenderer", "SkinningRenderer": "FSkinningRenderer",
    "HuggingFaceClient": "FHuggingFaceClient", "HFAuthenticator": "FHFAuthenticator",
    "TestSuite": "FTestSuite", "BVHExporter": "FBVHExporter",
    "GLBExporter": "FGLBExporter", "CharacterGLBExporter": "FCharacterGLBExporter",
    "Toasts": "SToasts",
    # S UI widgets
    "NavRail": "SNavRail", "HeaderBar": "SHeaderBar", "StatusBar": "SStatusBar",
    "TimelineBar": "STimelineBar",
    "PageHome": "SPageHome", "PageGenerate": "SPageGenerate", "PageModels": "SPageModels",
    "PageLibrary": "SPageLibrary", "PageCharacters": "SPageCharacters",
    "PageRetarget": "SPageRetarget", "PageExport": "SPageExport", "PageSettings": "SPageSettings",
}

# Longest names first so e.g. "AnimationExporter" wins over "Animation".
ORDERED = sorted(RENAMES.items(), key=lambda kv: -len(kv[0]))
ALT = re.compile(r"(//.*|/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'|R\"[^\"]*\()",
                 re.VERBOSE)

def split_regions(text):
    """Yield (is_code, chunk) pairs; string/char/comment chunks are non-code."""
    out, pos = [], 0
    for m in ALT.finditer(text):
        if m.start() > pos:
            out.append((True, text[pos:m.start()]))
        out.append((False, m.group(0)))
        pos = m.end()
    if pos < len(text):
        out.append((True, text[pos:]))
    return out

def rename_code(code):
    for old, new in ORDERED:
        code = re.sub(r"\b" + old + r"\b", new, code)
    return code

def process(path):
    text = path.read_text(encoding="utf-8")
    result = "".join(rename_code(c) if is_code else c for is_code, c in split_regions(text))
    if result != text:
        path.write_text(result, encoding="utf-8", newline="\n")
        return True
    return False

if __name__ == "__main__":
    root = pathlib.Path(__file__).resolve().parent.parent / "kimodo-studio" / "src"
    changed = 0
    for p in sorted(root.rglob("*")):
        if p.suffix in (".h", ".cpp") and process(p):
            changed += 1
            print("updated", p.relative_to(root))
    print(f"--- {changed} files changed")
