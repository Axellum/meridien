#!/usr/bin/env python3
"""Génère la police des textes de l'écran : un SOUS-ENSEMBLE de Roboto Bold (main/assets/roboto_700.ttf), réglé par
`tools/icones/police_texte.json`.

    python tools/icones/generer_police_texte.py --source <roboto_700.ttf d'origine>   # écrit la police et met à jour le manifeste
    python tools/icones/generer_police_texte.py --verifier                            # contrôle le fichier de l'arbre, sans la source

Ce qui est retiré (et pourquoi le dessin ne change pas) :
  * aucun caractère : les blocs retirés du manifeste sont vides. Le grec et le cyrillique RESTENT : les alertes de Vigie peuvent
    être rédigées dans ces écritures (ecran/vigie_alertes.hpp, PLAGES_POLICE ; test/host/controle_python_vigie.py le vérifie). Un
    bloc ajouté au manifeste doit d'abord sortir de ces plages ;
  * les instructions de hinting (tables fpgm, prep, cvt et instructions des glyphes) : TinyTTF (stb_truetype) ne les exécute pas ;
  * les fonctions typographiques autres que le crénage (`kern`, dans GPOS) : stb_truetype ne lit ni GSUB ni les autres fonctions.
Les contours des glyphes gardés sont ceux d'origine, octet pour octet : le banc de rendu (tools/rendre.ps1) le constate au pixel.

Le fichier est reproductible : mêmes sources, mêmes octets (aucun horodatage recalculé). Dépendance : fontTools (`pip install
fonttools`), outil de développement, pas livré dans le firmware ; la compilation n'a pas besoin de Python. Les caractères des sept
langues sont contrôlés par components/core/outils/verifier_glyphes.py.

Licence : SIL OFL 1.1. Un sous-ensemble est une « Modified Version » au sens de l'OFL ; Roboto n'a pas de « Reserved Font Name »
(main/assets/OFL.txt), le nom peut donc rester. La table de noms (copyright, licence) est gardée entière. Voir main/assets/README.md.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
MANIFESTE = Path(__file__).resolve().parent / "police_texte.json"


def lire_manifeste() -> dict:
    return json.loads(MANIFESTE.read_text(encoding="utf-8"))


def sha256(chemin: Path) -> str:
    return hashlib.sha256(chemin.read_bytes()).hexdigest()


def blocs_retires(manifeste: dict) -> list[tuple[int, int, str]]:
    return [(int(b["de"], 16), int(b["a"], 16), b["nom"]) for b in manifeste["blocs_retires"]]


def retire(point: int, blocs: list[tuple[int, int, str]]) -> bool:
    return any(de <= point <= a for de, a, _ in blocs)


def ouvrir_police(chemin: Path):
    from fontTools.ttLib import TTFont  # noqa: PLC0415  (outil de développement)

    # recalcTimestamp=False : sinon fontTools écrit l'heure courante dans `head.modified` et deux générations diffèrent.
    return TTFont(str(chemin), recalcTimestamp=False)


def generer(source: Path) -> int:
    from fontTools import subset  # noqa: PLC0415

    manifeste = lire_manifeste()
    attendu = manifeste["source"]["sha256"]
    if sha256(source) != attendu:
        print(f"ECHEC  la police source n'est pas celle du manifeste (SHA-256 {sha256(source)[:16]}… au lieu de {attendu[:16]}…)")
        return 1
    police = ouvrir_police(source)
    blocs = blocs_retires(manifeste)
    gardes = sorted(p for p in (police.getBestCmap() or {}) if not retire(p, blocs))
    options = subset.Options()
    options.layout_features = ["kern"]  # le seul que stb_truetype lit (GPOS, paires)
    options.drop_tables += ["GSUB"]  # substitutions (ligatures, formes) : stb_truetype ne les lit pas
    options.hinting = False
    options.glyph_names = False
    # Copyright, licence, auteurs : toute la table de noms est gardée (identifiants et langues lus dans la source).
    options.name_IDs = sorted({r.nameID for r in police["name"].names})
    options.name_languages = sorted({r.langID for r in police["name"].names})
    options.recalc_timestamp = False
    options.notdef_outline = True
    sous_ensemble = subset.Subsetter(options)
    sous_ensemble.populate(unicodes=gardes)
    sous_ensemble.subset(police)
    sortie = RACINE / manifeste["sortie"]["fichier"]
    police.save(str(sortie))
    manifeste["sortie"]["sha256"] = sha256(sortie)
    manifeste["sortie"]["taille"] = sortie.stat().st_size
    manifeste["sortie"]["caracteres"] = len(gardes)
    MANIFESTE.write_text(json.dumps(manifeste, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"ecrit  {sortie.relative_to(RACINE)} : {manifeste['sortie']['taille']} octets, {len(gardes)} caractères, "
          f"SHA-256 {manifeste['sortie']['sha256']}")
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
    try:
        police = ouvrir_police(sortie)
    except ImportError:
        print("  saute   fonttools absent (pip install fonttools) : cmap non contrôlée")
        return echecs
    cmap = police.getBestCmap() or {}
    if len(cmap) == manifeste["sortie"]["caracteres"]:
        ok(f"cmap : {len(cmap)} caractères, comme le manifeste")
    else:
        ko(f"cmap : {len(cmap)} caractères au lieu de {manifeste['sortie']['caracteres']}")
    blocs = blocs_retires(manifeste)
    restes = [p for p in cmap if retire(p, blocs)]
    if restes:
        ko(f"{len(restes)} caractère(s) d'un bloc retiré encore présents (U+{restes[0]:04X}…)")
    else:
        ok("aucun caractère des blocs retirés")
    absents = [chr(p) for p in range(0x20, 0x250) if not retire(p, blocs) and p not in cmap and chr(p).isprintable()]
    source_latin = manifeste["source"]["latin_absents"]
    if "".join(absents) == source_latin:
        ok("alphabet latin (U+0020 à U+024F) : tout ce que la source a")
    else:
        ko(f"alphabet latin incomplet : manquent {''.join(absents)!r} (la source n'avait pas {source_latin!r})")
    for table in ("fpgm", "prep", "cvt ", "GSUB"):
        if table in police:
            ko(f"table « {table.strip()} » encore présente")
    if "GPOS" in police:
        ok("crénage (GPOS) gardé")
    else:
        ko("crénage (GPOS) perdu : l'espacement des lettres changerait")
    noms = {r.nameID for r in police["name"].names}
    if {0, 14} <= noms:
        ok("table de noms : copyright (0) et adresse de la licence (14) gardés")
    else:
        ko(f"table de noms incomplète : {sorted(noms)}")
    return echecs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", type=Path, help="roboto_700.ttf d'origine (celle du manifeste : le SHA-256 est contrôlé)")
    ap.add_argument("--verifier", action="store_true", help="contrôle le fichier de l'arbre, sans la police source")
    a = ap.parse_args()
    if a.source:
        return generer(a.source)
    if a.verifier:
        return verifier()
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
