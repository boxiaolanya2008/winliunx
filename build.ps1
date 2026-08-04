param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",

    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

function Refresh-Path {
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $env:Path += ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}

Refresh-Path

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake not found. Install via choco install cmake."
}
if (-not (Get-Command mingw32-make -ErrorAction SilentlyContinue) -and
    -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw "Neither mingw32-make nor ninja found. Install a C toolchain."
}

$cfgDir = Join-Path $PSScriptRoot "build-$($Config.ToLower())"

if ($Clean -and (Test-Path $cfgDir)) {
    Remove-Item -LiteralPath $cfgDir -Recurse -Force
}

$generator = if (Get-Command ninja -ErrorAction SilentlyContinue) { "Ninja" }
             else { "MinGW Makefiles" }

cmake -S $PSScriptRoot -B $cfgDir -G $generator -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

cmake --build $cfgDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

ctest --test-dir $cfgDir --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed" }

$exe = Join-Path $cfgDir "winlinux.exe"
if (-not (Test-Path $exe)) {
    throw "Build did not produce $exe"
}
Write-Host "Built: $exe"

if ($Run) {
    & $exe
}
