#!/usr/bin/env python3
"""Capture des VRAIES réponses d'Open-Meteo pour les tests de la météo riche (meteo_riche.hpp).

    python test/host/capturer_meteo_riche.py                  # capture tous les lieux de LIEUX dans donnees/
    python test/host/capturer_meteo_riche.py --seulement lyon,bergen
    python test/host/capturer_meteo_riche.py --a-cote CHEMIN  # écrit ailleurs (pour comparer sans toucher aux fixtures)

L'adresse est celle de l'appareil : elle est LUE dans components/core/include/ecran/meteo_requete.hpp (REQUETE_DEBUT,
REQUETE_MILIEU, REQUETE_FIN), jamais recopiée ici, pour que la capture et le firmware demandent exactement la même chose.
Les coordonnées sont écrites comme le fait le firmware (5 décimales, « e5 »).

Ce script appelle le réseau : à lancer à la main, pas dans le contrôle. Les fixtures ne changent que si on le relance ; les
valeurs attendues sont ensuite recalculées par test/host/controle_python_riche.py.

API gratuite, usage non commercial, attribution CC BY 4.0 (https://open-meteo.com) ; limites 600 appels/min, 5 000/h,
10 000/jour : ce script en fait une dizaine, espacés d'une seconde.
"""
import argparse
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[1]
DONNEES = ICI / "donnees"
REQUETE_HPP = RACINE / "components/core/include/ecran/meteo_requete.hpp"

# nom du fichier, latitude e5, longitude e5, ce que ce lieu apporte aux tests
LIEUX = [
    ("lyon", 4574906, 484789, "cas courant, Europe, heure d'hiver (UTC+1)"),
    ("longyearbyen", 7822000, 1562000, "nuit polaire : lever == coucher, ensoleillement nul ; neige"),
    ("seattle", 4760621, -12233207, "longitude négative (UTC-8) ; pluie en cours, par quarts d'heure"),
    ("singapour", 129000, 10385000, "tropiques, jour ; fuseau sans changement d'heure"),
    ("sydney", -3386785, 15120732, "hémisphère sud ; heure d'été (UTC+11) ; latitude négative"),
    ("katmandou", 2771720, 8532400, "fuseau à +5 h 45 : les séries commencent 45 min AVANT l'heure courante"),
    ("terreneuve", 4756000, -5271000, "fuseau à -3 h 30 (hiver) : les séries commencent 30 min APRÈS l'heure courante"),
]


def morceaux(source: str, nom: str) -> str:
    """Valeur d'une constante `std::string_view NOM = "…" "…";` : les littéraux voisins sont recollés."""
    m = re.search(r"std::string_view\s+" + nom + r"\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", source)
    if not m:
        raise SystemExit(f"{nom} introuvable dans {REQUETE_HPP}")
    return "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))


def degres_e5(v: int) -> str:
    signe = "-" if v < 0 else ""
    a = abs(v)
    return f"{signe}{a // 100000}.{a % 100000:05d}"


def adresse(lat_e5: int, lon_e5: int) -> str:
    src = REQUETE_HPP.read_text(encoding="utf-8")
    return (morceaux(src, "REQUETE_DEBUT") + degres_e5(lat_e5) + morceaux(src, "REQUETE_MILIEU") + degres_e5(lon_e5)
            + morceaux(src, "REQUETE_FIN"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seulement", help="noms de lieux séparés par des virgules")
    ap.add_argument("--a-cote", help="dossier de sortie (par défaut test/host/donnees)")
    a = ap.parse_args()
    sortie = Path(a.a_cote) if a.a_cote else DONNEES
    sortie.mkdir(parents=True, exist_ok=True)
    voulus = {m.strip() for m in a.seulement.split(",")} if a.seulement else None
    echecs = 0
    for nom, lat, lon, _ in LIEUX:
        if voulus is not None and nom not in voulus:
            continue
        url = adresse(lat, lon)
        try:
            with urllib.request.urlopen(url, timeout=30) as r:
                brut, code = r.read(), r.status
        except urllib.error.HTTPError as e:
            brut, code = e.read(), e.code
        except OSError as e:
            print(f"{nom}: ÉCHEC réseau ({e})")
            echecs += 1
            continue
        if code != 200 or b'"error"' in brut[:80]:
            print(f"{nom}: ÉCHEC HTTP {code} : {brut[:200]!r}")
            echecs += 1
            continue
        (sortie / f"openmeteo_riche_{nom}.json").write_bytes(brut)
        print(f"{nom}: {len(brut)} octets, adresse de {len(url)} octets")
        time.sleep(1)
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
