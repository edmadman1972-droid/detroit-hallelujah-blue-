# Collects the built Windows plug-ins into dist\: a zip and (if Inno Setup is installed) an installer .exe
# Usage: scripts\package_windows.ps1 [-BuildDir build] [-Config Release]
param(
    [string]$BuildDir = "build",
    [string]$Config = "Release"
)
$ErrorActionPreference = "Stop"

$version = (Select-String -Path "CMakeLists.txt" -Pattern 'VERSION (\d+\.\d+\.\d+)' | Select-Object -First 1).Matches[0].Groups[1].Value
$dist = "dist"
$folder = Join-Path $dist "Detroit Hallelujah (Windows)"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $folder "VST3") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $folder "Standalone") | Out-Null

$found = 0
foreach ($target in @("HallelujahBlue", "HallelujahDuck")) {
    $art = Join-Path $BuildDir "plugins\$target\${target}_artefacts\$Config"
    Get-ChildItem -Path (Join-Path $art "VST3") -Filter "*.vst3" -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName -Destination (Join-Path $folder "VST3") -Recurse
        $found++
    }
    Get-ChildItem -Path (Join-Path $art "Standalone") -Filter "*.exe" -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName -Destination (Join-Path $folder "Standalone")
        $found++
    }
}
if ($found -eq 0) { throw "No built plug-ins found under $BuildDir ($Config)." }

Copy-Item "README.md" -Destination $folder
Compress-Archive -Path $folder -DestinationPath (Join-Path $dist "DetroitHallelujah-$version-Windows.zip")

$iscc = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
if (-not $iscc) {
    $candidate = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    if (Test-Path $candidate) { $iscc = Get-Item $candidate }
}
if ($iscc) {
    $src = (Resolve-Path $folder).Path
    $out = (Resolve-Path $dist).Path
    & $iscc.Source "/DAppVersion=$version" "/DSourceDir=$src" "/DOutputDir=$out" "scripts\installer.iss"
} else {
    Write-Host "Inno Setup not found - skipping the installer, the zip is ready."
}

Write-Host "Packaged $found items into $dist\"
Get-ChildItem $dist
