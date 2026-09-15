# demo.ps1 – local text-to-motion web UI
param([int]$Port = 8094)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root
$env:PATH = "$root\build\bin\Release;$root\build\Release;" + $env:PATH
if (-not (Get-Command go -ErrorAction SilentlyContinue)) { throw "Go not found. Install with: winget install -e --id GoLang.Go, then open a new terminal." }
if (-not (Test-Path "build\Release\kmd-generate.exe")) { throw "kmd-generate.exe missing - rebuild first." }
$exe = "build\Release\kimodo-demo.exe"
if (-not (Test-Path $exe) -or (Get-Item demo\main.go).LastWriteTime -gt (Get-Item $exe).LastWriteTime) {
    Write-Host "Compiling demo..." -ForegroundColor Cyan
    go build -o $exe ./demo
    if ($LASTEXITCODE -ne 0) { throw "go build failed" }
}
$url = "http://localhost:$Port"
Write-Host "Starting demo at $url  (Ctrl+C or close this window to stop)" -ForegroundColor Green
$proc = Start-Process -FilePath $exe -ArgumentList "-addr","127.0.0.1:$Port","-generator","build\Release\kmd-generate.exe" -NoNewWindow -PassThru
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 500
    if ($proc.HasExited) { throw "Demo server exited early (exit code $($proc.ExitCode)) - see output above." }
    try { $null = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 1; break } catch {}
}
Start-Process $url
$outDir = Join-Path $root "demo-output"
Write-Host "Watching $outDir - each generation gets ue_animation.glb / .bvh automatically" -ForegroundColor Cyan
while (-not $proc.HasExited) {
    Start-Sleep -Seconds 3
    if (-not (Test-Path $outDir)) { continue }
    foreach ($d in Get-ChildItem $outDir -Directory) {
        $raw = Join-Path $d.FullName "local_rotations_xyzw.f32"
        $glb = Join-Path $d.FullName "ue_animation.glb"
        if ((Test-Path $raw) -and -not (Test-Path $glb)) {
            $s1 = (Get-Item $raw).Length; Start-Sleep -Seconds 2
            if ((Get-Item $raw).Length -ne $s1) { continue }
            Write-Host "Exporting $($d.Name) for Unreal..." -ForegroundColor Cyan
            & python "$root\scripts\export_glb.py" --motion-dir $d.FullName --output $glb
            & python "$root\scripts\export_bvh.py" --motion-dir $d.FullName --output (Join-Path $d.FullName "ue_animation.bvh")
            Write-Host "  -> $glb" -ForegroundColor Green
        }
    }
}
