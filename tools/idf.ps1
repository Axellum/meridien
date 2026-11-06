# Lance idf.py avec l'ESP-IDF deja installe par ESPHome dans H:\idf (rien de nouveau a installer).
#
# Usage (PowerShell, jamais Git Bash) :
#   .\tools\idf.ps1 --version
#   .\tools\idf.ps1 set-target esp32p4
#   .\tools\idf.ps1 build
#
# Pourquoi ce script et pas export.ps1 : l'install de H:\idf appartient a ESPHome (fichier
# .esphome.stamp.json dans penvs\5.5.5) et son export.ps1 cherche un environnement Python a un autre
# emplacement (python_env\idf5.5_py3.13_env), qui n'existe pas -> erreur. On appelle donc idf.py avec le
# Python du penv et on pose le PATH nous-memes. Verifie le 06/11/2026 : idf.py v5.5.5, GCC RISC-V 14.2.0.
# Ne JAMAIS modifier H:\idf (ESPHome peut le mettre a jour) ; s'il disparait ou change de version,
# installer un ESP-IDF a part (voir docs/decisions/0001-pile-esp-idf-natif.md).
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$ArgsIdf)

$ErrorActionPreference = 'Stop'
$racineIdf = 'H:\idf'
$versionIdf = '5.5.5'
$idfPath = Join-Path $racineIdf "frameworks\$versionIdf"
$python = Join-Path $racineIdf "penvs\$versionIdf\Scripts\python.exe"
foreach ($chemin in $idfPath, $python) {
    if (-not (Test-Path $chemin)) { throw "Introuvable : $chemin (voir l'en-tete de ce script)" }
}

$bins = @()
foreach ($outil in 'cmake', 'ninja', 'ccache') {
    $dossier = Get-ChildItem (Join-Path $racineIdf "tools\$outil") -Directory | Select-Object -First 1
    $exe = Get-ChildItem $dossier.FullName -Recurse -Filter "$outil.exe" -Depth 3 | Select-Object -First 1
    if ($exe) { $bins += $exe.DirectoryName }
}
$dossier = Get-ChildItem (Join-Path $racineIdf 'tools\riscv32-esp-elf') -Directory | Select-Object -First 1
$gcc = Get-ChildItem $dossier.FullName -Recurse -Filter 'riscv32-esp-elf-gcc.exe' -Depth 4 | Select-Object -First 1
if (-not $gcc) { throw "Compilateur RISC-V introuvable sous $($dossier.FullName)" }
$bins += $gcc.DirectoryName

$projet = Split-Path -Parent $PSScriptRoot
$env:IDF_TOOLS_PATH = $racineIdf
$env:IDF_PATH = $idfPath
# Dit a idf_tools.py ou est l'environnement Python (sinon il cherche python_env\idf5.5_py3.13_env).
$env:IDF_PYTHON_ENV_PATH = Split-Path -Parent (Split-Path -Parent $python)
$env:PATH = (($bins + (Split-Path -Parent $python)) -join ';') + ';' + $env:PATH
# Le PC a deja plante a 6 compilations paralleles (consigne d'Axel : 4 au plus).
if (-not $env:CMAKE_BUILD_PARALLEL_LEVEL) { $env:CMAKE_BUILD_PARALLEL_LEVEL = '4' }
# Composants telecharges : dans le projet (ignores par git), pas dans l'arbre d'ESPHome.
$env:IDF_COMPONENT_CACHE_PATH = Join-Path $projet '.cache\composants'

Push-Location $projet
try { & $python (Join-Path $idfPath 'tools\idf.py') @ArgsIdf; exit $LASTEXITCODE }
finally { Pop-Location }
