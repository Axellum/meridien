#!/usr/bin/env python3
"""Génère components/core/include/ecran/fuseaux_donnees.inc depuis posix_tz_db/zones.csv.

La table sert à passer d'un nom de fuseau IANA (donné par le géocodage d'Open-Meteo) à la chaîne POSIX TZ que
comprend `setenv("TZ", ...)` + `tzset()`. Fichier trié par octets sur le nom IANA : la recherche est dichotomique.

Compléments : posix_tz_db (commit du 2025-09-29) ne contient pas cinq zones de la base tz actuelle, dont
Europe/Kyiv que le géocodage d'Open-Meteo renvoie réellement, ni les noms « UTC » et « GMT ». Chaque
complément cite sa source (fichiers de
https://github.com/eggert/tz lus le 06/11/2026) ; le script refuse un complément devenu inutile.

Usage : python generer_fuseaux.py
"""
import csv
import re
import sys
from pathlib import Path

ICI = Path(__file__).resolve().parent
ENTREE = ICI / "posix_tz_db" / "zones.csv"
SORTIE = ICI.parent / "include" / "ecran" / "fuseaux_donnees.inc"
COMMIT = "93447c0ddac304ca6672a5fd905261c7e8905159"
DATE_COMMIT = "2025-09-29"

# nom -> ("alias", nom déjà présent dans zones.csv) ou ("posix", chaîne), avec la source de la décision.
COMPLEMENTS = {
    # fichier backward : « Link Europe/Kyiv Europe/Kiev » (Kyiv est le nom actuel de Kiev)
    "Europe/Kyiv": ("alias", "Europe/Kiev"),
    # fichier backward : « Link Pacific/Kanton Pacific/Enderbury »
    "Pacific/Kanton": ("alias", "Pacific/Enderbury"),
    # fichier northamerica : dernière ligne de la zone = « -7:00 US M%sT », comme America/Denver
    "America/Ciudad_Juarez": ("alias", "America/Denver"),
    # fichier southamerica : dernière ligne de la zone = « -3:00 - %z » (depuis le 20/03/2025)
    "America/Coyhaique": ("posix", "<-03>3"),
    # fichier asia : dernière ligne de la zone = « 5:00 - %z »
    "Asia/Qostanay": ("posix", "<+05>-5"),
    # fichier backward : « Link Etc/UTC UTC » ; fichier etcetera : « Link Etc/GMT GMT »
    "UTC": ("alias", "Etc/UTC"),
    "GMT": ("alias", "Etc/GMT"),
}

NOM_OK = re.compile(r"[A-Za-z0-9_+\-/]+")
POSIX_OK = re.compile(r"[A-Za-z0-9<>+\-:,./]+")


def main() -> int:
    table = {}
    with ENTREE.open(encoding="utf-8", newline="") as f:
        for ligne in csv.reader(f):
            nom, posix = ligne
            if not NOM_OK.fullmatch(nom) or not POSIX_OK.fullmatch(posix):
                print(f"caractères inattendus : {ligne}", file=sys.stderr)
                return 1
            if nom in table:
                print(f"doublon : {nom}", file=sys.stderr)
                return 1
            table[nom] = posix
    de_la_source = len(table)

    complements = {}
    for nom, (genre, valeur) in COMPLEMENTS.items():
        if nom in table:
            print(f"complément inutile (déjà dans zones.csv) : {nom}", file=sys.stderr)
            return 1
        posix = table[valeur] if genre == "alias" else valeur
        if not POSIX_OK.fullmatch(posix):
            print(f"chaîne POSIX inattendue pour {nom} : {posix}", file=sys.stderr)
            return 1
        complements[nom] = posix
    table.update(complements)

    lignes = [
        "// FICHIER GÉNÉRÉ par components/core/outils/generer_fuseaux.py : ne pas modifier à la main.",
        f"// Source : posix_tz_db (MIT), commit {COMMIT[:7]} du {DATE_COMMIT} : {de_la_source} zones,",
        f"// plus {len(complements)} compléments tirés de la base tz (voir ATTRIBUTION.md).",
        "// {nom IANA, chaîne POSIX TZ}, trié par octets sur le nom IANA.",
    ]
    for nom in sorted(table, key=lambda n: n.encode("ascii")):
        marque = "  // complément" if nom in complements else ""
        lignes.append(f'{{"{nom}", "{table[nom]}"}},{marque}')
    SORTIE.parent.mkdir(parents=True, exist_ok=True)
    SORTIE.write_text("\n".join(lignes) + "\n", encoding="utf-8", newline="\n")
    print(f"écrit {SORTIE} : {len(table)} zones ({de_la_source} + {len(complements)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
