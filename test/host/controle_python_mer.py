#!/usr/bin/env python3
"""Seconde lecture des trames de la mer (SEA, ACT, CRIT), en Python seul, sans une ligne du noyau C++ (mer_trame.hpp, mer_vue.hpp).

    python test/host/controle_python_mer.py              # (ré)écrit donnees/mer_<spot>.inc et donnees/attendu_mer_<spot>.inc
    python test/host/controle_python_mer.py --verifier   # ne modifie rien ; code 1 si un .inc n'est plus à jour

Pour chaque trame réelle `donnees/mer_<spot>.txt` (test/host/capturer_mer.py) :
  - `mer_<spot>.inc` : les mêmes octets en littéral C++ brut, pour `#include` dans un test ;
  - `attendu_mer_<spot>.inc` : ce que le noyau doit lire de la trame (CRC, ligne SEA, quatre lignes ACT et leurs CRIT) et, pour une
    vingtaine d'instants pris autour des bornes (premier instant, âge de 600 s et de 6 h, fin des 24 heures, chaque marée), ce que les
    fonctions de vue doivent répondre. La hauteur de la mer entre deux marées est calculée à virgule (`math.cos`), en millièmes de
    centimètre : le noyau, lui, n'a que des entiers.

La lecture est faite d'après le CONTRAT (vigie-api, docs/embedded-frame.md : 18 champs pour SEA, 12 pour ACT, 9 pour CRIT, CRC32 IEEE en
majuscules sur les octets avant la ligne END), pas d'après le code de l'écran de Vigie ni celui du noyau. Les décimales sont lues avec
`decimal` (arrondi loin de zéro), jamais par un flottant. Un champ vide reste INCONNU.

Ordre des champs de `Att` dans test_mer_trame.cpp / test_mer_vue.cpp (la structure y est écrite, ce script la remplit dans cet ordre) :
  crc, nom, lieu, genere, premiere_heure, gardees, vent_min_d, vent_max_d, lever, coucher, lever2, coucher2,
  nb_marees, marees[8]{haute, instant, cm}, nb_coefficients, coefficients[4]{pleine_mer, valeur},
  vigilances_lues, vigilance_presente, niveau, vigilance_fin, source,
  activites[4]{nom, disponible, verdict, raison_critere, raison_etat, creneau_debut, creneau_fin, heures, lumiere, detail, points, sur,
               nb_criteres, criteres[6]{id, valeur_d, unite, etat, verdict, points, points_max}},
  nb_points, points[48]{maintenant, heure, fraicheur, meilleure, prochaine_maree, coefficient},
  nb_hauteurs, hauteurs[64]{instant, milli_cm}, nb_hors, hors[8] (instants sans hauteur).
"""
import argparse
import json
import math
import re
import sys
import zlib
from decimal import ROUND_HALF_UP, Decimal
from pathlib import Path

ICI = Path(__file__).resolve().parent
DONNEES = ICI / "donnees"
ENTETE = "// FICHIER GÉNÉRÉ par test/host/controle_python_mer.py : ne pas modifier à la main.\n"
RACINE = ICI.parents[1]
POLICE = RACINE / "main" / "assets" / "roboto_700.ttf"
TEXTES_HPP = RACINE / "components" / "core" / "include" / "ecran" / "mer_textes.hpp"

SPOTS = ["hossegor", "la_torche", "rochebonne", "port_vieux", "govelle", "ondarraitz"]
ACTIVITES = ["surf", "kite", "boat", "shore_fishing"]
HEURES = 24
VIEILLIE_APRES_S = 600  # choix de Vigie, pas du contrat : au-delà, on dit l'âge
EXPIREE_APRES_S = 21600  # au-delà, on ne montre plus les verdicts
INCONNU = "INCONNU"  # le jeton du noyau pour un nombre inconnu (mer_trame.hpp)


# ---- lecture de la trame d'après le contrat ----------------------------------------------------------------------------------------
def fixe(champ: str, decimales: int):
    """'-1.66' -> -166 (2 décimales) ; vide -> None. Arrondi loin de zéro."""
    if champ == "":
        return None
    v = Decimal(champ).scaleb(decimales).quantize(Decimal(1), rounding=ROUND_HALF_UP)
    return int(v)


def entier(champ: str) -> int:
    return int(champ) if champ != "" else 0


def points(champ: str) -> int:
    if champ == "":
        return -1
    v = int(champ)
    return v if 0 <= v <= 100 else -1


def lire(octets: bytes) -> dict:
    debut_end = octets.rindex(b"END|")
    crc_annonce = int(octets[debut_end + 4 : debut_end + 12], 16)
    crc_calcule = zlib.crc32(octets[:debut_end]) & 0xFFFFFFFF
    assert crc_annonce == crc_calcule, f"CRC de la capture : annoncé {crc_annonce:08X}, calculé {crc_calcule:08X}"
    texte = octets.decode("utf-8")
    sea = None
    activites: dict[str, dict] = {}
    courante = None
    for ligne in texte.split("\n"):
        champs = ligne.split("|")
        if champs[0] == "SEA":
            assert len(champs) == 18, f"SEA a {len(champs)} champs"
            sea = champs
        elif champs[0] == "ACT":
            assert len(champs) == 12, f"ACT a {len(champs)} champs"
            courante = champs[1]
            activites[courante] = {"act": champs, "crit": []}
        elif champs[0] == "CRIT":
            assert len(champs) == 9, f"CRIT a {len(champs)} champs"
            assert courante is not None and champs[1] == courante
            activites[courante]["crit"].append(champs)
    assert sea is not None
    marees = []
    if sea[12]:
        for element in sea[12].split(","):
            sens, instant, hauteur = element.split(":")
            marees.append((1 if sens == "H" else 0, int(instant), fixe(hauteur, 2)))
    coefficients = []
    if sea[13]:
        for element in sea[13].split(","):
            instant, valeur = element.split(":")
            coefficients.append((int(instant), int(valeur)))
    return {
        "crc": crc_calcule,
        "nom": sea[1],
        "lieu": sea[2],
        "genere": int(sea[3]),
        "premiere": entier(sea[4]),
        "gardees": 1 if sea[5] == "1" else 0,
        "vent_min_d": fixe(sea[6], 1),
        "vent_max_d": fixe(sea[7], 1),
        "lever": entier(sea[8]),
        "coucher": entier(sea[9]),
        "lever2": entier(sea[10]),
        "coucher2": entier(sea[11]),
        "marees": marees,
        "coefficients": coefficients,
        "statut": sea[14],
        "niveau": sea[15],
        "vigilance_fin": entier(sea[16]),
        "source": sea[17],
        "activites": activites,
    }


# ---- les fonctions de vue, d'après les règles de la page (pas le code du noyau) --------------------------------------------------
def indice_heure(m: dict, maintenant: int) -> int:
    if m["premiere"] == 0 or maintenant < m["premiere"]:
        return -1
    i = (maintenant - m["premiere"]) // 3600
    return i if i < HEURES else -1


def fraicheur(m: dict, maintenant: int) -> int:
    """1 fraîche, 2 vieillie, 3 expirée (0 : rien reçu, pas de cas ici)."""
    if maintenant == 0:
        return 2
    age = max(0, maintenant - m["genere"])
    if age > EXPIREE_APRES_S:
        return 3
    if m["premiere"] != 0 and maintenant >= m["premiere"] + HEURES * 3600:
        return 3
    if m["gardees"] or age > VIEILLIE_APRES_S:
        return 2
    return 1


def meilleure_activite(m: dict) -> int:
    rang = {"good": 3, "possible": 2, "poor": 1}
    meilleure, meilleur_rang = -1, 0
    for i, nom in enumerate(ACTIVITES):
        a = m["activites"].get(nom)
        if a is None or a["act"][2] != "1":
            continue
        r = rang.get(a["act"][3], 0)
        if r > meilleur_rang:
            meilleure, meilleur_rang = i, r
    return meilleure


def prochaine_maree(m: dict, maintenant: int) -> int:
    for i, (_, instant, _) in enumerate(m["marees"]):
        if instant >= maintenant:
            return i
    return -1


def coefficient_pour(m: dict, maintenant: int) -> int:
    for i, (instant, valeur) in enumerate(m["coefficients"]):
        if valeur != 0 and instant >= maintenant:
            return i
    return -1


def hauteur_milli_cm(m: dict, instant: int):
    """Hauteur à virgule entre deux marées qui se suivent : a + (b - a) (1 - cos(pi t)) / 2, en millièmes de centimètre ; None hors des marées."""
    mar = m["marees"]
    for i in range(len(mar) - 1):
        (_, ta, ha), (_, tb, hb) = mar[i], mar[i + 1]
        if instant < ta or instant > tb:
            continue
        t = (instant - ta) / (tb - ta)
        h = ha + (hb - ha) * (1 - math.cos(math.pi * t)) / 2
        return round(h * 1000)
    return None


def instants_d_essai(m: dict) -> list:
    g, p = m["genere"], m["premiere"]
    pts = {0, p - 1, p, p + 1, g - 1, g, g + 1, g + VIEILLIE_APRES_S, g + VIEILLIE_APRES_S + 1, p + 3599, p + 3600, p + 23 * 3600,
           p + HEURES * 3600 - 1, p + HEURES * 3600, g + EXPIREE_APRES_S, g + EXPIREE_APRES_S + 1}
    for _, instant, _ in m["marees"][:3]:
        pts.update({instant - 1, instant, instant + 1})
    for instant, _ in m["coefficients"][:2]:
        pts.update({instant - 1, instant, instant + 1})
    return sorted(pts)


def instants_de_hauteur(m: dict) -> list:
    pts = set()
    mar = m["marees"]
    for i in range(len(mar) - 1):
        ta, tb = mar[i][1], mar[i + 1][1]
        d = tb - ta
        pts.update({ta, ta + 1, ta + d // 4, ta + d // 2, ta + (3 * d) // 4, tb - 1, tb})
        # Des angles qui ne sont PAS des degrés entiers (51,4° ; 69,2° ; 124,6°) : aux quarts de la durée l'angle est entier (45°, 90°,
        # 135°) et le cosinus n'a pas besoin d'être interpolé entre deux degrés, donc un défaut d'interpolation y resterait invisible.
        pts.update({ta + (2 * d) // 7, ta + (5 * d) // 13, ta + (9 * d) // 13})
    return sorted(pts)


# ---- écriture du .inc ------------------------------------------------------------------------------------------------------------
def chaine(s: str) -> str:
    return json.dumps(s, ensure_ascii=False)


def nombre(v) -> str:
    return INCONNU if v is None else str(v)


def attendu(m: dict) -> str:
    out = ["{"]
    out.append(f" {m['crc']}LL, {chaine(m['nom'])}, {chaine(m['lieu'])}, {m['genere']}LL, {m['premiere']}LL, {m['gardees']},")
    out.append(f" {nombre(m['vent_min_d'])}, {nombre(m['vent_max_d'])},")
    out.append(f" {m['lever']}LL, {m['coucher']}LL, {m['lever2']}LL, {m['coucher2']}LL,")
    out.append(f" {len(m['marees'])}, {{ " + ", ".join(f"{{{h}, {t}LL, {nombre(cm)}}}" for h, t, cm in m["marees"]) + " },")
    out.append(f" {len(m['coefficients'])}, {{ " + ", ".join(f"{{{t}LL, {v}}}" for t, v in m["coefficients"]) + " },")
    lues = 1 if m["statut"] == "ok" else 0
    present = 1 if lues and m["niveau"] != "" else 0
    out.append(f" {lues}, {present}, {chaine(m['niveau'] if present else '')}, {m['vigilance_fin'] if present else 0}LL, "
               f"{chaine(m['source'] if present else '')},")
    out.append(" {")
    for nom in ACTIVITES:
        a = m["activites"][nom]["act"]
        crit = m["activites"][nom]["crit"]
        dispo = 1 if a[2] == "1" else 0
        raison = a[4].split(".", 1) if a[4] else ["", ""]
        debut, fin = entier(a[5]), entier(a[6])
        if debut == 0 or fin <= debut:
            debut = fin = 0
        out.append(f"  {{{chaine(nom)}, {dispo}, {chaine(a[3])}, {chaine(raison[0])}, {chaine(raison[1])}, {debut}LL, {fin}LL, "
                   f"{chaine(a[7])}, {chaine(a[8])}, {entier(a[9])}LL, {points(a[10])}, {points(a[11])}, {len(crit)},")
        out.append("   { " + ", ".join(
            f"{{{chaine(c[2])}, {nombre(fixe(c[3], 1))}, {chaine(c[4])}, {chaine(c[5])}, {chaine(c[6])}, {points(c[7])}, {points(c[8])}}}"
            for c in crit) + " }},")
    out.append(" },")
    instants = instants_d_essai(m)
    meilleure = meilleure_activite(m)
    assert len(instants) <= 48, len(instants)
    out.append(f" {len(instants)}, {{")
    for t in instants:
        out.append(f"  {{{t}LL, {indice_heure(m, t)}, {fraicheur(m, t)}, {meilleure}, {prochaine_maree(m, t)}, {coefficient_pour(m, t)}}},")
    out.append(" },")
    hauteurs = [(t, hauteur_milli_cm(m, t)) for t in instants_de_hauteur(m)]
    assert all(h is not None for _, h in hauteurs)
    assert len(hauteurs) <= 64, len(hauteurs)
    out.append(f" {len(hauteurs)}, {{ " + ", ".join(f"{{{t}LL, {h}LL}}" for t, h in hauteurs) + " },")
    hors = [m["marees"][0][1] - 1, m["marees"][-1][1] + 1]
    assert all(hauteur_milli_cm(m, t) is None for t in hors)
    out.append(f" {len(hors)}, {{ " + ", ".join(f"{t}LL" for t in hors) + " }")
    out.append("}")
    return ENTETE + "\n".join(out) + "\n"


def litteral(octets: bytes) -> str:
    texte = octets.decode("utf-8")
    assert ')MER"' not in texte
    return ENTETE + 'R"MER(' + texte + ')MER"\n'


def decoder_litteral(litteral: str) -> str:
    """Le texte d'un littéral C++ : `\\xNN` est un octet (le source écrit « · » et « ° » ainsi), le reste est du UTF-8."""
    octets = bytearray()
    i = 0
    while i < len(litteral):
        if litteral[i] == "\\" and litteral[i + 1] == "x":
            octets.append(int(litteral[i + 2 : i + 4], 16))
            i += 4
        elif litteral[i] == "\\":
            octets += litteral[i + 1].encode("utf-8")
            i += 2
        else:
            octets += litteral[i].encode("utf-8")
            i += 1
    return octets.decode("utf-8")


def caracteres_des_textes() -> set:
    """Chaque caractère des littéraux de mer_textes.hpp (les commentaires ne s'affichent pas : ils sont écartés)."""
    code = "\n".join(re.sub(r"//.*$", "", ligne) for ligne in TEXTES_HPP.read_text(encoding="utf-8").splitlines())
    trouves = set()
    for litteral in re.findall(r'"((?:[^"\\\n]|\\.)*)"', code):
        trouves |= set(decoder_litteral(litteral))
    return {c for c in trouves if ord(c) >= 0x20}


def controle_glyphes() -> int:
    """0 si chaque caractère des textes de la page mer est dans la police embarquée (sinon il s'afficherait en rectangle vide)."""
    try:
        from fontTools.ttLib import TTFont  # noqa: PLC0415  (outil de développement, pas toujours installé)
    except ImportError:
        print("  saute   fonttools absent (pip install fonttools) : caractères de mer_textes.hpp non comparés à la police")
        return 0
    cmap = TTFont(POLICE).getBestCmap() or {}
    caracteres = caracteres_des_textes()
    manque = sorted(c for c in caracteres if ord(c) not in cmap)
    if manque:
        print(f"  ECHEC   caractères de mer_textes.hpp absents de {POLICE.name} : {', '.join(f'U+{ord(c):04X} {c}' for c in manque)}")
        return 1
    print(f"  ok      les {len(caracteres)} caractères des textes de la page mer sont tous dans {POLICE.name}")
    return 0


def controle_mer(verifier: bool = True) -> int:
    """Appelé par lancer.py : 0 si tous les .inc sont à jour. Sans `verifier`, les réécrit."""
    mauvais = 0
    for spot in SPOTS:
        source = DONNEES / f"mer_{spot}.txt"
        if not source.exists():
            print(f"mer : trame {source.name} absente (test/host/capturer_mer.py)")
            mauvais += 1
            continue
        octets = source.read_bytes()
        m = lire(octets)
        voulus = {f"mer_{spot}.inc": litteral(octets), f"attendu_mer_{spot}.inc": attendu(m)}
        for nom, contenu in voulus.items():
            chemin = DONNEES / nom
            if chemin.exists() and chemin.read_bytes() == contenu.encode("utf-8"):
                continue
            if verifier:
                print(f"mer : {nom} n'est plus à jour (python test/host/controle_python_mer.py)")
                mauvais += 1
            else:
                chemin.write_bytes(contenu.encode("utf-8"))
                print(f"  écrit   {nom}")
    if verifier and mauvais == 0:
        print(f"\nmer : {len(SPOTS)} trames lues en Python seul, tous les .inc sont à jour")
    if verifier:
        mauvais += controle_glyphes()
    return mauvais


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verifier", action="store_true", help="ne modifie rien ; code 1 si un .inc n'est plus à jour")
    a = ap.parse_args()
    return 1 if controle_mer(verifier=a.verifier) else 0


if __name__ == "__main__":
    sys.exit(main())
