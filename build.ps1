param([switch]$Clean)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$butanoCommit = 'c66094ae514c74068f992a4896c5e1f234e9f6e2'
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { throw 'Install and start Docker Desktop (Linux containers), then rerun this script.' }
docker info --format '{{.ServerVersion}}' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Docker Desktop must be running in Linux container mode.' }
if (-not (Test-Path '.tools/butano/.git')) {
    New-Item -ItemType Directory -Force '.tools' | Out-Null
    git clone --depth 1 --branch 21.8.0 https://github.com/GValiente/butano.git .tools/butano
    if ($LASTEXITCODE -ne 0) { throw 'Could not download Butano.' }
}
$actual = git -C .tools/butano rev-parse HEAD
if ($actual -ne $butanoCommit) { throw "Unexpected Butano revision: $actual. Expected $butanoCommit." }
docker build -t dustline-build:1 -f tools/Dockerfile .
if ($LASTEXITCODE -ne 0) { throw 'Toolchain image build failed.' }
$mount = "type=bind,source=$PSScriptRoot,target=/work"
if ($Clean) {
    docker run --rm --mount $mount dustline-build:1 make clean
    if ($LASTEXITCODE -ne 0) { throw 'Clean failed.' }
}
docker run --rm --mount $mount dustline-build:1 python3 tools/generate_assets.py
if ($LASTEXITCODE -ne 0) { throw 'Asset generation failed.' }
docker run --rm --mount $mount dustline-build:1 g++ -std=c++17 -O2 -Wall -Wextra -Iinclude tools/test_active_cardinal_exits.cpp -o build/test_active_cardinal_exits
if ($LASTEXITCODE -ne 0) { throw 'Could not compile active cardinal exit audit.' }
docker run --rm --mount $mount dustline-build:1 ./build/test_active_cardinal_exits
if ($LASTEXITCODE -ne 0) { throw 'An active map cannot realize its cardinal exits.' }
docker run --rm --mount $mount dustline-build:1 make -j4
if ($LASTEXITCODE -ne 0) { throw 'ROM compilation failed.' }
New-Item -ItemType Directory -Force dist | Out-Null
Copy-Item -LiteralPath 'dustline.gba' -Destination 'dist/dustline.gba'
New-Item -ItemType Directory -Force dist/licenses | Out-Null
Copy-Item -Path '.tools/butano/licenses/*.txt' -Destination 'dist/licenses/' -Force
Copy-Item -LiteralPath 'readme.txt' -Destination 'dist/readme.txt'
Copy-Item -LiteralPath 'LICENSE' -Destination 'dist/LICENSE'
Get-FileHash -Algorithm SHA256 dist/dustline.gba
Write-Host 'Playable ROM: dist/dustline.gba'
