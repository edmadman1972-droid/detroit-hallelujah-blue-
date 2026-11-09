# Build + package on Windows. Needs Visual Studio 2022 or later ("Desktop development with C++")
# and CMake 3.22+ (included with Visual Studio). Run from a PowerShell prompt:
#   powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")
cmake -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
& (Join-Path $PSScriptRoot "package_windows.ps1") -BuildDir build -Config Release
Write-Host "Done. Run the installer in dist\, or copy the .vst3 folders to C:\Program Files\Common Files\VST3."
