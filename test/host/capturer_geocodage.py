#!/usr/bin/env python3
"""Réponses RÉELLES du géocodage d'Open-Meteo pour les tests de components/core/include/ecran/geocodage.hpp.

    python test/host/capturer_geocodage.py --capturer   # RÉSEAU : interroge Open-Meteo, écrit donnees/geocodage_*.json
    python test/host/capturer_geocodage.py              # sans réseau : régénère donnees/geocodage_*.inc depuis les .json
    python test/host/capturer_geocodage.py --verifier   # sans réseau : code 1 si un .inc n'est pas à jour

Pourquoi : un analyseur testé sur du JSON écrit à la main ne prouve pas qu'il lit ce que le service envoie vraiment (champs
absents, accents, ordre, nombres). Les .json sont donc des captures, jamais éditées ; les .inc en sont le littéral C++ brut
`R"OM(...)OM"`, inclus par test_geocodage.cpp. Les requêtes reproduisent octet pour octet l'adresse que construit
`reglages::adresse_geocodage` (name=…&count=8&language=…&format=json, pourcentage en hexadécimal majuscule).

Le réseau n'est utilisé qu'avec --capturer : le contrôle de la CI et du crochet de commit n'en a pas besoin. Aucune clé, aucun
identifiant : l'API gratuite d'Open-Meteo ne demande rien.
"""
import argparse
import json
import sys
import urllib.parse
import urllib.request
from pathlib import Path

DONNEES = Path(__file__).resolve().parent / "donnees"
BASE = "https://geocoding-api.open-meteo.com/v1/search"

# (nom du fichier, texte cherché, langue) — chaque cas a une raison, écrite dans donnees/LISEZMOI_geocodage.md.
CAS = [
    ("marseille", "Marseille", "fr"),
    ("saint_etienne", "Saint-Étienne", "fr"),
    ("springfield", "Springfield", "en"),
    ("zurich", "Zürich", "de"),
    ("istanbul", "İstanbul", "tr"),
    ("paris", "Paris", "fr"),
    ("aucun_resultat", "xqzvbnmwkj", "fr"),
]


def adresse(texte, langue):
    # quote(safe="") laisse libres A-Z a-z 0-9 - _ . ~ et encode le reste en %XX majuscule : comme encoder_url() du noyau.
    return "%s?name=%s&count=8&language=%s&format=json" % (BASE, urllib.parse.quote(texte, safe=""), langue)


def chemin_json(nom):
    return DONNEES / ("geocodage_%s.json" % nom)


def chemin_inc(nom):
    return DONNEES / ("geocodage_%s.inc" % nom)


def contenu_inc(nom, texte, langue, octets):
    corps = octets.decode("utf-8")
    if ')OM"' in corps:
        sys.exit("le délimiteur )OM\" apparaît dans la réponse « %s » : choisir un autre délimiteur" % nom)
    return ("// FICHIER GÉNÉRÉ par test/host/capturer_geocodage.py : ne pas modifier à la main.\n"
            "// Réponse réelle du géocodage d'Open-Meteo pour « %s » (language=%s), voir donnees/LISEZMOI_geocodage.md.\n"
            'R"OM(%s)OM"\n' % (texte, langue, corps))


def libelle(*parties):
    return ", ".join(p for p in parties if p)


def attendu_de(nom):
    """Ce que `geocodage::analyser` DOIT lire dans la capture `nom`, déduit ici des règles écrites dans geocodage.hpp, avec le
    module json de Python (analyseur indépendant du noyau) : au plus 8 lieux, nom et fuseau non vides, coordonnées dans la plage,
    région et pays facultatifs, altitude gardée seulement dans [-500 ; 9000] m, population gardée si entière et positive."""
    donnees = json.loads(chemin_json(nom).read_bytes().decode("utf-8"))
    lieux = []
    for r in donnees.get("results", []):
        if len(lieux) == 8:
            break
        n = r.get("name") or ""
        tz = r.get("timezone") or ""
        lat, lon = r.get("latitude"), r.get("longitude")
        if not n or not tz or not isinstance(lat, (int, float)) or not isinstance(lon, (int, float)):
            continue
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            continue
        region = r.get("admin1") or ""
        pays = r.get("country") or ""
        alt = r.get("elevation")
        alt_ok = isinstance(alt, (int, float)) and -500 <= alt <= 9000
        pop = r.get("population")
        pop_ok = isinstance(pop, int) and pop >= 0
        ident = r.get("id")
        lieux.append(dict(id=ident if isinstance(ident, int) else None, nom=n, region=region, pays=pays,
                          code=r.get("country_code") or "", fuseau=tz, lat=float(lat), lon=float(lon),
                          alt=float(alt) if alt_ok else None, pop=pop if pop_ok else None,
                          libelle=libelle(n, region, pays)))
    return lieux


def contenu_attendu():
    """donnees/attendu_geocodage.inc : un tableau `AttenduLieu` par capture. Ordre des champs de AttenduLieu (déclaré dans
    test_geocodage.cpp) : id_connu, id, nom, region, pays, code_pays, fuseau, latitude, longitude, altitude_connue, altitude,
    population_connue, population, libelle."""
    def s(t):
        if ')AT"' in t:
            sys.exit("délimiteur )AT\" dans un texte attendu : %r" % t)
        return 'R"AT(%s)AT"' % t

    sortie = ["// FICHIER GÉNÉRÉ par test/host/capturer_geocodage.py : ne pas modifier à la main.",
              "// Ce que geocodage::analyser doit lire dans chaque capture, calculé par Python (json.loads), pas par le noyau.",
              "// Champs : id_connu, id, nom, region, pays, code_pays, fuseau, latitude, longitude, altitude_connue, altitude,",
              "//          population_connue, population, libelle."]
    for nom, _, _ in CAS:
        sortie.append("constexpr AttenduLieu ATTENDU_%s[] = {" % nom.upper())
        for x in attendu_de(nom):
            sortie.append("    {%s, %dLL, %s, %s, %s, %s, %s, %r, %r, %s, %r, %s, %dLL, %s}," % (
                "true" if x["id"] is not None else "false", x["id"] or 0, s(x["nom"]), s(x["region"]), s(x["pays"]),
                s(x["code"]), s(x["fuseau"]), x["lat"], x["lon"], "true" if x["alt"] is not None else "false",
                x["alt"] if x["alt"] is not None else 0.0, "true" if x["pop"] is not None else "false", x["pop"] or 0,
                s(x["libelle"])))
        if not attendu_de(nom):
            sortie.append("    {false, 0LL, \"\", \"\", \"\", \"\", \"\", 0.0, 0.0, false, 0.0, false, 0LL, \"\"}, // aucun lieu")
        sortie.append("};")
        sortie.append("constexpr std::size_t N_%s = %d;" % (nom.upper(), len(attendu_de(nom))))
    return "\n".join(sortie) + "\n"


def capturer():
    for nom, texte, langue in CAS:
        url = adresse(texte, langue)
        requete = urllib.request.Request(url, headers={"Accept": "application/json", "User-Agent": "meridien-tests/1"})
        with urllib.request.urlopen(requete, timeout=20) as r:
            if r.status != 200:
                sys.exit("HTTP %d pour %s" % (r.status, url))
            octets = r.read()
        octets.decode("utf-8")  # strict : une réponse qui n'est pas de l'UTF-8 valide n'est pas une capture utilisable
        chemin_json(nom).write_bytes(octets)
        print("capturé  %-16s %5d octets  %s" % (nom, len(octets), url))


def generer(verifier):
    ecarts = 0
    for nom, texte, langue in CAS:
        if not chemin_json(nom).exists():
            sys.exit("capture absente : %s (lancer --capturer, avec le réseau)" % chemin_json(nom))
        attendu = contenu_inc(nom, texte, langue, chemin_json(nom).read_bytes())
        cible = chemin_inc(nom)
        actuel = cible.read_bytes().decode("utf-8").replace("\r\n", "\n") if cible.exists() else None  # autocrlf=true sous Windows
        if verifier:
            if actuel != attendu:
                ecarts += 1
                print("PAS À JOUR : %s" % cible.name)
        elif actuel != attendu:
            cible.write_bytes(attendu.encode("utf-8"))
            print("écrit    %s" % cible.name)
    attendu = contenu_attendu()
    cible = DONNEES / "attendu_geocodage.inc"
    actuel = cible.read_bytes().decode("utf-8").replace("\r\n", "\n") if cible.exists() else None  # autocrlf=true sous Windows
    if verifier:
        if actuel != attendu:
            ecarts += 1
            print("PAS À JOUR : %s" % cible.name)
        print("tout est à jour" if ecarts == 0 else "%d fichier(s) à régénérer" % ecarts)
    elif actuel != attendu:
        cible.write_bytes(attendu.encode("utf-8"))
        print("écrit    %s" % cible.name)
    return 1 if ecarts else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--capturer", action="store_true", help="interroge Open-Meteo (réseau) et écrit les .json")
    ap.add_argument("--verifier", action="store_true", help="vérifie que les .inc correspondent aux .json")
    a = ap.parse_args()
    if a.capturer:
        capturer()
    return generer(a.verifier)


if __name__ == "__main__":
    sys.exit(main())
