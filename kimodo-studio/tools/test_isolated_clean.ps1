$ErrorActionPreference = "Stop"
$TestDir = Join-Path $env:TEMP ("kimodo_isolated_test_" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $TestDir | Out-Null
$Zip = (Get-ChildItem -Path dist\KimodoStudio-*.zip)[0].FullName
Expand-Archive -Path $Zip -DestinationPath $TestDir
Write-Host "Extracted to: $TestDir"

$Files = Get-ChildItem -Path $TestDir -Recurse | Select-Object -ExpandProperty Name
$Checks = @("kimodo_studio.exe", "ggml.dll", "ggml-vulkan.dll", "models.json", "CesiumMan.glb", "Roboto-Regular.ttf")
foreach ($c in $Checks) {
    if ($Files -contains $c) {
        Write-Host "  [OK] $c present"
    } else {
        Write-Error "  [MISSING] $c missing!"
    }
}

$Exe = Join-Path $TestDir "kimodo_studio.exe"
& $Exe --selftest-all
if ($LASTEXITCODE -ne 0) {
    Write-Error "Self-test failed in clean directory!"
} else {
    Write-Host "  -> Clean isolated directory self-test PASSED!"
}

Remove-Item -Path $TestDir -Recurse -Force
Write-Host "Cleaned up isolated test directory."
