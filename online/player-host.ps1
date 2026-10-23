# Local Player Hosted relay. The game starts this script invisibly on demand
# and terminates its whole process tree when the host leaves the session.
param(
    [ValidateRange(1024,65535)]
    [int]$Port = 8787,
    [string]$RegistryUrl = 'https://dhd-online.downhill-revived.workers.dev'
)

$ErrorActionPreference = 'Stop'
if ($RegistryUrl -notmatch '^https://[A-Za-z0-9._-]+(?::443)?$') {
    throw 'RegistryUrl must be an HTTPS host without path or user info.'
}

$server = Join-Path -Path $PSScriptRoot -ChildPath 'server'
if (-not (Test-Path -LiteralPath (Join-Path $server 'package.json'))) {
    throw 'Missing online/server/package.json. Host files must be bundled beside the game.'
}

function Get-NodeVersion([string]$executable) {
    if (-not (Test-Path -LiteralPath $executable)) { return 0 }
    try {
        $text = (& $executable -p 'process.versions.node').Trim()
        if ($LASTEXITCODE -eq 0) { return [int]($text.Split('.')[0]) }
    } catch { }
    return 0
}

# Prefer a working system Node.js 22+, but for a player with no Node installed
# install a portable official LTS runtime under the user's LocalAppData.
# No administrator account, UAC or change to system PATH is needed.
$systemNode = Get-Command node.exe -ErrorAction SilentlyContinue
$systemNpm = Get-Command npm.cmd -ErrorAction SilentlyContinue
$nodeOk = $systemNode -and $systemNpm -and (Get-NodeVersion $systemNode.Source) -ge 22

if (-not $nodeOk) {
    Write-Host 'Node.js 22+ was not found. Preparing a portable Node.js runtime...'
    $cacheRoot = Join-Path $env:LOCALAPPDATA 'DownhillRevived\host-runtime'
    $nodeDir = Join-Path $cacheRoot 'node'
    $portableNode = Join-Path $nodeDir 'node.exe'
    $portableNpm = Join-Path $nodeDir 'npm.cmd'

    if ((Get-NodeVersion $portableNode) -lt 22 -or
        -not (Test-Path -LiteralPath $portableNpm)) {
        # Official release index; select the most recent LTS that provides a
        # Windows x64 ZIP. Verify its SHA-256 using Node.js' SHASUMS256.txt.
        $releases = Invoke-RestMethod -Uri 'https://nodejs.org/dist/index.json' -TimeoutSec 30
        $release = $releases | Where-Object {
            $_.lts -and $_.lts -ne $false -and
            $_.files -contains 'win-x64-zip' -and
            [int]($_.version.TrimStart('v').Split('.')[0]) -ge 22
        } | Select-Object -First 1
        if (-not $release) { throw 'Could not find an official Node.js LTS Windows x64 ZIP.' }

        $version = [string]$release.version
        $base = 'node-' + $version + '-win-x64'
        $archive = $base + '.zip'
        $rootUrl = 'https://nodejs.org/dist/' + $version
        $manifest = (Invoke-WebRequest -UseBasicParsing -Uri ($rootUrl + '/SHASUMS256.txt') -TimeoutSec 30).Content
        $pattern = '(?m)^([a-fA-F0-9]{64})\s+' + [regex]::Escape($archive) + '\s*$'
        $hashLine = [regex]::Match($manifest, $pattern)
        if (-not $hashLine.Success) { throw 'The official Node.js checksum was not found.' }

        New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
        $scratch = Join-Path $env:TEMP ('dhd-node-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Force -Path $scratch | Out-Null
        try {
            $zip = Join-Path $scratch $archive
            Write-Host "Downloading official Node.js $version (first run only)..."
            Invoke-WebRequest -UseBasicParsing -Uri ($rootUrl + '/' + $archive) -OutFile $zip -TimeoutSec 180
            $actual = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
            if ($actual -ine $hashLine.Groups[1].Value) {
                throw 'Downloaded Node.js runtime did not match the official SHA-256 checksum.'
            }
            Expand-Archive -LiteralPath $zip -DestinationPath $scratch -Force
            if (-not (Test-Path -LiteralPath (Join-Path $scratch $base))) {
                throw 'The official Node.js ZIP did not contain its expected directory.'
            }
            if (Test-Path -LiteralPath $nodeDir) {
                Remove-Item -LiteralPath $nodeDir -Recurse -Force
            }
            Move-Item -LiteralPath (Join-Path $scratch $base) -Destination $nodeDir
        } finally {
            Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    if ((Get-NodeVersion $portableNode) -lt 22 -or
        -not (Test-Path -LiteralPath $portableNpm)) {
        throw 'The portable Node.js runtime was not installed correctly.'
    }
    $env:PATH = $nodeDir + ';' + $env:PATH
}

Push-Location -LiteralPath $server
try {
    $wrangler = Join-Path $server 'node_modules\.bin\wrangler.cmd'
    if (-not (Test-Path -LiteralPath $wrangler)) {
        Write-Host 'Installing local relay dependencies (first run only)...'
        & npm.cmd ci --no-audit --no-fund
        if ($LASTEXITCODE -ne 0) {
            throw "npm ci failed (exit code $LASTEXITCODE)."
        }
    }
    if (-not (Test-Path -LiteralPath $wrangler)) {
        throw 'Wrangler was not installed. See npm output.'
    }

    Write-Host "Local Player Hosted relay on TCP port $Port."
    Write-Host 'Friends enter through ONLINE > Join With A Code.'
    Write-Host 'Internet hosting requires a public TCP port and updated room-code directory.'
    Write-Host ''

    # Run the same durable-object relay locally; all child processes are
    # managed by dhd.exe via a Windows Job Object (no console window).
    & $wrangler dev --local --ip '0.0.0.0' --port $Port --var "DHD_PLAYER_HOST_PORT:$Port" --var "DHD_HOST_REGISTRY:$RegistryUrl"
    if ($LASTEXITCODE -ne 0) {
        throw "The local relay exited with code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
