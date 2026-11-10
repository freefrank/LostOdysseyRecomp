# Builds LostOdysseyRecomp.nro on Windows with Docker Desktop.
#
#   powershell -ExecutionPolicy Bypass -File tools\switch\build-switch.ps1
#   powershell -ExecutionPolicy Bypass -File tools\switch\build-switch.ps1 -Jobs 4 -Clean
#   powershell -ExecutionPolicy Bypass -File tools\switch\build-switch.ps1 -Pgo GEN
#
# -Pgo GEN builds the instrumented training NRO in out\switch\pgo-GEN\, -Pgo USE
# the optimised one from the profile in out\pgo\ into out\switch\pgo-USE\
# (docs\SWITCH-PGO.md). The training NRO is slow and never released.
#
# Everything runs inside ghcr.io/autorunhq/switch-dev (devkitA64, libnx and the
# mesa-switch NVK driver); nothing else needs installing on Windows. The NRO
# lands in out\switch\. See docs\SWITCH.md.
param(
    [string]$Image = "ghcr.io/autorunhq/switch-dev:2026.10.05",
    [int]$Jobs = 0,
    [ValidateSet("Release", "RelWithDebInfo")][string]$BuildType = "Release",
    [switch]$Clean,
    [ValidateSet("OFF", "GEN", "USE")][string]$Pgo = "OFF"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "Docker is not installed or not on PATH. Install Docker Desktop (WSL 2 backend)."
}
if (-not (Test-Path (Join-Path $root "LostOdysseyRecompLib\private\disc1\default.xex"))) {
    throw "Copy your Disc 1 default.xex to LostOdysseyRecompLib\private\disc1\ first (docs\SWITCH.md)."
}

$envArgs = @("-e", "BUILD_TYPE=$BuildType")
if ($Jobs -gt 0) { $envArgs += @("-e", "JOBS=$Jobs") }
if ($Clean) { $envArgs += @("-e", "CLEAN=1") }
if ($Pgo -ne "OFF") {
    if ($Pgo -eq "USE" -and -not (Get-ChildItem -Path (Join-Path $root "out\pgo") -Recurse -Filter *.gcda -ErrorAction SilentlyContinue | Select-Object -First 1)) {
        throw "No profile in out\pgo\. Build -Pgo GEN, train it on the console and fetch the .gcda files first (docs\SWITCH-PGO.md)."
    }
    $envArgs += @("-e", "PGO=$Pgo")
}

docker run --rm @envArgs -v "${root}:/work" -w /work $Image bash tools/switch/build-switch.sh
if ($LASTEXITCODE -ne 0) { throw "Switch build failed (exit $LASTEXITCODE)" }
Write-Host ""
switch ($Pgo) {
    "GEN" { Write-Host "Training NRO: out\switch\pgo-GEN\LostOdysseyRecomp.nro. Deploy it, train it (tools\switch\pgo_train.py), never release it." }
    "USE" { Write-Host "Optimised NRO: out\switch\pgo-USE\LostOdysseyRecomp.nro. Keep the .elf next to it for crash addresses." }
    default { Write-Host "Copy out\switch\LostOdysseyRecomp.nro to sdmc:/switch/LostOdysseyRecomp/ on the SD card." }
}
