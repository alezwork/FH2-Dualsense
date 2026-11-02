# FH2-DualSense: install / uninstall (Install.cmd, Uninstall.cmd run this).
# Puts d3d12.dll next to azimuth.exe. Nothing goes into the game's folder; Extra Options is not needed (and is not
# touched when it is there). Another mod's d3d12.dll that is already there (ReShade and the like) is kept as
# d3d12_next.dll and still works: the mod hands Direct3D 12 on to it. Older versions of this mod are moved out of
# the way. Uninstalling puts it all back.
#   install.ps1 [-Uninstall] [-Azimuth <folder with azimuth.exe>] [-Quiet]
param([switch]$Uninstall, [string]$Azimuth = '', [switch]$Quiet)

$ErrorActionPreference = 'Stop'
function Say([string]$text, [string]$color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Done([int]$code) {
    if (-not $Quiet) { Write-Host ''; Read-Host 'Press Enter to close' | Out-Null }
    exit $code
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$marker = [Text.Encoding]::ASCII.GetBytes('FH2-DualSense ')

function Test-Ours([string]$dll) {
    if (-not (Test-Path -LiteralPath $dll)) { return $false }
    $b = [IO.File]::ReadAllBytes($dll)
    for ($i = 0; $i -le $b.Length - $marker.Length; $i++) {
        if ($b[$i] -ne $marker[0]) { continue }
        $j = 1
        while ($j -lt $marker.Length -and $b[$i + $j] -eq $marker[$j]) { $j++ }
        if ($j -eq $marker.Length) { return $true }
    }
    return $false
}

function Find-Azimuth {
    $c = New-Object System.Collections.Generic.List[string]
    if ($Azimuth) { $c.Add($Azimuth) }
    $c.Add($here); $c.Add((Split-Path -Parent $here))
    Get-Process -Name azimuth -ErrorAction SilentlyContinue | ForEach-Object { if ($_.Path) { $c.Add((Split-Path -Parent $_.Path)) } }
    foreach ($d in [IO.DriveInfo]::GetDrives() | Where-Object { $_.DriveType -eq 'Fixed' -and $_.IsReady }) {
        foreach ($sub in 'Azimuth', 'Games\Azimuth', 'Emulators\Azimuth', 'Games\Emulators\Azimuth', 'Program Files\Azimuth') {
            $c.Add((Join-Path $d.RootDirectory.FullName $sub))
        }
        foreach ($top in 'Games', 'Emulators') {
            $t = Join-Path $d.RootDirectory.FullName $top
            if (Test-Path -LiteralPath $t) {
                Get-ChildItem -LiteralPath $t -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                    $c.Add($_.FullName); $c.Add((Join-Path $_.FullName 'Azimuth'))
                }
            }
        }
    }
    foreach ($p in $c) {
        if ($p -and (Test-Path -LiteralPath (Join-Path $p 'azimuth.exe'))) { return (Resolve-Path -LiteralPath $p).Path }
    }
    # not found: ask
    Add-Type -AssemblyName System.Windows.Forms
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = 'Choose the Azimuth folder (the one with azimuth.exe)'
    $dlg.ShowNewFolderButton = $false
    while ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        if (Test-Path -LiteralPath (Join-Path $dlg.SelectedPath 'azimuth.exe')) { return $dlg.SelectedPath }
        Say 'There is no azimuth.exe in that folder.' 'Yellow'
    }
    return $null
}

Say 'FH2-DualSense -- the DualSense and the keyboard for Forza Horizon 2 on Azimuth' 'Cyan'
$az = Find-Azimuth
if (-not $az) { Say 'Azimuth not found: nothing done.' 'Red'; Done 1 }
Say "Azimuth: $az"

if (Get-Process -Name azimuth -ErrorAction SilentlyContinue) {
    Say 'Azimuth is running: close it first, then run this again.' 'Red'
    Done 1
}

$dll = Join-Path $az 'd3d12.dll'
$next = Join-Path $az 'd3d12_next.dll'
$dsDir = Join-Path $az 'DualSense'
$old = Join-Path $dsDir 'old versions'

if ($Uninstall) {
    if (Test-Ours $dll) {
        Remove-Item -LiteralPath $dll -Force
        Say 'd3d12.dll of the mod removed.' 'Green'
        if (Test-Path -LiteralPath $next) {
            Rename-Item -LiteralPath $next -NewName 'd3d12.dll'
            Say 'The d3d12.dll that was there before is back.' 'Green'
        }
    } else {
        Say 'The mod is not installed there (no d3d12.dll of it).' 'Yellow'
    }
    if (Test-Path -LiteralPath $dsDir) {
        $cache = Join-Path $dsDir 'cache'
        if (Test-Path -LiteralPath $cache) { Remove-Item -LiteralPath $cache -Recurse -Force }
        $ans = if ($Quiet) { 'n' } else { Read-Host 'Delete the settings too (the DualSense folder)? [y/N]' }
        if ($ans -match '^[yY]') { Remove-Item -LiteralPath $dsDir -Recurse -Force; Say 'The DualSense folder removed.' 'Green' }
        else { Say 'Settings kept: DualSense\DualSense.ini' }
    }
    Say 'Uninstalled. The game and Azimuth are as they were.' 'Green'
    Done 0
}

$src = Join-Path $here 'd3d12.dll'
if (-not (Test-Path -LiteralPath $src)) { Say "No d3d12.dll next to this script ($here)." 'Red'; Done 1 }
if (-not (Test-Ours $src)) { Say 'The d3d12.dll next to this script is not FH2-DualSense.' 'Red'; Done 1 }

# someone else's d3d12.dll: kept, chained
if ((Test-Path -LiteralPath $dll) -and -not (Test-Ours $dll)) {
    if (Test-Path -LiteralPath $next) {
        Say 'There is another d3d12.dll and a d3d12_next.dll already: move one of them away first.' 'Red'
        Done 1
    }
    Rename-Item -LiteralPath $dll -NewName 'd3d12_next.dll'
    Say 'Another mod''s d3d12.dll kept as d3d12_next.dll (it keeps working).' 'Yellow'
}

# older versions of this mod out of the way (2.0/2.1: fh2-mods\DualSense.dll; 1.x: mods\DualSense.dll)
foreach ($o in (Join-Path $az 'fh2-mods\DualSense.dll'), (Join-Path $az 'mods\DualSense.dll')) {
    if (Test-Path -LiteralPath $o) {
        New-Item -ItemType Directory -Force -Path $old | Out-Null
        Move-Item -LiteralPath $o -Destination (Join-Path $old ((Split-Path -Leaf (Split-Path -Parent $o)) + '-DualSense.dll')) -Force
        Say "An old version moved away: $o" 'Yellow'
    }
}

Copy-Item -LiteralPath $src -Destination $dll -Force
New-Item -ItemType Directory -Force -Path $dsDir | Out-Null
Say 'Installed: d3d12.dll next to azimuth.exe.' 'Green'
if (Test-Path -LiteralPath (Join-Path $az 'winmm.dll')) {
    Say 'Extra Options found: the mod works with it.'
} else {
    Say 'No Extra Options: the mod works on its own.'
}
Write-Host ''
Say 'Before playing: turn off Steam''s PlayStation controller support for Azimuth and close DS4Windows.' 'Yellow'
Say 'Settings: Settings > Controls in the game, or DualSense\DualSense.ini (made at the first start).'
Say 'The log: DualSense\DualSense.log.'
Done 0
