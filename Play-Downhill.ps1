# Portable Windows player launcher: no compiler required on the player's PC.
# Only downloads a compiled release from this repo. The player's ISO stays local.
param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$repo = 'Soutzzx/Downhill-Port'
$target = Join-Path $PSScriptRoot 'play'
$exe = Join-Path $target 'dhd.exe'
$ini = Join-Path $target 'dhd.ini'
$releases = "https://github.com/$repo/releases"
$api = "https://api.github.com/repos/$repo/releases/latest"
$headers = @{ Accept = 'application/vnd.github+json'; 'User-Agent' = 'Downhill-Port-Launcher' }

function Release-Url-Valid([string]$url, [string]$filename) {
    if ([string]::IsNullOrWhiteSpace($url)) { return $false }
    if (-not $url.StartsWith("$releases/download/", [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }
    try {
        $u = [uri]$url
        return $u.Scheme -eq 'https' -and $u.Host -eq 'github.com' -and
               [Uri]::UnescapeDataString($u.Segments[-1]) -ceq $filename
    } catch { return $false }
}

function Install-Native-Release {
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Write-Host "Procurando a ultima release publicada em $releases ..."
    try {
        $release = Invoke-RestMethod -Uri $api -Headers $headers -TimeoutSec 40
    } catch {
        throw ('Nenhuma release pronta foi encontrada, ou o GitHub esta indisponivel. ' +
            'O responsavel pelo projeto deve publicar um ZIP nativo e seu latest.json ' +
            "em $releases antes que outra pessoa consiga jogar sem compilar. " + $_.Exception.Message)
    }
    $meta = @($release.assets | Where-Object { $_.name -ceq 'latest.json' } | Select-Object -First 1)
    if ($meta.Count -ne 1) {
        throw 'Esta release nao contem latest.json. Publique ambos: ZIP do jogo e latest.json.'
    }
    $metaUrl = [string]$meta[0].browser_download_url
    if (-not (Release-Url-Valid $metaUrl 'latest.json')) {
        throw 'O endereco do manifesto nao pertence ao repositorio configurado.'
    }
    $manifest = Invoke-RestMethod -Uri $metaUrl -TimeoutSec 60
    $version = [string]$manifest.version
    if ($version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$' -or
        [string]$release.tag_name -cne "v$version") {
        throw 'A versao do manifesto nao corresponde a tag da release.'
    }
    $zipName = "DownhillRevived-$version-win.zip"
    $asset = @($release.assets | Where-Object { $_.name -ceq $zipName } | Select-Object -First 1)
    if ($asset.Count -ne 1) {
        throw "A release $version nao tem o arquivo $zipName."
    }
    $zipUrl = [string]$asset[0].browser_download_url
    if (-not (Release-Url-Valid $zipUrl $zipName) -or
        [string]$manifest.url -cne $zipUrl -or
        [string]$manifest.sha256 -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'latest.json esta inconsistente com o download oficial do repositorio.'
    }

    $temp = Join-Path ([IO.Path]::GetTempPath()) ('downhill-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $temp | Out-Null
    try {
        $zip = Join-Path $temp $zipName
        Write-Host "Baixando o executavel nativo da versao $version ..."
        Invoke-WebRequest -Uri $zipUrl -OutFile $zip -UseBasicParsing -TimeoutSec 240
        $hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -ne ([string]$manifest.sha256).ToLowerInvariant()) {
            throw 'O SHA-256 do ZIP nao confere. Nenhum executavel foi instalado.'
        }
        Write-Host 'Integridade verificada. Preparando os arquivos portateis ...'
        $extract = Join-Path $temp 'files'
        Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force
        $src = Join-Path $extract "DownhillRevived-$version"
        if (-not (Test-Path -LiteralPath (Join-Path $src 'dhd.exe') -PathType Leaf)) {
            throw 'A release nao tem dhd.exe no diretorio esperado.'
        }
        # Stage the executable before the final rename, so a cancelled download
        # cannot leave a partial dhd.exe that the next run would attempt to start.
        $stagedExe = Join-Path $target 'dhd.exe.new'
        Copy-Item -LiteralPath (Join-Path $src 'dhd.exe') -Destination $stagedExe -Force
        foreach ($name in @('README.txt', 'LICENSE.txt', 'THIRD-PARTY-NOTICES.txt')) {
            $from = Join-Path $src $name
            if (Test-Path -LiteralPath $from -PathType Leaf) {
                Copy-Item -LiteralPath $from -Destination (Join-Path $target $name) -Force
            }
        }
        Move-Item -LiteralPath $stagedExe -Destination $exe -Force
        Write-Host "Instalado em $target"
    } finally {
        if (Test-Path -LiteralPath $temp) {
            Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

try {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw 'O port so esta disponivel para Windows.'
    }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { Install-Native-Release }
    if ($CheckOnly) {
        Write-Host "Pronto: $exe"
        exit 0
    }
    if (-not (Test-Path -LiteralPath $ini -PathType Leaf)) {
        $settings = @(
            '; Downhill Port: o jogo pergunta a ISO USA na primeira execucao.'
            'renderer = native'
            'scale = 2'
            'window = borderless'
            'widescreen = 1'
            'DHD_ASPECT = wide'
            'auto_update = ask'
            "update_url = $releases/latest/download/latest.json"
        )
        [IO.File]::WriteAllLines($ini, [string[]]$settings, (New-Object System.Text.UTF8Encoding($false)))
    }
    Write-Host 'Abrindo o jogo. Na primeira execucao selecione sua ISO USA (SCUS-97177).'
    Start-Process -FilePath $exe -WorkingDirectory $target
} catch {
    $text = 'Nao foi possivel iniciar o Downhill Port: ' + $_.Exception.Message
    Write-Host $text -ForegroundColor Red
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [void][System.Windows.Forms.MessageBox]::Show(
            $text, 'Downhill Port', [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error)
    } catch { }
    exit 1
}
