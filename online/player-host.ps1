# Local Player Hosted relay. Reuses the exact existing Cloudflare Worker
# lobby/room protocol, running under Wrangler's local workerd process.
# No deployment, access token or Cloudflare account is necessary.
param(
    [ValidateRange(1024,65535)]
    [int]$Port = 8787
)
$ErrorActionPreference = 'Stop'
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
    Write-Host 'In Downhill Revived select ONLINE > Host Game On This PC.'
    Write-Host 'Tell friends your public IP, TCP port, and the in-game room code.'
    Write-Host 'For internet connections, the router must forward TCP port 8787'
    Write-Host 'to this computer. CGNAT requires a private VPN or a TCP tunnel.'
    Write-Host 'Allow Windows Firewall to access the port if prompted.'
    Write-Host 'Closing this window disconnects everyone.'
    Write-Host ''
    # --local ensures the race relay actually runs on this machine.
    # --ip 0.0.0.0 allows remote clients instead of only localhost.
    & '.\node_modules\.bin\wrangler.cmd' dev --local --ip 0.0.0.0 --port $Port
    if ($LASTEXITCODE -ne 0) { throw "The local relay exited with code $LASTEXITCODE." }
} finally {
    Pop-Location
}
