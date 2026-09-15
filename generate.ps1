# generate.ps1 – text prompt -> motion output (+ .glb/.bvh for Unreal/Blender)
# Usage: .\generate.ps1 "a person walking forward and waving" [-Frames 120] [-Steps 50] [-Seed 42] [-Out my_anim]
param(
    [Parameter(Mandatory=$true, Position=0)][string]$Prompt,
    [int]$Frames = 120,
    [int]$Steps  = 50,
    [int]$Seed   = 42,
    [string]$Out = "output_motion",
    [string]$Model = "soma-rp-v1.1"
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
