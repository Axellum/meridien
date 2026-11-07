#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des tests de MET Norway et du NWS (components/core/include/ecran/meteo_metno.hpp, meteo_nws.hpp,
soleil_calcule.hpp).

    python test/host/controle_python_fournisseurs.py              # (ré)génère donnees/*.inc
    python test/host/controle_python_fournisseurs.py --verifier   # ne modifie rien : échoue si un .inc n'est pas à jour

Entrées : les réponses RÉELLES capturées par test/host/capturer_fournisseurs_meteo.py (donnees/metno_<lieu>.json,
donnees/nws_<lieu>_points.json, donnees/nws_<lieu>_grille.json, voir donnees/LISEZMOI.md et docs/mesures/fournisseurs-meteo.md).
Sorties (générées, ne pas modifier à la main) :
  metno_<lieu>.inc, nws_<lieu>_points.inc, nws_<lieu>_grille.inc   la réponse en littéral brut C++ (copie octet par octet)
  attendu_metno_<lieu>.inc, attendu_nws_<lieu>.inc                 ce que le lecteur du noyau doit produire, une valeur entière
                                                                   par ligne, dans l'ordre de `aplatir()` des tests (chaque
                                                                   ligne porte son rang et son nom ; un test qui échoue rend
                                                                   10000 + rang)
  attendu_soleil_almanac.inc                                               lever et coucher du soleil par un AUTRE algorithme

Aucune ligne de C++ du noyau n'est reprise : les réponses sont lues avec json.loads(parse_float=Decimal), les instants avec
datetime et zoneinfo (la base de fuseaux de Python, pas celle de fuseaux.hpp), le soleil avec l'algorithme « Almanac for
Computers » de l'US Naval Observatory (le noyau emploie celui du NOAA). Les règles de lecture (quel champ devient quelle valeur,
ce qui reste « inconnu ») sont écrites ici à partir de la documentation de chaque service, pas à partir du code C++.
"""
import argparse
import datetime as dt
import json
import math
import sys
from pathlib import Path
from zoneinfo import ZoneInfo

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_fournisseurs.py : ne pas modifier à la main.\n"

# ----------------------------------------------------------------------------------------------------------------------------
# Soleil : « Almanac for Computers » (US Naval Observatory, 1990), zénith 90°50' (= -0,8333 °), UT d'après la longitude.
# ----------------------------------------------------------------------------------------------------------------------------

ZENITH = 90.0 + 50.0 / 60.0


def _sin(d):
    return math.sin(math.radians(d))


def _cos(d):
    return math.cos(math.radians(d))


def _usno(jour_de_l_annee, lat, lon, leve):
    """(cosH, UT en heures) pour le lever (leve=True) ou le coucher, ou (cosH, None) si le soleil ne se lève/couche pas."""
    lng_heure = lon / 15.0
    t = jour_de_l_annee + ((6.0 if leve else 18.0) - lng_heure) / 24.0
    m = 0.9856 * t - 3.289
    ell = (m + 1.916 * _sin(m) + 0.020 * _sin(2 * m) + 282.634) % 360.0
    ra = math.degrees(math.atan(0.91764 * math.tan(math.radians(ell)))) % 360.0
    ra += math.floor(ell / 90.0) * 90.0 - math.floor(ra / 90.0) * 90.0
    ra /= 15.0
    sin_dec = 0.39782 * _sin(ell)
    cos_dec = math.cos(math.asin(sin_dec))
    cos_h = (_cos(ZENITH) - sin_dec * _sin(lat)) / (cos_dec * _cos(lat))
    if cos_h > 1 or cos_h < -1:
        return cos_h, None
    h = (360.0 - math.degrees(math.acos(cos_h))) if leve else math.degrees(math.acos(cos_h))
    h /= 15.0
    t_local = h + ra - 0.06571 * t - 6.622
    return cos_h, (t_local - lng_heure) % 24.0


def soleil(debut_jour_s, lat, lon):
    """(cas, lever_s, coucher_s, marge) pour le jour local qui commence à `debut_jour_s`.

    cas : 0 normal, 1 nuit polaire, 2 jour polaire. `marge` : 1 - |cosH| ; proche de 0, le lever est à la limite de
    l'horizon et le résultat est instable (les tests ne retiennent pas ces jours-là)."""
    milieu = debut_jour_s + 43200
    jour_utc = dt.datetime.fromtimestamp(milieu, dt.timezone.utc)
    meilleur = {}
    marge = 1.0
    cas = 0
    cos_h0 = 0.0
    for decalage in (-1, 0, 1):
        d = (jour_utc + dt.timedelta(days=decalage)).date()
        n = d.timetuple().tm_yday
        base = int(dt.datetime(d.year, d.month, d.day, tzinfo=dt.timezone.utc).timestamp())
        for leve in (True, False):
            cos_h, ut = _usno(n, lat, lon, leve)
            if decalage == 0:
                marge = min(marge, abs(1.0 - abs(cos_h)))
                cos_h0 = cos_h
            if ut is None:
                if decalage == 0:
                    cas = 1 if cos_h > 1 else 2
                continue
            meilleur.setdefault(leve, []).append(base + ut * 3600.0)
    if cas:
        return cas, 0, 0, marge
    # midi solaire le plus proche, puis le lever et le coucher les plus proches de midi -/+ le demi-jour fourni par cosH
    midi = round((milieu - 43200.0 + lon * 240.0) / 86400.0) * 86400.0 + 43200.0 - lon * 240.0
    demi = math.degrees(math.acos(max(-1.0, min(1.0, cos_h0)))) / 15.0 * 3600.0
    lever = min(meilleur[True], key=lambda x: abs(x - (midi - demi)))
    coucher = min(meilleur[False], key=lambda x: abs(x - (midi + demi)))
    if abs(lever - (midi - demi)) > 1800 or abs(coucher - (midi + demi)) > 1800:
        raise SystemExit(f"soleil : incohérent pour {debut_jour_s} {lat} {lon} : {lever} {coucher} {midi} {demi}")
    lever, coucher = [lever], [coucher]
    return 0, int(round(lever[0])), int(round(coucher[0])), marge


# nom, fuseau IANA, latitude, longitude (degrés), jours locaux (AAAA-MM-JJ)
LIEUX_SOLEIL = [
    ("Paris", "Europe/Paris", 48.8566, 2.3522, ["2026-03-28", "2026-03-29", "2026-03-30", "2026-06-21", "2026-10-24", "2026-10-25", "2026-10-26", "2026-12-21"]),
    ("Oslo", "Europe/Oslo", 59.9139, 10.7522, ["2026-06-21", "2026-11-07", "2026-12-21"]),
    ("Reykjavik", "Atlantic/Reykjavik", 64.1466, -21.9426, ["2026-03-20", "2026-11-07"]),
    ("Helsinki", "Europe/Helsinki", 60.1699, 24.9384, ["2026-06-21", "2026-11-07"]),
    ("Londres", "Europe/London", 51.5072, -0.1276, ["2026-03-29", "2026-11-07", "2026-12-21"]),
    ("Tromso", "Europe/Oslo", 69.6492, 18.9553, ["2026-05-10", "2026-05-25", "2026-07-20", "2026-07-30", "2026-11-20", "2026-12-21", "2027-01-20"]),
    ("Longyearbyen", "Arctic/Longyearbyen", 78.2232, 15.6267, ["2026-03-10", "2026-04-25", "2026-06-21", "2026-08-15", "2026-10-15", "2026-12-21"]),
    ("New York", "America/New_York", 40.7128, -74.0060, ["2026-03-07", "2026-03-08", "2026-03-09", "2026-06-21", "2026-11-01", "2026-12-21"]),
    ("Seattle", "America/Los_Angeles", 47.6062, -122.3321, ["2026-06-21", "2026-12-21"]),
    ("Anchorage", "America/Anchorage", 61.2181, -149.9003, ["2026-03-20", "2026-06-21", "2026-12-21"]),
    ("Honolulu", "Pacific/Honolulu", 21.3069, -157.8583, ["2026-06-21", "2026-12-21"]),
    ("Miami", "America/New_York", 25.7617, -80.1918, ["2026-06-21", "2026-12-21"]),
    ("San Juan", "America/Puerto_Rico", 18.4655, -66.1057, ["2026-06-21", "2026-12-21"]),
    ("Guam", "Pacific/Guam", 13.4443, 144.7937, ["2026-06-21", "2026-12-21"]),
    ("Tokyo", "Asia/Tokyo", 35.6895, 139.6917, ["2026-06-21", "2026-12-21"]),
    ("Sydney", "Australia/Sydney", -33.8679, 151.2073, ["2026-04-04", "2026-04-05", "2026-04-06", "2026-10-03", "2026-10-04", "2026-10-05", "2026-12-21", "2026-06-21"]),
    ("Auckland", "Pacific/Auckland", -36.8485, 174.7633, ["2026-09-26", "2026-09-27", "2026-09-28", "2026-12-21"]),
    ("Le Cap", "Africa/Johannesburg", -33.9249, 18.4241, ["2026-06-21", "2026-12-21"]),
    ("Quito", "America/Guayaquil", -0.1807, -78.4678, ["2026-06-21", "2026-12-21"]),
    ("Nairobi", "Africa/Nairobi", -1.2921, 36.8219, ["2026-06-21", "2026-12-21"]),
    ("Singapour", "Asia/Singapore", 1.29, 103.85, ["2026-06-21", "2026-12-21"]),
    ("Kiritimati", "Pacific/Kiritimati", 1.8721, -157.4278, ["2026-06-21"]),
    ("Buenos Aires", "America/Argentina/Buenos_Aires", -34.6037, -58.3816, ["2026-06-21", "2026-12-21"]),
    ("McMurdo", "Antarctica/McMurdo", -77.8419, 166.6863, ["2026-02-10", "2026-04-20", "2026-06-21", "2026-12-21"]),
]


def minuit_local(jour_iso, fuseau):
    """Instant UNIX du premier instant du jour local (jour_iso) dans le fuseau, par zoneinfo."""
    d = dt.date.fromisoformat(jour_iso)
    local = dt.datetime(d.year, d.month, d.day, 0, 0, 0, tzinfo=ZoneInfo(fuseau))
    return int(local.timestamp())


def e5(deg):
    return int(round(deg * 100000))


def inc_soleil():
    lignes = [ENTETE,
              "// Lever et coucher du soleil par l'algorithme « Almanac for Computers » (US Naval Observatory), sans rien du noyau.\n",
              "// Une ligne par jour local : début du jour (UNIX), latitude e5, longitude e5, cas (0 normal, 1 nuit polaire, 2 jour polaire), lever, coucher.\n"]
    n = 0
    for nom, fuseau, lat, lon, jours in LIEUX_SOLEIL:
        for jour in jours:
            debut = minuit_local(jour, fuseau)
            cas, lever, coucher, marge = soleil(debut, lat, lon)
            if marge < 0.02:  # lever à la limite de l'horizon : le résultat dépend de la façon dont chaque algorithme arrondit
                continue
            lignes.append(f"{debut}LL, {e5(lat)}LL, {e5(lon)}LL, {cas}LL, {lever}LL, {coucher}LL, // {nom} {jour}\n")
            n += 1
    return "".join(lignes), n


def inc_soleil_metno():
    """Lever et coucher du produit « Sunrise 3.0 » de MET Norway (réponses réelles, capturer_soleil_metno.py), à la minute."""
    ref = json.loads((DONNEES / "soleil_metno_reference.json").read_text(encoding="utf-8"))
    lignes = [ENTETE,
              "// Lever et coucher du soleil donnés par le produit Sunrise 3.0 de MET Norway (réponses réelles du 07/11/2026,\n",
              "// donnees/soleil_metno_reference.json), à la minute. Mêmes colonnes que attendu_soleil_almanac.inc.\n"]
    n = 0
    for r in ref:
        if r["http"] != 200:
            raise SystemExit(f"réponse {r['http']} pour {r['lieu']} {r['jour']}")
        doc = json.loads(r["corps"])
        p = doc["properties"]
        if p["sunrise"]["time"] is None or p["sunset"]["time"] is None:
            if p["sunrise"]["time"] is not None or p["sunset"]["time"] is not None:
                continue  # un seul des deux : cas que le noyau ne représente pas, écarté
            cas = 2 if p["solarnoon"]["visible"] and p["solarmidnight"]["visible"] else 1
            if cas == 2 or not p["solarnoon"]["visible"]:
                lever = coucher = 0
            else:
                continue
        else:
            cas = 0
            lever = int(dt.datetime.fromisoformat(p["sunrise"]["time"]).timestamp())
            coucher = int(dt.datetime.fromisoformat(p["sunset"]["time"]).timestamp())
            if not lever < coucher:
                continue  # le coucher tombe après minuit local : le jour local suivant, hors du modèle « un lever, un coucher »
        lignes.append(f"{r['debut_jour_s']}LL, {r['lat_e5']}LL, {r['lon_e5']}LL, {cas}LL, {lever}LL, {coucher}LL, "
                      f"// {r['lieu']} {r['jour']}\n")
        n += 1
    return "".join(lignes), n


# ----------------------------------------------------------------------------------------------------------------------------


LIEUX_METNO = [  # nom du fichier, fuseau IANA (la réponse n'en donne pas)
    ("oslo", "Europe/Oslo"), ("reykjavik", "Atlantic/Reykjavik"), ("helsinki", "Europe/Helsinki"),
    ("tromso", "Europe/Oslo"), ("longyearbyen", "Arctic/Longyearbyen"), ("londres", "Europe/London"),
    ("tokyo", "Asia/Tokyo"), ("sydney", "Australia/Sydney"),
]
LIEUX_NWS = ["new_york", "miami", "seattle", "anchorage", "honolulu", "san_juan"]


TAILLE_MORCEAU = 60000  # octets ; -Woverlength-strings refuse un littéral de plus de 65536 caractères


def morceaux(octets, taille=TAILLE_MORCEAU):
    """Découpe en morceaux d'au plus `taille` octets, jamais au milieu d'un caractère UTF-8."""
    res = []
    i = 0
    while i < len(octets):
        j = min(i + taille, len(octets))
        while j < len(octets) and (octets[j] & 0xC0) == 0x80:  # octet de continuation : on est au milieu d'un caractère
            j -= 1
        assert j > i
        res.append(octets[i:j])
        i = j
    return res


def inc_litteral(nom, brut, delimiteur, origine):
    """Une liste de littéraux bruts (un par morceau) : les tests la recollent avec joindre() de aplatir_fournisseurs.hpp."""
    texte = brut.decode("utf-8")
    assert f'){delimiteur}"' not in texte
    lignes = [ENTETE, f"// Réponse réelle de {origine} ({nom}), voir donnees/LISEZMOI.md.\n",
              f"// {len(brut)} octets en {len(morceaux(brut))} morceau(x) de {TAILLE_MORCEAU} octets au plus.\n"]
    for m in morceaux(brut):
        lignes.append(f'R"{delimiteur}({m.decode("utf-8")}){delimiteur}",\n')
    return "".join(lignes)


def inc_attendu(nom, plat, source):
    lignes = [ENTETE, f"// Valeurs attendues pour {source} {nom} : relecture indépendante (lecture_fournisseurs.py), sans rien du noyau.\n",
              f"// {len(plat.v)} lignes : {{valeur, tolérance en unités de la valeur}} ; rang et nom en commentaire "
              "(un test qui échoue rend 10000 + rang).\n"]
    for rang, (champ, valeur, tol) in enumerate(plat.v):
        lignes.append(f"{{{valeur}LL, {tol}LL}}, // {rang} {champ}\n")
    return "".join(lignes)


def sorties():
    """{nom de fichier: contenu}"""
    import lecture_fournisseurs as lf  # pyright: ignore[reportMissingImports]

    s = {}
    texte, n = inc_soleil()
    s["attendu_soleil_almanac.inc"] = texte
    texte_m, n_m = inc_soleil_metno()
    s["attendu_soleil_metno.inc"] = texte_m
    meta = json.loads((DONNEES / "fournisseurs_capture.json").read_text(encoding="utf-8"))
    resume = [f"soleil (USNO) : {n} jours", f"soleil (MET Norway Sunrise 3.0) : {n_m} jours"]

    # symboles de MET Norway : les 41 de la légende officielle, avec leurs trois variantes
    lignes = [ENTETE, "// Code WMO du projet de chaque symbole de MET Norway (41 de la légende officielle, 3 variantes chacun).\n",
              "// {symbole, code attendu} ; règle écrite dans lecture_fournisseurs.py, d'après l'ADR-0014.\n"]
    for nom in lf.SYMBOLES_METNO:
        for variante in ("", "_day", "_night", "_polartwilight"):
            lignes.append(f'{{"{nom}{variante}", {lf.code_symbole_metno(nom + variante)}}},\n')
    s["attendu_symboles_metno.inc"] = "".join(lignes)

    # MET Norway
    lieux = [ENTETE, "// {nom, reçu à (UNIX), latitude e5, longitude e5, fuseau} : la position et l'heure de chaque capture MET Norway.\n"]
    for nom, iana in LIEUX_METNO:
        brut = (DONNEES / f"metno_{nom}.json").read_bytes()
        m = meta[f"metno_{nom}"]
        s[f"metno_{nom}.inc"] = inc_litteral(nom, brut, "MET", "MET Norway Locationforecast 2.0 complete")
        plat = lf.lire_metno(json.loads(brut.decode("utf-8"), parse_float=__import__("decimal").Decimal), m["recu_a_s"],
                             m["lat_e5"], m["lon_e5"], iana, soleil)
        s[f"attendu_metno_{nom}.inc"] = inc_attendu(nom, plat, "MET Norway")
        lieux.append(f'{{"{nom}", {m["recu_a_s"]}LL, {m["lat_e5"]}, {m["lon_e5"]}, "{iana}"}},\n')
        resume.append(f"MET Norway {nom} : {len(brut)} octets, {len(plat.v)} valeurs attendues")
    s["lieux_metno.inc"] = "".join(lieux)

    # NWS
    lieux = [ENTETE, "// {nom, reçu à (UNIX), latitude e5, longitude e5, bureau, x, y, fuseau} : chaque capture NWS (maille lue dans /points).\n"]
    for nom in LIEUX_NWS:
        bp = (DONNEES / f"nws_{nom}_points.json").read_bytes()
        bg = (DONNEES / f"nws_{nom}_grille.json").read_bytes()
        mp, mg = meta[f"nws_{nom}_points"], meta[f"nws_{nom}_grille"]
        s[f"nws_{nom}_points.inc"] = inc_litteral(nom + " /points", bp, "NWS", "NWS api.weather.gov /points")
        s[f"nws_{nom}_grille.inc"] = inc_litteral(nom + " /gridpoints", bg, "NWS", "NWS api.weather.gov /gridpoints")
        dec = __import__("decimal").Decimal
        points = json.loads(bp.decode("utf-8"), parse_float=dec)
        grille = json.loads(bg.decode("utf-8"), parse_float=dec)
        pp = points["properties"]
        plat = lf.lire_nws(points, grille, mg["recu_a_s"], mp["lat_e5"], mp["lon_e5"], soleil)
        s[f"attendu_nws_{nom}.inc"] = inc_attendu(nom, plat, "NWS")
        lieux.append(f'{{"{nom}", {mg["recu_a_s"]}LL, {mp["lat_e5"]}, {mp["lon_e5"]}, "{pp["gridId"]}", {pp["gridX"]}, '
                     f'{pp["gridY"]}, "{pp["timeZone"]}"}},\n')
        resume.append(f"NWS {nom} : {len(bg)} octets, {len(plat.v)} valeurs attendues")
    s["lieux_nws.inc"] = "".join(lieux)
    # hors couverture : la réponse 404 de Lyon
    brut = (DONNEES / "nws_lyon_points.json").read_bytes()
    s["nws_lyon_points.inc"] = inc_litteral("lyon /points (HTTP 404)", brut, "NWS", "NWS api.weather.gov /points")
    return s, resume


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    a = ap.parse_args()
    fichiers, resume = sorties()
    ok = True
    for nom, texte in fichiers.items():
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
