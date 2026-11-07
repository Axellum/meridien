# Prepare l'instantane propre qui sera le premier (et seul) commit du depot public. Decision d'Axel du 06/11/2026 : un depot NEUF
# cree a partir d'un instantane, pas l'historique actuel (docs/publication.md dit pourquoi). Ne publie rien, ne touche a aucun
# depot distant, n'ecrit que dans -Destination.
#
# Usage (PowerShell 5.1 ou 7), depuis un clone ou un worktree du depot :
#   .\tools\instantane_public.ps1 -Destination <dossier neuf> -Courriel <id>+Axellum@users.noreply.github.com `
#       [-Nom 'Prenom Nom'] [-FichierEnv <.env>] [-Interdit 192.168.x.y,...] [-Reference refs/heads/main]
#
# Etapes, dans cet ordre :
#   1. git archive de la reference (par defaut refs/heads/main : ecrire la reference en entier, car « main » est aussi un dossier du
#      depot et git ne sait plus lequel on veut) ;
#   2. tools\controle_publication.py sur ce dossier, avec les valeurs secretes du .env (mot de passe Wi-Fi) et les chaines
#      interdites : au moindre BLOQUANT, arret, le dossier reste pour inspection et rien n'est commite ;
#   3. git init et UN commit, au nom de -Nom / -Courriel (une adresse « noreply » de GitHub, imposee : l'adresse personnelle de
#      l'auteur est dans l'historique prive) ;
#   4. tools\controle_publication.py --historique sur le depot neuf : aucune identite de commit autre que « noreply ».
# Ensuite : relire la liste « A JUGER », regarder les images, puis seulement creer le depot distant (docs/publication.md).
param(
    [Parameter(Mandatory = $true)][string]$Destination,
    [Parameter(Mandatory = $true)][string]$Courriel,
    [string]$Nom,
    [string]$FichierEnv,
    [string[]]$Interdit = @(),
    [string]$Reference = 'refs/heads/main'
)

$ErrorActionPreference = 'Stop'

function Invoke-Git {
    & git @args
    if ($LASTEXITCODE -ne 0) { throw "git $($args -join ' ') a echoue (code $LASTEXITCODE)" }
}

if ($Courriel -notmatch '@users\.noreply\.github\.com$') {
    throw "Courriel : une adresse noreply de GitHub est exigee (<id>+<compte>@users.noreply.github.com), pas une adresse personnelle."
}
$racine = (& git rev-parse --show-toplevel).Trim()
if (-not $Nom) { $Nom = (& git config user.name).Trim() }
if (-not $Nom) { throw "Nom : passer -Nom, git config user.name est vide." }

if (Test-Path $Destination) {
    if (Get-ChildItem -Force $Destination | Select-Object -First 1) { throw "Destination non vide : $Destination (jamais d'ecrasement ; en donner une neuve)." }
} else {
    New-Item -ItemType Directory -Path $Destination | Out-Null
}
$Destination = (Resolve-Path $Destination).Path

$sha = (& git -C $racine rev-parse --verify "$Reference^{commit}").Trim()
if ($LASTEXITCODE -ne 0 -or -not $sha) { throw "Reference introuvable : $Reference" }
$date = (& git -C $racine log -1 --format=%cs $sha).Trim()
Write-Host "1/4 archive de $Reference ($($sha.Substring(0, 7)), $date) dans $Destination"
$tar = Join-Path ([IO.Path]::GetTempPath()) ("instantane_" + [guid]::NewGuid().ToString('N') + '.tar')
try {
    Invoke-Git -C $racine archive --format=tar -o $tar $sha
    & tar -xf $tar -C $Destination
    if ($LASTEXITCODE -ne 0) { throw "tar -x a echoue (code $LASTEXITCODE)" }
} finally {
    if (Test-Path $tar) { Remove-Item -LiteralPath $tar -Force }
}
$attendus = @(& git -C $racine ls-tree -r --name-only $sha).Count
$presents = @(Get-ChildItem -Recurse -File -Force $Destination).Count
if ($attendus -ne $presents) { throw "Archive incomplete : $presents fichier(s) extrait(s), $attendus attendu(s)." }

Write-Host "2/4 controle de publication"
$controle = Join-Path $PSScriptRoot 'controle_publication.py'
$arguments = @($controle, $Destination)
if ($FichierEnv) { $arguments += @('--env', $FichierEnv, '--variable', 'MERIDIEN_WIFI_SSID', '--variable', 'MERIDIEN_WIFI_MOT_DE_PASSE') }
foreach ($valeur in $Interdit) { $arguments += @('--interdit', $valeur) }
& python @arguments
if ($LASTEXITCODE -ne 0) { throw "Le controle de publication a trouve du bloquant : rien n'est commite. Le dossier est laisse tel quel : $Destination" }

Write-Host "3/4 depot neuf et un seul commit"
$message = @"
Initial public snapshot of Meridien

Snapshot of the author's private repository at commit $($sha.Substring(0, 7)) ($date), made with tools/instantane_public.ps1.
The history stays in the private archive; the commit numbers quoted in the documents point to it (see docs/publication.md).

Carte: aucune
Teste-sur: voir docs/compatibilite.md (instantane : rien de nouveau n'est teste)
"@
$fichierMessage = Join-Path ([IO.Path]::GetTempPath()) ("instantane_msg_" + [guid]::NewGuid().ToString('N') + '.txt')
[IO.File]::WriteAllText($fichierMessage, ($message -replace "`r`n", "`n"), (New-Object Text.UTF8Encoding $false))
Push-Location $Destination
try {
    Invoke-Git init -b main
    Invoke-Git config user.name $Nom
    Invoke-Git config user.email $Courriel
    # Le dossier ne contient QUE l'archive (comptee plus haut) : tout ajouter revient a ajouter la liste de la reference.
    Invoke-Git add --all
    Invoke-Git commit -q -F $fichierMessage
    Write-Host "4/4 identites de commit"
    & python $controle --historique
    if ($LASTEXITCODE -ne 0) { throw "Le controle de l'historique neuf a trouve du bloquant." }
    $neuf = (& git rev-parse --short HEAD).Trim()
} finally {
    Pop-Location
    if (Test-Path $fichierMessage) { Remove-Item -LiteralPath $fichierMessage -Force }
}
Write-Host "Instantane pret : $Destination (commit $neuf, $presents fichier(s)). Aucun depot distant, rien n'est publie."
