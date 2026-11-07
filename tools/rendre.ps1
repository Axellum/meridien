# Rendu PNG de l'interface du firmware, hors carte (PowerShell 5.1 ou 7 ; ASCII seulement a dessein : pas de BOM).
#
#   .\tools\rendre.ps1                              toutes les scenes, langues et tailles par defaut, dans docs\captures\ecran\
#   .\tools\rendre.ps1 -Scene bonjour -Taille 720x1280
#   .\tools\rendre.ps1 -Scene temoin -Langue fr,de,tr -Planche   une planche par langue (les ecrans d'une sequence en une image)
#   .\tools\rendre.ps1 -Lister                      scenes, variantes, langues, tailles, sequences
#   .\tools\rendre.ps1 -Variante attente            une variante seulement
#   .\tools\rendre.ps1 -Instant 2026-11-06T14:05:00Z   autre heure fixee (UTC)
#   .\tools\rendre.ps1 -Nettoyer                    oublie le cache de compilation (LVGL compris : ~10 s la fois suivante)
#
# Une scene = UN fichier tools\rendu\scenes\scene_<nom>.cpp, decouvert tout seul (voir docs/outils.md).
# Compile sur le PC (clang du dossier des outils, tools\trouver_outils.ps1, sans ESP-IDF) LVGL avec la configuration du firmware et les
# MEMES fichiers main/ui_*.cpp que le firmware, puis ecrit un PNG par scene, variante, langue et taille.
# Ce n'est PAS une photo de la carte : voir docs/outils.md, section "Rendu de l'ecran sur le PC".
# Ne prend ni la carte ni le verrou `build` (aucune compilation ESP-IDF). Une seule compilation lourde a la fois sur le PC.
param(
    [string[]]$Scene,
    [string[]]$Taille,
    [string[]]$Variante,
    [string[]]$Langue,
    [switch]$Planche,
    [int]$Colonnes = 0,
    [switch]$Lister,
    [string]$Instant,
    [string]$Sortie,
    [int]$Jobs = 4,
    [switch]$Nettoyer,
    [switch]$Forcer
)

$ErrorActionPreference = 'Stop'
$racine = (& git rev-parse --show-toplevel).Trim()
Set-Location $racine

. (Join-Path $PSScriptRoot 'trouver_outils.ps1')
$outils = Trouver-Outils
if (-not $outils) {
    Write-Host "ECHEC : dossier des outils introuvable (MERIDIEN_OUTILS, `$Outils de tools\local.ps1, ou un dossier _outils a cote du depot). Voir docs/outils.md." -ForegroundColor Red
    exit 2
}
. "$outils\env_hote.ps1" | Out-Null
# rendre.py retrouve ainsi le meme dossier (variable du processus seulement, rien n'est ecrit).
$env:MERIDIEN_OUTILS = $outils

# AGENTS.md : une seule compilation a la fois sur le PC (il a plante a 6 en parallele).
$occupes = @(Get-Process -Name ninja, esphome, cc1plus, 'riscv32-esp-elf-g++' -ErrorAction SilentlyContinue)
if ($occupes.Count -gt 0 -and -not $Forcer) {
    Write-Host ("ECHEC : une compilation tourne deja ({0}). Attendre, ou relancer avec -Forcer." -f (($occupes | Select-Object -ExpandProperty ProcessName -Unique) -join ', ')) -ForegroundColor Red
    exit 2
}

$arguments = @()
foreach ($s in $Scene) { $arguments += @('--scene', $s) }
foreach ($t in $Taille) { $arguments += @('--taille', $t) }
foreach ($v in $Variante) { $arguments += @('--variante', $v) }
foreach ($l in $Langue) { $arguments += @('--langue', $l) }
if ($Planche) { $arguments += '--planche' }
if ($Colonnes -gt 0) { $arguments += @('--colonnes', [string]$Colonnes) }
if ($Lister) { $arguments += '--lister' }
if ($Instant) { $arguments += @('--instant', $Instant) }
if ($Sortie) { $arguments += @('--sortie', $Sortie) }
if ($Nettoyer) { $arguments += '--nettoyer' }
$arguments += @('--jobs', [string]$Jobs)

& python (Join-Path $racine 'tools\rendu\rendre.py') @arguments
exit $LASTEXITCODE
