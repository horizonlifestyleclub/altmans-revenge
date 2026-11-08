# Altman's Revenge - installer for Windows.
#
# What it changes, and nothing else:
#   <recomp>\mods\yr\Singularity\                 the mod
#   <recomp>\mods\yr\Singularity - Brutal Plus\   the AI profile (Qwen Commander slot)
#   <recomp>\llm-bridge\                          the local Qwen commander
# <recomp> is the folder holding the game's build folder (build\ra2.exe).
# Your Red Alert 2 game files are never touched. Folders of the same name that
# were not put there by this installer are renamed to *.backup-<date>, so
# uninstall.ps1 can put them back.
param([string]$Target = "", [switch]$Yes, [switch]$NoQwen, [switch]$Uninstall)
$ErrorActionPreference = "Stop"
$Pkg = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path "$Pkg\payload")) { $Pkg = $PSScriptRoot }
$Payload = "$Pkg\payload"
$Version = if (Test-Path "$Payload\VERSION") { (Get-Content "$Payload\VERSION" -Raw).Trim() } else { "dev" }
$Model = "qwen3:4b"
$Names = @("mods\yr\Singularity", "mods\yr\Singularity - Brutal Plus", "llm-bridge")
function Ask($q) { if ($Yes) { return $true }; return (Read-Host "$q [y/N]") -match '^[Yy]' }

Write-Host ""
Write-Host "   ====================================================="
Write-Host "      A L T M A N ' S   R E V E N G E"
Write-Host "      a Singularity mod for Red Alert 2: Yuri's Revenge"
Write-Host "   ====================================================="
Write-Host "   version $Version"
Write-Host ""

if (-not $Target) {
  foreach ($c in @("$env:LOCALAPPDATA\RA2-Recomp", "$env:USERPROFILE\RA2-Recomp", "$env:USERPROFILE\redalert2-recomp")) { if (Test-Path $c) { $Target = $c; break } }
  $in = Read-Host "Path of the Red Alert 2 recomp folder (the one with build\ra2.exe)$(if ($Target) { " - Enter alone: $Target" })"
  if ($in) { $Target = $in.Trim('"', ' ') }
}
if (Test-Path $Target -PathType Leaf) { $Target = Split-Path -Parent (Split-Path -Parent $Target) }
if (-not (Test-Path $Target -PathType Container)) { Write-Host "Not a folder: $Target"; exit 1 }
$Target = (Resolve-Path $Target).Path

if ($Uninstall) {
  $n = 0
  foreach ($rel in $Names) {
    $dst = Join-Path $Target $rel
    if (Test-Path "$dst\.altmans-revenge") {
      Remove-Item -Recurse -Force $dst; $n++; Write-Host "  removed $rel"
      $b = Get-ChildItem -Directory -Path (Split-Path -Parent $dst) -Filter ((Split-Path -Leaf $dst) + ".backup-*") -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
      if ($b) { Rename-Item $b.FullName (Split-Path -Leaf $dst); Write-Host "  restored $rel from $($b.Name)" }
    }
  }
  if ($n -eq 0) { Write-Host "  Nothing from Altman's Revenge found in $Target." }
  Write-Host "Ollama and the Qwen model are left installed (remove with: ollama rm $Model)."
  exit 0
}

if (-not (Test-Path "$Payload\mods\yr\Singularity")) { Write-Host "The mod files are missing next to this installer ($Payload)."; exit 1 }
$Exe = @("build\ra2-singularity.exe", "build\ra2.exe", "ra2.exe") | ForEach-Object { Join-Path $Target $_ } | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Exe) { Write-Host "No recompiled game (build\ra2.exe) in $Target. Install and build Red Alert 2 recomp first, then run this again."; exit 1 }
$Root = Split-Path -Parent (Split-Path -Parent $Exe)
Write-Host "Game build: $Exe"
Write-Host "Note: on Windows the new rules, units and AI profiles load, but the native"
Write-Host "      Singularity runtime (drone control, kamikaze clean-up, Qwen Commander)"
Write-Host "      is only built for macOS and Linux so far."
$Game = Join-Path $Root "game"
if (-not ((Test-Path "$Game\langmd.mix") -or (Test-Path "$Game\audio.idx"))) {
  $Game = (Read-Host "Red Alert 2 game folder (with langmd.mix; Enter to skip the new sounds)").Trim('"', ' ')
}
function Find-Python { foreach ($c in @("py", "python", "python3")) { $g = Get-Command $c -ErrorAction SilentlyContinue; if ($g) { return $g.Source } }; return $null }

$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
function Place($src, $dst) {
  if (Test-Path $dst) {
    if (Test-Path "$dst\.altmans-revenge") { Remove-Item -Recurse -Force $dst }
    else { Rename-Item $dst ((Split-Path -Leaf $dst) + ".backup-$Stamp"); Write-Host "  kept your existing $(Split-Path -Leaf $dst) as $(Split-Path -Leaf $dst).backup-$Stamp" }
  }
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
  Copy-Item -Recurse $src $dst
  Set-Content -Path "$dst\.altmans-revenge" -Value $Version
}
Write-Host ""; Write-Host "Installing into $Root"
$Py = Find-Python
foreach ($m in Get-ChildItem -Directory "$Payload\mods\yr") {
  $dst = Join-Path $Root "mods\yr\$($m.Name)"; Place $m.FullName $dst; Write-Host "  mods\yr\$($m.Name)"
  # the mod's new sounds, merged into your own copy of the game's sound bank
  if ($Game -and $Py) { & $Py "$Payload\merge_audio.py" $Game "$Payload\sounds\patch.idx" $dst | ForEach-Object { "    $_" } }
}
if (-not ($Game -and $Py)) { Write-Host "  (new sounds skipped: needs Python 3 and your game folder; the game plays its own sounds)" }
$lb = Join-Path $Root "llm-bridge"
if ((Test-Path "$lb\.altmans-revenge") -and (Test-Path "$lb\memory")) {   # keep what the commander learned
  $keep = Join-Path $Root ".ar-memory-$Stamp"; Move-Item "$lb\memory" $keep
  Place "$Payload\llm-bridge" $lb; Remove-Item -Recurse -Force "$lb\memory"; Move-Item $keep "$lb\memory"
} else { Place "$Payload\llm-bridge" $lb }
Write-Host "  llm-bridge (Qwen commander)"

if (-not $NoQwen) {
  Write-Host ""; Write-Host "Qwen Commander (the learning AI) needs Python 3, Ollama and the $Model model (about 2.5 GB)."
  if (-not (Get-Command python -ErrorAction SilentlyContinue) -and -not (Get-Command py -ErrorAction SilentlyContinue)) {
    if ((Get-Command winget -ErrorAction SilentlyContinue) -and (Ask "  Python 3 is missing. Install it with winget now?")) { winget install -e --id Python.Python.3.12 }
    else { Write-Host "  Install Python 3 from https://www.python.org/downloads/" }
  }
  $Oll = (Get-Command ollama -ErrorAction SilentlyContinue).Source
  if (-not $Oll -and (Test-Path "$env:LOCALAPPDATA\Programs\Ollama\ollama.exe")) { $Oll = "$env:LOCALAPPDATA\Programs\Ollama\ollama.exe" }
  if (-not $Oll) {
    if ((Get-Command winget -ErrorAction SilentlyContinue) -and (Ask "  Ollama is not installed. Install it with winget now?")) {
      winget install -e --id Ollama.Ollama; $Oll = "$env:LOCALAPPDATA\Programs\Ollama\ollama.exe"
    } else { Write-Host "  Install Ollama from https://ollama.com/download, then run this installer again." }
  }
  if ($Oll -and (Test-Path $Oll)) {
    try { Invoke-WebRequest -UseBasicParsing -TimeoutSec 2 http://127.0.0.1:11434/api/tags | Out-Null }
    catch { Start-Process -WindowStyle Hidden $Oll "serve"; Start-Sleep 3 }
    $have = (& $Oll list 2>$null) | ForEach-Object { ($_ -split '\s+')[0] }
    if ($have -contains $Model) { Write-Host "  $Model is ready." }
    elseif (Ask "  Download the $Model model now (about 2.5 GB)?") { & $Oll pull $Model }
    else { Write-Host "  Later: ollama pull $Model   (until then the Qwen slot plays as Brutal Plus and says so)" }
  }
}
Write-Host ""
Write-Host "Done. Start the game, open Settings (F10) > Mod and choose 'Singularity - Brutal Plus'."
Write-Host "To remove it: Uninstall-AltmansRevenge.cmd"
