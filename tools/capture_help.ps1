# Targeted capture of the F1 help overlay only. Separate from the main
# capture_screenshots.ps1 because the help overlay needs precise timing
# — F1 must fire WHILE the three-pane summary is on screen, not during
# the visual journey cards that /QUICK skips but detect still produces.

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
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
'@

function Send-Scancode {
    param([byte]$VK, [byte]$Scan)
    $KEYEVENTF_SCANCODE = 0x0008
    $KEYEVENTF_KEYUP    = 0x0002
    # Key down + key up with scancode flag set so the OS uses the raw scan
    # code; DOSBox Staging's SDL backend reads scancodes, not virtual
    # keycodes, so this reaches the guest reliably where SendKeys does not.
    [WinAPI]::keybd_event($VK, $Scan, $KEYEVENTF_SCANCODE, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [WinAPI]::keybd_event($VK, $Scan, ($KEYEVENTF_SCANCODE -bor $KEYEVENTF_KEYUP), [UIntPtr]::Zero)
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $repoRoot "docs\screenshots"
$dosboxExe = "C:\Users\tonyu\AppData\Local\Programs\DOSBox Staging\dosbox.exe"

function Grab-Window {
    param([string]$OutFile)
    $proc = Get-Process "dosbox" -ErrorAction SilentlyContinue
    if (-not $proc) { return $false }
    $hwnd = $proc[0].MainWindowHandle
    [WinAPI]::SetForegroundWindow($hwnd) | Out-Null
    Start-Sleep -Milliseconds 500
    $rect = New-Object WinAPI+RECT
    [WinAPI]::GetClientRect($hwnd, [ref]$rect) | Out-Null
    $pt = New-Object WinAPI+POINT; $pt.X = 0; $pt.Y = 0
    [WinAPI]::ClientToScreen($hwnd, [ref]$pt) | Out-Null
    $w = $rect.R - $rect.L; $h = $rect.B - $rect.T
    if ($w -le 0 -or $h -le 0) { return $false }
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    return $true
}

function Focus-Dosbox {
    $proc = Get-Process "dosbox" -ErrorAction SilentlyContinue
    if (-not $proc) { return }
    [WinAPI]::SetForegroundWindow($proc[0].MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 500
}

$conf = Join-Path $repoRoot "devenv\cap_help.conf"
$p = Start-Process -FilePath $dosboxExe -ArgumentList @("--exit", "-conf", $conf) -PassThru
# Wait for /QUICK run to finish the detect phase and land at the
# three-pane summary. Detect runs about a dozen probes (CPU / FPU /
# memory / cache / bus / video / audio / BIOS / network / timing) so
# needs 30+ sec on DOSBox Staging.
Start-Sleep -Seconds 38
# Fire F1 via low-level scancode (VK_F1 = 0x70, scan = 0x3B).
# DOSBox Staging's SDL backend reads scancodes; SendKeys' virtual-key
# path did not propagate F1 to the guest reliably.
Focus-Dosbox
Send-Scancode -VK 0x70 -Scan 0x3B
Start-Sleep -Seconds 2
$ok = Grab-Window -OutFile (Join-Path $outDir "05-help-overlay.png")
Write-Host "Help overlay captured: $ok"

# Dismiss (Esc = VK 0x1B / scan 0x01) + exit (Q = VK 0x51 / scan 0x10)
Focus-Dosbox
Send-Scancode -VK 0x1B -Scan 0x01
Start-Sleep -Seconds 1
Focus-Dosbox
Send-Scancode -VK 0x51 -Scan 0x10
try { $p.WaitForExit(10000) | Out-Null } catch {}
if (-not $p.HasExited) { try { $p.Kill() } catch {} }
