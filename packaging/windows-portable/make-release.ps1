<#
.SYNOPSIS
    Builds the portable Windows release zip (projectMSDL-portable-windows-x64.zip).

.DESCRIPTION
    1. Builds the static (DLL-free) Release exe in the build folder. If the folder doesn't exist yet,
       it is configured first (see "Building the standalone Windows version" in the README).
    2. Installs it into a clean staging folder.
    3. Downloads the current Cream of the Crop presets and Milkdrop texture pack from the official
       projectM repositories and copies them into "presets" and "textures".
    4. Adds portable.txt, README.txt (both from this folder), LICENSE.md and the Spout2 license.
    5. Starts the exe for a few seconds as a smoke test, then removes the "userdata" folder it creates.
    6. Zips the staging folder.

    Nothing in your own install (e.g. C:\dev\projectm-standalone) or your settings is touched.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File packaging\windows-portable\make-release.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File packaging\windows-portable\make-release.ps1 -OutputDir D:\release -SkipTest
#>
param(
    # Where the staging folder and zip are created. Default: a "release" folder next to the repository.
    [string]$OutputDir = (Join-Path $PSScriptRoot "..\..\..\release"),
    # Static build folder, relative to the repository root or absolute.
    [string]$BuildDir = "cmake-build-static",
    # vcpkg location, only used if the build folder has to be configured first.
    [string]$VcpkgRoot = $(if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "C:\vcpkg" }),
    # Visual Studio CMake generator, only used if the build folder has to be configured first.
    [string]$Generator = "Visual Studio 18 2026",
    # Skip starting the exe as a smoke test.
    [switch]$SkipTest
)

$ErrorActionPreference = "Stop"

$name = "projectMSDL-portable-windows-x64"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $repoRoot $BuildDir }
New-Item -ItemType Directory -Force $OutputDir | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path
$staging = Join-Path $OutputDir $name
$zip = Join-Path $OutputDir "$name.zip"
$downloads = Join-Path $OutputDir "src"

function Invoke-Native([string]$what, [scriptblock]$command)
{
    Write-Host "==> $what"
    & $command
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)." }
}

# 1. Build (configure first if needed)
if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt")))
{
    Invoke-Native "Configuring static build in $BuildDir (vcpkg builds all libraries, this can take a while)" {
        cmake -G $Generator -A x64 -S $repoRoot -B $BuildDir `
            "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
            -DVCPKG_TARGET_TRIPLET=x64-windows-static `
            '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' `
            -DENABLE_FLAT_PACKAGE=ON
    }
}
Invoke-Native "Building Release" { cmake --build $BuildDir --config Release --parallel }

# 2. Install into a clean staging folder
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
Invoke-Native "Installing into $staging" { cmake --install $BuildDir --config Release --prefix $staging }

# 3. Presets and textures, fresh from the official repositories
if (Test-Path $downloads) { Remove-Item $downloads -Recurse -Force }
New-Item -ItemType Directory $downloads | Out-Null
$presetRepo = Join-Path $downloads "presets-cream-of-the-crop"
$textureRepo = Join-Path $downloads "presets-milkdrop-texture-pack"
Invoke-Native "Downloading presets" { git clone -q --depth 1 https://github.com/projectM-visualizer/presets-cream-of-the-crop.git $presetRepo }
Invoke-Native "Downloading textures" { git clone -q --depth 1 https://github.com/projectM-visualizer/presets-milkdrop-texture-pack.git $textureRepo }

New-Item -ItemType Directory (Join-Path $staging "presets"), (Join-Path $staging "textures") | Out-Null
# Without -Force, Get-ChildItem skips the hidden .git folder.
Get-ChildItem -LiteralPath $presetRepo -Exclude ".git" | Copy-Item -Destination (Join-Path $staging "presets") -Recurse
# The images are in the pack's "textures" subfolder. Its README.md credits the textures.
Get-ChildItem -LiteralPath (Join-Path $textureRepo "textures") | Copy-Item -Destination (Join-Path $staging "textures") -Recurse
Copy-Item (Join-Path $textureRepo "README.md") (Join-Path $staging "textures")

# 4. Text files
Copy-Item (Join-Path $PSScriptRoot "portable.txt"), (Join-Path $PSScriptRoot "README.txt") $staging
Copy-Item (Join-Path $repoRoot "LICENSE.md") $staging
# Spout2 is BSD licensed: its license text must come with the program.
$spoutLicense = Join-Path $BuildDir "vcpkg_installed\x64-windows-static\share\spout2\copyright"
Copy-Item $spoutLicense (Join-Path $staging "LICENSE-Spout2.txt")

# 5. Smoke test: the exe must still be running after a few seconds.
if (-not $SkipTest)
{
    Write-Host "==> Starting projectMSDL.exe for a few seconds"
    $process = Start-Process (Join-Path $staging "projectMSDL.exe") -WorkingDirectory $env:TEMP -PassThru
    Start-Sleep -Seconds 6
    if ($process.HasExited) { throw "projectMSDL.exe exited early with code $($process.ExitCode)." }
    Stop-Process -Id $process.Id -Force
    $process.WaitForExit()
    Start-Sleep -Seconds 1
}
# Never ship settings or favorites.
$userdata = Join-Path $staging "userdata"
if (Test-Path $userdata) { Remove-Item $userdata -Recurse -Force }

# 6. Zip (tar.exe ships with Windows 10/11 and is much faster than Compress-Archive for ~10,000 files)
if (Test-Path $zip) { Remove-Item $zip -Force }
Push-Location $OutputDir
try { Invoke-Native "Creating $zip" { tar.exe -a -c -f "$name.zip" $name } }
finally { Pop-Location }

$presetCount = (Get-ChildItem (Join-Path $staging "presets") -Recurse -Filter *.milk).Count
$textureCount = (Get-ChildItem (Join-Path $staging "textures") -File | Where-Object { $_.Extension -ne ".md" }).Count
Write-Host ""
Write-Host ("Done: {0} ({1:N0} MB), {2} presets, {3} textures." -f $zip, ((Get-Item $zip).Length / 1MB), $presetCount, $textureCount)
