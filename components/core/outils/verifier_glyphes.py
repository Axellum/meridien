#!/usr/bin/env python3
"""Vérifie que la police embarquée a les caractères des sept langues (ADR-0006). Lecture seule.

    python components/core/outils/verifier_glyphes.py [police.ttf]     (défaut : main/assets/roboto_700.ttf)

Ne touche ni à la police ni à main/. Lit la table « cmap » de la police avec fontTools (pip install fonttools) et
compare avec :
  1. les textes réels de l'écran : toutes les sections « Écran : … » de i18n/<code>.yaml (le français, langue source,
     est lu dans les clés) ;
  2. les alphabets : les lettres propres à chaque langue, minuscules et capitales, plus la ponctuation typographique
     (liste écrite ci-dessous, indépendante des textes actuels : une phrase ajoutée demain ne doit pas en dépendre) ;
  3. pour information, les textes de la page de réglages et de l'API (rendus par le navigateur du téléphone, pas par
     cette police).
Code de sortie 1 si un caractère des points 1 ou 2 manque. Une présence dans la cmap prouve que la police sait
dessiner le caractère, pas que le rendu TinyTTF sur l'écran est correct : cela reste à constater sur carte.
"""

import sys
import unicodedata
from pathlib import Path

from fontTools.ttLib import TTFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generer_i18n  # noqa: E402

POLICE_PAR_DEFAUT = generer_i18n.RACINE / "main" / "assets" / "roboto_700.ttf"

# Lettres non ASCII propres à chaque langue (minuscules puis capitales) et signes que ses textes emploient.
COMMUN = "’‘“”«»…–—°·×±"
ALPHABETS = {
    "fr": "àâäçéèêëîïôöùûüÿœæÀÂÄÇÉÈÊËÎÏÔÖÙÛÜŸŒÆ" + COMMUN,
    "en": COMMUN,
    "de": "äöüßÄÖÜẞ" + COMMUN,
    "nl": "ëïéèêöüĳËÏÉÈÊÖÜĲ" + COMMUN,
    "es": "áéíóúüñÁÉÍÓÚÜÑ¿¡" + COMMUN,
    "it": "àèéìíîòóùúÀÈÉÌÍÎÒÓÙÚ" + COMMUN,
    "tr": "çğıöşüâîûÇĞİÖŞÜÂÎÛ" + COMMUN,
}
# Facultatif : écrits ici pour que le rapport les nomme, mais leur absence ne fait pas échouer le contrôle
# (« ẞ » capitale du ß, « ĳ » et « Ĳ » ligature néerlandaise qu'on écrit presque toujours « ij »).
FACULTATIFS = set("ẞĳĲ")


def nom(c):
    return f"U+{ord(c):04X} {c!r} {unicodedata.name(c, '?')}"


def caracteres(textes):
    """Caractères distincts des textes, sans l'ASCII imprimable ni les sauts de ligne (l'ASCII est vérifié à part)."""
    return sorted({c for t in textes for c in t})


def main(argv):
    chemin = Path(argv[1]) if len(argv) > 1 else POLICE_PAR_DEFAUT
    police = TTFont(chemin)
    cmap = police.getBestCmap()
    if not cmap:
        print(f"ÉCHEC : {chemin} n'a pas de table cmap Unicode lisible")
        return 1
    famille = police["name"].getDebugName(4)
    print(f"Police : {chemin} — {famille}, {len(cmap)} caractères dans la cmap")

    ascii_imprimable = [chr(c) for c in range(0x20, 0x7F)]
    absents_ascii = [c for c in ascii_imprimable if ord(c) not in cmap]
    print(f"ASCII imprimable (U+0020 à U+007E) : {'complet' if not absents_ascii else 'manque ' + ''.join(absents_ascii)}")

    langues, cles, _ = generer_i18n.charger()
    ecran = [c for c, s in cles if s.startswith("Écran")]
    autres = [c for c, s in cles if not s.startswith("Écran")]
    en = next(lg for lg in langues if lg["code"] == generer_i18n.REPLI)["entrees"]
    echec = bool(absents_ascii)

    print(f"\n1. Textes de l'écran ({len(ecran)} textes par langue, sections « Écran : … »)")
    for lg in langues:
        textes = ([generer_i18n.texte_francais(c) for c in ecran] if lg["code"] == "fr" else
                  [lg["entrees"].get(c) or en.get(c) or generer_i18n.texte_francais(c) for c in ecran])
        utilises = caracteres(textes)
        manquants = [c for c in utilises if ord(c) not in cmap]
        print(f"  {lg['code']} : {len(utilises)} caractères distincts, manquants : "
              f"{', '.join(nom(c) for c in manquants) if manquants else 'aucun'}")
        echec = echec or bool(manquants)

    print("\n2. Alphabets de chaque langue (lettres propres + signes typographiques)")
    for code, lettres in ALPHABETS.items():
        manquants = [c for c in dict.fromkeys(lettres) if ord(c) not in cmap]
        obligatoires = [c for c in manquants if c not in FACULTATIFS]
        facultatifs = [c for c in manquants if c in FACULTATIFS]
        dit = "aucun" if not manquants else ", ".join(nom(c) for c in manquants)
        suite = " (facultatifs : la langue s'en passe)" if facultatifs and not obligatoires else ""
        print(f"  {code} : {len(set(lettres))} caractères attendus, manquants : {dit}{suite}")
        echec = echec or bool(obligatoires)

    print("\n3. Pour information : textes de la page de réglages et de l'API (rendus par le navigateur, pas par la police)")
    for lg in langues:
        textes = ([generer_i18n.texte_francais(c) for c in autres] if lg["code"] == "fr" else
                  [lg["entrees"].get(c) or en.get(c) or generer_i18n.texte_francais(c) for c in autres])
        manquants = [c for c in caracteres(textes) if ord(c) not in cmap]
        print(f"  {lg['code']} : hors police : {', '.join(nom(c) for c in manquants) if manquants else 'aucun'}")

    print(f"\nRÉSULTAT : {'ÉCHEC — au moins un caractère manque' if echec else 'tous les caractères contrôlés sont dans la police'}")
    return 1 if echec else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
