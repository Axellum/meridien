#!/usr/bin/env python3
"""Génère la police d'icônes de la roue d'appui long : un SOUS-ENSEMBLE de Material Design Icons (ADR-0019), seulement les glyphes
listés dans `tools/icones/roue_glyphes.json`.

    python tools/icones/generer_police_roue.py --source <materialdesignicons-webfont.ttf>   # écrit la police, le .inc et le manifeste
    python tools/icones/generer_police_roue.py --verifier                                   # contrôle les fichiers de l'arbre, sans la source

Écrit :
  main/assets/mdi_roue.ttf       le sous-ensemble (quelques Ko), lu à l'exécution par TinyTTF comme Roboto ;
  main/ui_roue_icones.inc        une constante C++ par glyphe (`ICONE_<ID>`, le caractère en UTF-8), pour que le code ne recopie pas
                                 un point de code ;
  tools/icones/roue_glyphes.json la taille et le SHA-256 de la sortie (rien d'autre n'est réécrit).

Ce que `--verifier` prouve, sans la police source : la `cmap` du fichier contient EXACTEMENT les glyphes du manifeste, chacun a des
contours (une icône n'est jamais un rectangle vide : un glyphe sans contour s'affiche vide), le .inc est celui que le manifeste donne, le
SHA-256 du manifeste est celui du fichier. Ce qu'il ne prouve pas : que l'icône ressemble à ce qu'on attend (cela se regarde, sur un
rendu du banc PC puis sur la carte).

Le fichier est reproductible : mêmes sources, mêmes octets (aucun horodatage recalculé). Dépendance : fontTools (`pip install fonttools`),
outil de développement, pas livré dans le firmware ; la compilation n'a pas besoin de Python.

Licence de la police source : voir main/assets/README.md et THIRD_PARTY.md. Un sous-ensemble est un fichier MODIFIÉ au sens de l'Apache 2.0
(§4) : le README le dit.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
MANIFESTE = Path(__file__).resolve().parent / "roue_glyphes.json"
INC = RACINE / "main" / "ui_roue_icones.inc"


def lire_manifeste() -> dict:
    return json.loads(MANIFESTE.read_text(encoding="utf-8"))


def point_de_code(glyphe: dict) -> int:
    return int(glyphe["point"], 16)


def utf8_c(point: int) -> str:
    """Les octets UTF-8 d'un point de code, en échappements hexadécimaux C (« \\xF3\\xB0... »)."""
    return "".join(f"\\x{octet:02X}" for octet in chr(point).encode("utf-8"))


def contenu_inc(manifeste: dict) -> str:
    lignes = [
        "// GÉNÉRÉ par tools/icones/generer_police_roue.py d'après tools/icones/roue_glyphes.json : ne pas modifier à la main.",
        "// Un caractère par glyphe de main/assets/mdi_roue.ttf (Material Design Icons, sous-ensemble), en UTF-8.",
    ]
    # Les commentaires de fin de ligne sont alignés comme le fait clang-format (le crochet de commit le contrôle) : la sortie du
    # générateur est déjà conforme, donc `--verifier` peut la comparer octet pour octet.
    declarations = [
        (f'inline constexpr char ICONE_{g["id"].upper()}[] = "{utf8_c(point_de_code(g))}";', f'// mdi {g["mdi"]} U+{g["point"]}')
        for g in manifeste["glyphes"]
    ]
    largeur = max((len(d) for d, _ in declarations), default=0)
    lignes += [f"{d.ljust(largeur)} {c}" for d, c in declarations]
    return "\n".join(lignes) + "\n"


def sha256(chemin: Path) -> str:
    return hashlib.sha256(chemin.read_bytes()).hexdigest()


def ouvrir_police(chemin: Path):
    from fontTools.ttLib import TTFont  # noqa: PLC0415  (outil de développement)

    # recalcTimestamp=False : sinon fontTools écrit l'heure courante dans `head.modified` et deux générations diffèrent d'un octet.
    return TTFont(str(chemin), recalcTimestamp=False)


def generer(source: Path) -> int:
    from fontTools import subset  # noqa: PLC0415

    manifeste = lire_manifeste()
    attendu = manifeste["source"]["sha256"]
    if sha256(source) != attendu:
        print(f"ECHEC  la police source n'est pas celle du manifeste (SHA-256 {sha256(source)[:16]}… au lieu de {attendu[:16]}…)")
        return 1
    police = ouvrir_police(source)
    cmap = police.getBestCmap() or {}
    for g in manifeste["glyphes"]:
        nom = cmap.get(point_de_code(g))
        if nom != g["mdi"]:
            print(f"ECHEC  U+{g['point']} est « {nom} » dans la police source, le manifeste dit « {g['mdi']} »")
            return 1
    options = subset.Options()
    options.layout_features = []  # aucune ligature, aucun décalage : un caractère = une icône
    options.hinting = False
    options.glyph_names = False
    options.name_IDs = [1, 5]  # ce que la police source porte (famille et version) : rien d'autre à garder
    options.recalc_timestamp = False  # reproductible : mêmes octets à chaque génération
    options.notdef_outline = True
    sous_ensemble = subset.Subsetter(options)
    sous_ensemble.populate(unicodes=[point_de_code(g) for g in manifeste["glyphes"]])
    sous_ensemble.subset(police)
    sortie = RACINE / manifeste["sortie"]["fichier"]
    police.save(str(sortie))
    INC.write_text(contenu_inc(manifeste), encoding="utf-8", newline="\n")
    manifeste["sortie"]["sha256"] = sha256(sortie)
    manifeste["sortie"]["taille"] = sortie.stat().st_size
    MANIFESTE.write_text(json.dumps(manifeste, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"ecrit  {sortie.relative_to(RACINE)} : {manifeste['sortie']['taille']} octets, SHA-256 {manifeste['sortie']['sha256']}")
    print(f"ecrit  {INC.relative_to(RACINE)} ({len(manifeste['glyphes'])} constantes)")
    return verifier()


def verifier() -> int:
    manifeste = lire_manifeste()
    sortie = RACINE / manifeste["sortie"]["fichier"]
    echecs = 0

    def ok(texte: str) -> None:
        print(f"  ok      {texte}")

    def ko(texte: str) -> None:
        nonlocal echecs
        echecs += 1
        print(f"  ECHEC   {texte}")

    if not sortie.exists():
        ko(f"{sortie.relative_to(RACINE)} n'existe pas")
        return 1
    if sha256(sortie) == manifeste["sortie"]["sha256"] and sortie.stat().st_size == manifeste["sortie"]["taille"]:
        ok(f"{sortie.name} : taille et SHA-256 du manifeste ({manifeste['sortie']['taille']} octets)")
    else:
        ko(f"{sortie.name} n'est pas celui du manifeste : régénérer avec --source")
    if INC.exists() and INC.read_text(encoding="utf-8") == contenu_inc(manifeste):
        ok(f"{INC.name} : les constantes du manifeste, octet pour octet")
    else:
        ko(f"{INC.name} n'est pas celui que le manifeste donne : régénérer avec --source")
    try:
        police = ouvrir_police(sortie)
    except ImportError:
        print("  saute   fonttools absent (pip install fonttools) : cmap et contours non contrôlés")
        return echecs
    cmap = police.getBestCmap() or {}
    voulus ={point_de_code(g): g for g in manifeste["glyphes"]}
    en_trop = sorted(set(cmap) - set(voulus))
    manquants = sorted(set(voulus) - set(cmap))
    if en_trop or manquants:
        ko(f"la cmap n'est pas celle du manifeste (en trop : {[hex(p) for p in en_trop]}, manquants : {[hex(p) for p in manquants]})")
    else:
        ok(f"la cmap contient exactement les {len(voulus)} glyphes du manifeste")
    glyf = police["glyf"]
    for point, g in sorted(voulus.items()):
        nom = cmap.get(point)
        if nom is None:
            continue
        contours = glyf[nom].numberOfContours
        if contours == 0:
            ko(f"U+{g['point']} ({g['mdi']}) n'a aucun contour : il s'afficherait comme un rectangle vide")
        else:
            ok(f"U+{g['point']} {g['mdi']:14} {'composé' if contours < 0 else str(contours) + ' contour(s)'}")
    return echecs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", type=Path, help="materialdesignicons-webfont.ttf (celle du manifeste : le SHA-256 est contrôlé)")
    ap.add_argument("--verifier", action="store_true", help="contrôle les fichiers de l'arbre, sans la police source")
    a = ap.parse_args()
    if a.source:
        return generer(a.source)
    if a.verifier:
        return verifier()
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
