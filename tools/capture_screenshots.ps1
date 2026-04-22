# Capture canonical CERBERUS screenshots from DOSBox Staging.
# Writes PNG files to docs/screenshots/.
#
# Usage from the CERBERUS root:
#   powershell -ExecutionPolicy Bypass -File tools\capture_screenshots.ps1
#
# Assumes:
#   - DOSBox Staging at the default install path
#   - dist\CERBERUS.EXE is the current stock v0.8.1 build
#   - devenv\cap_ui.conf runs CERBERUS with the three-pane UI enabled
#   - devenv\cap_noui.conf runs CERBERUS /NOUI for the text dump

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class WinAPI {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
'@

$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $repoRoot "docs\screenshots"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

$dosboxExe = "C:\Users\tonyu\AppData\Local\Programs\DOSBox Staging\dosbox.exe"
if (-not (Test-Path $dosboxExe)) {
    Write-Error "DOSBox Staging not found at $dosboxExe"
    exit 1
}

function Grab-Window {
    param(
        [string]$OutFile,
        [string]$ProcName = "dosbox"
    )
    $proc = Get-Process $ProcName -ErrorAction SilentlyContinue
    if (-not $proc) {
        Write-Warning "$ProcName not running at capture time"
        return $false
    }
    $hwnd = $proc[0].MainWindowHandle
    [WinAPI]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 500

    $rect = New-Object WinAPI+RECT
    [WinAPI]::GetClientRect($hwnd, [ref]$rect) | Out-Null
    $pt = New-Object WinAPI+POINT
    $pt.X = 0; $pt.Y = 0
    [WinAPI]::ClientToScreen($hwnd, [ref]$pt) | Out-Null

    $w = $rect.R - $rect.L
    $h = $rect.B - $rect.T
    if ($w -le 0 -or $h -le 0) {
        Write-Warning "$ProcName has zero-size client rect; skipping"
        return $false
    }
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "  -> $OutFile ($w x $h)"
    return $true
}

function Send-KeyToDosbox {
    param([string]$Keys)
    $proc = Get-Process "dosbox" -ErrorAction SilentlyContinue
    if (-not $proc) { return }
    [WinAPI]::SetForegroundWindow($proc[0].MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait($Keys)
}

function Run-Capture {
    param(
        [string]$Conf,
        [array]$Captures  # array of hashtables each with Delay, Name, optional Key
    )
    Write-Host "Launching: $Conf"
    $p = Start-Process -FilePath $dosboxExe -ArgumentList @("--exit", "-conf", $Conf) -PassThru
    $totalWaited = 0
    foreach ($entry in $Captures) {
        $wait = [int]$entry.Delay
        $name = $entry.Name
        $key  = $entry.Key
        Start-Sleep -Seconds ($wait - $totalWaited)
        $totalWaited = $wait
        $outFile = Join-Path $outDir $name
        $ok = Grab-Window -OutFile $outFile
        if ($key) {
            Send-KeyToDosbox -Keys $key
            Start-Sleep -Milliseconds 800
        }
    }
    # Ensure DOSBox quits.
    Send-KeyToDosbox -Keys "Q"
    try { $p.WaitForExit(15000) | Out-Null } catch {}
    if (-not $p.HasExited) {
        Write-Warning "DOSBox did not exit cleanly; killing"
        try { $p.Kill() } catch {}
    }
}

# --------------------------------------------------------------------------
# Run 1: interactive UI capture
# --------------------------------------------------------------------------
Write-Host "=== Run 1: three-pane UI + help overlay ==="
$uiConf = Join-Path $repoRoot "devenv\cap_ui.conf"
Run-Capture -Conf $uiConf -Captures @(
    @{ Delay = 30; Name = "01-three-pane-summary.png";   Key = "{F1}" }
    @{ Delay = 33; Name = "02-help-overlay.png";         Key = "{ESC}" }
    @{ Delay = 36; Name = "03-summary-after-help.png";   Key = $null  }
)

Start-Sleep -Seconds 2

# --------------------------------------------------------------------------
# Run 2: /NOUI batch text dump capture
# --------------------------------------------------------------------------
Write-Host "=== Run 2: /NOUI batch text output ==="
$nouiConf = Join-Path $repoRoot "devenv\cap_noui.conf"
Run-Capture -Conf $nouiConf -Captures @(
    @{ Delay = 30; Name = "04-batch-text-output.png"; Key = $null }
)

Write-Host ""
Write-Host "Done. Screenshots in $outDir"
Get-ChildItem $outDir | Select-Object Name, Length | Format-Table
