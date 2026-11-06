# Ecrit reglages_locaux.cmake (a la racine du projet, ignore par git) avec le nom et le mot de passe du Wi-Fi, lus dans un
# fichier .env. main/CMakeLists.txt en tire identifiants_wifi.h, dans le dossier de construction : les identifiants ne passent
# JAMAIS par git, le journal serie ni un document. Les valeurs ne sont pas affichees par ce script.
#
# Usage (PowerShell 5.1 ou 7) :
#   .\tools\preparer_wifi.ps1 -FichierEnv <chemin du .env>
# Le chemin du .env vient, dans cet ordre : du parametre -FichierEnv, de la variable d'environnement MERIDIEN_ENV, de la
# variable $FichierEnv de tools\local.ps1 (modele : tools\local.example.ps1).
# Variables lues dans le .env : MERIDIEN_WIFI_SSID et MERIDIEN_WIFI_MOT_DE_PASSE. Reseau 2,4 GHz requis (l'ESP32-C6 ne fait pas
# le 5 GHz).
param([string]$FichierEnv)

$ErrorActionPreference = 'Stop'

if (-not $FichierEnv) { $FichierEnv = $env:MERIDIEN_ENV }
if (-not $FichierEnv) {
    $locaux = @(Join-Path $PSScriptRoot 'local.ps1')
    $commun = $null
    try { $commun = git -C $PSScriptRoot rev-parse --path-format=absolute --git-common-dir 2>$null } catch { }
    if ($commun) { $locaux += Join-Path (Split-Path -Parent $commun) 'tools\local.ps1' }
    foreach ($local in $locaux) {
        if (Test-Path $local) {
            $FichierEnv = $null
            . $local
            if ($FichierEnv) { break }
        }
    }
}
if (-not $FichierEnv -or $FichierEnv -like '<*') {
    throw "Chemin du .env inconnu : passer -FichierEnv, definir MERIDIEN_ENV, ou renseigner `$FichierEnv dans tools\local.ps1."
}
if (-not (Test-Path -LiteralPath $FichierEnv)) { throw "Fichier .env introuvable : $FichierEnv" }

$lignes = Get-Content -LiteralPath $FichierEnv -Encoding UTF8

function Lire-Variable([string[]]$Lignes, [string]$Nom) {
    $motif = '^\s*(?:export\s+)?' + [regex]::Escape($Nom) + '\s*=\s*(.*?)\s*$'
    $valeur = $null
    foreach ($ligne in $Lignes) {
        $m = [regex]::Match($ligne, $motif)
        if ($m.Success) { $valeur = $m.Groups[1].Value } # la derniere definition l'emporte
    }
    if ($null -eq $valeur) { return $null }
    if ($valeur.Length -ge 2) {
        $premier = $valeur.Substring(0, 1)
        $dernier = $valeur.Substring($valeur.Length - 1, 1)
        if (($premier -eq '"' -and $dernier -eq '"') -or ($premier -eq "'" -and $dernier -eq "'")) {
            $valeur = $valeur.Substring(1, $valeur.Length - 2)
        }
    }
    return $valeur
}

$ssid = Lire-Variable $lignes 'MERIDIEN_WIFI_SSID'
$mdp = Lire-Variable $lignes 'MERIDIEN_WIFI_MOT_DE_PASSE'
$manquantes = @()
if (-not $ssid) { $manquantes += 'MERIDIEN_WIFI_SSID' }
if (-not $mdp) { $manquantes += 'MERIDIEN_WIFI_MOT_DE_PASSE' }
if ($manquantes.Count -gt 0) {
    throw ("Variable(s) absente(s) ou vide(s) dans le .env : " + ($manquantes -join ', ') + ". Rien n'a ete ecrit.")
}
foreach ($valeur in $ssid, $mdp) {
    if ($valeur.Contains(']==]')) { throw "Une valeur contient la suite ']==]', que le fichier CMake ne peut pas porter." }
}
$octetsSsid = [System.Text.Encoding]::UTF8.GetByteCount($ssid)
$octetsMdp = [System.Text.Encoding]::UTF8.GetByteCount($mdp)
if ($octetsSsid -gt 32) { throw "SSID trop long : 32 octets au plus." }
if ($octetsMdp -gt 64) { throw "Mot de passe trop long : 64 octets au plus." }
if ($octetsMdp -lt 8) { Write-Warning "Mot de passe de moins de 8 octets : un reseau WPA2 le refusera." }

$projet = Split-Path -Parent $PSScriptRoot
$sortie = Join-Path $projet 'reglages_locaux.cmake'
$horodatage = Get-Date -Format 'yyyy-MM-dd HH:mm'
$contenu = "# Genere par tools\preparer_wifi.ps1 le $horodatage. Hors git. Contient le mot de passe Wi-Fi EN CLAIR : ne pas copier.`n" +
    "set(MERIDIEN_WIFI_SSID_BRUT [==[$ssid]==])`n" +
    "set(MERIDIEN_WIFI_MOT_DE_PASSE_BRUT [==[$mdp]==])`n"
[System.IO.File]::WriteAllText($sortie, $contenu, (New-Object System.Text.UTF8Encoding($false)))

# Garde-fou : le fichier doit etre ignore par git, sinon on le supprime.
git -C $projet check-ignore -q -- 'reglages_locaux.cmake'
if ($LASTEXITCODE -ne 0) {
    Remove-Item -LiteralPath $sortie -Force
    throw "reglages_locaux.cmake n'est pas ignore par git (.gitignore) : fichier supprime, rien n'est ecrit."
}

# Force CMake a relire les reglages au prochain build, meme si le fichier vient d'etre cree.
(Get-Item -LiteralPath (Join-Path $projet 'main\CMakeLists.txt')).LastWriteTime = Get-Date

Write-Host "reglages_locaux.cmake ecrit : SSID et mot de passe presents (valeurs non affichees). Fichier ignore par git."
Write-Host "Reseau 2,4 GHz requis. Le binaire compile contient ces valeurs en clair : ne pas le partager."
