param(
    [string]$RunDir = 'D:\Mihoyo\LostOdysseyRecomp-windows-x64',
    [ValidateRange(1, 65535)][int]$Port = 8769,
    [ValidatePattern('^[^\\/:*?"<>|]+\.exe$')][string]$ExeName = 'LostOdysseyRecomp-debug.exe',
    [switch]$NoAutoContinue
)
$ErrorActionPreference = 'Stop'
$panelUrl = "http://127.0.0.1:$Port"
$connection = [System.Net.Sockets.TcpClient]::new()
try {
    $connect = $connection.ConnectAsync('127.0.0.1', $Port)
    $listening = $connect.Wait(500) -and $connection.Connected
} catch {
    $listening = $false
} finally {
    $connection.Dispose()
}
if ($listening) {
    try {
        $existing = Invoke-RestMethod -Uri "$panelUrl/api/state" -TimeoutSec 2
        if ($existing.service -eq 'lorecomp-taa-debug' -and $existing.can_relaunch -eq $true -and $existing.game.status -eq 'exited') {
            $null = Invoke-RestMethod -Uri "$panelUrl/api/launch" -Method Post -ContentType 'application/json' -Body '{}' -TimeoutSec 5
            Write-Host 'Restarted the exited game using the existing panel launch settings.'
        } else {
            Write-Host "Port $Port is already in use. No additional game or server was started."
        }
    } catch {
        Write-Host "Existing service was left unchanged: $($_.Exception.Message)"
    }
    Start-Process $panelUrl
    return
}
$resolvedRunDir = (Resolve-Path -LiteralPath $RunDir).Path
$gamePath = Join-Path $resolvedRunDir $ExeName
if (-not (Test-Path -LiteralPath $gamePath -PathType Leaf)) {
    throw "Debug game executable not found: $gamePath"
}
$pythonCommand = Get-Command python -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $pythonCommand) {
    $pythonCommand = Get-Command python3 -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $pythonCommand) { throw 'Python 3 is required and must be available on PATH.' }
$controlDir = Join-Path $resolvedRunDir 'taa-debug'
New-Item -ItemType Directory -Path $controlDir -Force | Out-Null
$serverScript = Join-Path $PSScriptRoot 'taa_debug.py'
# Explicit quotes preserve paths with spaces in Start-Process native arguments.
$serverArgs = @(
    ('"{0}"' -f $serverScript),
    '--run-dir', ('"{0}"' -f $resolvedRunDir),
    '--control-dir', ('"{0}"' -f $controlDir),
    '--port', "$Port", '--launch', '--exe', $ExeName
)
if (-not $NoAutoContinue) { $serverArgs += '--native-continue' }
$server = Start-Process -FilePath $pythonCommand.Source -ArgumentList $serverArgs -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $controlDir 'panel.stdout.log') `
    -RedirectStandardError (Join-Path $controlDir 'panel.stderr.log')
$ready = $false
for ($attempt = 0; $attempt -lt 40; $attempt++) {
    if ($server.HasExited) { throw "Debug panel exited. See $controlDir\panel.stderr.log" }
    try {
        $null = Invoke-RestMethod -Uri "$panelUrl/api/state" -TimeoutSec 1
        $ready = $true
        break
    } catch { Start-Sleep -Milliseconds 250 }
}
if (-not $ready) { throw "Debug panel did not respond. See $controlDir\panel.stderr.log" }
Write-Host "Debug panel: $panelUrl (server PID $($server.Id))"
Start-Process $panelUrl
