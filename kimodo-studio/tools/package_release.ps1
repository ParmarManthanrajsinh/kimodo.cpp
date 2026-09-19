<#
.SYNOPSIS
    Kimodo Studio Automated Release Distribution Script
.DESCRIPTION
    Builds Release configuration, runs self-tests, packages with CPack,
    verifies archive contents, generates SHA-256 checksums, and stages artifacts.
#>

param (
    [string]$BuildDir = "build/windows-vs2022",
    [string]$DistDir = "dist",
    [switch]$SkipTests = $false
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Get-Item "$PSScriptRoot\..").FullName
Set-Location $RepoRoot

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "  KIMODO STUDIO - RELEASE DISTRIBUTION PIPELINE   " -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan

# 1. Verify build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory '$BuildDir' not found. Configure CMake first."
}

# 2. Build Release Target
Write-Host ""
Write-Host "[1/5] Building Release target (MSVC x64)..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release --target kimodo_studio
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE"
}

# 3. Run Self-Test Suite
if (-not $SkipTests) {
    Write-Host ""
    Write-Host "[2/5] Running verification test suite..." -ForegroundColor Yellow
    $ExePath = Join-Path $BuildDir "Release\kimodo_studio.exe"
    if (-not (Test-Path $ExePath)) {
        Write-Error "Executable not found at '$ExePath'"
    }
    & $ExePath --selftest-all
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Self-test suite FAILED. Aborting distribution packaging."
    }
    Write-Host "  -> Self-tests PASSED." -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "[2/5] Skipping self-tests (-SkipTests specified)." -ForegroundColor DarkGray
}

# 4. Generate CPack Package
Write-Host ""
Write-Host "[3/5] Generating portable CPack package..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release --target package
if ($LASTEXITCODE -ne 0) {
    Write-Error "CPack packaging failed."
}

# 5. Locate generated archive
$ZipFiles = Get-ChildItem -Path $BuildDir -Filter "KimodoStudio-*.zip" | Sort-Object LastWriteTime -Descending
if ($ZipFiles.Count -eq 0) {
    Write-Error "No KimodoStudio-*.zip found in '$BuildDir'."
}
$Package = $ZipFiles[0]
$PackageSizeMb = [math]::Round($Package.Length / 1048576, 2)
Write-Host "  -> Found package: $($Package.Name) ($PackageSizeMb MB)" -ForegroundColor Green

# 6. Verify Archive Contents
Write-Host ""
Write-Host "[4/5] Auditing archive contents..." -ForegroundColor Yellow
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Zip = [System.IO.Compression.ZipFile]::OpenRead($Package.FullName)
$Entries = $Zip.Entries | ForEach-Object { $_.FullName }
$Zip.Dispose()

$RequiredFiles = @(
    "kimodo_studio.exe",
    "ggml.dll",
    "ggml-base.dll",
    "ggml-cpu.dll",
    "ggml-vulkan.dll",
    "README.md",
    "config/models.json",
    "fonts/Roboto-Regular.ttf",
    "fonts/fa-solid-900.ttf",
    "assets/characters/CesiumMan.glb"
)

$Missing = @()
foreach ($Req in $RequiredFiles) {
    if ($Entries -notcontains $Req) {
        $Missing += $Req
    }
}

if ($Missing.Count -gt 0) {
    Write-Error "Archive audit FAILED! Missing required files:`n$($Missing -join "`n")"
}
Write-Host "  -> All $($RequiredFiles.Count) required payload files verified." -ForegroundColor Green

# 7. Stage into Dist directory & calculate SHA-256
Write-Host ""
Write-Host "[5/5] Staging distribution artifacts into '$DistDir'..." -ForegroundColor Yellow
if (-not (Test-Path $DistDir)) {
    New-Item -ItemType Directory -Path $DistDir | Out-Null
}

$DistPkg = Join-Path $DistDir $Package.Name
Copy-Item $Package.FullName -Destination $DistPkg -Force

$Hash = (Get-FileHash -Path $DistPkg -Algorithm SHA256).Hash
$ChecksumFile = "$DistPkg.sha256"
"$Hash  $($Package.Name)" | Out-File -FilePath $ChecksumFile -Encoding utf8

# Generate release notes stub
$ReleaseNotes = Join-Path $DistDir "RELEASE_NOTES.md"
$NotesContent = @(
    "# Kimodo Studio Release ($($Package.BaseName))",
    "",
    "## Package Info",
    "- **File**: ``$($Package.Name)``",
    "- **Size**: $PackageSizeMb MB",
    "- **SHA-256**: ``$Hash``",
    "- **Platform**: Windows 10/11 x64",
    "",
    "## System Requirements",
    "- Windows 10 (Build 19041+) or Windows 11 64-bit",
    "- Vulkan-capable GPU (NVIDIA RTX 3060/4060 or equivalent recommended)",
    "- Microsoft Visual C++ 2015-2022 Redistributable (x64)",
    "",
    "## Installation & First Launch",
    "1. Extract ``$($Package.Name)`` to your desired directory.",
    "2. Launch ``kimodo_studio.exe``.",
    "3. Open the **Models** tab to download model weights from Hugging Face or import local GGUF models."
) -join "`r`n"

Set-Content -Path $ReleaseNotes -Value $NotesContent -Encoding utf8

Write-Host ""
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "  DISTRIBUTION PACKAGE READY FOR SHIPMENT!        " -ForegroundColor Green
Write-Host "  Artifact: $DistPkg" -ForegroundColor Green
Write-Host "  SHA-256:  $Hash" -ForegroundColor Green
Write-Host "  Notes:    $ReleaseNotes" -ForegroundColor Green
Write-Host "===================================================" -ForegroundColor Cyan
