#!/usr/bin/env python3
"""Capture, pour chaque modèle national d'Open-Meteo, la VRAIE requête complète de l'écran et dit ce qui revient.

    python test/host/capturer_modeles_meteo.py                      # tous les modèles de MODELES, résumé à l'écran
    python test/host/capturer_modeles_meteo.py --seulement jma_seamless,dmi_seamless
    python test/host/capturer_modeles_meteo.py --sortie CHEMIN      # garde aussi chaque réponse brute (sinon : dossier temporaire)
    python test/host/capturer_modeles_meteo.py --candidats          # essaie aussi des identifiants non retenus (existent-ils ?)

L'adresse est celle de l'appareil : LUE dans components/core/include/ecran/meteo_requete.hpp (REQUETE_DEBUT, REQUETE_MILIEU,
REQUETE_FIN) puis `&models=<valeur>` ; jamais recopiée ici. Pour chaque modèle, un point DANS son pays.

Trois verdicts par modèle :
  1. HTTP et taille de la réponse, non compressée et avec `Accept-Encoding: gzip` (ce que l'appareil enverra) ;
  2. variables vides (`null`) section par section, comptées ici en Python (courant, 24 heures, 7 jours, quarts d'heure) ;
  3. ce que le LECTEUR DU FIRMWARE en garde : test/host/analyser_reponse_riche.cpp appelle `analyser_riche()`, qui vide et masque
     déjà une section absente ou invraisemblable. C'est ce verdict-là qui compte pour l'écran.

Ce script appelle le réseau : à lancer à la main, pas dans le contrôle. API gratuite, usage non commercial, attribution CC BY 4.0
(https://open-meteo.com) ; limites 600 appels/min, 5 000/h, 10 000/jour : ce script fait une quarantaine d'appels, espacés d'une
seconde. Le compilateur C++ vient de la variable CXX (. _outils\\env_hote.ps1), à défaut de clang++ ou g++ du PATH.
"""
import argparse
import gzip
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[1]
REQUETE_HPP = RACINE / "components/core/include/ecran/meteo_requete.hpp"
INCLUDE_NOYAU = RACINE / "components/core/include"

# valeur de `models=`, ville, latitude e5, longitude e5, pays (code ISO)
MODELES = [
    ("meteofrance_seamless", "Lyon", 4574906, 484789, "FR"),
    ("icon_seamless", "Berlin", 5252000, 1340500, "DE"),
    ("ukmo_seamless", "Londres", 5150700, -12700, "GB"),
    ("metno_seamless", "Oslo", 5991400, 1075200, "NO"),
    ("dmi_seamless", "Copenhague", 5567600, 1256800, "DK"),
    ("knmi_seamless", "Amsterdam", 5237000, 489000, "NL"),
    ("meteoswiss_icon_seamless", "Berne", 4695200, 744600, "CH"),
    ("jma_seamless", "Tokyo", 3568900, 13969200, "JP"),
    ("gem_seamless", "Ottawa", 4542100, -7569300, "CA"),
    ("ncep_gfs013", "New York", 4071280, -7400600, "US"),
    ("gfs_seamless", "New York", 4071280, -7400600, "US"),
    ("ecmwf_ifs025", "Istanbul", 4100800, 2897800, "TR"),
    # Écartés par le rapport du 07/11/2026 (valeurs vides) : relus ici avec la requête complète
    ("kma_seamless", "Séoul", 3756650, 12697800, "KR"),
    ("bom_access_global", "Sydney", -3386785, 15120732, "AU"),
]

# Identifiants proposés par la doc d'Open-Meteo mais pas dans la liste du coordinateur : existent-ils ? rendent-ils l'essentiel ?
CANDIDATS = [
    ("geosphere_seamless", "Vienne", 4820800, 1637300, "AT"),
    ("italia_meteo_arpae_icon_2i", "Rome", 4189000, 1248000, "IT"),
    ("chmi_aladin_seamless", "Prague", 5008800, 1442100, "CZ"),
    ("metno_seamless", "Stockholm", 5933000, 1806800, "SE"),
    ("metno_seamless", "Helsinki", 6017000, 2494000, "FI"),
    ("meteofrance_seamless", "Saint-Denis (La Réunion)", -2087900, 5545000, "RE"),
    ("ecmwf_ifs025", "Madrid", 4041700, -370400, "ES"),
    ("jma_seamless", "Séoul (JMA hors Japon)", 3756650, 12697800, "KR"),
    ("gem_seamless", "New York (GEM hors Canada)", 4071280, -7400600, "US"),
    ("icon_seamless", "Lyon (ICON hors Allemagne)", 4574906, 484789, "FR"),
    ("cma_grapes_global", "Pékin", 3990400, 11640700, "CN"),
]

# Deuxième série d'essais (07/11/2026) : l'étendue des modèles qui rendent tout, et les variantes non « seamless »
COMPLEMENTS = [
    ("gfs_seamless", "Lyon (GFS hors États-Unis)", 4574906, 484789, "FR"),
    ("gfs_seamless", "Tokyo (GFS hors États-Unis)", 3568900, 13969200, "JP"),
    ("gfs_seamless", "Sydney (GFS hors États-Unis)", -3386785, 15120732, "AU"),
    ("gfs_seamless", "Sao Paulo (GFS hors États-Unis)", -2355000, -4663300, "BR"),
    ("gfs_seamless", "Le Cap (GFS hors États-Unis)", -3392500, 1842400, "ZA"),
    ("gfs_seamless", "Ottawa (GFS Canada)", 4542100, -7569300, "CA"),
    ("knmi_seamless", "Bruxelles", 5085000, 435200, "BE"),
    ("knmi_seamless", "Luxembourg", 4961100, 613000, "LU"),
    ("knmi_seamless", "Lyon (KNMI hors Pays-Bas)", 4574906, 484789, "FR"),
    ("metno_seamless", "Copenhague (MET Norway hors Nordique ?)", 5567600, 1256800, "DK"),
    ("metno_seamless", "Reykjavik", 6414600, -2190400, "IS"),
    ("metno_seamless", "Tallinn", 5943700, 2475300, "EE"),
    ("metno_seamless", "Lyon (MET Norway hors Nordique)", 4574906, 484789, "FR"),
    ("metno_seamless", "Tokyo (MET Norway hors Nordique)", 3568900, 13969200, "JP"),
    ("meteofrance_arome_france", "Lyon", 4574906, 484789, "FR"),
    ("meteofrance_arpege_europe", "Lyon", 4574906, 484789, "FR"),
    ("meteofrance_arome_france_hd", "Lyon", 4574906, 484789, "FR"),
    ("ecmwf_aifs025_single", "Madrid", 4041700, -370400, "ES"),
    ("ukmo_uk_deterministic_2km", "Londres", 5150700, -12700, "GB"),
    ("ukmo_global_deterministic_10km", "Londres", 5150700, -12700, "GB"),
    ("jma_msm", "Tokyo", 3568900, 13969200, "JP"),
    ("ncep_hrrr_conus", "New York", 4071280, -7400600, "US"),
    ("ncep_nbm_conus", "New York", 4071280, -7400600, "US"),
    ("gem_global", "Ottawa", 4542100, -7569300, "CA"),
    ("gem_hrdps_continental", "Ottawa", 4542100, -7569300, "CA"),
]

# Contrôle : la requête de l'écran sans `models=` (le comportement actuel, « Automatique »)
AUTOMATIQUE = (None, "Lyon", 4574906, 484789, "FR")


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


def adresse(lat_e5: int, lon_e5: int, modele) -> str:
    src = REQUETE_HPP.read_text(encoding="utf-8")
    url = (morceaux(src, "REQUETE_DEBUT") + degres_e5(lat_e5) + morceaux(src, "REQUETE_MILIEU") + degres_e5(lon_e5)
           + morceaux(src, "REQUETE_FIN"))
    return url + (f"&models={modele}" if modele else "")


def telecharger(url: str, gzip_ok: bool):
    """(statut, octets reçus sur le fil, corps décompressé)."""
    req = urllib.request.Request(url, headers={"Accept-Encoding": "gzip" if gzip_ok else "identity",
                                               "User-Agent": "meridien-capture/1 (essai de modeles)"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            fil, code, codage = r.read(), r.status, r.headers.get("Content-Encoding", "")
    except urllib.error.HTTPError as e:
        fil, code, codage = e.read(), e.code, e.headers.get("Content-Encoding", "")
    corps = gzip.decompress(fil) if codage == "gzip" else fil
    return code, len(fil), corps


def valeurs_vides(doc: dict) -> dict:
    """Section -> {variable: (nombre de null, longueur)} pour les seules variables qui ont au moins un null."""
    r = {}
    for section in ("current", "hourly", "daily", "minutely_15"):
        bloc = doc.get(section)
        if not isinstance(bloc, dict):
            r[section] = "ABSENTE"
            continue
        vides = {}
        for nom, v in bloc.items():
            if nom in ("time", "interval"):
                continue
            if isinstance(v, list):
                n = sum(1 for x in v if x is None)
                if n:
                    vides[nom] = (n, len(v))
            elif v is None:
                vides[nom] = (1, 1)
        r[section] = vides
    return r


def compiler_outil(dossier: Path) -> Path:
    cxx = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not cxx:
        raise SystemExit("aucun compilateur C++ (CXX, clang++ ou g++)")
    exe = dossier / ("analyser_reponse_riche.exe" if os.name == "nt" else "analyser_reponse_riche")
    cmd = [cxx, "-std=c++17", "-O1", "-I", str(INCLUDE_NOYAU), "-fconstexpr-steps=2000000000",
           str(ICI / "analyser_reponse_riche.cpp"), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        # g++ n'a pas le même nom pour la limite d'évaluation constante
        cmd = [c for c in cmd if not c.startswith("-fconstexpr-steps")] + ["-fconstexpr-ops-limit=2000000000"]
        r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise SystemExit("compilation de l'outil d'analyse en échec :\n" + r.stderr[:2000])
    return exe


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seulement", help="valeurs de models= séparées par des virgules")
    ap.add_argument("--sortie", help="dossier où garder les réponses brutes (par défaut : temporaire, effacé)")
    ap.add_argument("--candidats", action="store_true", help="essaie aussi des identifiants non retenus")
    ap.add_argument("--complements", action="store_true", help="deuxième série : étendue des modèles complets, variantes")
    a = ap.parse_args()
    liste = [AUTOMATIQUE] + MODELES + (CANDIDATS if a.candidats else []) + (COMPLEMENTS if a.complements else [])
    if a.complements and not a.candidats and not a.seulement:
        liste = [AUTOMATIQUE] + COMPLEMENTS
    if a.seulement:
        voulus = {m.strip() for m in a.seulement.split(",")}
        liste = [m for m in liste if (m[0] or "automatique") in voulus]
    with tempfile.TemporaryDirectory() as tmp:
        sortie = Path(a.sortie) if a.sortie else Path(tmp)
        sortie.mkdir(parents=True, exist_ok=True)
        exe = compiler_outil(Path(tmp))
        lignes = []
        for modele, ville, lat, lon, pays in liste:
            nom = re.sub(r"[^A-Za-z0-9_.-]+", "_", f"{modele or 'automatique'}_{ville}").strip("_")
            url = adresse(lat, lon, modele)
            instant = int(time.time())
            code, fil_gz, corps = telecharger(url, True)
            code0, fil_brut, _ = telecharger(url, False)
            time.sleep(1)
            fichier = sortie / f"{nom}.json"
            fichier.write_bytes(corps)
            ligne = {"modele": modele or "(automatique)", "ville": ville, "pays": pays, "http": code, "http_sans_gzip": code0,
                     "octets_gzip": fil_gz, "octets_bruts": fil_brut, "url_octets": len(url)}
            if code != 200:
                ligne["erreur"] = corps[:200].decode("utf-8", "replace")
            else:
                try:
                    ligne["vides"] = valeurs_vides(json.loads(corps))
                except ValueError as e:
                    ligne["erreur"] = f"JSON illisible : {e}"
                v = subprocess.run([str(exe), str(fichier), str(instant)], capture_output=True, text=True,
                                   encoding="utf-8", errors="replace")
                ligne["firmware"] = v.stdout.strip().split(" | ", 1)[-1] if v.stdout.strip() else v.stderr.strip()
            lignes.append(ligne)
            print(json.dumps(ligne, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
