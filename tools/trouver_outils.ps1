# Trouve le dossier des outils de developpement du PC (llvm-mingw : clang, clang-format ; gitleaks ; env_hote.ps1).
# PowerShell 5.1 ou 7 ; ASCII seulement a dessein : pas de BOM. A charger par un point, jamais a lancer :
#
#   . (Join-Path $PSScriptRoot 'trouver_outils.ps1')
#   $outils = Trouver-Outils            # $null si rien n'est trouve : l'appelant dit alors quoi faire
#
# Rien n'est ecrit dans le depot : le dossier des outils est propre a chaque PC. Ordre de recherche (le premier qui contient
# env_hote.ps1 gagne ; la regle est decrite dans docs/outils.md) :
#   1. la variable d'environnement MERIDIEN_OUTILS ;
#   2. la variable $Outils de tools\local.ps1 (ignore par git ; modele : tools\local.example.ps1), celui de ce dossier de travail
#      puis celui du dossier principal du depot (utile dans un worktree git, qui n'a pas le sien) ;
#   3. un dossier nomme _outils, dans un dossier parent du depot (le cas de l'auteur : _outils a cote du depot).
# Un chemin qui commence par le signe "<" (le modele local.example.ps1 le laisse tel quel) est ignore, comme une valeur absente.

function Trouver-Outils {
    param([string]$Depart = $PSScriptRoot)

    $candidats = New-Object System.Collections.ArrayList
    if ($env:MERIDIEN_OUTILS) { [void]$candidats.Add($env:MERIDIEN_OUTILS) }

    $locaux = @(Join-Path $Depart 'local.ps1')
    $commun = $null
    try { $commun = git -C $Depart rev-parse --path-format=absolute --git-common-dir 2>$null } catch { }
    $global:LASTEXITCODE = 0   # hors d'un depot git la commande ci-dessus rend 128 : ne pas le laisser a l'appelant
    if ($commun) { $locaux += Join-Path (Split-Path -Parent $commun) 'tools\local.ps1' }
    foreach ($local in $locaux) {
        if (Test-Path $local) {
            $Outils = $null
            . $local
            if ($Outils) { [void]$candidats.Add($Outils) }
            break
        }
    }

    $dossier = $Depart
    while ($dossier) {
        [void]$candidats.Add((Join-Path $dossier '_outils'))
        $parent = Split-Path -Parent $dossier
        if (-not $parent -or $parent -eq $dossier) { break }
        $dossier = $parent
    }

    foreach ($candidat in $candidats) {
        if ($candidat -like '<*') { continue }
        if (Test-Path (Join-Path $candidat 'env_hote.ps1')) { return (Resolve-Path $candidat).Path }
    }
    return $null
}
