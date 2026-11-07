#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des tests de la table des sources météo (components/core/include/ecran/fournisseurs_meteo.hpp).

    python test/host/controle_python_modeles.py              # (ré)génère donnees/openmeteo_modele_*.inc et modeles_liste.inc
    python test/host/controle_python_modeles.py --verifier   # ne modifie rien : échoue si un .inc n'est pas à jour

Entrées : test/host/donnees/openmeteo_modele_<id>_<ville>.json = réponses RÉELLES d'Open-Meteo à la requête COMPLÈTE de l'écran
(24 heures, 7 jours, UV, pluie par quarts d'heure) avec `&models=<valeur>`, capturées par test/host/capturer_modeles_meteo.py
(voir donnees/LISEZMOI.md). Une réponse par source de la table, plus une par pays que la table annonce pour cette source.
Sortie (générée, ne pas modifier à la main) :
  openmeteo_modele_<id>_<ville>.inc   la réponse en littéral brut C++ (copie octet par octet du .json)
  modeles_liste.inc                   une ligne par réponse : id, ville, pays, instant de réception, LIMITES, verdict attendu du
                                      lecteur, nombre d'heures / jours / quarts d'heure attendus, puis la réponse elle-même

Les LIMITES et le verdict sont calculés ici en regardant les `null` du JSON (json.loads), sans reprendre une ligne du noyau C++ :
  SANS_UV              un des jours (dont les codes de ciel existent) n'a pas d'`uv_index_max`
  SANS_RAFALES         `current.wind_gusts_10m` est null
  SANS_PROBA           un des jours (dont les codes de ciel existent) n'a pas de `precipitation_probability_max`
  JOURS_COURTS         moins de sept jours, ou un des sept sans code de ciel / température
  SANS_ENSOLEILLEMENT  un des jours (dont les codes de ciel existent) n'a pas de `sunshine_duration`
Le lecteur du firmware REFUSE la réponse entière pour les quatre premières (et pour une pression au niveau de la mer vide) :
c'est ce que dit meteo_openmeteo.hpp ; `test_fournisseurs_meteo.cpp` vérifie que le vrai lecteur fait bien cela sur ces réponses.
"""
import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_modeles.py : ne pas modifier à la main.\n"
HEURES_MAX, QUARTS_MAX, JOURS_MAX = 24, 9, 7
RECU_APRES = 100  # l'appareil « reçoit » la réponse 100 s après l'instant de la capture (comme test_meteo_riche.cpp)

# bits de ecran::meteo::fournisseurs::limite (fournisseurs_meteo.hpp) : écrits ici une fois, comparés à la table par le test C++
SANS_UV, SANS_RAFALES, SANS_PROBA, JOURS_COURTS, SANS_ENSOLEILLEMENT = 1, 2, 4, 8, 16
REFUS = SANS_UV | SANS_RAFALES | SANS_PROBA | JOURS_COURTS

# (identifiant de la table, ville, code pays ISO) : l'ordre est celui de modeles_liste.inc
REPONSES = [
    ("auto", "Lyon", "FR"),
    ("meteofrance", "Lyon", "FR"),
    ("meteofrance", "Saint-Denis (La Réunion)", "RE"),
    ("dwd", "Berlin", "DE"),
    ("metoffice", "Londres", "GB"),
    ("metno", "Oslo", "NO"),
    ("metno", "Stockholm", "SE"),
    ("metno", "Helsinki", "FI"),
    ("metno", "Copenhague", "DK"),
    ("metno", "Reykjavik", "IS"),
    ("metno", "Tallinn", "EE"),
    ("dmi", "Copenhague", "DK"),
    ("knmi", "Amsterdam", "NL"),
    ("knmi", "Bruxelles", "BE"),
    ("knmi", "Luxembourg", "LU"),
    ("meteosuisse", "Berne", "CH"),
    ("jma", "Tokyo", "JP"),
    ("gem", "Ottawa", "CA"),
    ("geosphere", "Vienne", "AT"),
    ("arpae", "Rome", "IT"),
    ("chmi", "Prague", "CZ"),
    ("noaa", "New York", "US"),
    ("noaa", "Lyon", "FR"),
    ("noaa", "Tokyo", "JP"),
    ("noaa", "Sydney", "AU"),
    ("noaa", "Sao Paulo", "BR"),
    ("noaa", "Le Cap", "ZA"),
    ("noaa", "Ottawa", "CA"),
    ("ecmwf", "Istanbul", "TR"),
]


def ascii_minuscule(texte):
    sans_accent = unicodedata.normalize("NFKD", texte).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "_", sans_accent.lower()).strip("_")


def nom_fichier(ident, ville):
    return f"openmeteo_modele_{ident}_{ascii_minuscule(ville)}"


def charger(ident, ville):
    brut = (DONNEES / f"{nom_fichier(ident, ville)}.json").read_bytes()
    return brut, json.loads(brut.decode("utf-8"))


def limites_et_verdict(doc):
    """(bits de limite, accepté par le lecteur ?, heures, jours, quarts) vus dans les `null` de la réponse."""
    cur, h, d, m = doc["current"], doc["hourly"], doc["daily"], doc["minutely_15"]
    jours = min(len(d["time"]), JOURS_MAX)

    def nul(col, i):
        v = d.get(col)
        return v is None or i >= len(v) or v[i] is None

    # un jour est « tronqué » quand son code de ciel ou une température manque : c'est JOURS_COURTS, pas une limite de plus
    tronque = [nul("weather_code", i) or nul("temperature_2m_max", i) or nul("temperature_2m_min", i) for i in range(jours)]
    bits = 0
    if len(d["time"]) < JOURS_MAX or any(tronque):
        bits |= JOURS_COURTS
    entiers = [i for i in range(jours) if not tronque[i]]
    if any(nul("uv_index_max", i) for i in entiers):
        bits |= SANS_UV
    if any(nul("precipitation_probability_max", i) for i in entiers):
        bits |= SANS_PROBA
    if any(nul("sunshine_duration", i) for i in entiers):
        bits |= SANS_ENSOLEILLEMENT
    if cur.get("wind_gusts_10m") is None:
        bits |= SANS_RAFALES
    pression_vide = cur.get("pressure_msl") is None
    accepte = (bits & REFUS) == 0 and not pression_vide
    heures = min(len(h["time"]) - 1, HEURES_MAX)
    quarts = min(len(m["time"]), QUARTS_MAX)
    return bits, accepte, heures, jours, quarts


def inc_litteral(ident, ville, brut):
    texte = brut.decode("utf-8")
    assert ')OM"' not in texte
    return (ENTETE + f"// Réponse réelle d'Open-Meteo ({ident}, {ville}, requête complète du firmware + models=), "
            "voir donnees/LISEZMOI.md.\n" + f'R"OM({texte})OM"\n')


def liste_inc(lignes):
    sortie = [ENTETE, "// id, ville, pays, instant de réception, limites, accepté, heures, jours, quarts, puis la réponse.\n",
              "// accepté = 1 si le lecteur du firmware garde la réponse ; heures/jours/quarts = -1 quand elle est refusée.\n"]
    for ident, ville, pays, recu, bits, accepte, heures, jours, quarts, fichier in lignes:
        h, j, q = (heures, jours, quarts) if accepte else (-1, -1, -1)
        sortie.append(f'{{"{ident}", "{ville}", "{pays}", {recu}LL, {bits}, {1 if accepte else 0}, {h}, {j}, {q},\n')
        sortie.append(f'#include "{fichier}.inc"\n')
        sortie.append("},\n")
    return "".join(sortie)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    a = ap.parse_args()
    sorties, lignes, resume = {}, [], []
    for ident, ville, pays in REPONSES:
        brut, doc = charger(ident, ville)
        fichier = nom_fichier(ident, ville)
        sorties[f"{fichier}.inc"] = inc_litteral(ident, ville, brut)
        bits, accepte, heures, jours, quarts = limites_et_verdict(doc)
        recu = doc["current"]["time"] + RECU_APRES
        lignes.append((ident, ville, pays, recu, bits, accepte, heures, jours, quarts, fichier))
        resume.append(f"{ident:12s} {ville:26s} {pays}  limites={bits:2d}  {'accepte' if accepte else 'refuse '}  "
                      f"{heures}h {jours}j {quarts}q  {len(brut)} octets")
    sorties["modeles_liste.inc"] = liste_inc(lignes)
    ok = True
    for nom, texte in sorties.items():
        chemin = DONNEES / nom
        if a.verifier:
            # git peut convertir les fins de ligne (core.autocrlf) : on compare sans tenir compte de CRLF/LF
            if not chemin.exists() or chemin.read_bytes().replace(b"\r\n", b"\n") != texte.encode("utf-8"):
                print("PAS À JOUR :", nom)
                ok = False
        else:
            chemin.write_bytes(texte.encode("utf-8"))
    print("\n".join(resume))
    print("vérifié : tout est à jour" if a.verifier and ok else ("écrit" if not a.verifier else "ÉCHEC"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
