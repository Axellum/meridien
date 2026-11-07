#!/usr/bin/env python3
"""Contrôle INDÉPENDANT du noyau « surf » (components/core/include/ecran/surf_*.hpp) sur les réponses RÉELLES d'Open-Meteo.

    python test/host/controle_python_surf.py                    # (ré)génère donnees/surf_cas.inc
    python test/host/controle_python_surf.py --verifier         # ne modifie rien : échoue si le .inc n'est pas à jour
    python test/host/controle_python_surf.py --expliquer hossegor 1500   # dit quelle valeur est le rang 1500 de ce cas

Entrées : test/host/donnees/surf_marine_<id>.json, surf_vent_<id>.json et surf_manifeste.json, capturés avec les adresses exactes du
firmware par test/host/capturer_surf.py (voir donnees/LISEZMOI_surf.md).
Sortie (générée, ne pas modifier à la main) : donnees/surf_cas.inc = pour chaque lieu la réponse en littéral brut C++ (octet pour
octet) et la liste des valeurs entières que le noyau doit produire, dans l'ordre de `aplatir()` de test_surf_reponses.cpp.

CE QUI REND CE CONTRÔLE INDÉPENDANT : aucune ligne du noyau C++ n'est reprise. Les valeurs sont lues avec json.loads(parse_float=
Decimal) et le module decimal (arrondi « moitié loin de zéro », comme Open-Meteo écrit ses nombres) ; le cosinus vient de
math.cos ; la marée est trouvée par une autre méthode que le C++ (les paliers sont d'abord regroupés, puis on cherche les
changements de sens) ; le barème, le vent, le meilleur créneau sont réécrits ici À PARTIR DU TEXTE de l'ADR-0013 et des en-têtes (les
constantes du BAREME ci-dessous sont celles du tableau de l'ADR : si l'un des deux change, un test échoue).
LIMITE : pour la lecture du JSON, le cosinus et la marée, le C++ et ce script se trompent chacun à sa façon ; pour le barème et le
créneau, les deux versions viennent du MÊME texte, lu par la même personne : une erreur de lecture de ce texte serait commune aux
deux. Ce contrôle prouve que le C++ fait ce que l'ADR écrit, pas que l'ADR décrit bien un jour de surf.
"""
import argparse
import json
import math
import sys
from decimal import ROUND_HALF_UP, Decimal
from fractions import Fraction
from pathlib import Path

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_surf.py : ne pas modifier à la main.\n"
ABSENT = -32768

# --- barème de l'ADR-0013 (à tenir identique au tableau de l'ADR et aux constantes de surf_note.hpp / surf_vent.hpp) -------------
BAREME = {
    "points_hauteur": 40, "points_periode": 25, "points_vent": 35,
    "hauteur_sous_min_nul_pm": 400, "hauteur_sur_max_nul_pm": 1600,
    "periode_nulle_ds": 50, "periode_pleine_ds": 130,
    "calme_dixiemes": 100,
    "offshore_plein_jusqua": 250, "offshore_nul_a": 700, "travers_nul_a": 450, "onshore_nul_a": 300,
    "cos_offshore_max": -500, "cos_onshore_min": 500,
    "limitant_seuil": 8,
    "creneau_debut_h": 6, "creneau_fin_h": 21, "creneau_min": 2, "creneau_max": 6, "creneau_tolerance": 10,
    "maree_amplitude_min_cm": 30, "maree_etale_s": 1800, "maree_extrema_max": 16,
}
B = BAREME
FOURCHETTE_DEFAUT = (100, 250)  # cm, défauts de surf_spot.hpp


def fixe(d, decimales):
    """d (Decimal ou int) × 10^decimales, arrondi à l'entier le plus proche, moitié loin de zéro."""
    return int((Decimal(d) * (Decimal(10) ** decimales)).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def arrondi(f: Fraction) -> int:
    """Fraction -> entier le plus proche, moitié vers le haut (valeurs positives seulement dans ce barème)."""
    assert f >= 0
    return int((f + Fraction(1, 2)).__floor__())


def cos_pm(deg: int) -> int:
    return int(Decimal(math.cos(math.radians(deg % 360)) * 1000).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def rampe(v, va, pa, vb, pb) -> int:
    """Droite entre (va, pa) et (vb, pb), plate au-delà, valeur totale arrondie à l'entier le plus proche (moitié vers le haut)."""
    if va == vb:
        return pa
    if va > vb:
        va, pa, vb, pb = vb, pb, va, pa
    w = min(max(v, va), vb)
    return arrondi(Fraction(pa) + Fraction((pb - pa) * (w - va), vb - va))


# ---------------------------------------------------------------------------------------------------------------------------
def lire_marine(doc):
    """Lit la réponse MER comme l'ADR la décrit. Rend un dict ; None = null."""
    h = doc["hourly"]
    n = len(h["time"])

    def serie(nom, dec):
        brut = h.get(nom)
        if brut is None:
            return [None] * n
        return [None if x is None else fixe(x, dec) for x in brut]

    def direction(nom):
        return [None if x is None else (0 if x == 360 else x) for x in serie(nom, 0)]

    return {
        "decalage": doc["utc_offset_seconds"], "n": n, "t": h["time"],
        "vagues": serie("wave_height", 2), "vagues_p": serie("wave_period", 1), "vagues_d": direction("wave_direction"),
        "houle": serie("swell_wave_height", 2), "houle_p": serie("swell_wave_period", 1), "houle_d": direction("swell_wave_direction"),
        "mv": serie("wind_wave_height", 2), "mv_p": serie("wind_wave_period", 1), "mv_d": direction("wind_wave_direction"),
        "sst": serie("sea_surface_temperature", 1), "maree": serie("sea_level_height_msl", 2),
    }


def lire_vent(doc):
    h = doc["hourly"]
    n = len(h["time"])

    def serie(nom, dec):
        brut = h.get(nom)
        return [None if x is None else fixe(x, dec) for x in brut] if brut is not None else [None] * n

    dirs = [None if x is None else (0 if x == 360 else x) for x in serie("wind_direction_10m", 0)]
    return {"decalage": doc["utc_offset_seconds"], "n": n, "t": h["time"], "v": serie("wind_speed_10m", 1),
            "g": serie("wind_gusts_10m", 1), "d": dirs}


def a(x):
    return ABSENT if x is None else x


# ---------------------------------------------------------------------------------------------------------------------------
def extrema_maree(mer):
    """Méthode DIFFÉRENTE du C++ : on regroupe les paliers de valeurs égales, puis on cherche les paliers intérieurs plus hauts
    (ou plus bas) que leurs deux voisins, séparément dans chaque suite de valeurs lues (un null coupe) ; enfin filtre en zigzag."""
    t, y = mer["t"], mer["maree"]
    segments, courant = [], []
    for i in range(mer["n"]):
        if y[i] is None:
            if courant:
                segments.append(courant)
            courant = []
        else:
            courant.append((t[i], y[i]))
    if courant:
        segments.append(courant)
    candidats = []
    for seg in segments:
        paliers = []  # [premier instant, dernier instant, niveau]
        for instant, niveau in seg:
            if paliers and paliers[-1][2] == niveau:
                paliers[-1][1] = instant
            else:
                paliers.append([instant, instant, niveau])
        for k in range(1, len(paliers) - 1):
            milieu = (paliers[k][0] + paliers[k][1]) // 2
            if paliers[k][2] > paliers[k - 1][2] and paliers[k][2] > paliers[k + 1][2]:
                candidats.append((milieu, paliers[k][2], True))
            elif paliers[k][2] < paliers[k - 1][2] and paliers[k][2] < paliers[k + 1][2]:
                candidats.append((milieu, paliers[k][2], False))
    gardes = []
    for c in candidats:
        if not gardes:
            gardes.append(c)
            continue
        dernier = gardes[-1]
        if c[2] == dernier[2]:
            if (c[1] > dernier[1]) if c[2] else (c[1] < dernier[1]):
                gardes[-1] = c
            continue
        if abs(c[1] - dernier[1]) < B["maree_amplitude_min_cm"]:
            continue
        if len(gardes) < B["maree_extrema_max"]:
            gardes.append(c)
    return gardes


def maree_complete(mer, maintenant):
    """(notable, nb, marnage, debut, fin, extrema, tendance, prochaine pleine, prochaine basse)."""
    gardes = extrema_maree(mer)
    n = mer["n"]
    debut, fin = (mer["t"][0], mer["t"][-1] + 3600) if n else (0, 0)
    notable = len(gardes) >= 2
    marnage = (max(g[1] for g in gardes) - min(g[1] for g in gardes)) if notable else 0
    if not notable or maintenant < debut or maintenant >= fin:
        tendance = 0
    else:
        avant = [g for g in gardes if g[0] <= maintenant]
        apres = [g for g in gardes if g[0] > maintenant]
        avant = avant[-1] if avant else None
        apres = apres[0] if apres else None
        if (avant and maintenant - avant[0] <= B["maree_etale_s"]) or (apres and apres[0] - maintenant <= B["maree_etale_s"]):
            tendance = 3
        elif apres:
            tendance = 1 if apres[2] else 2
        elif avant:
            tendance = 2 if avant[2] else 1
        else:
            tendance = 0

    def prochaine(pleine):
        if not notable:
            return 0
        for g in gardes:
            if g[2] == pleine and g[0] >= maintenant:
                return g[0]
        return 0

    return notable, len(gardes), marnage, debut, fin, gardes, tendance, prochaine(True), prochaine(False)


# ---------------------------------------------------------------------------------------------------------------------------
def conditions(mer, vent, i):
    """(hauteur, période, vitesse, rafale, direction) de l'heure i de la mer, vent apparié par l'instant."""
    if mer["houle"][i] is not None:
        hauteur = mer["houle"][i]
        periode = mer["houle_p"][i] if mer["houle_p"][i] is not None else mer["vagues_p"][i]
    else:
        hauteur, periode = mer["vagues"][i], mer["vagues_p"][i]
    v = g = d = None
    for k in range(vent["n"]):
        if vent["t"][k] == mer["t"][i]:
            v, g, d = vent["v"][k], vent["g"][k], vent["d"][k]
            break
    return hauteur, periode, v, g, d


def vent_classe(v, g, d, orientation):
    """(classe, composante, effectif) ; classe 0 inconnue, 1 calme, 2 offshore, 3 travers, 4 onshore."""
    if v is None or d is None:
        return 0, ABSENT, ABSENT
    plein = g if (g is not None and g > v) else v
    effectif = arrondi(Fraction(v + plein, 2))
    c = cos_pm(d - orientation)
    x = Fraction(v * c, 1000)  # v · cos(d − β), arrondi moitié loin de zéro
    comp = int((abs(x) + Fraction(1, 2)).__floor__()) * (1 if x >= 0 else -1)
    if effectif < B["calme_dixiemes"]:
        return 1, comp, effectif
    if c <= B["cos_offshore_max"]:
        return 2, comp, effectif
    if c >= B["cos_onshore_min"]:
        return 4, comp, effectif
    return 3, comp, effectif


def noter(spot, cond):
    """(disponible, points, ph, pp, pv, note, position hauteur, facteur limitant)."""
    hauteur, periode, v, g, d = cond
    if hauteur is None or periode is None or v is None or d is None:
        return (0, 0, 0, 0, 0, 0, 0, 0)
    bas, haut = spot["hmin"], spot["hmax"]
    if hauteur < bas:
        ph, pos = rampe(hauteur, arrondi(Fraction(bas * B["hauteur_sous_min_nul_pm"], 1000)), 0, bas, B["points_hauteur"]), 1
    elif hauteur > haut:
        ph, pos = rampe(hauteur, haut, B["points_hauteur"], arrondi(Fraction(haut * B["hauteur_sur_max_nul_pm"], 1000)), 0), 3
    else:
        ph, pos = B["points_hauteur"], 2
    pp = rampe(periode, B["periode_nulle_ds"], 0, B["periode_pleine_ds"], B["points_periode"])
    classe, _, effectif = vent_classe(v, g, d, spot["orientation"])
    calme = B["calme_dixiemes"]
    if classe == 2:
        pv = rampe(effectif, B["offshore_plein_jusqua"], B["points_vent"], B["offshore_nul_a"], 0)
    elif classe == 3:
        pv = rampe(effectif, calme, B["points_vent"], B["travers_nul_a"], 0)
    elif classe == 4:
        pv = rampe(effectif, calme, B["points_vent"], B["onshore_nul_a"], 0)
    else:
        pv = B["points_vent"]
    points = ph + pp + pv
    note = min(max((points + 5) // 10, 0), 10)
    manque = [(B["points_vent"] - pv, 3), (B["points_hauteur"] - ph, 1), (B["points_periode"] - pp, 2)]
    pire, limitant = manque[0]
    for m, code in manque[1:]:
        if m > pire:  # strict : à égalité le vent, puis la hauteur, puis la période
            pire, limitant = m, code
    if pire < B["limitant_seuil"]:
        limitant = 0
    return (1, points, ph, pp, pv, note, pos, limitant)


def creneau(spot, mer, vent, maintenant, decalage, jour_decale):
    """(trouvé, début, fin, heures, points, note)."""
    jour_cible = (maintenant + decalage) // 86400 + jour_decale
    pts = []
    for i in range(mer["n"]):
        t = mer["t"][i]
        local = t + decalage
        jour, seconde = local // 86400, local % 86400
        utilisable = (jour == jour_cible and seconde >= B["creneau_debut_h"] * 3600
                      and seconde + 3600 <= B["creneau_fin_h"] * 3600 and t + 3600 > maintenant)
        n = noter(spot, conditions(mer, vent, i)) if utilisable else None
        pts.append(n[1] if (n and n[0]) else None)
    meilleure, somme0 = None, -1
    for i in range(mer["n"] - 1):
        if pts[i] is None or pts[i + 1] is None:
            continue
        if pts[i] + pts[i + 1] > somme0:
            somme0, meilleure = pts[i] + pts[i + 1], i
    if meilleure is None:
        return (0, 0, 0, 0, 0, 0)
    bas, haut, somme = meilleure, meilleure + 1, somme0
    while haut - bas + 1 < B["creneau_max"]:
        g_ok = bas > 0 and pts[bas - 1] is not None and 2 * pts[bas - 1] + 2 * B["creneau_tolerance"] >= somme0
        d_ok = haut + 1 < mer["n"] and pts[haut + 1] is not None and 2 * pts[haut + 1] + 2 * B["creneau_tolerance"] >= somme0
        if not g_ok and not d_ok:
            break
        if g_ok and (not d_ok or pts[bas - 1] >= pts[haut + 1]):
            bas -= 1
            somme += pts[bas]
        else:
            haut += 1
            somme += pts[haut]
    heures = haut - bas + 1
    points = arrondi(Fraction(somme, heures))
    return (1, mer["t"][bas], mer["t"][haut] + 3600, heures, points, min((points + 5) // 10, 10))


# ---------------------------------------------------------------------------------------------------------------------------
def valeurs_attendues(cas, mer, vent, maintenant):
    """Liste de (nom, entier) dans l'ordre de aplatir() de test_surf_reponses.cpp."""
    v = []

    def ajouter(nom, valeur):
        v.append((nom, int(valeur)))

    spot = {"orientation": cas["orientation_deg"], "hmin": FOURCHETTE_DEFAUT[0], "hmax": FOURCHETTE_DEFAUT[1]}
    n = mer["n"]
    # A. mer
    ajouter("mer.decalage_utc_s", mer["decalage"])
    ajouter("mer.nb", n)
    ajouter("mer.a_vagues", any(x is not None for x in mer["vagues"]))
    ajouter("mer.a_houle", any(x is not None for x in mer["houle"]))
    ajouter("mer.a_maree", any(x is not None for x in mer["maree"]))
    ajouter("mer.a_temperature", any(x is not None for x in mer["sst"]))
    for i in range(n):
        for nom, cle in (("debut_s", "t"), ("vagues_cm", "vagues"), ("vagues_periode_ds", "vagues_p"),
                         ("vagues_direction_deg", "vagues_d"), ("houle_cm", "houle"), ("houle_periode_ds", "houle_p"),
                         ("houle_direction_deg", "houle_d"), ("mer_vent_cm", "mv"), ("mer_vent_periode_ds", "mv_p"),
                         ("mer_vent_direction_deg", "mv_d"), ("temperature_mer_dixiemes", "sst"), ("maree_cm", "maree")):
            ajouter(f"mer.heures[{i}].{nom}", mer[cle][i] if cle == "t" else a(mer[cle][i]))
    # B. vent
    ajouter("vent.decalage_utc_s", vent["decalage"])
    ajouter("vent.nb", vent["n"])
    for i in range(vent["n"]):
        ajouter(f"vent.heures[{i}].debut_s", vent["t"][i])
        ajouter(f"vent.heures[{i}].vitesse_dixiemes_kmh", a(vent["v"][i]))
        ajouter(f"vent.heures[{i}].rafale_dixiemes_kmh", a(vent["g"][i]))
        ajouter(f"vent.heures[{i}].direction_deg", a(vent["d"][i]))
    # C. marée
    notable, nb, marnage, debut, fin, gardes, tendance, pleine, basse = maree_complete(mer, maintenant)
    ajouter("maree.notable", notable)
    ajouter("maree.nb", nb)
    ajouter("maree.marnage_cm", marnage)
    ajouter("maree.serie_debut_s", debut)
    ajouter("maree.serie_fin_s", fin)
    for k, g in enumerate(gardes):
        ajouter(f"maree.extrema[{k}].instant_s", g[0])
        ajouter(f"maree.extrema[{k}].niveau_cm", g[1])
        ajouter(f"maree.extrema[{k}].pleine", g[2])
    ajouter("maree.tendance", tendance)
    ajouter("maree.prochaine_pleine_s", pleine)
    ajouter("maree.prochaine_basse_s", basse)
    # D. vent classé et E. note, heure par heure
    for i in range(n):
        cond = conditions(mer, vent, i)
        classe, comp, eff = vent_classe(cond[2], cond[3], cond[4], spot["orientation"])
        ajouter(f"vent[{i}].classe", classe)
        ajouter(f"vent[{i}].composante_dixiemes_kmh", comp)
        ajouter(f"vent[{i}].effectif_dixiemes_kmh", eff)
        for nom, valeur in zip(("disponible", "points", "points_hauteur", "points_periode", "points_vent", "note", "hauteur",
                                "limitant"), noter(spot, cond)):
            ajouter(f"note[{i}].{nom}", valeur)
    # F. créneaux d'aujourd'hui et de demain
    decalage = vent["decalage"] if vent["n"] > 0 else mer["decalage"]
    for jour in (0, 1):
        for nom, valeur in zip(("trouve", "debut_s", "fin_s", "heures", "points", "note"),
                               creneau(spot, mer, vent, maintenant, decalage, jour)):
            ajouter(f"creneau[{jour}].{nom}", valeur)
    # G. évaluation de l'heure en cours et clé de classement
    donnees = any(x is not None for x in mer["vagues"]) and vent["n"] > 0
    heure = next((i for i in range(n) if mer["t"][i] <= maintenant < mer["t"][i] + 3600), -1)
    maint = noter(spot, conditions(mer, vent, heure)) if (donnees and heure >= 0) else (0,) * 8
    ajouter("evaluation.donnees", donnees)
    ajouter("evaluation.maintenant.disponible", maint[0])
    ajouter("evaluation.maintenant.points", maint[1])
    if not donnees:
        cle = -1
    else:
        cr = creneau(spot, mer, vent, maintenant, decalage, 0)
        cle = cr[4] if cr[0] else (maint[1] if maint[0] else -1)
    ajouter("evaluation.cle", cle)
    return v


def litteral(nom, brut):
    texte = brut.decode("utf-8")
    assert ')OM"' not in texte
    return f'R"OM({texte})OM"'


def construire(sans_ecrire=False):
    manifeste = json.loads((DONNEES / "surf_manifeste.json").read_text(encoding="utf-8"))
    maintenant = manifeste["capture_debut_s"]
    lignes = [ENTETE,
              "// Contenu : pour chaque lieu de surf_manifeste.json, la réponse MER et la réponse VENT en littéral brut (octet pour octet)\n"
              "// et les valeurs attendues (ordre de aplatir() dans test_surf_reponses.cpp). Inclus DANS un espace de noms anonyme,\n"
              "// après la définition de `struct CasSurf`, par test_surf_reponses.cpp seulement.\n",
              f"constexpr long long SURF_MAINTENANT_S = {maintenant}LL;\n",
              f"constexpr long long SURF_PREMIERE_HEURE_S = {manifeste['premiere_heure_s']}LL;\n",
              "// Cosinus de 0° à 90° en millièmes : math.cos, arrondi moitié loin de zéro.\nconstexpr int SURF_COSINUS_PM[91] = {"
              + ", ".join(str(cos_pm(d)) for d in range(91)) + "};\n"]
    resume, tableau, ordre = [], [], []
    for cas in manifeste["spots"]:
        ident = cas["id"]
        brut_mer = (DONNEES / f"surf_marine_{ident}.json").read_bytes()
        brut_vent = (DONNEES / f"surf_vent_{ident}.json").read_bytes()
        mer = lire_marine(json.loads(brut_mer.decode("utf-8"), parse_float=Decimal, parse_int=int))
        vent = lire_vent(json.loads(brut_vent.decode("utf-8"), parse_float=Decimal, parse_int=int))
        valeurs = valeurs_attendues(cas, mer, vent, maintenant)
        lignes.append(f"\n// ---- {ident} : {cas['nom']} ({cas['note']}) ; {len(valeurs)} valeurs attendues\n")
        lignes.append(f"constexpr std::string_view SURF_MER_{ident.upper()} =\n{litteral(ident, brut_mer)};\n")
        lignes.append(f"constexpr std::string_view SURF_VENT_{ident.upper()} =\n{litteral(ident, brut_vent)};\n")
        corps = [f"{x}LL" for _, x in valeurs]
        bloc = ",\n".join(", ".join(corps[i:i + 16]) for i in range(0, len(corps), 16))
        lignes.append(f"constexpr long long SURF_ATT_{ident.upper()}[] = {{\n{bloc}}};\n")
        tableau.append(f'    {{"{ident}", {cas["latitude_e5"]}, {cas["longitude_e5"]}, {cas["orientation_deg"]}, '
                       f'SURF_MER_{ident.upper()}, SURF_VENT_{ident.upper()}, SURF_ATT_{ident.upper()}, {len(valeurs)}}},\n')
        ordre.append((ident, valeurs))
        notable, nb, marnage, *_ = maree_complete(mer, maintenant)
        resume.append(f"{ident}: mer {len(brut_mer)} o, vent {len(brut_vent)} o, {len(valeurs)} valeurs ; marée notable={notable} "
                      f"extrema={nb} marnage={marnage} cm")
    lignes.append("\nconstexpr CasSurf SURF_CAS[] = {\n" + "".join(tableau) + "};\n")
    return "".join(lignes), resume, ordre


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true")
    ap.add_argument("--expliquer", nargs=2, metavar=("ID", "RANG"), help="nom de la valeur de ce rang pour ce lieu")
    a_ = ap.parse_args()
    texte, resume, ordre = construire()
    if a_.expliquer:
        ident, rang = a_.expliquer[0], int(a_.expliquer[1])
        for i, valeurs in ordre:
            if i == ident:
                print(f"{ident} rang {rang} = {valeurs[rang][0]} attendu {valeurs[rang][1]}")
                return 0
        raise SystemExit(f"lieu inconnu : {ident}")
    chemin = DONNEES / "surf_cas.inc"
    print("\n".join(resume))
    if a_.verifier:
        # git peut convertir les fins de ligne (core.autocrlf) : on compare sans tenir compte de CRLF/LF
        ok = chemin.exists() and chemin.read_bytes().replace(b"\r\n", b"\n") == texte.encode("utf-8")
        print("vérifié : tout est à jour" if ok else "PAS À JOUR : surf_cas.inc")
        return 0 if ok else 1
    chemin.write_bytes(texte.encode("utf-8"))
    print(f"écrit : {chemin.name} ({len(texte)} caractères)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
