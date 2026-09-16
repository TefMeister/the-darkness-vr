param([int]$WaitSec = 20, [string]$Tag = "look")
# Captures ONLY the game's own window, via PrintWindow(PW_RENDERFULLCONTENT).
# It never brings the window forward, never takes focus and never sends input, so it is
# safe while the user is working in another program on the same machine.
$ErrorActionPreference = "Continue"
$dir  = "E:\the-darkness\build-local"
$shot = "E:\the-darkness\screens"
New-Item -ItemType Directory -Force $shot | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GW {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
}
"@

# Focus guard: name the process that owns the foreground window. Records the program only,
# never the window title, so nothing about the user's own work is captured.
function FgProc {
  $h = [GW]::GetForegroundWindow(); $fpid = 0
  [void][GW]::GetWindowThreadProcessId($h, [ref]$fpid)
  $n = (Get-Process -Id $fpid -ErrorAction SilentlyContinue).ProcessName
  if ($n) { return $n } else { return "none" }
}

$fgBefore = [GW]::GetForegroundWindow()
Get-Process darknessrecomp -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Start-Process -FilePath "$dir\darknessrecomp.exe" -WorkingDirectory $dir -PassThru
Write-Output "launched pid=$($p.Id)"

function SnapWin($proc, $name) {
  $proc.Refresh()
  $h = $proc.MainWindowHandle
  if ($h -eq [IntPtr]::Zero) { return "no-window-yet" }
  $r = New-Object GW+RECT
  [void][GW]::GetWindowRect($h, [ref]$r)
  $w = $r.R - $r.L; $ht = $r.B - $r.T
  if ($w -le 0 -or $ht -le 0) { return "zero-size-window" }
  $bmp = New-Object System.Drawing.Bitmap $w, $ht
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  $ok = [GW]::PrintWindow($h, $hdc, 2)   # 2 = PW_RENDERFULLCONTENT, captures DX content
  $g.ReleaseHdc($hdc)
  # measure: is the capture all one colour (black), or does it have content?
  $sample = @{}; $step = [math]::Max(1, [int]($w / 40))
  for ($x = 0; $x -lt $w; $x += $step) { for ($y = 0; $y -lt $ht; $y += $step) {
    $c = $bmp.GetPixel($x, $y); $sample["$($c.R),$($c.G),$($c.B)"] = 1 } }
  $path = Join-Path $shot "$Tag-$name.png"
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  return "$path  ${w}x${ht}  printwindow=$ok  distinctColours=$($sample.Count)"
}

for ($t = 0; $t -lt $WaitSec; $t += 5) {
  Start-Sleep -Seconds 5
  $q = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
  if (-not $q) { Write-Output "EXITED by t=$($t+5)s"; break }
  $s = SnapWin $q ("t" + ($t + 5))
  Write-Output "t=$($t+5)s  cpuSec=$([math]::Round($q.CPU,1))  mem=$([math]::Round($q.WorkingSet64/1MB))MB  focus=$(FgProc)  -> $s"
}

$fgAfter = [GW]::GetForegroundWindow()
Write-Output ("focus unchanged by this run: " + ($fgBefore -eq $fgAfter))
Get-Process darknessrecomp -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "closed."
