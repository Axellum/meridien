#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des tests du noyau « météo et heure » : Python (json.loads, decimal, zoneinfo) calcule les
valeurs attendues, le C++ des tests les compare à ce que lit/calcule le noyau (évalué à la compilation).

    python test/host/controle_python.py              # (ré)génère les fichiers donnees/*.inc
    python test/host/controle_python.py --verifier   # ne modifie rien : échoue si un .inc n'est pas à jour

Entrées : test/host/donnees/openmeteo_<nom>.json = réponses RÉELLES d'Open-Meteo (voir donnees/LISEZMOI.md).
Sorties (générées, ne pas modifier à la main) :
  openmeteo_<nom>.inc        la réponse en littéral brut C++ (copie octet par octet du .json)
  attendu_json_<nom>.inc     chaque valeur du JSON lue par json.loads : chemin, double exact (hexadécimal), entier ou non,
                             valeur ×10 arrondie (Decimal, ROUND_HALF_UP = « loin de zéro »), tailles, chaînes
  attendu_meteo_<nom>.inc    le relevé que le noyau doit produire, calculé ici à partir de json.loads seul
  attendu_heure.inc          instants -> heures locales par zoneinfo, autour des changements d'heure
"""
import argparse
import json
import sys
from datetime import datetime, timedelta, timezone
from decimal import ROUND_HALF_UP, Decimal
from pathlib import Path
from zoneinfo import ZoneInfo

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
RACINE = ICI.parents[1]
FIXTURES = ["paris", "longyearbyen"]
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python.py : ne pas modifier à la main.\n"


def c_chaine(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def fixe(d, decimales):
    """d (Decimal ou int) × 10^decimales, arrondi à l'entier le plus proche, moitié loin de zéro."""
    return int((Decimal(d) * (Decimal(10) ** decimales)).quantize(Decimal(1), rounding=ROUND_HALF_UP))


# ---------------------------------------------------------------------------------------------------------------
def charger(nom):
    brut = (DONNEES / f"openmeteo_{nom}.json").read_bytes()
    doc = json.loads(brut.decode("utf-8"), parse_float=Decimal, parse_int=int)
    return brut, doc


def inc_litteral(nom, brut):
    texte = brut.decode("utf-8")
    assert ')OM"' not in texte
    return ENTETE + f"// Réponse réelle d'Open-Meteo ({nom}), voir donnees/LISEZMOI.md.\nR\"OM({texte})OM\"\n"


def feuilles(doc, chemin=""):
    """Génère (chemin, valeur Python) pour toute valeur du document, conteneurs compris."""
    yield chemin, doc
    if isinstance(doc, dict):
        for cle, v in doc.items():
            yield from feuilles(v, f"{chemin}.{cle}" if chemin else cle)
    elif isinstance(doc, list):
        for i, v in enumerate(doc):
            yield from feuilles(v, f"{chemin}[{i}]")


def inc_attendu_json(nom, doc):
    nombres, textes, tailles, autres = [], [], [], []
    for chemin, v in feuilles(doc):
        if chemin == "":
            continue
        if isinstance(v, bool):
            autres.append(f'    {{{c_chaine(chemin)}, {"true" if v else "false"}}},')
        elif v is None:
            autres.append(f"    {{{c_chaine(chemin)}, 2}},")
        elif isinstance(v, int):
            nombres.append(f'    {{{c_chaine(chemin)}, {float(v).hex()}, true, {v}LL, {fixe(v, 1)}LL}},')
        elif isinstance(v, Decimal):
            nombres.append(f'    {{{c_chaine(chemin)}, {float(v).hex()}, false, 0LL, {fixe(v, 1)}LL}},')
        elif isinstance(v, str):
            textes.append(f'    {{{c_chaine(chemin)}, {c_chaine(v)}}},')
        elif isinstance(v, (list, dict)):
            tailles.append(f'    {{{c_chaine(chemin)}, {len(v)}}},')
    corps = [ENTETE, f"// Source : {nom} -> json.loads(parse_float=Decimal)\n"]
    corps.append("// ---- NOMBRES : {chemin, double exact, écrit comme un entier ?, entier, valeur×10 arrondie}\n")
    corps.append("#ifdef ATTENDU_NOMBRES_ICI\n" + "\n".join(nombres) + "\n#endif\n")
    corps.append("// ---- TEXTES : {chemin, chaîne UTF-8 décodée}\n")
    corps.append("#ifdef ATTENDU_TEXTES_ICI\n" + "\n".join(textes) + "\n#endif\n")
    corps.append("// ---- TAILLES : {chemin, nombre d'éléments}\n")
    corps.append("#ifdef ATTENDU_TAILLES_ICI\n" + "\n".join(tailles) + "\n#endif\n")
    corps.append("// ---- AUTRES : {chemin, 0/1 booléen} ou {chemin, 2} pour null\n")
    corps.append("#ifdef ATTENDU_AUTRES_ICI\n" + "\n".join(autres) + "\n#endif\n")
    return "".join(corps), (len(nombres), len(textes), len(tailles), len(autres))


def inc_attendu_meteo(nom, doc):
    cur, h, d = doc["current"], doc.get("hourly"), doc["daily"]
    t_now = cur["time"]
    proba_now = -1
    if h and "precipitation_probability" in h:
        for t, p in zip(h["time"], h["precipitation_probability"]):
            if t <= t_now < t + 3600:
                proba_now = int(p) if p is not None else -1
                break
    lignes = []
    n = min(len(d["time"]), 7)
    for i in range(n):
        lignes.append("        {%dLL, %d, %d, %d, %dLL, %dLL, %d, %d}," % (
            d["time"][i], d["weather_code"][i], fixe(d["temperature_2m_min"][i], 1), fixe(d["temperature_2m_max"][i], 1),
            d["sunrise"][i], d["sunset"][i], fixe(d["uv_index_max"][i], 1), fixe(d["precipitation_probability_max"][i], 0)))
    corps = (ENTETE + f"// Relevé attendu pour {nom}, calculé par Python à partir de json.loads seul.\n"
             "// {instant, décalage UTC, fuseau, température, ressentie, code, jour, vent, rafales, direction, pression,\n"
             "//  humidité, proba précipitations (heure en cours), nb_jours, {jours}}\n"
             "// jour : {date, code, min, max, lever, coucher, uv, proba max}\n")
    corps += "%dLL, %d, %s, %d, %d, %d, %s, %d, %d, %d, %d, %d, %d, %d,\n    {\n%s\n    }\n" % (
        t_now, doc["utc_offset_seconds"], c_chaine(doc["timezone"]), fixe(cur["temperature_2m"], 1),
        fixe(cur["apparent_temperature"], 1), cur["weather_code"], "true" if cur["is_day"] == 1 else "false",
        fixe(cur["wind_speed_10m"], 1), fixe(cur["wind_gusts_10m"], 1), fixe(cur["wind_direction_10m"], 0),
        fixe(cur["pressure_msl"], 1), fixe(cur["relative_humidity_2m"], 0), proba_now, n, "\n".join(lignes))
    return corps


def inc_attendu_proba(nom, doc):
    """Probabilité de précipitations attendue pour des instants pris dans (et autour de) la fenêtre horaire."""
    h = doc["hourly"]
    t, p = h["time"], h["precipitation_probability"]
    lignes = [ENTETE, f"// Probabilité de précipitations attendue ({nom}) : {{instant, % ou -1 si la fenêtre ne le couvre pas}}\n"]
    lignes.append("{%dLL, -1},\n" % (t[0] - 1))                    # juste avant la première case
    for i in range(0, len(t), 2):
        for dt in (0, 1799, 3599):
            lignes.append("{%dLL, %d},\n" % (t[i] + dt, int(p[i])))
        if i > 0:                                                    # la seconde d'avant appartient à la case précédente
            lignes.append("{%dLL, %d},\n" % (t[i] - 1, int(p[i - 1])))
    lignes.append("{%dLL, -1},\n" % (t[-1] + 3600))                  # juste après la dernière case
    return "".join(lignes)


# ---------------------------------------------------------------------------------------------------------------
# Heure locale : zoneinfo (base IANA du système/tzdata) contre la chaîne POSIX de components/core/.../fuseaux_donnees.inc
def table_posix():
    import re
    texte = (RACINE / "components/core/include/ecran/fuseaux_donnees.inc").read_text(encoding="utf-8")
    return dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', texte))


def instant(an, mois, jour, h=0, mi=0, s=0):
    return int(datetime(an, mois, jour, h, mi, s, tzinfo=timezone.utc).timestamp())


def attendu_zone(iana, t):
    z = ZoneInfo(iana)
    loc = datetime.fromtimestamp(t, z)
    off = int((loc.utcoffset() or timedelta(0)).total_seconds())
    return (loc.year, loc.month, loc.day, loc.hour, loc.minute, loc.second, (loc.weekday() + 1) % 7, off,
            bool(loc.dst()))


def transitions(iana, annees):
    """Instants (UTC, secondes) où le décalage de `iana` change, trouvés par dichotomie sur zoneinfo (indépendant)."""
    z = ZoneInfo(iana)
    res = []
    t = instant(annees[0], 1, 1)
    fin = instant(annees[-1] + 1, 1, 1)
    pas = 86400
    prec = datetime.fromtimestamp(t, z).utcoffset()
    while t < fin:
        nxt = t + pas
        cour = datetime.fromtimestamp(nxt, z).utcoffset()
        if cour != prec:
            bas, haut = t, nxt                  # le changement est dans (bas, haut]
            while haut - bas > 1:
                m = (bas + haut) // 2
                if datetime.fromtimestamp(m, z).utcoffset() == prec:
                    bas = m
                else:
                    haut = m
            res.append(haut)                     # premier instant APRÈS le changement
            prec = cour
        t = nxt
    return res


ZONES_TRANSITIONS = ["Europe/Paris", "America/New_York", "Asia/Kolkata", "Australia/Sydney", "Pacific/Chatham",
                     "Europe/Dublin", "America/Godthab", "Australia/Lord_Howe"]
ANNEES = [2026, 2027, 2028]


def points_pour_zone(iana):
    """Instants autour de chaque changement (juste avant, pendant, juste après) + une grille régulière."""
    pts = set()
    for tr in transitions(iana, ANNEES):
        for dt in (-86400, -3601, -1801, -1, 0, 1, 1799, 3599, 3600, 3601, 86400):
            pts.add(tr + dt)
    # grille : le 1er et le 15 de chaque mois à 00:00:00 et 12:34:56 UTC, plus des cas de calendrier
    for an in ANNEES:
        for mois in range(1, 13):
            pts.add(instant(an, mois, 1, 0, 0, 0))
            pts.add(instant(an, mois, 15, 12, 34, 56))
    for t in (instant(2028, 2, 29, 23, 59, 59), instant(2028, 3, 1, 0, 0, 0), instant(2026, 12, 31, 23, 59, 59),
              instant(2027, 1, 1, 0, 0, 0), instant(2026, 11, 6, 10, 40, 0), instant(2000, 2, 29, 12, 0, 0),
              instant(2100, 2, 28, 23, 59, 59), instant(2100, 3, 1, 0, 0, 0), instant(2038, 1, 19, 3, 14, 7)):
        pts.add(t)
    if iana == "America/Godthab":      # le Groenland a changé de règle en 2023 : pas de date d'avant 2024 pour cette zone
        pts = {t for t in pts if t >= instant(2024, 1, 1)}
    return sorted(pts)


def inc_attendu_heure():
    posix = table_posix()
    lignes = [ENTETE, "// Heures locales attendues, calculées par Python zoneinfo (indépendant de la chaîne POSIX).\n",
              "// ZONES : noms IANA ; LIGNES : {indice de zone, instant UTC, an, mois, jour, heure, minute, seconde,\n",
              "//         jour de semaine (0=dimanche), décalage UTC (s), heure d'été ?}\n",
              "#ifdef ATTENDU_HEURE_ZONES\n"]
    for iana in ZONES_TRANSITIONS:
        assert iana in posix, iana
        lignes.append("    %s,\n" % c_chaine(iana))
    lignes.append("#endif\n#ifdef ATTENDU_HEURE_LIGNES\n")
    total = 0
    for k, iana in enumerate(ZONES_TRANSITIONS):
        for t in points_pour_zone(iana):
            a = attendu_zone(iana, t)
            lignes.append("{%d, %dLL, %d, %d, %d, %d, %d, %d, %d, %d, %s},\n" % (
                k, t, *a[:8], "true" if a[8] else "false"))
            total += 1
    lignes.append("#endif\n")
    return "".join(lignes), total


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    a = ap.parse_args()
    sorties = {}
    resume = []
    for nom in FIXTURES:
        brut, doc = charger(nom)
        sorties[f"openmeteo_{nom}.inc"] = inc_litteral(nom, brut)
        texte, comptes = inc_attendu_json(nom, doc)
        sorties[f"attendu_json_{nom}.inc"] = texte
        sorties[f"attendu_meteo_{nom}.inc"] = inc_attendu_meteo(nom, doc)
        sorties[f"attendu_proba_{nom}.inc"] = inc_attendu_proba(nom, doc)
        resume.append(f"{nom}: {len(brut)} octets, {comptes[0]} nombres, {comptes[1]} chaînes, {comptes[2]} conteneurs, {comptes[3]} autres")
    heure, total = inc_attendu_heure()
    sorties["attendu_heure.inc"] = heure
    resume.append(f"heure: {total} instants, {len(ZONES_TRANSITIONS)} zones")
    ok = True
    for nom, texte in sorties.items():
        chemin = DONNEES / nom
        if a.verifier:
            if not chemin.exists() or chemin.read_bytes() != texte.encode("utf-8"):
                print("PAS À JOUR :", nom)
                ok = False
        else:
            chemin.write_bytes(texte.encode("utf-8"))
    print("\n".join(resume))
    print("vérifié : tout est à jour" if a.verifier and ok else ("écrit" if not a.verifier else "ÉCHEC"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
