# Run the Player Hosted relay locally, keeping the existing Cloudflare rooms separate.
# Friends still use ONLINE > Join With A Code (six-character private room code).
param(
    [ValidateRange(1024,65535)]
    [int]$Port = 8787,
    [string]$RegistryUrl = 'https://dhd-online.downhill-revived.workers.dev'
)

$ErrorActionPreference = 'Stop'

# The directory must be HTTPS, without any user info, path, or query.
# It needs the updated online/server code to resolve player-hosted room codes.
if ($RegistryUrl -notmatch '^https://[A-Za-z0-9._-]+(?::443)?$') {
    throw 'RegistryUrl must be an HTTPS host without a path (for example https://your-worker.workers.dev).'
}

$server = Join-Path -Path $PSScriptRoot -ChildPath 'server'
if (-not (Test-Path -LiteralPath (Join-Path -Path $server -ChildPath 'package.json'))) {
    throw 'Missing online/server/package.json. Run PlayerHost.cmd from the cloned Downhill-Port project folder.'
}

$node = Get-Command node.exe -ErrorAction SilentlyContinue
$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $node -or -not $npm) {
    throw 'Node.js 22 or newer (including npm) is required. Install Node.js and try again.'
}

$nodeVersion = (& node.exe -p 'process.versions.node').Trim()
$major = [int]($nodeVersion.Split('.')[0])
if ($major -lt 22) {
    throw "Node.js 22 or newer is required (installed version: $nodeVersion)."
}

Push-Location -LiteralPath $server
try {
    $wrangler = Join-Path -Path $server -ChildPath 'node_modules\.bin\wrangler.cmd'
    if (-not (Test-Path -LiteralPath $wrangler)) {
        Write-Host 'Installing Player Hosted dependencies (first run only)...'
        & npm.cmd ci
        if ($LASTEXITCODE -ne 0) {
            throw "npm ci failed (exit code $LASTEXITCODE)."
        }
    }

    if (-not (Test-Path -LiteralPath $wrangler)) {
        throw 'Wrangler was not installed. Check npm ci output.'
    }

    Write-Host ''
    Write-Host "Local Player Hosted relay: TCP port $Port" -ForegroundColor Green
    Write-Host 'In-game: ONLINE > Host A Private Game (Your PC).'
    Write-Host 'Friends: ONLINE > Join With A Code (same six-character code).'
    Write-Host ''
    Write-Host 'For internet play, TCP port forwarding / public IP is required.'
    Write-Host 'CGNAT needs an accessible tunnel or a public IP; there is no automatic relay fallback.'
    Write-Host 'The directory must be deployed with updated player-host endpoints.'
    Write-Host 'Leave this window open while hosting.'
    Write-Host ''

    # Host traffic stays on this computer: Wrangler runs workerd locally.
    # The registry only resolves the private code; it does not relay race data.
    & $wrangler dev --local --ip '0.0.0.0' --port $Port --var "DHD_PLAYER_HOST_PORT:$Port" --var "DHD_HOST_REGISTRY:$RegistryUrl"
    if ($LASTEXITCODE -ne 0) {
        throw "Player Hosted relay exited with code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}
