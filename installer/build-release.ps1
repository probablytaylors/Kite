#Requires -Version 5
param(
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if (-not $Version) {
    $line = Select-String -Path (Join-Path $root "CMakeLists.txt") -Pattern "project\(Kite VERSION ([0-9.]+)"
    $Version = $line.Matches[0].Groups[1].Value
}
Write-Host "Building Kite $Version"

cmake -S $root -B (Join-Path $root "build") -G "Visual Studio 17 2022" -A x64 -D KITE_WERROR=ON
cmake --build (Join-Path $root "build") --config Release

$iscc = "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
& $iscc "/DAppVersion=$Version" (Join-Path $PSScriptRoot "kite.iss")

$setup = Join-Path $PSScriptRoot "output\kite-setup.exe"
$hash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLower()
"$hash  kite-setup.exe" | Out-File -Encoding ascii "$setup.sha256"

Write-Host ""
Write-Host "Wrote $setup"
Write-Host "SHA-256 $hash"
Write-Host ""
Write-Host "To publish:"
Write-Host "  git tag v$Version"
Write-Host "  git push origin main --tags"
Write-Host "  gh release create v$Version `"$setup`" `"$setup.sha256`" --title `"Kite $Version`" --generate-notes"
