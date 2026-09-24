param(
    [switch]$Help,
    [string]$RunDirectory,
    [string]$OutputDirectory,
    [string]$PlayerSaveDirectory
)

if ($Help) {
    Write-Output 'Usage: pwsh -File tools/perf/drive-city.ps1 -RunDirectory <game-build> -OutputDirectory <results> -PlayerSaveDirectory <player-save> [-Help]'
    Write-Output 'Launches and controls LostOdysseyRecomp.exe, writes logs/screenshots/summary and checks save metadata; review the local README before running.'
    return
}
if (-not $RunDirectory -or -not $OutputDirectory -or -not $PlayerSaveDirectory) {
    throw 'RunDirectory, OutputDirectory and PlayerSaveDirectory are required. Use -Help for usage.'
}
$ErrorActionPreference = 'Stop'
$run = (Resolve-Path -LiteralPath $RunDirectory).Path
if (-not (Test-Path -LiteralPath (Join-Path $run 'LostOdysseyRecomp.exe') -PathType Leaf)) {
    throw "Game executable not found in RunDirectory: $run"
}
$out = [System.IO.Path]::GetFullPath($OutputDirectory)
$playerSave = [System.IO.Path]::GetFullPath($PlayerSaveDirectory)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$shots = Join-Path $run 'shots2'
$logDir = Join-Path $env:TEMP 'lo-city-logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$env:LO_LOG_FILE = Join-Path $logDir ('runtime-{0}.log' -f [DateTime]::UtcNow.Ticks)
$controlDir = Join-Path (Join-Path $env:TEMP 'lo-city-control') ([System.IO.Path]::GetFileNameWithoutExtension($env:LO_LOG_FILE))
New-Item -ItemType Directory -Force -Path $controlDir | Out-Null
$inputPath = Join-Path $controlDir 'input.txt'
$shotRequestPath = Join-Path $controlDir 'shots.txt'
New-Item -ItemType Directory -Force -Path $shots | Out-Null
Set-Content -Path $inputPath -Value '1 0 0 0 0' -Encoding ascii
Set-Content -Path $shotRequestPath -Value '0 0' -Encoding ascii

$saveRoot = Join-Path $run 'save'
$origSaves = Get-ChildItem $saveRoot -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
    [pscustomobject]@{ Path = $_.FullName; Length = $_.Length; LastWriteTimeUtc = $_.LastWriteTimeUtc.ToString('o') }
}
$origPlayerSaves = Get-ChildItem $playerSave -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
    [pscustomobject]@{ Path = $_.FullName; Length = $_.Length; LastWriteTimeUtc = $_.LastWriteTimeUtc.ToString('o') }
}

# Proven Continue load from encounter-animation.md, plus extra A for Last Saved Game.
# Do not send Down: that opens 千年之梦. Last saved is user01 (second slot, city).
$env:LO_BACKGROUND = '1'
$env:LO_AUDIO_MUTE = '1'
$env:LO_RENDER_TIMING = '1'
$env:LO_GPU_STATS = '1'
$env:LO_FRAME_TIMING = '1'
$env:LO_TEST_INPUT_FILE = $inputPath
$env:LO_SCREENSHOT_REQUEST = $shotRequestPath
$env:LO_SCREENSHOT_PATH = Join-Path $shots 'shot.ppm'
$env:LO_AUTO_BUTTONS = 's@120,a@240,a@360,a@480,a@700,a@900'
$env:LO_AUTO_PULSE = '6'
$env:LO_AUTO_STICK = '0,28000,1400,2800'
Remove-Item Env:LO_SCREENSHOT_EVERY -ErrorAction SilentlyContinue
Remove-Item Env:LO_SCREENSHOT_SWAP -ErrorAction SilentlyContinue

$beforeLogs = @(Get-ChildItem $logDir -Filter 'runtime-*.log' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
$proc = Start-Process -FilePath (Join-Path $run 'LostOdysseyRecomp.exe') -WorkingDirectory $run -WindowStyle Hidden -PassThru -ArgumentList '--quiet-kernel'
$proc.Id | Set-Content (Join-Path $run 'pid.txt')
$started = Get-Date
$statusPath = Join-Path $out 'drive-status.json'
function Write-Status($phase, $extra = @{}) {
    $obj = [ordered]@{
        phase = $phase
        pid = $proc.Id
        elapsed_s = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
        extra = $extra
        time = (Get-Date).ToString('s')
    }
    ($obj | ConvertTo-Json -Compress) | Set-Content $statusPath -Encoding utf8
}
Write-Status 'launched'

$logPath = $null
for ($i = 0; $i -lt 80; $i++) {
    Start-Sleep -Milliseconds 400
    $logPath = Get-ChildItem $logDir -Filter 'runtime-*.log' -ErrorAction SilentlyContinue |
        Where-Object { $beforeLogs -notcontains $_.Name } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
    if ($logPath) { break }
}
if (-not $logPath) { throw 'runtime log not created' }
Write-Status 'log' @{ log = $logPath }

$shotSerial = 1
$lastDraws = 0
$lastSwap = 0
$lastShotSwap = -1
$highDrawStreak = 0
$phase = 'boot'
$cityStart = $null
$walked = $false
$serial = 2

function Get-LogTail($path, $maxBytes = 65536) {
    if (-not $path -or -not (Test-Path $path)) { return @() }
    try {
        $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $len = $fs.Length
            if ($len -le 0) { return @() }
            $start = [Math]::Max([int64]0, $len - $maxBytes)
            [void]$fs.Seek($start, [System.IO.SeekOrigin]::Begin)
            $buf = New-Object byte[] ($len - $start)
            $n = $fs.Read($buf, 0, $buf.Length)
            $text = [System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
            return $text -split "`r?`n"
        } finally { $fs.Dispose() }
    } catch { return @() }
}
function Request-Shot($swapHint) {
    if ($swapHint -lt 0) { return }
    if ($swapHint -eq $script:lastShotSwap) { return }
    $script:shotSerial++
    Set-Content -Path $shotRequestPath -Value ('{0} 1' -f $script:shotSerial) -Encoding ascii
    $script:lastShotSwap = $swapHint
}
function Send-Input($mask, $x, $y, $polls) {
    $script:serial++
    $line = '{0} {1:x} {2} {3} {4}' -f $script:serial, $mask, $x, $y, $polls
    Set-Content -Path $inputPath -Value $line -Encoding ascii
    return $line
}

$shotMarks = @(200, 400, 600, 800, 1000)
$deadline = $started.AddMinutes(5)
try {
    while ((Get-Date) -lt $deadline) {
        if ($proc.HasExited) {
            Write-Status 'exited' @{ code = $proc.ExitCode }
            break
        }
        if ($phase -eq 'city_hold') {
            $held = ((Get-Date) - $cityStart).TotalSeconds
            Write-Status $phase @{
                swap = $lastSwap
                draws = $lastDraws
                walked = $walked
                log = $logPath
            }
            if ($held -ge 28) { $phase = 'done'; break }
            Start-Sleep -Milliseconds 600
            continue
        }
        $tail = Get-LogTail $logPath
        $drawLine = $tail | Where-Object { $_ -match 'frame (\d+) stats: draws=(\d+)' } | Select-Object -Last 1
        $timingLine = $tail | Where-Object { $_ -match 'render timing frame=(\d+) draws=(\d+)' } | Select-Object -Last 1
        if ($drawLine -match 'frame (\d+) stats: draws=(\d+)') {
            $lastSwap = [int]$Matches[1]
            $lastDraws = [int]$Matches[2]
        } elseif ($timingLine -match 'render timing frame=(\d+) draws=(\d+)') {
            $lastSwap = [int]$Matches[1]
            $lastDraws = [int]$Matches[2]
        }

        foreach ($mark in $shotMarks) {
            if ($lastSwap -ge $mark -and $lastShotSwap -lt $mark) { Request-Shot $lastSwap }
        }

        if ($lastDraws -ge 800) { $highDrawStreak++ } else { $highDrawStreak = 0 }

        if ($phase -eq 'boot' -and $lastSwap -ge 1) { $phase = 'load' }
        if ($highDrawStreak -ge 3 -and $phase -ne 'city_hold' -and $phase -ne 'done') {
            if (-not $walked) {
                $null = Send-Input 0 0 28000 2400
                $walked = $true
            }
            $cityStart = Get-Date
            $phase = 'city_hold'
            Request-Shot $lastSwap
        }
        if ($phase -eq 'city_hold') {
            $held = ((Get-Date) - $cityStart).TotalSeconds
            if ($held -ge 28) { $phase = 'done'; break }
        }

        Write-Status $phase @{
            swap = $lastSwap
            draws = $lastDraws
            walked = $walked
            log = $logPath
        }
        Start-Sleep -Milliseconds 600
    }
}
finally {
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 800
        if (-not $proc.HasExited) { try { $proc.Kill() } catch {} }
    }
}

$afterSaves = Get-ChildItem $saveRoot -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
    [pscustomobject]@{ Path = $_.FullName; Length = $_.Length; LastWriteTimeUtc = $_.LastWriteTimeUtc.ToString('o') }
}
$afterPlayerSaves = Get-ChildItem $playerSave -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
    [pscustomobject]@{ Path = $_.FullName; Length = $_.Length; LastWriteTimeUtc = $_.LastWriteTimeUtc.ToString('o') }
}
$saveChanged = Compare-Object $origSaves $afterSaves -Property Path, Length, LastWriteTimeUtc
$playerSavesChanged = Compare-Object $origPlayerSaves $afterPlayerSaves -Property Path, Length, LastWriteTimeUtc

$city = @(); $menu = @(); $batch = @()
Select-String -Path $logPath -Pattern 'render timing frame=(\d+) draws=(\d+).*draw_ms=([0-9.]+).*vertex_ms=([0-9.]+).*bind_ms=([0-9.]+).*record_ms=([0-9.]+).*fence_wait_ms=([0-9.]+).*rt_acquire_ms=([0-9.]+).*taa_ms=([0-9.]+).*nested_flush_ms=([0-9.]+).*shader_lookup_ms=([0-9.]+).*pipeline_lookup_ms=([0-9.]+).*scene_copy_ms=([0-9.]+).*gpu_queue_batches_elapsed_ms=([0-9.unknown]+).*gpu_batches=(\d+)' | ForEach-Object {
    $d = [int]$_.Matches[0].Groups[2].Value
    $row = [pscustomobject]@{
        frame = [int]$_.Matches[0].Groups[1].Value
        draws = $d
        draw_ms = [double]$_.Matches[0].Groups[3].Value
        vertex_ms = [double]$_.Matches[0].Groups[4].Value
        bind_ms = [double]$_.Matches[0].Groups[5].Value
        record_ms = [double]$_.Matches[0].Groups[6].Value
        fence_wait_ms = [double]$_.Matches[0].Groups[7].Value
        rt_acquire_ms = [double]$_.Matches[0].Groups[8].Value
        taa_ms = [double]$_.Matches[0].Groups[9].Value
        nested_flush_ms = [double]$_.Matches[0].Groups[10].Value
        shader_lookup_ms = [double]$_.Matches[0].Groups[11].Value
        pipeline_lookup_ms = [double]$_.Matches[0].Groups[12].Value
        scene_copy_ms = [double]$_.Matches[0].Groups[13].Value
        gpu_queue = $_.Matches[0].Groups[14].Value
        gpu_batches = [int]$_.Matches[0].Groups[15].Value
    }
    if ($d -ge 800) { $city += $row }
    elseif ($d -ge 70 -and $d -le 250) { $menu += $row }
}
Select-String -Path $logPath -Pattern 'render batch capacity frame=(\d+) limit=(\d+) descriptor_splits=(\d+) upload_splits=(\d+) arena_splits=(\d+) descriptor_hits=(\d+) descriptor_misses=(\d+)' | ForEach-Object {
    $batch += [pscustomobject]@{
        frame = [int]$_.Matches[0].Groups[1].Value
        limit = [int]$_.Matches[0].Groups[2].Value
        splits = [int]$_.Matches[0].Groups[3].Value
        upload = [int]$_.Matches[0].Groups[4].Value
        arena = [int]$_.Matches[0].Groups[5].Value
        hits = [int]$_.Matches[0].Groups[6].Value
        misses = [int]$_.Matches[0].Groups[7].Value
    }
}
function Avg($arr, $prop) {
    if (-not $arr -or $arr.Count -eq 0) { return $null }
    [math]::Round((($arr | Measure-Object $prop -Average).Average), 3)
}
function MaxOf($arr, $prop) {
    if (-not $arr -or $arr.Count -eq 0) { return $null }
    [math]::Round((($arr | Measure-Object $prop -Maximum).Maximum), 3)
}
function Percentile($arr, $prop, $p) {
    if (-not $arr -or $arr.Count -eq 0) { return $null }
    $sorted = @($arr | ForEach-Object { $_.$prop } | Sort-Object)
    $idx = [math]::Min($sorted.Count - 1, [math]::Max(0, [math]::Ceiling($p * $sorted.Count) - 1))
    [math]::Round($sorted[$idx], 3)
}
function OverBudget($arr, $prop, $limit) {
    if (-not $arr -or $arr.Count -eq 0) { return 0 }
    @($arr | Where-Object { $_.$prop -gt $limit }).Count
}
$cityBatch = @()
if ($city.Count -gt 0) {
    $cityFrames = $city.frame
    $cityBatch = $batch | Where-Object { $cityFrames -contains $_.frame }
}
$summary = [ordered]@{
    pid = $proc.Id
    log = $logPath
    control_dir = $controlDir
    phase_end = $phase
    elapsed_s = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
    original_saves_changed = [bool]$saveChanged
    player_saves_changed = [bool]$playerSavesChanged
    city_frames = $city.Count
    menu_frames = $menu.Count
    city = @{
        draws = (Avg $city 'draws')
        draw_ms = (Avg $city 'draw_ms')
        draw_ms_p95 = (Percentile $city 'draw_ms' 0.95)
        draw_ms_p99 = (Percentile $city 'draw_ms' 0.99)
        draw_ms_max = (MaxOf $city 'draw_ms')
        draw_ms_over_16_67 = (OverBudget $city 'draw_ms' 16.67)
        vertex_ms = (Avg $city 'vertex_ms')
        bind_ms = (Avg $city 'bind_ms')
        record_ms = (Avg $city 'record_ms')
        fence_wait_ms = (Avg $city 'fence_wait_ms')
        rt_acquire_ms = (Avg $city 'rt_acquire_ms')
        rt_acquire_ms_p95 = (Percentile $city 'rt_acquire_ms' 0.95)
        rt_acquire_ms_p99 = (Percentile $city 'rt_acquire_ms' 0.99)
        rt_acquire_ms_max = (MaxOf $city 'rt_acquire_ms')
        taa_ms = (Avg $city 'taa_ms')
        taa_ms_p95 = (Percentile $city 'taa_ms' 0.95)
        taa_ms_p99 = (Percentile $city 'taa_ms' 0.99)
        taa_ms_max = (MaxOf $city 'taa_ms')
        nested_flush_ms = (Avg $city 'nested_flush_ms')
        nested_flush_ms_p95 = (Percentile $city 'nested_flush_ms' 0.95)
        nested_flush_ms_p99 = (Percentile $city 'nested_flush_ms' 0.99)
        nested_flush_ms_max = (MaxOf $city 'nested_flush_ms')
        shader_lookup_ms = (Avg $city 'shader_lookup_ms')
        pipeline_lookup_ms = (Avg $city 'pipeline_lookup_ms')
        scene_copy_ms = (Avg $city 'scene_copy_ms')
        gpu_batches = (Avg $city 'gpu_batches')
        splits = (Avg $cityBatch 'splits')
        upload_splits = (Avg $cityBatch 'upload')
        arena_splits = (Avg $cityBatch 'arena')
        hits = (Avg $cityBatch 'hits')
        misses = (Avg $cityBatch 'misses')
    }
    menu = @{
        draws = (Avg $menu 'draws')
        draw_ms = (Avg $menu 'draw_ms')
        fence_wait_ms = (Avg $menu 'fence_wait_ms')
        gpu_batches = (Avg $menu 'gpu_batches')
    }
    last_swap = $lastSwap
    last_draws = $lastDraws
}
$summary | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $out 'drive-summary.json') -Encoding utf8
Write-Status 'finished' $summary
Write-Output ($summary | ConvertTo-Json -Depth 5)
$classifier = Join-Path $PSScriptRoot 'classify-city-timing.ps1'
if (Test-Path -LiteralPath $classifier) {
    Write-Output '--- classify-city-timing ---'
    & $classifier -LogPath $logPath
}
