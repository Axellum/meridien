#!/usr/bin/env python3
"""Capture des VRAIES réponses de Vigie pour les tests des alertes (vigie_alertes.hpp, vigie_vue.hpp).

    python test/host/capturer_vigie.py                          # capture tous les lieux de LIEUX dans donnees/
    python test/host/capturer_vigie.py --seulement la_palma,oslo
    python test/host/capturer_vigie.py --a-cote CHEMIN          # écrit ailleurs (pour comparer sans toucher aux fixtures)
    python test/host/capturer_vigie.py --langue en              # langue de l'écran simulée (défaut : fr, comme l'appareil par défaut)

L'adresse est celle de l'appareil : l'adresse publique et les morceaux du chemin sont LUS dans
components/core/include/ecran/vigie_requete.hpp (ADRESSE_PUBLIQUE, REQUETE_DEBUT, REQUETE_MILIEU, REQUETE_LANGUE, LANGUE_DE_SECOURS),
jamais recopiés ici. Les coordonnées sont arrondies comme le fait le firmware (centaines de millièmes e5 -> millièmes, la moitié loin
de zéro). Comme le firmware, le script demande « Accept-Encoding: gzip » : Vigie ne compresse que les corps d'au moins ≈ 500 octets.

Pour chaque lieu : `vigie_<nom>.json` = le corps tel que l'API l'a rendu (décompressé, octet pour octet), `vigie_<nom>.gz` = les octets
reçus sur le fil quand ils étaient compressés (sert à tester l'en-tête gzip), et une ligne dans `vigie_captures.json` (adresse, instant
du serveur, tailles, en-têtes utiles). Ce script appelle le réseau : à lancer à la main, pas dans le contrôle. Les alertes changent d'une
heure à l'autre : une fixture n'est valable qu'à l'instant de sa capture (`server_utc`) ; c'est cet instant que les tests prennent
pour « maintenant ». L'instance publique est limitée par adresse et « pas faite pour bâtir un service dessus » : ce script fait une
douzaine d'appels espacés de deux secondes.
"""
import argparse
import gzip
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[1]
DONNEES = ICI / "donnees"
REQUETE_HPP = RACINE / "components/core/include/ecran/vigie_requete.hpp"

# nom du fichier, latitude e5, longitude e5, ce que ce lieu apporte aux tests
LIEUX = [
    ("la_palma", 2865850, -1791820, "Espagne (AEMET, service du registre OMM) : plusieurs avertissements orange d'un coup"),
    ("eberswalde", 5283490, 1380730, "Allemagne (DWD) : gel jaune, texte de l'émetteur en huit langues, paramètres"),
    ("oslo", 5991390, 1075220, "Norvège (MeteoAlarm) : neige jaune, texte en norvégien et en anglais"),
    ("bodo", 6728040, 1440490, "Norvège : vent jaune, état « update » (mise à jour)"),
    ("londres", 5150740, -12780, "Royaume-Uni (MeteoAlarm) : brouillard jaune, texte en anglais seulement"),
    ("funchal", 3266690, -1692410, "Portugal : AUCUN avertissement, mais 45 sur 45 ne peuvent pas être rattachés à un point"),
    ("ancona", 4361580, 1351890, "Italie : aucun avertissement, 4 non rattachables"),
    ("hambourg", 5355110, 999370, "Allemagne : rien en vigueur, couverture saine (le « pas d'alerte » ordinaire)"),
    ("valence", 4493340, 489240, "France (Drôme) : rien en vigueur, ce que Vigie rend pour la France"),
    ("tokyo", 3567620, 13965030, "Japon : pays hors des sources de cette instance (champ `uncovered`) ou service du registre OMM"),
]


def morceaux(source: str, nom: str) -> str:
    """Valeur d'une constante `std::string_view NOM = "…";` : les littéraux voisins sont recollés."""
    m = re.search(r"std::string_view\s+" + nom + r"\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", source)
    if not m:
        raise SystemExit(f"{nom} introuvable dans {REQUETE_HPP}")
    return "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))


def e5_vers_e3(v: int) -> int:
    r = (abs(v) + 50) // 100
    return -r if v < 0 else r


def degres_e3(v: int) -> str:
    return f"{'-' if v < 0 else ''}{abs(v) // 1000}.{abs(v) % 1000:03d}"


def adresse(lat_e5: int, lon_e5: int, langue: str) -> str:
    src = REQUETE_HPP.read_text(encoding="utf-8")
    base = morceaux(src, "ADRESSE_PUBLIQUE")
    secours = morceaux(src, "LANGUE_DE_SECOURS")
    langues = langue if langue == secours else f"{langue},{secours}"
    return (base + morceaux(src, "REQUETE_DEBUT") + degres_e3(e5_vers_e3(lat_e5)) + morceaux(src, "REQUETE_MILIEU")
            + degres_e3(e5_vers_e3(lon_e5)) + morceaux(src, "REQUETE_LANGUE") + langues)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seulement", help="noms de lieux séparés par des virgules")
    ap.add_argument("--a-cote", help="dossier de sortie (par défaut test/host/donnees)")
    ap.add_argument("--langue", default="fr", help="code de la langue de l'écran (défaut fr)")
    a = ap.parse_args()
    sortie = Path(a.a_cote) if a.a_cote else DONNEES
    sortie.mkdir(parents=True, exist_ok=True)
    voulus = {m.strip() for m in a.seulement.split(",")} if a.seulement else None
    index = {}
    index_fichier = sortie / "vigie_captures.json"
    if index_fichier.exists():
        index = json.loads(index_fichier.read_text(encoding="utf-8"))
    echecs = 0
    for nom, lat, lon, _ in LIEUX:
        if voulus is not None and nom not in voulus:
            continue
        url = adresse(lat, lon, a.langue)
        requete = urllib.request.Request(url, headers={"Accept-Encoding": "gzip", "User-Agent": "meridien-tests/1"})
        try:
            with urllib.request.urlopen(requete, timeout=30) as r:
                fil, code, entetes = r.read(), r.status, r.headers
        except urllib.error.HTTPError as erreur:
            fil, code, entetes = erreur.read(), erreur.code, erreur.headers
        except OSError as erreur:
            print(f"  ECHEC   {nom}: {erreur}")
            echecs += 1
            continue
        compresse = fil[:2] == b"\x1f\x8b"
        corps = gzip.decompress(fil) if compresse else fil
        if code != 200:
            print(f"  ECHEC   {nom}: HTTP {code} {corps[:200]!r}")
            echecs += 1
            continue
        donnees = json.loads(corps)
        (sortie / f"vigie_{nom}.json").write_bytes(corps)
        fichier_gz = sortie / f"vigie_{nom}.gz"
        if compresse:
            fichier_gz.write_bytes(fil)
        elif fichier_gz.exists():
            fichier_gz.unlink()
        index[nom] = {
            "url": url,
            "http": code,
            "serveur_utc": donnees["server_utc"],
            "octets_sur_le_fil": len(fil),
            "octets_decompresses": len(corps),
            "content_encoding": entetes.get("Content-Encoding"),
            "transfer_encoding": entetes.get("Transfer-Encoding"),
            "content_length": entetes.get("Content-Length"),
            "cache_control": entetes.get("Cache-Control"),
            "alertes": len(donnees["vigilances"]),
        }
        print(f"  ok      {nom}: {len(fil)} octets sur le fil, {len(corps)} décompressés, {len(donnees['vigilances'])} alerte(s)")
        time.sleep(2)
    index_fichier.write_text(json.dumps(index, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
