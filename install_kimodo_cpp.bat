@echo off
:: ============================================================
::  kimodo.cpp - Windows one-click installer (PixelArtistry)
::  Single file: double-click it. Installs Git/Python/VS Build
::  Tools/Vulkan SDK/Go if missing, builds kimodo.cpp, downloads
::  the models and creates demo.bat + helper scripts in .\Kimodo
::
::  Optional args (run from a terminal):
::    install_kimodo.bat -InstallDir E:\Kimodo
::    install_kimodo.bat -NoVulkan  |  -SkipWeights  |  -NoDemo
:: ============================================================
setlocal
cd /d "%~dp0"
set "PS1=%TEMP%\install_kimodo.ps1"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=[IO.File]::ReadAllText('%~f0'); $m='::PS1' + 'START'; $i=$t.IndexOf($m); $s=$t.Substring($t.IndexOf([char]10,$i)+1); [IO.File]::WriteAllText('%PS1%', $s, (New-Object Text.UTF8Encoding $false))"
if not exist "%PS1%" ( echo Failed to extract installer script & pause & exit /b 1 )
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%" %*
set EXITCODE=%ERRORLEVEL%
del "%PS1%" >nul 2>&1
echo.
if %EXITCODE% neq 0 ( echo Installer finished with errors ^(exit code %EXITCODE%^). ) else ( echo Installer finished successfully. )
pause
exit /b %EXITCODE%

:: ---- PowerShell payload below - do not edit the marker line ----
::PS1START
# ============================================================
#  kimodo.cpp – Windows one-click installer (PixelArtistry)
#
#  Installs missing prerequisites via winget (Git, Python,
#  VS 2022 Build Tools, Vulkan SDK), clones + builds kimodo.cpp,
#  handles the Hugging Face login/licence for the gated weights,
#  downloads the GGUF models and writes a generate.ps1 helper.
#
#  Run via install_kimodo.bat, or:
#    powershell -ExecutionPolicy Bypass -File install_kimodo.ps1
#
#  Options:
#    -InstallDir E:\Kimodo   target folder (default: .\Kimodo)
#    -Model soma-rp-v1.1     soma-rp-v1.1 | soma-seed-v1.1 | g1-rp-v1 | g1-seed-v1
#    -NoVulkan               CPU-only build, skip Vulkan SDK
#    -SkipWeights            build only
#    -NoAutoInstall          never call winget, just report what's missing
#    -NoDemo                 skip Go install / browser demo (demo.ps1)
# ============================================================
param(
    [string]$InstallDir = (Join-Path (Get-Location) "Kimodo"),
    [string]$Model      = "soma-rp-v1.1",
    [switch]$NoVulkan,
    [switch]$SkipWeights,
    [switch]$NoAutoInstall,
    [switch]$NoDemo          # skip installing Go + the browser demo
)

$ErrorActionPreference = "Stop"
function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Warn($msg) { Write-Host "  ! $msg" -ForegroundColor Yellow }
function Fail($msg) { Write-Host "`nERROR: $msg" -ForegroundColor Red; exit 1 }

function Refresh-Path {
    $env:PATH = [Environment]::GetEnvironmentVariable("PATH","Machine") + ";" +
                [Environment]::GetEnvironmentVariable("PATH","User")
    $env:VULKAN_SDK = [Environment]::GetEnvironmentVariable("VULKAN_SDK","Machine")
}

$script:winget = $null
function Find-Winget {
    $w = (Get-Command winget -ErrorAction SilentlyContinue).Source
    if (-not $w) {
        $cand = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps\winget.exe"
        if (Test-Path $cand) { $w = $cand }
    }
    return $w
}

function Ensure-Package($displayName, $wingetId, $testCmd, $override) {
    if (& $testCmd) { Write-Host "  ${displayName}: OK"; return $true }
    if ($NoAutoInstall -or -not $script:winget) {
        Warn "$displayName missing (winget not available for auto-install). Install manually: winget install -e --id $wingetId"
        return $false
    }
    Write-Host "  $displayName missing -> installing via winget (a UAC prompt may appear)" -ForegroundColor Yellow
    $wgArgs = @("install","-e","--id",$wingetId,"--accept-source-agreements","--accept-package-agreements","--silent")
    if ($override) { $wgArgs += @("--override", $override) }
    & $script:winget @wgArgs
    Refresh-Path
    if (& $testCmd) { Write-Host "  ${displayName}: installed"; return $true }
    Warn "$displayName still not detected. If it was just installed, close this window and rerun the installer."
    return $false
}

function Find-CMake {
    $c = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if ($c) { return $c }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            $cand = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $cand) { return $cand }
        }
    }
    return $null
}

# ---------- 1. Prerequisites ----------
Step "Checking / installing prerequisites"
Refresh-Path
$script:winget = Find-Winget
if ($script:winget) { Write-Host "  winget: $script:winget" } else { Warn "winget not found - missing tools must be installed manually" }

$okGit = Ensure-Package "Git" "Git.Git" { Get-Command git -ErrorAction SilentlyContinue }

$okPy = Ensure-Package "Python 3" "Python.Python.3.12" {
    $p = Get-Command python -ErrorAction SilentlyContinue
    if (-not $p) { return $false }
    $v = & python -c "import sys;print(f'{sys.version_info[0]}.{sys.version_info[1]}')" 2>$null
    return ($v -and [version]$v -ge [version]"3.10")
}

# VS 2022 Build Tools with C++ workload (includes CMake). ~3-5 GB, takes a while.
$okVS = Ensure-Package "Visual Studio 2022 C++ tools" "Microsoft.VisualStudio.2022.BuildTools" {
    [bool](Find-CMake)
} "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"

$useVulkan = $false
if (-not $NoVulkan) {
    $okVk = Ensure-Package "Vulkan SDK" "KhronosGroup.VulkanSDK" {
        $env:VULKAN_SDK -and (Test-Path (Join-Path $env:VULKAN_SDK "Bin\glslc.exe"))
    }
    if ($okVk) { $useVulkan = $true } else { Warn "Building CPU-only. Install the Vulkan SDK and rerun for GPU inference." }
}

$okGo = $false
if (-not $NoDemo) {
    $okGo = Ensure-Package "Go (for browser demo)" "GoLang.Go" { [bool](Get-Command go -ErrorAction SilentlyContinue) }
    if (-not $okGo) { Warn "Browser demo skipped; CLI (generate.ps1) still works." }
}

if (-not ($okGit -and $okPy -and $okVS)) { Fail "Required tools missing (see above). Fix and rerun." }
$cmake = Find-CMake
Write-Host "  cmake: $cmake"

# ---------- 2. Clone ----------
Step "Cloning kimodo.cpp into $InstallDir"
if (Test-Path (Join-Path $InstallDir ".git")) {
    Write-Host "  Repo exists, pulling latest"
    git -C $InstallDir pull --ff-only
} else {
    git clone https://github.com/localai-org/kimodo.cpp.git $InstallDir
}
Set-Location $InstallDir
git submodule update --init --recursive

# ---------- 3. Build ----------
Step "Configuring (VS 17 2022, x64, Release, Vulkan=$useVulkan)"
$vkFlag = if ($useVulkan) { "ON" } else { "OFF" }
# Options go through a CMake initial-cache file: immune to PowerShell argument mangling.
$cacheFile = Join-Path $InstallDir "kimodo_options.cmake"
@"
set(KIMODO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(KIMODO_ENABLE_VULKAN $vkFlag CACHE BOOL "" FORCE)
set(GGML_VULKAN $vkFlag CACHE BOOL "" FORCE)
set(GGML_CCACHE OFF CACHE BOOL "" FORCE)
"@ | Set-Content -Path $cacheFile -Encoding ASCII
if (Test-Path "build\CMakeCache.txt") { Remove-Item "build\CMakeCache.txt" }   # stale options from a failed run
& $cmake -C $cacheFile -B build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed" }

Step "Building (a few minutes)"
& $cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { Fail "Build failed" }
$exe = Join-Path $InstallDir "build\Release\kmd-generate.exe"
if (-not (Test-Path $exe)) { Fail "kmd-generate.exe not produced" }
Write-Host "  Built: $exe"

# ---------- 4. Weights (with Hugging Face login handling) ----------
if (-not $SkipWeights) {
    Step "Checking huggingface_hub"
    & python -c "import huggingface_hub" 2>$null
    if ($LASTEXITCODE -ne 0) {
        # Install only if missing - never --upgrade, that breaks transformers/tokenizers pins in a shared Python
        python -m pip install --quiet --disable-pip-version-check "huggingface_hub<1.0"
    } else { Write-Host "  huggingface_hub: OK (existing version kept)" }

    $textRepoUrl = "https://huggingface.co/LocalAI-io/Llama-3-Kimodo-GGML"

    function Test-HFLogin {
        & python -c "from huggingface_hub import whoami; whoami()" 2>$null
        return ($LASTEXITCODE -eq 0)
    }
    function Do-HFLogin {
        Write-Host ""
        Write-Host "  The Llama-3 text encoder is gated on Hugging Face." -ForegroundColor Yellow
        Write-Host "  1) A browser tab opens - click 'Agree and access repository' (free HF account needed)."
        Write-Host "  2) Create a READ token at https://huggingface.co/settings/tokens and paste it below."
        Start-Process $textRepoUrl
        $tok = Read-Host "  Paste your Hugging Face token (input hidden)" -AsSecureString
        $plain = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($tok))
        $env:HF_TOKEN_TMP = $plain
        & python -c "import os; from huggingface_hub import login; login(token=os.environ['HF_TOKEN_TMP'], add_to_git_credential=False)"
        Remove-Item Env:HF_TOKEN_TMP
        if (-not (Test-HFLogin)) { Fail "Hugging Face login failed (bad token?)" }
        Write-Host "  Logged in to Hugging Face"
    }

    if (-not (Test-HFLogin)) { Do-HFLogin } else { Write-Host "  Hugging Face: already logged in" }

    Step "Downloading weights ($Model + Llama-3 text encoder, ~8 GB)"
    python scripts/download_gguf_weights.py --model $Model --output .
    if ($LASTEXITCODE -ne 0) {
        Warn "Download failed - usually the licence isn't accepted yet or the token lacks read access."
        Do-HFLogin
        python scripts/download_gguf_weights.py --model $Model --output .
        if ($LASTEXITCODE -ne 0) { Fail "Weight download failed twice. Accept the licence at $textRepoUrl and rerun with the same InstallDir (build is kept)." }
    }
}

# ---------- 5. Helper script ----------
Step "Writing generate.ps1 helper"
$helper = @'
# generate.ps1 – text prompt -> Unreal-ready .glb (+ .bvh for Blender)
# Usage: .\generate.ps1 "a person walking forward and waving" [-Frames 120] [-Steps 50] [-Seed 42] [-Out my_anim]
param(
    [Parameter(Mandatory=$true, Position=0)][string]$Prompt,
    [int]$Frames = 120,     # 30 fps -> 120 = 4 s
    [int]$Steps  = 50,
    [int]$Seed   = 42,
    [string]$Out = "output_motion",
    [string]$Model = "__MODEL__"
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$env:PATH = "$root\build\bin\Release;$root\build\Release;" + $env:PATH
$gguf = Get-ChildItem "$root\models\kimodo-$Model-*.gguf" | Select-Object -First 1
if (-not $gguf) { throw "No GGUF for model '$Model' in $root\models" }
New-Item -ItemType Directory -Force $Out | Out-Null
$promptFile = Join-Path $Out "prompt.txt"
[IO.File]::WriteAllText($promptFile, $Prompt, (New-Object Text.UTF8Encoding $false))
& "$root\build\Release\kmd-generate.exe" $gguf.FullName "$root\generated\llm2vec-text-bundle" $promptFile $Frames $Steps $Seed "$Out\"
if ($LASTEXITCODE -ne 0) { throw "kmd-generate failed" }
python "$root\scripts\export_glb.py" --motion-dir $Out --output "$Out\animation.glb"
python "$root\scripts\export_bvh.py" --motion-dir $Out --output "$Out\animation.bvh"
Write-Host "`nDone -> $Out\animation.glb (Unreal)  |  $Out\animation.bvh (Blender)" -ForegroundColor Green
'@
$helper.Replace("__MODEL__", $Model) | Set-Content -Path (Join-Path $InstallDir "generate.ps1") -Encoding UTF8

if ($okGo) {
    Step "Writing demo.ps1 + demo.bat (browser UI)"
    @'
@echo off
:: Double-click to start the kimodo.cpp browser demo (needs demo.ps1 in the same folder)
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo.ps1" %*
pause
'@ | Set-Content -Path (Join-Path $InstallDir "demo.bat") -Encoding ASCII
    @'
# demo.ps1 – local text-to-motion web UI  (place in the Kimodo folder)
param([int]$Port = 8094)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root
$env:PATH = "$root\build\bin\Release;$root\build\Release;" + $env:PATH
if (-not (Get-Command go -ErrorAction SilentlyContinue)) { throw "Go not found. Install with: winget install -e --id GoLang.Go, then open a new terminal." }
if (-not (Test-Path "build\Release\kmd-generate.exe")) { throw "kmd-generate.exe missing - run the installer first." }

# Build once, reuse (go run would recompile on every start)
$exe = "build\Release\kimodo-demo.exe"
if (-not (Test-Path $exe) -or (Get-Item demo\main.go).LastWriteTime -gt (Get-Item $exe).LastWriteTime) {
    Write-Host "Compiling demo..." -ForegroundColor Cyan
    go build -o $exe ./demo
    if ($LASTEXITCODE -ne 0) { throw "go build failed" }
}

$url = "http://localhost:$Port"
Write-Host "Starting demo at $url  (Ctrl+C or close this window to stop)" -ForegroundColor Green
$proc = Start-Process -FilePath $exe -ArgumentList "-addr","127.0.0.1:$Port","-generator","build\Release\kmd-generate.exe" -NoNewWindow -PassThru

# Open browser only once the port answers
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 500
    if ($proc.HasExited) { throw "Demo server exited early (exit code $($proc.ExitCode)) - see output above." }
    try { $null = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 1; break } catch {}
}
Start-Process $url

# Auto-export: whenever a new generation appears, write Unreal-ready ue_animation.glb + .bvh next to it
$outDir = Join-Path $root "demo-output"
Write-Host "Watching $outDir - each generation gets ue_animation.glb / .bvh automatically" -ForegroundColor Cyan
while (-not $proc.HasExited) {
    Start-Sleep -Seconds 3
    if (-not (Test-Path $outDir)) { continue }
    foreach ($d in Get-ChildItem $outDir -Directory) {
        $raw = Join-Path $d.FullName "local_rotations_xyzw.f32"
        $glb = Join-Path $d.FullName "ue_animation.glb"
        if ((Test-Path $raw) -and -not (Test-Path $glb)) {
            # wait until the generator has finished writing (file size stable)
            $s1 = (Get-Item $raw).Length; Start-Sleep -Seconds 2
            if ((Get-Item $raw).Length -ne $s1) { continue }
            Write-Host "Exporting $($d.Name) for Unreal..." -ForegroundColor Cyan
            & python "$root\scripts\export_glb.py" --motion-dir $d.FullName --output $glb
            & python "$root\scripts\export_bvh.py" --motion-dir $d.FullName --output (Join-Path $d.FullName "ue_animation.bvh")
            Write-Host "  -> $glb" -ForegroundColor Green
        }
    }
}
'@ | Set-Content -Path (Join-Path $InstallDir "demo.ps1") -Encoding UTF8
}

Step "Writing export_ue.ps1/.bat (manual re-export of a demo generation)"
@'
@echo off
:: Double-click: convert the newest demo generation to Unreal-ready .glb/.bvh
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0export_ue.ps1" %*
pause
'@ | Set-Content -Path (Join-Path $InstallDir "export_ue.bat") -Encoding ASCII
@'
# export_ue.ps1 – convert a demo generation (or any output folder) to an Unreal-ready skinned .glb + .bvh
# Usage:  .\export_ue.ps1              -> newest generation in demo-output\
#         .\export_ue.ps1 7b76b4fe     -> generation whose id starts with 7b76b4fe
#         .\export_ue.ps1 output_motion -> any folder containing root_positions.f32
param([string]$Which = "")
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
if ($Which -and (Test-Path (Join-Path $root $Which "root_positions.f32"))) {
    $dir = Join-Path $root $Which
} else {
    $dirs = Get-ChildItem (Join-Path $root "demo-output") -Directory | Sort-Object LastWriteTime -Descending
    if ($Which) { $dirs = $dirs | Where-Object { $_.Name -like "$Which*" } }
    if (-not $dirs) { throw "No generation found for '$Which' in demo-output" }
    $dir = $dirs[0].FullName
}
python "$root\scripts\export_glb.py" --motion-dir $dir --output "$dir\ue_animation.glb"
python "$root\scripts\export_bvh.py" --motion-dir $dir --output "$dir\ue_animation.bvh"
Write-Host "`nUnreal:  $dir\ue_animation.glb" -ForegroundColor Green
Write-Host "Blender: $dir\ue_animation.bvh"
explorer $dir
'@ | Set-Content -Path (Join-Path $InstallDir "export_ue.ps1") -Encoding UTF8

Step "Install complete"
Write-Host @"

  cd $InstallDir
  .\generate.ps1 "a person walking forward enthusiastically and waving their right hand"

  Browser UI:  double-click demo.bat  (http://localhost:8094)
               Every generation automatically gets demo-output\<id>\ue_animation.glb (+ .bvh)
               -> drag that into Unreal. (The UI's own "Download GLB" is preview-only.)
               Manual re-export: double-click export_ue.bat

  Output: output_motion\animation.glb -> drag into UE5 Content Browser
          (Skeletal Mesh + Import Animations ON, Skeleton = None)
          then Right-click anim -> Retarget Animations -> SKM_Manny
"@ -ForegroundColor Green
