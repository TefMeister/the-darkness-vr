# start-newest-vr.ps1 -- The Darkness VR, the NEWEST build, in the headset with motion-controlled hands (2026-11-07, Tefa: "yes please
# keep updating the desktop icon for the newer ones"). The Desktop shortcut "The Darkness VR" runs this copy in C:\NonSteam\the-darkness\.
# It picks the build-home-xr-bNNN folder with the highest number that holds dk_vr_headset_hands.ps1 and runs that, so a new build
# folder is picked up with no change to the shortcut. Falls back to build-home-xr.
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$best = Get-ChildItem -Path $root -Directory -Filter "build-home-xr-b*" |
    Where-Object { $_.Name -match '^build-home-xr-b(\d+)$' -and (Test-Path (Join-Path $_.FullName "dk_vr_headset_hands.ps1")) } |
    Sort-Object { [int]($_.Name -replace '^build-home-xr-b', '') } -Descending | Select-Object -First 1
$dir = if ($best) { $best.FullName } else { Join-Path $root "build-home-xr" }
Write-Host "The Darkness VR: starting $(Split-Path -Leaf $dir)"
& (Join-Path $dir "dk_vr_headset_hands.ps1")
Start-Sleep -Seconds 3
