# Build the native Downhill Port on Windows without uploading game data.
# Run from the repository root: powershell -ExecutionPolicy Bypass -File .\Build-Downhill.ps1
param([switch]$SkipDependencies)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw 'Python 3.12+ is required: https://www.python.org/downloads/'
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git is required: https://git-scm.com/downloads'
}
$isoDir = Join-Path $PSScriptRoot 'iso'
New-Item -ItemType Directory -Force -Path $isoDir | Out-Null
$elf = Join-Path $isoDir 'SCUS_971.77'
$expectedSha1 = '4e25a222ad8a475b5d26c2e928e90b5f2b7075eb'

if (-not (Test-Path $elf)) {
    $isos = @(Get-ChildItem -LiteralPath $isoDir -File | Where-Object { $_.Extension -ieq '.iso' })
    if ($isos.Count -ne 1) {
        throw "Place exactly one USA Downhill Domination .iso inside $isoDir (or put its extracted SCUS_971.77 there). Found $($isos.Count)."
    }
    $sevenZip = Get-Command 7z,7za,7zz -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $sevenZip) {
        foreach ($path in @('C:\Program Files\7-Zip\7z.exe','C:\Program Files (x86)\7-Zip\7z.exe')) {
            if (Test-Path $path) { $sevenZip = @{ Source = $path }; break }
        }
    }
    if (-not $sevenZip) { throw 'Install 7-Zip and put 7z on PATH to extract SCUS_971.77.' }
    & $sevenZip.Source e $isos[0].FullName 'SCUS_971.77' "-o$isoDir" -y
    if ($LASTEXITCODE -ne 0) { throw 'Unable to extract SCUS_971.77 from ISO.' }
}
$sha = (Get-FileHash -Algorithm SHA1 -LiteralPath $elf).Hash.ToLowerInvariant()
if ($sha -ne $expectedSha1) {
    throw "SCUS_971.77 does not match the expected USA retail version. SHA1: $sha"
}
if (-not $SkipDependencies) {
    python -m pip install -r requirements.txt
    if ($LASTEXITCODE -ne 0) { throw 'Python dependencies installation failed.' }
}
python tools/split.py
if ($LASTEXITCODE -ne 0) { throw 'Game executable splitting failed.' }
python port/tools/build_clean.py
if ($LASTEXITCODE -ne 0) { throw 'PC build failed. Check build/port/clean/logs/.' }

$manifest = Get-ChildItem 'build/port/clean' -Filter 'manifest.json' -Recurse |
    Where-Object { $_.DirectoryName -notmatch '[\\/]tree[\\/]' } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($manifest) {
    Write-Host "Build completed: $(Join-Path $manifest.DirectoryName 'dhd.exe')" -ForegroundColor Green
    Write-Host 'Run dhd.exe and select the USA ISO. This is an experimental 8-player build.'
} else {
    Write-Host 'Build command returned success; inspect build/port/clean/ for dhd.exe.'
}
