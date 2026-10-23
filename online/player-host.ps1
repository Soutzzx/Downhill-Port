# Local Player Hosted relay. Reuses the exact existing Cloudflare Worker
# lobby/room protocol, running under Wrangler's local workerd process.
# No deployment, access token or Cloudflare account is necessary.
param(
    [ValidateRange(1024,65535)]
    [int]$Port = 8787,
    [string]$RegistryUrl = 'https://dhd-online.downhill-revived.workers.dev'
)
$ErrorActionPreference = 'Stop'
if ($RegistryUrl -notmatch '^https://[A-Za-z0-9._-]+(?::443)?
$server = Join-Path $PSScriptRoot 'server'
if (-not (Test-Path -LiteralPath (Join-Path $server 'package.json'))) {
    throw 'online/server was not found. Run PlayerHost.cmd from the cloned project folder.'
}
$node = Get-Command node.exe -ErrorAction SilentlyContinue
$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $node -or -not $npm) {
    Write-Host 'Node.js 22 or newer is required on the host PC.' -ForegroundColor Yellow
    Write-Host 'Install Node.js, then run PlayerHost.cmd again.'
    exit 1
}
$major = 0
try { $major = [int]((& node.exe -p 'process.versions.node.split(".")[0]') | Select-Object -First 1) } catch {}
if ($major -lt 22) {
    throw 'Node.js 22 or newer is required for this local relay.'
}
Push-Location $server
try {
    if (-not (Test-Path -LiteralPath '.\node_modules\.bin\wrangler.cmd')) {
        Write-Host 'Installing the local relay dependencies (first run only)...'
        & npm.cmd ci
        if ($LASTEXITCODE -ne 0) { throw 'npm ci failed; check your internet connection.' }
    }
    Write-Host ''
    Write-Host "Player Hosted is running on this PC, TCP port $Port" -ForegroundColor Green
    Write-Host 'In the game select ONLINE > Host A Private Game (Your PC).'
    Write-Host 'Give your friends ONLY the six-character room code.' -ForegroundColor Green
    Write-Host 'They can use the normal ONLINE > Join With A Code option.'
    Write-Host ''
    Write-Host 'The central service registers your room code to your public IP.'
    Write-Host 'For friends outside your network, forward TCP port 8787 to this PC.'
    Write-Host 'With CGNAT, a tunnel/VPN or relay fallback is required.'
    Write-Host 'Allow incoming connections in Windows Firewall if prompted.'
    Write-Host 'The host registration requires the public directory to run'
    Write-Host 'the latest online/server code from this project.'
    Write-Host 'Close this window to stop hosting.'
    Write-Host ''
    # The local-only port var opts this Worker into the code registry.
    # --ip 0.0.0.0 accepts friends connecting over the internet.
    & '.\node_modules\.bin\wrangler.cmd' dev --local --ip 0.0.0.0 --port $Port --var "DHD_PLAYER_HOST_PORT:$Port" --var "DHD_HOST_REGISTRY:$RegistryUrl"
    if ($LASTEXITCODE -ne 0) { throw "The local relay exited with code $LASTEXITCODE." }
} finally {
    Pop-Location
}
) {
    throw 'RegistryUrl must be a secure HTTPS host (without a path).'
}
$server = Join-Path $PSScriptRoot 'server'
if (-not (Test-Path -LiteralPath (Join-Path $server 'package.json'))) {
    throw 'online/server was not found. Run PlayerHost.cmd from the cloned project folder.'
}
$node = Get-Command node.exe -ErrorAction SilentlyContinue
$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $node -or -not $npm) {
    Write-Host 'Node.js 22 or newer is required on the host PC.' -ForegroundColor Yellow
    Write-Host 'Install Node.js, then run PlayerHost.cmd again.'
    exit 1
}
$major = 0
try { $major = [int]((& node.exe -p 'process.versions.node.split(".")[0]') | Select-Object -First 1) } catch {}
if ($major -lt 22) {
    throw 'Node.js 22 or newer is required for this local relay.'
}
Push-Location $server
try {
    if (-not (Test-Path -LiteralPath '.\node_modules\.bin\wrangler.cmd')) {
        Write-Host 'Installing the local relay dependencies (first run only)...'
        & npm.cmd ci
        if ($LASTEXITCODE -ne 0) { throw 'npm ci failed; check your internet connection.' }
    }
    Write-Host ''
    Write-Host "Player Hosted is running on this PC, TCP port $Port" -ForegroundColor Green
    Write-Host 'In the game select ONLINE > Host A Private Game (Your PC).'
    Write-Host 'Give your friends ONLY the six-character room code.' -ForegroundColor Green
    Write-Host 'They can use the normal ONLINE > Join With A Code option.'
    Write-Host ''
    Write-Host 'The central service registers your room code to your public IP.'
    Write-Host 'For friends outside your network, forward TCP port 8787 to this PC.'
    Write-Host 'With CGNAT, a tunnel/VPN or relay fallback is required.'
    Write-Host 'Allow incoming connections in Windows Firewall if prompted.'
    Write-Host 'The host registration requires the public directory to run'
    Write-Host 'the latest online/server code from this project.'
    Write-Host 'Close this window to stop hosting.'
    Write-Host ''
    # The local-only port var opts this Worker into the code registry.
    # --ip 0.0.0.0 accepts friends connecting over the internet.
    & '.\node_modules\.bin\wrangler.cmd' dev --local --ip 0.0.0.0 --port $Port --var "DHD_PLAYER_HOST_PORT:$Port"
    if ($LASTEXITCODE -ne 0) { throw "The local relay exited with code $LASTEXITCODE." }
} finally {
    Pop-Location
}
