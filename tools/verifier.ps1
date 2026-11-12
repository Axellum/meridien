# Verifications du depot en une commande (PowerShell 5.1 ou 7 ; ASCII seulement a dessein : pas de BOM).
#
#   .\tools\verifier.ps1             tout le depot : secrets, tests du noyau (executes, avec detecteurs), controles
#                                    independants (attendus Python, icones, polices, liens), ruff, mise en forme C/C++
#   .\tools\verifier.ps1 -Mutations  + la campagne COMPLETE des mutations du noyau (des heures : jamais exigee, a la main ;
#                                    avant un push : lancer.py --mutations --seulement <tests touches>)
#   .\tools\verifier.ps1 -Web       + parcours Playwright de la page de reglages (demande le reseau)
#   .\tools\verifier.ps1 -Rapide     seulement ce qui est dans l'index git (appele par .githooks/pre-commit)
#
# Code de sortie non nul = au moins un controle a ECHOUE, la mise en forme comprise (bloquante depuis le reformatage
# d'un seul coup du code). Dossier des outils : tools\trouver_outils.ps1 (voir docs/outils.md).
param(
    [switch]$Rapide,
    [switch]$Mutations,
    [switch]$Web
)

$ErrorActionPreference = 'Continue'
$racine = (& git rev-parse --show-toplevel).Trim()
Set-Location $racine

. (Join-Path $PSScriptRoot 'trouver_outils.ps1')
$outils = Trouver-Outils
if (-not $outils) {
    Write-Host "ECHEC : dossier des outils introuvable (MERIDIEN_OUTILS, `$Outils de tools\local.ps1, ou un dossier _outils a cote du depot). Voir docs/outils.md." -ForegroundColor Red
    exit 2
}
. "$outils\env_hote.ps1" -Sanitizers | Out-Null
$env:Path = "$outils\gitleaks;$env:Path"

$resultats = New-Object System.Collections.ArrayList
function Ajouter($nom, $etat, $detail) {
    [void]$resultats.Add([pscustomobject]@{ Controle = $nom; Etat = $etat; Detail = $detail })
}

# --- fichiers concernes -----------------------------------------------------------------------------------------
if ($Rapide) {
    $fichiers = @(& git diff --cached --name-only --diff-filter=ACM)
} else {
    $fichiers = @(& git ls-files)
}
$fichiers = $fichiers | Where-Object { $_ -and (Test-Path $_) }
# Memes exclusions que .clang-format-ignore (fichiers generes), pour que le compte affiche soit juste.
$cpp = @($fichiers | Where-Object { $_ -match '\.(c|cc|cpp|h|hpp|inc)$' -and $_ -notmatch '(^|/)(managed_components|build|\.cache)/' -and $_ -notmatch 'fuseaux_donnees\.inc$' -and $_ -notmatch 'test/host/donnees/' })
$py = @($fichiers | Where-Object { $_ -match '\.py$' })
$noyauTouche = (-not $Rapide) -or @($fichiers | Where-Object { $_ -match '^(components/core/|test/host/)' }).Count -gt 0
# Un fichier hors du noyau peut etre lu par un test (main/horloge.cpp, un dossier d'inclusion des cartes) : meme calcul
# que la CI (lancer.py --concerne, par le preprocesseur du compilateur). Tout doute repond oui.
$concerne = ''
if ($Rapide -and -not $noyauTouche -and @($fichiers).Count -gt 0) {
    $concerne = ((@($fichiers) | & python test/host/lancer.py --concerne - 2>&1) -join ' ').Trim()
    if ($concerne -notmatch '^non') { $noyauTouche = $true }
}

# --- 1. secrets ---------------------------------------------------------------------------------------------------
if ($Rapide) {
    $sortie = & gitleaks git --pre-commit --staged --redact --no-banner 2>&1
} else {
    $sortie = & gitleaks git --redact --no-banner 2>&1
}
if ($LASTEXITCODE -eq 0) {
    Ajouter 'secrets (gitleaks)' 'OK' 'aucun secret trouve'
} else {
    Ajouter 'secrets (gitleaks)' 'ECHEC' (($sortie | Select-Object -Last 12) -join "`n")
}

# --- 2. tests du noyau ---------------------------------------------------------------------------------------------
if ($noyauTouche) {
    $sortie = & python test/host/lancer.py 2>&1
    $code = $LASTEXITCODE
    $texte = $sortie -join "`n"
    if ($code -ne 0) {
        Ajouter 'tests du noyau' 'ECHEC' (($sortie | Select-Object -Last 12) -join "`n")
    } else {
        # Meme preuve que la CI : la ligne exacte du mode complet ET au moins un test execute (le mot "complet" seul
        # figure aussi dans les messages du mode "syntaxe", ou rien n'est execute).
        $total = ([regex]::Matches($texte, '(?m)^(\d+) test\(s\)') | ForEach-Object { [int]$_.Groups[1].Value } | Measure-Object -Sum).Sum
        if (($texte -notmatch 'mode        : complet') -or -not ($total -gt 0)) {
            Ajouter 'tests du noyau' 'ECHEC' "le lanceur n'est pas en mode complet (compile + execute) ou n'a execute aucun test"
        } else {
            Ajouter 'tests du noyau' 'OK' "$total test(s) executes avec AddressSanitizer + UBSan"
        }
    }
    if ($Mutations) {
        $sortie = & python test/host/lancer.py --mutations 2>&1
        $derniere = ($sortie | Select-Object -Last 1)
        if ($LASTEXITCODE -eq 0) { Ajouter 'mutations du noyau' 'OK' "$derniere" }
        else { Ajouter 'mutations du noyau' 'ECHEC' (($sortie | Select-Object -Last 8) -join "`n") }
    }
} else {
    # Aucun test C++ concerne : les controles croises Python (traduction, contrat de l'API, cartes, mer) tournent quand
    # meme, comme dans la CI.
    $sortie = & python test/host/lancer.py --controles-seuls 2>&1
    if ($LASTEXITCODE -eq 0) { Ajouter 'tests du noyau' 'SAUTE' "aucun test C++ concerne ($concerne) ; controles croises OK" }
    else { Ajouter 'controles croises' 'ECHEC' (($sortie | Select-Object -Last 12) -join "`n") }
}

# --- 2 bis. controles independants (attendus recalcules par Python, icones, polices, liens Markdown) ----------------
$sortie = & python test/host/controles_independants.py 2>&1
$derniere = ($sortie | Select-Object -Last 1)
if ($LASTEXITCODE -eq 0) { Ajouter 'controles independants' 'OK' "$derniere" }
else { Ajouter 'controles independants' 'ECHEC' ((($sortie | Select-Object -Last 20) -join "`n")) }

# --- 3. Python (ruff) ---------------------------------------------------------------------------------------------
if ($py.Count -gt 0) {
    $sortie = & python -m ruff check --no-cache $py 2>&1
    if ($LASTEXITCODE -eq 0) { Ajouter 'Python (ruff)' 'OK' "$($py.Count) fichier(s)" }
    else { Ajouter 'Python (ruff)' 'ECHEC' (($sortie | Select-Object -Last 14) -join "`n") }
} else {
    Ajouter 'Python (ruff)' 'SAUTE' 'aucun fichier Python'
}

# --- 4. mise en forme C/C++ (bloquante) -------------------------------------------------------------------------
# .clang-format est la regle, .clang-format-ignore exclut les fichiers generes (clang-format n'y touche pas).
if ($cpp.Count -gt 0) {
    $ecarts = 0
    $fautifs = @()
    foreach ($f in $cpp) {
        $n = @(& clang-format --style=file --dry-run $f 2>&1 | Select-String 'warning:').Count
        if ($n -gt 0) { $ecarts += $n; $fautifs += "$f ($n)" }
    }
    if ($ecarts -eq 0) { Ajouter 'mise en forme (clang-format)' 'OK' "$($cpp.Count) fichier(s) conformes" }
    else {
        $detail = "$ecarts ecart(s) dans $($fautifs.Count) fichier(s) ; corriger avec : clang-format -i --style=file <fichier>`n" + ($fautifs -join "`n")
        Ajouter 'mise en forme (clang-format)' 'ECHEC' $detail
    }
} else {
    Ajouter 'mise en forme (clang-format)' 'SAUTE' 'aucun fichier C/C++'
}

# --- 5. page de reglages (option) ------------------------------------------------------------------------------
if ($Web) {
    $sortie = & python web/tests/parcours.py 2>&1
    $dernier = ($sortie | Select-String 'RESULTAT|R.SULTAT' | Select-Object -Last 1)
    if ($LASTEXITCODE -eq 0) { Ajouter 'page de reglages (Playwright)' 'OK' "$dernier" }
    else { Ajouter 'page de reglages (Playwright)' 'ECHEC' (($sortie | Select-Object -Last 10) -join "`n") }
}

# --- bilan -----------------------------------------------------------------------------------------------------------
Write-Host ''
$echecs = 0
foreach ($r in $resultats) {
    $couleur = switch ($r.Etat) { 'OK' { 'Green' } 'ECHEC' { 'Red' } 'AVERT' { 'Yellow' } default { 'DarkGray' } }
    Write-Host ('{0,-7} {1}' -f $r.Etat, $r.Controle) -ForegroundColor $couleur -NoNewline
    Write-Host ("  " + ($r.Detail -split "`n")[0])
    if ($r.Etat -eq 'ECHEC') {
        $echecs++
        ($r.Detail -split "`n" | Select-Object -Skip 1) | ForEach-Object { Write-Host "          $_" -ForegroundColor Red }
    }
}
Write-Host ''
if ($echecs -gt 0) {
    Write-Host "$echecs controle(s) en ECHEC : corriger avant de commiter (jamais --no-verify)." -ForegroundColor Red
    exit 1
}
Write-Host 'Tous les controles bloquants passent.' -ForegroundColor Green
exit 0
