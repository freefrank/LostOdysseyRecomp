param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$BaselineDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$GameDirectory,
    [ValidateRange(10,600)][int]$Seconds = 120,
    [switch]$DisableFg,
    [ValidateSet('Settings','Legacy','Off','Dlss','Fsr')][string]$FgProvider = 'Legacy',
    [ValidateSet('Fixed','Dynamic')][string]$FgMode = 'Fixed',
    [ValidateRange(2,16)][int]$FgMultiplier = 2,
    [ValidateRange(0,1000)][double]$FgTargetFps = 0,
    [switch]$DisableObjectMotion,
    [switch]$DisableHybridMotion,
    [switch]$WindowCycle,
    [switch]$HiddenResizeCycle,
    [ValidateSet('Baseline','D3D12','Vulkan')][string]$Backend = 'Baseline',
    [ValidateSet('Baseline','Off','Dlss','Fsr')][string]$Upscaler = 'Baseline',
    [ValidateRange(-1,3)][int]$Quality = -1,
    [ValidateSet('Diagnostic','Lightweight')][string]$CaptureMode = 'Diagnostic',
    [switch]$Background,
    [switch]$CaptureScreenshots,
    [string]$ValidationLayerDirectory,
    [string]$InputRequestPath
)
# ACTIVE GAME DRIVER: isolated profile/save/config copies, optional hidden gameplay,
# muted audio, bounded automated input, then closes only its own game process.
$ErrorActionPreference = 'Stop'
if ($Background -and $WindowCycle) { throw 'WindowCycle requires foreground interaction.' }
if ($HiddenResizeCycle -and !$Background) { throw 'HiddenResizeCycle requires Background.' }
if ($HiddenResizeCycle -and $WindowCycle) { throw 'HiddenResizeCycle and WindowCycle are mutually exclusive.' }
if ($DisableFg -and $FgProvider -notin @('Legacy','Off')) { throw 'DisableFg conflicts with the selected FG provider.' }
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$baseline = (Resolve-Path -LiteralPath $BaselineDirectory).Path
$game = (Resolve-Path -LiteralPath $GameDirectory).Path
$validationLayer = if ($ValidationLayerDirectory) { (Resolve-Path -LiteralPath $ValidationLayerDirectory).Path } else { $null }
if ($validationLayer -and !(Test-Path -LiteralPath (Join-Path $validationLayer 'VkLayer_khronos_validation.json'))) {
    throw 'ValidationLayerDirectory lacks VkLayer_khronos_validation.json'
}
$run = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $run) { throw 'OutputDirectory must be new; evidence is never overwritten.' }
New-Item -ItemType Directory -Path $run | Out-Null
foreach ($name in @('settings.ini','save','profile','shaders')) {
    $source = Join-Path $baseline $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $run -Recurse }
}
foreach ($file in Get-ChildItem -LiteralPath $build -File) {
    if ($file.Extension -eq '.dll' -or $file.Name -in @('LostOdysseyRecomp.exe','source-version.txt','LostOdysseyRecomp.exe.build.json')) {
        Copy-Item -LiteralPath $file.FullName -Destination $run
    }
}
# Overrides apply only to the isolated copy, never to the baseline.
$settingsPath = Join-Path $run 'settings.ini'
$settingsText = Get-Content -LiteralPath $settingsPath -Raw
$overrides = @{}
if ($Backend -ne 'Baseline') { $overrides['graphics_backend'] = $(if ($Backend -eq 'D3D12') { 0 } else { 1 }) }
if ($Upscaler -ne 'Baseline') { $overrides['upscaler'] = @{Off=0; Dlss=1; Fsr=2}[$Upscaler] }
if ($Quality -ge 0) { $overrides['dlss_quality'] = $Quality; $overrides['fsr_quality'] = $Quality }
foreach ($entry in $overrides.GetEnumerator()) {
    $pattern = '(?m)^' + [regex]::Escape($entry.Key) + '=.*$'
    $line = $entry.Key + '=' + $entry.Value
    if ([regex]::IsMatch($settingsText, $pattern)) { $settingsText = [regex]::Replace($settingsText, $pattern, $line) }
    else { $settingsText += "`n$line`n" }
}
if ($overrides.Count) { [IO.File]::WriteAllText($settingsPath, $settingsText) }
$exe = Join-Path $run 'LostOdysseyRecomp.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'BuildDirectory lacks LostOdysseyRecomp.exe' }
function BaselineMetadata {
    foreach ($name in @('settings.ini','save','profile')) {
        Get-ChildItem -LiteralPath (Join-Path $baseline $name) -File -Recurse -ErrorAction SilentlyContinue |
            ForEach-Object { [ordered]@{ path=$_.FullName; length=$_.Length; modified=$_.LastWriteTimeUtc.ToString('o') } }
    }
}
$before = @(BaselineMetadata)
$before | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $run 'baseline-before.json')
# Child-specific environment: the caller's LO_* and validation settings cannot
# silently change the experiment. Other normal Windows environment is inherited.
$start = [Diagnostics.ProcessStartInfo]::new($exe)
$start.WorkingDirectory = $run
$start.UseShellExecute = $false
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.CreateNoWindow = [bool]$Background
foreach ($name in @($start.Environment.Keys)) {
    if ($name.StartsWith('LO_') -or $name.StartsWith('VK_LAYER') -or $name.StartsWith('VK_INSTANCE_LAYERS')) {
        $start.Environment.Remove($name) | Out-Null
    }
}
$start.ArgumentList.Add('--game'); $start.ArgumentList.Add($game); $start.ArgumentList.Add('--quiet-kernel')
if ($FgProvider -ne 'Settings') { $start.Environment['LO_DLSS_FG'] = $(if ($DisableFg) { '0' } else { '1' }) }
if ($FgProvider -notin @('Settings','Legacy')) {
    $start.Environment['LO_FG_PROVIDER'] = $FgProvider.ToLowerInvariant()
    $start.Environment['LO_FG_MODE'] = $FgMode.ToLowerInvariant()
    $start.Environment['LO_FG_MULTIPLIER'] = $FgMultiplier.ToString()
    $start.Environment['LO_FG_TARGET_FPS'] = $FgTargetFps.ToString([Globalization.CultureInfo]::InvariantCulture)
}
if ($CaptureMode -eq 'Diagnostic') { $start.Environment['LO_MV_LOG'] = '1' }
if ($DisableObjectMotion) { $start.Environment['LO_MV_REPLAY'] = '0' }
if ($DisableHybridMotion) { $start.Environment['LO_SR_HYBRID_MV'] = '0' }
$start.Environment['LO_AUDIO_MUTE'] = '1'
if ($validationLayer) {
    $start.Environment['VK_LAYER_PATH'] = $validationLayer
    $start.Environment['VK_INSTANCE_LAYERS'] = 'VK_LAYER_KHRONOS_validation'
    $start.Environment['VK_LAYER_SETTINGS_PATH'] = $run
    # Event-only object attribution; no extra GPU waits or image-content scans.
    $start.Environment['LO_VK_OBJECT_TRACE'] = '1'
    @(
        'khronos_validation.validate_sync = true'
        'khronos_validation.report_flags = error,warn,info'
        'khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG'
        ('khronos_validation.log_filename = ' + (Join-Path $run 'validation.log'))
    ) | Set-Content -LiteralPath (Join-Path $run 'vk_layer_settings.txt')
}
if ($Background) { $start.Environment['LO_BACKGROUND'] = '1' }
if ($CaptureScreenshots) {
    $start.Environment['LO_SCREENSHOT_REQUEST'] = Join-Path $run 'screenshot-request.txt'
    $start.Environment['LO_SCREENSHOT_PATH'] = Join-Path $run 'scene.ppm'
}
$start.Environment['LO_LOG_FILE'] = Join-Path $run 'runtime.log'
$start.Environment['LO_SHADER_CACHE_DIR'] = Join-Path $run 'shader-cache'
$start.Environment['LO_AUTO_BUTTONS'] = 's@120,a@240,a@360,a@480,a@700,a@900'
$start.Environment['LO_AUTO_PULSE'] = '6'
if ($InputRequestPath) { $start.Environment['LO_TEST_INPUT_FILE'] = [IO.Path]::GetFullPath($InputRequestPath) }
if ($CaptureMode -eq 'Diagnostic') { $start.Environment['LO_RENDER_TIMING'] = '1' }
$start.Environment['LO_AUTO_STICK'] = '0,18000,1600,1900'
$process = [Diagnostics.Process]::Start($start)
$stdout = $process.StandardOutput.ReadToEndAsync()
$stderr = $process.StandardError.ReadToEndAsync()
$manifest = [ordered]@{ pid=$process.Id; started=[DateTime]::UtcNow.ToString('o'); exe=$exe;
    sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
    fg=$(if ($FgProvider -eq 'Settings') { $null } else { !$DisableFg -and $FgProvider -ne 'Off' });
    fg_provider=$FgProvider; fg_mode=$FgMode; fg_multiplier=$FgMultiplier; fg_target_fps=$FgTargetFps;
    input_request_path=$InputRequestPath;
    foreground=!$Background; muted=$true; object_motion=!$DisableObjectMotion;
    hybrid_motion=!$DisableHybridMotion; hidden_resize_cycle=[bool]$HiddenResizeCycle;
    validation_layer_directory=$validationLayer; synchronization_validation_requested=[bool]$validationLayer;
    screenshot_requests=[bool]$CaptureScreenshots;
    backend=$Backend; upscaler=$Upscaler; quality=$Quality; capture_mode=$CaptureMode;
    render_timing=($CaptureMode -eq 'Diagnostic'); mv_log=($CaptureMode -eq 'Diagnostic');
    game=$game; baseline=$baseline; seconds=$Seconds }
$manifest | ConvertTo-Json | Set-Content (Join-Path $run 'run.json')
# A hidden parent terminal can leave the SDL window hidden even when
# AppActivate reports success. Restore the actual game HWND before sampling.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FgGameWindow {
    public delegate bool EnumWindowCallback(IntPtr window, IntPtr state);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowCallback callback, IntPtr state);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr w, IntPtr l);
    public static bool CloseOwnedWindows(uint pid) {
        bool posted = false;
        EnumWindows((window, state) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == pid) posted |= PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
        return posted;
    }
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window, int command);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
'@
$focusTimer = [Diagnostics.Stopwatch]::StartNew()
while (!$Background -and !$process.HasExited -and $focusTimer.Elapsed.TotalSeconds -lt 10) {
    $process.Refresh()
    if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
        [void][FgGameWindow]::ShowWindow($process.MainWindowHandle, 9)
        [void][FgGameWindow]::SetForegroundWindow($process.MainWindowHandle)
        if ([FgGameWindow]::GetForegroundWindow() -ne $process.MainWindowHandle) {
            try {
                [FgGameWindow]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
                [void][FgGameWindow]::SetForegroundWindow($process.MainWindowHandle)
            } finally {
                [FgGameWindow]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
            }
        }
        break
    }
    Start-Sleep -Milliseconds 100
}
[ordered]@{ visible_window=$process.MainWindowHandle.ToInt64();
    foreground=([FgGameWindow]::GetForegroundWindow() -eq $process.MainWindowHandle) } |
    ConvertTo-Json | Set-Content (Join-Path $run 'window-focus.json')
if ($HiddenResizeCycle) {
    # SDL registers its game windows as SDL_app. MainWindowHandle is zero while hidden.
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class FgGameHiddenResize {
    public delegate bool EnumWindowCallback(IntPtr window, IntPtr state);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    public sealed class WindowState {
        public string TimeUtc { get; set; }
        public long Hwnd { get; set; }
        public uint Pid { get; set; }
        public string ClassName { get; set; }
        public bool IsWindow { get; set; }
        public int ClientWidth { get; set; }
        public int ClientHeight { get; set; }
        public int WindowLeft { get; set; }
        public int WindowTop { get; set; }
        public int WindowWidth { get; set; }
        public int WindowHeight { get; set; }
        public long ForegroundHwnd { get; set; }
        public uint ForegroundPid { get; set; }
    }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowCallback callback, IntPtr state);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr window, StringBuilder name, int maxCount);
    [DllImport("user32.dll")] static extern bool IsWindow(IntPtr window);
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window, out Rect rect);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError = true)] static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    static string WindowClass(IntPtr window) {
        var name = new StringBuilder(256);
        return GetClassName(window, name, name.Capacity) > 0 ? name.ToString() : "";
    }
    public static IntPtr FindOwnedSdl(uint pid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, state) => {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (IsWindow(window) && owner == pid && WindowClass(window) == "SDL_app") {
                found = window;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static bool IsOwnedSdl(IntPtr window, uint pid) {
        if (window == IntPtr.Zero || !IsWindow(window) || WindowClass(window) != "SDL_app") return false;
        uint owner;
        GetWindowThreadProcessId(window, out owner);
        return owner == pid;
    }
    public static WindowState Snapshot(IntPtr window) {
        var result = new WindowState { TimeUtc = DateTime.UtcNow.ToString("o"), Hwnd = window.ToInt64() };
        var foreground = GetForegroundWindow();
        result.ForegroundHwnd = foreground.ToInt64();
        uint foregroundPid;
        GetWindowThreadProcessId(foreground, out foregroundPid);
        result.ForegroundPid = foregroundPid;
        result.IsWindow = window != IntPtr.Zero && IsWindow(window);
        if (result.IsWindow) {
            uint pid;
            GetWindowThreadProcessId(window, out pid);
            result.Pid = pid;
            result.ClassName = WindowClass(window);
            Rect client, outer;
            if (GetClientRect(window, out client)) {
                result.ClientWidth = client.Right - client.Left;
                result.ClientHeight = client.Bottom - client.Top;
            }
            if (GetWindowRect(window, out outer)) {
                result.WindowLeft = outer.Left;
                result.WindowTop = outer.Top;
                result.WindowWidth = outer.Right - outer.Left;
                result.WindowHeight = outer.Bottom - outer.Top;
            }
        }
        return result;
    }
    public static bool SetClientSize(IntPtr window, uint pid, int width, int height) {
        if (!IsOwnedSdl(window, pid)) return false;
        Rect client, outer;
        if (!GetClientRect(window, out client) || !GetWindowRect(window, out outer)) return false;
        int outerWidth = outer.Right - outer.Left + width - (client.Right - client.Left);
        int outerHeight = outer.Bottom - outer.Top + height - (client.Bottom - client.Top);
        const uint flags = 0x0002 | 0x0004 | 0x0010 | 0x0200; // NOMOVE | NOZORDER | NOACTIVATE | NOOWNERZORDER
        return SetWindowPos(window, IntPtr.Zero, 0, 0, outerWidth, outerHeight, flags);
    }
}
'@
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $events = @()
    $resizeWindow = [IntPtr]::Zero
    $originalWidth = 0
    $originalHeight = 0
    $cycle = 0
    while (!($finished = $process.WaitForExit(250)) -and $timer.Elapsed.TotalSeconds -lt $Seconds) {
        if ($cycle -eq 0 -and $timer.Elapsed.TotalSeconds -ge 35) {
            $resizeWindow = [FgGameHiddenResize]::FindOwnedSdl([uint32]$process.Id)
            $beforeSize = [FgGameHiddenResize]::Snapshot($resizeWindow)
            $originalWidth = $beforeSize.ClientWidth
            $originalHeight = $beforeSize.ClientHeight
            $targetWidth = [Math]::Max(640, [int][Math]::Round($originalWidth * 0.8))
            $targetHeight = [Math]::Max(360, [int][Math]::Round($originalHeight * 0.8))
            $reason = $null
            $sent = $false
            if (![FgGameHiddenResize]::IsOwnedSdl($resizeWindow, [uint32]$process.Id)) { $reason = 'owned_sdl_window_unavailable' }
            elseif ($originalWidth -le 0 -or $originalHeight -le 0) { $reason = 'client_size_unavailable' }
            elseif ($targetWidth -eq $originalWidth -and $targetHeight -eq $originalHeight) { $reason = 'target_equals_original' }
            else { $sent = [FgGameHiddenResize]::SetClientSize($resizeWindow, [uint32]$process.Id, $targetWidth, $targetHeight) }
            $afterSize = [FgGameHiddenResize]::Snapshot($resizeWindow)
            $changed = $afterSize.ClientWidth -ne $beforeSize.ClientWidth -or $afterSize.ClientHeight -ne $beforeSize.ClientHeight
            $reached = $afterSize.ClientWidth -eq $targetWidth -and $afterSize.ClientHeight -eq $targetHeight
            if (!$reason -and !$sent) { $reason = 'set_window_pos_failed_or_window_changed' }
            elseif (!$reason -and (!$changed -or !$reached)) { $reason = 'client_size_did_not_reach_target' }
            $events += [ordered]@{ action='resize'; seconds=$timer.Elapsed.TotalSeconds; target=@{ width=$targetWidth; height=$targetHeight };
                set_window_pos=$sent; size_changed=$changed; target_reached=$reached; reason=$reason;
                before=$beforeSize; after=$afterSize }
            $events | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $run 'hidden-resize-cycle.json')
            $cycle = 1
        }
        if ($cycle -eq 1 -and $timer.Elapsed.TotalSeconds -ge 50) {
            $beforeSize = [FgGameHiddenResize]::Snapshot($resizeWindow)
            $reason = $null
            $sent = $false
            if (![FgGameHiddenResize]::IsOwnedSdl($resizeWindow, [uint32]$process.Id)) { $reason = 'owned_sdl_window_unavailable' }
            elseif ($originalWidth -le 0 -or $originalHeight -le 0) { $reason = 'original_client_size_unavailable' }
            else { $sent = [FgGameHiddenResize]::SetClientSize($resizeWindow, [uint32]$process.Id, $originalWidth, $originalHeight) }
            $afterSize = [FgGameHiddenResize]::Snapshot($resizeWindow)
            $changed = $afterSize.ClientWidth -ne $beforeSize.ClientWidth -or $afterSize.ClientHeight -ne $beforeSize.ClientHeight
            $reached = $afterSize.ClientWidth -eq $originalWidth -and $afterSize.ClientHeight -eq $originalHeight
            if (!$reason -and !$sent) { $reason = 'set_window_pos_failed_or_window_changed' }
            elseif (!$reason -and (!$changed -or !$reached)) { $reason = 'client_size_did_not_restore' }
            $events += [ordered]@{ action='restore'; seconds=$timer.Elapsed.TotalSeconds; target=@{ width=$originalWidth; height=$originalHeight };
                set_window_pos=$sent; size_changed=$changed; target_reached=$reached; reason=$reason;
                before=$beforeSize; after=$afterSize }
            $events | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $run 'hidden-resize-cycle.json')
            $cycle = 2
        }
    }
    if (!$events.Count) {
        [ordered]@{ reason=$(if ($finished) { 'process_exited_before_resize' } else { 'duration_before_resize' });
            elapsed_seconds=$timer.Elapsed.TotalSeconds; pid=$process.Id; time_utc=[DateTime]::UtcNow.ToString('o') } |
            ConvertTo-Json | Set-Content (Join-Path $run 'hidden-resize-cycle.json')
    }
} elseif ($WindowCycle) {
    $shell = New-Object -ComObject WScript.Shell
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FgGameKeys {
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
'@
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $cycle = 0
    $events = @()
    while (!($finished = $process.WaitForExit(500)) -and $timer.Elapsed.TotalSeconds -lt $Seconds) {
        if ($cycle -lt 2 -and $timer.Elapsed.TotalSeconds -ge (30 + 15 * $cycle)) {
            $activated = $shell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 150
            [uint32]$foregroundProcess = 0
            [void][FgGameKeys]::GetWindowThreadProcessId([FgGameKeys]::GetForegroundWindow(), [ref]$foregroundProcess)
            $activated = $activated -and $foregroundProcess -eq $process.Id
            if ($activated) {
                try {
                    [FgGameKeys]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
                    Start-Sleep -Milliseconds 150
                    [FgGameKeys]::keybd_event(0x0D, 0, 0, [UIntPtr]::Zero)
                    Start-Sleep -Milliseconds 150
                } finally {
                    [FgGameKeys]::keybd_event(0x0D, 0, 2, [UIntPtr]::Zero)
                    [FgGameKeys]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
                }
            }
            $events += [ordered]@{ seconds=$timer.Elapsed.TotalSeconds; activated=$activated; action='Alt+Enter'; pid=$process.Id }
            $events | ConvertTo-Json | Set-Content (Join-Path $run 'window-cycle.json')
            ++$cycle
        }
    }
} else {
    $finished = $process.WaitForExit($Seconds * 1000)
}
$closed = $false
if (!$finished) {
    $closed = if ($Background) { [FgGameWindow]::CloseOwnedWindows([uint32]$process.Id) } else { $process.CloseMainWindow() }
    $finished = $process.WaitForExit(10000)
    if (!$finished) { $process.Kill(); $process.WaitForExit() }
}
$stdout.Result | Set-Content (Join-Path $run 'stdout.log')
$stderr.Result | Set-Content (Join-Path $run 'stderr.log')
$after = @(BaselineMetadata)
$after | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $run 'baseline-after.json')
$preserved = ($before | ConvertTo-Json -Depth 3 -Compress) -eq ($after | ConvertTo-Json -Depth 3 -Compress)
[ordered]@{ exit_code=$process.ExitCode; close_requested=$closed; forced_stop=!$finished;
    baseline_preserved=$preserved; ended=[DateTime]::UtcNow.ToString('o') } |
    ConvertTo-Json | Tee-Object -FilePath (Join-Path $run 'termination.json')
if (!$preserved) { throw 'Baseline metadata changed during experiment.' }
