#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des tests de l'heure du lieu (ecran/heure_lieu.hpp) et du lever/coucher du soleil (ecran/soleil.hpp).

    python test/host/controle_python_fuseaux.py              # (ré)génère donnees/attendu_heure_lieu.inc et attendu_soleil.inc
    python test/host/controle_python_fuseaux.py --verifier   # ne modifie rien : échoue si un .inc n'est pas à jour

Python calcule ici les valeurs attendues avec `zoneinfo` (base IANA du paquet tzdata) et le module `math` ; aucune ligne du noyau C++
n'est reprise. Sorties (générées, ne pas modifier à la main) :

  attendu_heure_lieu.inc   - FENÊTRES : 24 instants consécutifs de prévision, à cheval sur un changement d'heure ou sur le 31/12 → 01/01,
                             avec l'étiquette attendue de chaque colonne (français et autres langues) ;
                           - JOURS : le jour de la semaine de jours de prévision consécutifs (minuit local donné par zoneinfo) ;
                           - LEVERS : l'heure « hh:mm » locale des levers et couchers RÉELS d'Open-Meteo (huit lieux) ;
  attendu_soleil.inc       - PYTHON : lever et coucher du soleil par l'algorithme de la NOAA, réécrit ici avec `math` (un jour par saison,
                             vingt lieux, du cercle polaire à l'équateur, tous les fuseaux dont les minutes ne sont pas nulles) ;
                           - OPEN-METEO : les levers et couchers réels des réponses capturées le 06/11/2026 (donnees/openmeteo_*.json et
                             openmeteo_riche_*.json), pour savoir ce que vaut l'algorithme face à une source de production.

Les étiquettes attendues sont écrites ICI à partir de la règle de l'écran (« 7h » / « 7:00 » à la minute nulle, « 05:30 » sinon) : c'est
la seconde écriture de la règle, pas une copie du C++.
"""
import argparse
import json
import math
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_fuseaux.py : ne pas modifier à la main.\n"


def c_chaine(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def instant(an, mois, jour, h=0, mi=0, s=0):
    return int(datetime(an, mois, jour, h, mi, s, tzinfo=timezone.utc).timestamp())


def local(iana, t):
    return datetime.fromtimestamp(t, ZoneInfo(iana))


def decalage_de(loc):
    return int((loc.utcoffset() or timedelta(0)).total_seconds())


def decalage(iana, t):
    return decalage_de(local(iana, t))


def etiquette(h, m, francais):
    """La règle de l'écran, écrite à part : minute nulle -> « 7h » (français) ou « 7:00 » ; sinon « 05:30 »."""
    if m == 0:
        return f"{h}h" if francais else f"{h}:00"
    return f"{h:02d}:{m:02d}"


# ----------------------------------------------------------------------------------------------------------------------------
# Fenêtres de prévision : 24 colonnes d'une heure, à cheval sur un changement d'heure ou sur le nouvel an.
# (zone, début UTC, alignement) ; alignement « utc » = colonnes aux heures pleines UTC (ce que fait un fournisseur qui ignore le fuseau),
# « local » = colonnes à l'heure pleine LOCALE du premier instant (ce que renvoie Open-Meteo : mesuré, voir LISEZMOI.md).
FENETRES = [
    ("America/New_York", instant(2026, 11, 1, 3), "utc"),       # retour à l'heure d'hiver le 1er novembre : 01 h se répète
    ("America/New_York", instant(2026, 3, 8, 5), "utc"),        # passage à l'heure d'été : 02 h n'existe pas
    ("Australia/Sydney", instant(2026, 10, 3, 14), "utc"),      # passage à l'heure d'été le 4 octobre : 02 h n'existe pas
    ("Australia/Sydney", instant(2026, 4, 4, 13), "utc"),       # retour à l'heure d'hiver le 5 avril
    ("Europe/Paris", instant(2026, 3, 28, 22), "utc"),
    ("Europe/Paris", instant(2026, 10, 24, 22), "utc"),
    ("Australia/Lord_Howe", instant(2026, 10, 3, 13), "utc"),   # changement d'une DEMI-heure : les colonnes passent de :30 à :00
    ("Australia/Lord_Howe", instant(2026, 4, 4, 12), "utc"),
    ("Asia/Tokyo", instant(2026, 11, 6, 10), "utc"),
    ("Asia/Kolkata", instant(2026, 11, 6, 10), "utc"),          # colonnes à :30
    ("Asia/Kolkata", instant(2026, 11, 6, 10, 30), "local"),    # colonnes à l'heure pleine locale
    ("Asia/Kathmandu", instant(2026, 11, 6, 10), "utc"),        # colonnes à :45
    ("Asia/Kathmandu", instant(2026, 11, 6, 10, 15), "local"),
    ("Pacific/Chatham", instant(2026, 11, 6, 10), "utc"),
    ("Pacific/Auckland", instant(2026, 12, 31, 9), "utc"),      # le nouvel an passe au milieu de la fenêtre
    ("Pacific/Honolulu", instant(2027, 1, 1, 6), "utc"),
    ("Europe/Paris", instant(2026, 12, 31, 20), "utc"),
]

# Jours de prévision consécutifs (minuit local de chaque jour, instant UNIX) : (zone, premier jour, nombre)
JOURS = [
    ("America/New_York", (2026, 10, 30), 9),   # le 1er novembre dure 25 heures
    ("America/New_York", (2026, 3, 6), 6),     # le 8 mars dure 23 heures
    ("Australia/Sydney", (2026, 10, 1), 8),
    ("Pacific/Auckland", (2026, 12, 28), 8),   # à cheval sur 2026 → 2027
    ("Pacific/Honolulu", (2026, 12, 28), 8),
    ("Asia/Kolkata", (2026, 11, 3), 7),
    ("Asia/Kathmandu", (2026, 11, 3), 7),
    ("Australia/Lord_Howe", (2026, 10, 1), 8),
    ("Europe/Paris", (2026, 10, 22), 7),
    ("Asia/Tokyo", (2028, 2, 26), 5),          # 29 février 2028
]

# Ce que l'horloge de l'appareil (main/horloge.cpp) doit écrire : pour chaque lieu et chaque instant, « HH:MM », « H h MM » et la date
# longue française avec l'année. Les instants tombent de part et d'autre d'un minuit local (le jour change à des heures UTC différentes
# selon le lieu), d'un changement d'heure, du nouvel an et d'un 29 février.
HORLOGE_ZONES = ["Europe/Paris", "Asia/Tokyo", "Asia/Kolkata", "Asia/Kathmandu", "America/New_York", "Australia/Sydney",
                 "Pacific/Auckland", "Pacific/Honolulu"]
HORLOGE_INSTANTS = [
    instant(2026, 11, 6, 10, 40),
    instant(2026, 11, 6, 20, 30),     # Paris 21:30 le 6, Tokyo 05:30 le 7
    instant(2026, 11, 6, 18, 29, 59), # Kolkata : 23:59:59 le 6
    instant(2026, 11, 6, 18, 30),     # Kolkata : minuit, le 7
    instant(2026, 12, 31, 23, 30),
    instant(2027, 1, 1, 0, 0),
    instant(2026, 11, 1, 5, 59, 59),  # New York : dernière seconde de l'heure d'été
    instant(2026, 11, 1, 6, 0),       # New York : retour à l'heure d'hiver
    instant(2026, 10, 3, 15, 59, 59), # Sydney : dernière seconde avant le passage à l'heure d'été
    instant(2026, 10, 3, 16, 0),
    instant(2027, 3, 28, 0, 59, 59),  # Paris : dernière seconde avant le passage à l'heure d'été
    instant(2027, 3, 28, 1, 0),
    instant(2028, 2, 29, 12, 0),
]
JOURS_FR = ["dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"]
MOIS_FR = ["janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre", "novembre", "décembre"]


JOURS_EN = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]
MOIS_EN = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]


def instant_vigie(loc, francais):
    """Le texte d'un début ou d'une fin d'alerte de Vigie : « 10:00 · samedi 7 novembre » ou « 10:00 · Saturday, 7 November »."""
    jour = (loc.weekday() + 1) % 7
    if francais:
        date = f"{JOURS_FR[jour]} {'1er' if loc.day == 1 else loc.day} {MOIS_FR[loc.month - 1]}"
    else:
        date = f"{JOURS_EN[jour]}, {loc.day} {MOIS_EN[loc.month - 1]}"
    return f"{loc.hour:02d}:{loc.minute:02d} · {date}"


def date_longue_fr(loc):
    """« vendredi 6 novembre 2026 » ; « dimanche 1er novembre 2026 » le premier du mois."""
    jour = "1er" if loc.day == 1 else str(loc.day)
    return f"{JOURS_FR[(loc.weekday() + 1) % 7]} {jour} {MOIS_FR[loc.month - 1]} {loc.year}"


# Réponses RÉELLES d'Open-Meteo (nom du fichier, préfixe). Chaque réponse porte 7 jours : lever, coucher, fuseau, position.
REPONSES = ["openmeteo_paris", "openmeteo_longyearbyen"] + [
    f"openmeteo_riche_{n}" for n in ["lyon", "longyearbyen", "seattle", "singapour", "sydney", "katmandou", "terreneuve"]
]


def charger(nom):
    return json.loads((DONNEES / f"{nom}.json").read_text(encoding="utf-8"))


def jours_reels(nom):
    """(lat, lon, zone, [(date locale, minuit local, lever, coucher)]) lus tels quels dans la réponse."""
    doc = charger(nom)
    zone = doc["timezone"]
    res = []
    for debut, lever, coucher in zip(doc["daily"]["time"], doc["daily"]["sunrise"], doc["daily"]["sunset"]):
        d = local(zone, debut + 12 * 3600).date()
        res.append((d, debut, lever, coucher))
    return doc["latitude"], doc["longitude"], zone, res


def inc_attendu_heure_lieu():
    zones = []

    def indice(z):
        if z not in zones:
            zones.append(z)
        return zones.index(z)

    fen = []
    for zone, debut, alignement in FENETRES:
        k = indice(zone)
        t0 = debut
        if alignement == "local":
            assert local(zone, t0).minute == 0 and local(zone, t0).second == 0, (zone, debut)
        for i in range(24):
            t = t0 + 3600 * i
            loc = local(zone, t)
            fen.append("{%d, %dLL, %s, %s, %d, %d, %d, %d},\n" % (
                k, t, c_chaine(etiquette(loc.hour, loc.minute, True)), c_chaine(etiquette(loc.hour, loc.minute, False)),
                loc.hour, loc.minute, decalage_de(loc), (loc.weekday() + 1) % 7))

    jours = []
    for zone, (a, m, j), n in JOURS:
        k = indice(zone)
        zi = ZoneInfo(zone)
        d0 = datetime(a, m, j)
        for i in range(n):
            d = d0 + timedelta(days=i)
            minuit = datetime(d.year, d.month, d.day, tzinfo=zi)
            assert minuit.hour == 0, (zone, d)  # le minuit existe dans ces zones
            ts = int(minuit.timestamp())
            midi = datetime.fromtimestamp(ts + 12 * 3600, zi)
            assert midi.date() == d.date()
            jours.append("{%d, %dLL, %d, %d},\n" % (k, ts, (d.weekday() + 1) % 7, d.toordinal()))

    levers = []
    for nom in REPONSES:
        _lat, _lon, zone, liste = jours_reels(nom)
        k = indice(zone)
        for _d, _debut, lever, coucher in liste:
            if lever == coucher:
                continue  # nuit polaire : pas d'heure (voir attendu_soleil.inc)
            for t in (lever, coucher):
                loc = local(zone, t)
                levers.append("{%d, %dLL, %s},\n" % (k, t, c_chaine(f"{loc.hour:02d}:{loc.minute:02d}")))

    horloge = []
    vigie = []
    for zone in HORLOGE_ZONES:
        k = indice(zone)
        for t in HORLOGE_INSTANTS:
            loc = local(zone, t)
            vigie.append("{%d, %dLL, %s, %s},\n" % (k, t, c_chaine(instant_vigie(loc, True)), c_chaine(instant_vigie(loc, False))))
            horloge.append("{%d, %dLL, %s, %s, %s, %d},\n" % (
                k, t, c_chaine(f"{loc.hour:02d}:{loc.minute:02d}"), c_chaine(f"{loc.hour} h {loc.minute:02d}"),
                c_chaine(date_longue_fr(loc)), decalage_de(loc)))

    corps = [ENTETE, "// Heures locales du lieu attendues, calculées par Python zoneinfo.\n"]
    corps.append("// ZONES : noms IANA.\n#ifdef ATTENDU_LIEU_ZONES\n")
    corps += ["    %s,\n" % c_chaine(z) for z in zones]
    corps.append("#endif\n")
    corps.append("// FENÊTRES : {indice de zone, instant UTC, étiquette française, étiquette autres langues, heure locale, minute locale,\n")
    corps.append("//            décalage (s), jour de la semaine (0 = dimanche)} ; 24 lignes par fenêtre, dans l'ordre de FENETRES.\n")
    corps.append("#ifdef ATTENDU_LIEU_FENETRES\n" + "".join(fen) + "#endif\n")
    corps.append("// JOURS : {indice de zone, minuit local (instant UTC), jour de la semaine (0 = dimanche), numéro du jour (date.toordinal())}\n")
    corps.append("#ifdef ATTENDU_LIEU_JOURS\n" + "".join(jours) + "#endif\n")
    corps.append("// LEVERS : {indice de zone, instant UTC d'un lever ou d'un coucher réel d'Open-Meteo, « hh:mm » local}\n")
    corps.append("#ifdef ATTENDU_LIEU_LEVERS\n" + "".join(levers) + "#endif\n")
    corps.append("// HORLOGE : {indice de zone, instant UTC, « HH:MM », « H h MM », date longue française avec l'année, décalage (s)}\n")
    corps.append("#ifdef ATTENDU_LIEU_HORLOGE\n" + "".join(horloge) + "#endif\n")
    corps.append("// VIGIE : {indice de zone, instant UTC, début ou fin d'alerte en français, en anglais} (vigie::formater_instant)\n")
    corps.append("#ifdef ATTENDU_LIEU_VIGIE\n" + "".join(vigie) + "#endif\n")
    return "".join(corps), (len(zones), len(fen), len(jours), len(levers), len(horloge))


# ----------------------------------------------------------------------------------------------------------------------------
# Soleil : l'algorithme de la NOAA, réécrit avec `math` (variante « itérer jusqu'à stabilisation », où le noyau C++ fait deux passes).
def noaa_position(jd):
    """(équation du temps en minutes, déclinaison en radians) au jour julien jd."""
    t = (jd - 2451545.0) / 36525.0
    l0 = math.radians(280.46646 + t * (36000.76983 + t * 0.0003032))
    m = math.radians(357.52911 + t * (35999.05029 - 0.0001537 * t))
    e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t)
    c = (math.sin(m) * (1.914602 - t * (0.004817 + 0.000014 * t)) + math.sin(2 * m) * (0.019993 - 0.000101 * t)
         + math.sin(3 * m) * 0.000289)
    vraie = math.degrees(l0) + c
    omega = 125.04 - 1934.136 * t
    apparente = math.radians(vraie - 0.00569 - 0.00478 * math.sin(math.radians(omega)))
    eps0 = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0
    eps = math.radians(eps0 + 0.00256 * math.cos(math.radians(omega)))
    decl = math.asin(math.sin(eps) * math.sin(apparente))
    y = math.tan(eps / 2.0) ** 2
    eq = (y * math.sin(2 * l0) - 2 * e * math.sin(m) + 4 * e * y * math.sin(m) * math.cos(2 * l0)
          - 0.5 * y * y * math.sin(4 * l0) - 1.25 * e * e * math.sin(2 * m))
    return 4.0 * math.degrees(eq), decl


def noaa_lever_coucher(lat, lon, an, mois, jour, decalage_s):
    """(cas, lever UTC, coucher UTC) ; cas 0 normal, 1 jour polaire, 2 nuit polaire."""
    midi_local = (datetime(an, mois, jour, tzinfo=timezone.utc) + timedelta(hours=12)).timestamp() - decalage_s
    midi_moyen = 720.0 - 4.0 * lon
    n = math.floor((midi_local - midi_moyen * 60.0) / 86400.0 + 0.5)
    jd_minuit = n + 2440587.5

    def evenement(lever):
        minutes = midi_moyen
        for _ in range(6):  # stabilisation
            eq, decl = noaa_position(jd_minuit + minutes / 1440.0)
            phi = math.radians(lat)
            cos_h = (math.cos(math.radians(90.833)) - math.sin(phi) * math.sin(decl)) / (math.cos(phi) * math.cos(decl))
            if cos_h > 1.0:
                return 2, 0.0
            if cos_h < -1.0:
                return 1, 0.0
            ha = math.degrees(math.acos(cos_h))
            minutes = 720.0 - 4.0 * (lon + (ha if lever else -ha)) - eq
        return 0, minutes

    cl, ml = evenement(True)
    cc, mc = evenement(False)
    if cl != 0 or cc != 0:
        return (cl or cc), 0, 0
    return 0, int(n * 86400 + math.floor(ml * 60.0 + 0.5)), int(n * 86400 + math.floor(mc * 60.0 + 0.5))


# (nom, latitude, longitude, zone IANA)
LIEUX = [
    ("Paris", 48.8566, 2.3522, "Europe/Paris"),
    ("New York", 40.7128, -74.0060, "America/New_York"),
    ("Tokyo", 35.6762, 139.6503, "Asia/Tokyo"),
    ("Kolkata", 22.5726, 88.3639, "Asia/Kolkata"),
    ("Katmandou", 27.7172, 85.3240, "Asia/Kathmandu"),
    ("Sydney", -33.8688, 151.2093, "Australia/Sydney"),
    ("Auckland", -36.8485, 174.7633, "Pacific/Auckland"),
    ("Honolulu", 21.3069, -157.8583, "Pacific/Honolulu"),
    ("Reykjavik", 64.1466, -21.9426, "Atlantic/Reykjavik"),
    ("Tromso", 69.6492, 18.9553, "Europe/Oslo"),
    ("Longyearbyen", 78.2232, 15.6267, "Arctic/Longyearbyen"),
    ("Le Cap", -33.9249, 18.4241, "Africa/Johannesburg"),
    ("Quito", -0.1807, -78.4678, "America/Guayaquil"),
    ("Singapour", 1.3521, 103.8198, "Asia/Singapore"),
    ("Ushuaia", -54.8019, -68.3030, "America/Argentina/Ushuaia"),
    ("Anchorage", 61.2181, -149.9003, "America/Anchorage"),
    ("Lord Howe", -31.5540, 159.0850, "Australia/Lord_Howe"),
    ("Chatham", -43.9535, -176.5597, "Pacific/Chatham"),
    ("Caracas", 10.4806, -66.9036, "America/Caracas"),
    ("Mexico", 19.4326, -99.1332, "America/Mexico_City"),
]
# Un jour par saison, plus le 06/11/2026 (jour des mesures) et un changement d'heure.
DATES = [(2026, 3, 20), (2026, 6, 21), (2026, 9, 23), (2026, 12, 21), (2026, 11, 6), (2027, 1, 1)]


def inc_attendu_soleil():
    py = []
    for nom, lat, lon, zone in LIEUX:
        for an, mois, jour in DATES:
            midi = int(datetime(an, mois, jour, 12, tzinfo=ZoneInfo(zone)).timestamp())
            dec = decalage(zone, midi)
            cas, lever, coucher = noaa_lever_coucher(lat, lon, an, mois, jour, dec)
            py.append("{%r, %r, %d, %d, %d, %d, %d, %dLL, %dLL},  // %s\n" % (lat, lon, an, mois, jour, dec, cas, lever, coucher, nom))
    reel = []
    ecart_max = 0
    for nom in REPONSES:
        lat, lon, zone, liste = jours_reels(nom)
        for d, debut, lever, coucher in liste:
            dec = decalage(zone, debut + 12 * 3600)
            if lever == coucher:
                cas, l_reel, c_reel = 2, 0, 0
            else:
                cas, l_reel, c_reel = 0, lever, coucher
            ref = noaa_lever_coucher(lat, lon, d.year, d.month, d.day, dec)
            if cas == 0 and ref[0] == 0:
                ecart_max = max(ecart_max, abs(ref[1] - l_reel), abs(ref[2] - c_reel))
            reel.append("{%r, %r, %d, %d, %d, %d, %d, %dLL, %dLL},  // %s\n" % (
                lat, lon, d.year, d.month, d.day, dec, cas, l_reel, c_reel, nom.replace("openmeteo_", "")))
    corps = [ENTETE, "// Lever et coucher du soleil attendus. Ligne : {latitude, longitude, an, mois, jour, décalage UTC du fuseau (s) ce jour-là,\n"]
    corps.append("//  cas (0 normal, 1 jour polaire, 2 nuit polaire), lever UTC, coucher UTC}\n")
    corps.append("// PYTHON : l'algorithme de la NOAA écrit en Python (math), un jour par saison.\n")
    corps.append("#ifdef ATTENDU_SOLEIL_PYTHON\n" + "".join(py) + "#endif\n")
    corps.append("// OPEN-METEO : levers et couchers RÉELS des réponses capturées (donnees/LISEZMOI.md) ; cas 2 = nuit polaire (lever == coucher).\n")
    corps.append("#ifdef ATTENDU_SOLEIL_OPEN_METEO\n" + "".join(reel) + "#endif\n")
    return "".join(corps), (len(py), len(reel), ecart_max)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    a = ap.parse_args()
    sorties = {}
    heure, comptes_h = inc_attendu_heure_lieu()
    sorties["attendu_heure_lieu.inc"] = heure
    soleil, comptes_s = inc_attendu_soleil()
    sorties["attendu_soleil.inc"] = soleil
    print(f"heure du lieu : {comptes_h[0]} zones, {comptes_h[1]} colonnes de fenêtre, {comptes_h[2]} jours, {comptes_h[3]} levers/couchers réels, {comptes_h[4]} lignes d'horloge")
    print(f"soleil : {comptes_s[0]} lignes Python, {comptes_s[1]} lignes Open-Meteo ; plus grand écart (algorithme, Open-Meteo) = {comptes_s[2]} s")
    ok = True
    for nom, texte in sorties.items():
        chemin = DONNEES / nom
        if a.verifier:
            if not chemin.exists() or chemin.read_bytes().replace(b"\r\n", b"\n") != texte.encode("utf-8"):
                print("PAS À JOUR :", nom)
                ok = False
        else:
            chemin.write_bytes(texte.encode("utf-8"))
    print("vérifié : tout est à jour" if a.verifier and ok else ("écrit" if not a.verifier else "ÉCHEC"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
