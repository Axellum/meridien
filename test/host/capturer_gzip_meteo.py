#!/usr/bin/env python3
"""Capture des VRAIES réponses gzip d'Open-Meteo (octets tels qu'ils passent sur le fil) pour les tests de ecran/gzip.hpp.

    python test/host/capturer_gzip_meteo.py                   # capture les lieux de LIEUX dans donnees/
    python test/host/capturer_gzip_meteo.py --seulement lyon
    python test/host/capturer_gzip_meteo.py --a-cote CHEMIN   # écrit ailleurs (pour comparer sans toucher aux fixtures)

La requête est celle de l'appareil (adresse LUE dans meteo_requete.hpp, comme capturer_meteo_riche.py) avec l'en-tête
`Accept-Encoding: gzip`, exactement ce que main/http_commun.cpp envoie. On garde le corps AVANT toute décompression :
`openmeteo_gzip_<lieu>.gz`. Le journal dit l'en-tête `Content-Encoding` reçu, la taille sur le fil et la taille décompressée.
Ce script appelle le réseau : à lancer à la main, pas dans le contrôle. Les fixtures ne changent que si on le relance ; les
tableaux d'octets et les valeurs attendues sont ensuite régénérés par test/host/controle_python_gzip.py.

API gratuite, usage non commercial, attribution CC BY 4.0 (https://open-meteo.com) ; ce script fait deux appels.
"""
import argparse
import gzip
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

from capturer_meteo_riche import adresse

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"

# nom du fichier, latitude e5, longitude e5
LIEUX = [
    ("lyon", 4574906, 484789),
    ("sydney", -3386785, 15120732),
]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seulement", help="noms de lieux séparés par des virgules")
    ap.add_argument("--a-cote", help="dossier de sortie (par défaut test/host/donnees)")
    a = ap.parse_args()
    sortie = Path(a.a_cote) if a.a_cote else DONNEES
    sortie.mkdir(parents=True, exist_ok=True)
    voulus = {m.strip() for m in a.seulement.split(",")} if a.seulement else None
    echecs = 0
    for nom, lat, lon in LIEUX:
        if voulus is not None and nom not in voulus:
            continue
        url = adresse(lat, lon)
        req = urllib.request.Request(url, headers={"Accept-Encoding": "gzip", "User-Agent": "meridien-capture/1 (tests gzip)"})
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                fil, code, codage = r.read(), r.status, r.headers.get("Content-Encoding", "")
        except urllib.error.HTTPError as e:
            fil, code, codage = e.read(), e.code, e.headers.get("Content-Encoding", "")
        except OSError as e:
            print(f"{nom}: ÉCHEC réseau ({e})")
            echecs += 1
            continue
        if code != 200 or codage != "gzip":
            print(f"{nom}: ÉCHEC HTTP {code}, Content-Encoding « {codage} » : {fil[:200]!r}")
            echecs += 1
            continue
        (sortie / f"openmeteo_gzip_{nom}.gz").write_bytes(fil)
        brut = gzip.decompress(fil)
        print(f"{nom}: {len(fil)} octets sur le fil, {len(brut)} décompressés, en-tête gzip {fil[:10].hex(' ')}")
        time.sleep(1)
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
