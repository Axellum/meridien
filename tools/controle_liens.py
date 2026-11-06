#!/usr/bin/env python3
"""Contrôle des liens relatifs et des ancres des fichiers Markdown du dépôt.

Pour chaque lien `[texte](cible)` ou image `![texte](cible)` qui n'est pas une adresse externe :
  - le fichier ou le dossier visé existe dans le dépôt (liste de git : suivi ou ajoutable, pas ignoré), avec la bonne
    casse (GitHub tourne sous Linux) et sans sortir du dépôt ;
  - l'ancre `#titre`, s'il y en a une, correspond à un titre du fichier visé (calcul de GitHub : minuscules,
    ponctuation retirée, espaces en tirets, suffixe -1, -2 pour un titre répété) ou à une balise `<a id="...">` ;
    une ancre de ligne `#L12` est acceptée pour tout fichier existant.
Les liens dans les blocs de code et dans le code en ligne sont ignorés. Les adresses http(s) ne sont PAS testées
(aucun accès au réseau). Code de sortie non nul = au moins un lien cassé.

    python tools/controle_liens.py                            # tous les .md du dépôt
    python tools/controle_liens.py README.md docs/depannage.md
    python tools/controle_liens.py --tolerer THIRD_PARTY.md   # cible annoncée, pas encore fusionnée
"""

import argparse
import io
import posixpath
import re
import subprocess
import sys
import unicodedata
from collections import Counter
from pathlib import Path
from urllib.parse import unquote

LIEN = re.compile(r"\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
DEFINITION = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*<?(\S+?)>?(?:\s|$)")
TITRE = re.compile(r"^ {0,3}(#{1,6})\s+(.*?)\s*#*\s*$")
CLOTURE = re.compile(r"^[\s>]*(```|~~~)")
CODE_EN_LIGNE = re.compile(r"`+[^`]*`+")
BALISE_ID = re.compile(r"<[^>]*\b(?:id|name)=[\"']([^\"']+)[\"']")
EXTERNE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
LIGNE = re.compile(r"^L\d+(-L\d+)?$")


def ancre_de_titre(titre: str) -> str:
    """Ancre que GitHub donne à un titre (sans le suffixe des titres répétés)."""
    texte = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", titre)
    texte = re.sub(r"<[^>]+>", "", texte)
    texte = unicodedata.normalize("NFC", texte).lower()
    return "".join(c for c in texte if c.isalnum() or c in " -_").replace(" ", "-")


def analyser(texte: str) -> tuple[set[str], list[tuple[int, str]]]:
    """Rend les ancres du fichier et ses liens (numéro de ligne, cible), hors blocs de code."""
    ancres: set[str] = set()
    vus: Counter[str] = Counter()
    liens: list[tuple[int, str]] = []
    cloture = None
    for numero, ligne in enumerate(texte.splitlines(), 1):
        borne = CLOTURE.match(ligne)
        if borne:
            if cloture is None:
                cloture = borne.group(1)
            elif borne.group(1) == cloture:
                cloture = None
            continue
        if cloture:
            continue
        titre = TITRE.match(ligne)
        if titre:
            base = ancre_de_titre(titre.group(2))
            ancres.add(base if vus[base] == 0 else f"{base}-{vus[base]}")
            vus[base] += 1
        sans_code = CODE_EN_LIGNE.sub("", ligne)
        ancres.update(BALISE_ID.findall(sans_code))
        cibles = LIEN.findall(sans_code)
        definition = DEFINITION.match(sans_code)
        if definition:
            cibles.append(definition.group(1))
        liens.extend((numero, cible) for cible in cibles)
    return ancres, liens


def lister(racine: Path) -> list[str]:
    sortie = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=racine, capture_output=True, check=True, encoding="utf-8",
    ).stdout
    return sorted(f for f in sortie.split("\0") if f and (racine / f).is_file())


def main() -> int:
    parser = argparse.ArgumentParser(description="Contrôle des liens relatifs et des ancres Markdown.")
    parser.add_argument("fichiers", nargs="*", help="fichiers .md à contrôler (défaut : tous ceux du dépôt)")
    parser.add_argument("--tolerer", action="append", default=[], metavar="CHEMIN",
                        help="cible attendue mais pas encore dans cette branche (chemin depuis la racine)")
    args = parser.parse_args()
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", line_buffering=True)

    racine = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, check=True,
                                 encoding="utf-8").stdout.strip())
    fichiers = lister(racine)
    existants = set(fichiers)
    dossiers = {p.as_posix() for f in fichiers for p in Path(f).parents if p.as_posix() != "."}
    minuscules = {f.lower() for f in existants | dossiers}
    cache: dict[str, tuple[set[str], list[tuple[int, str]]]] = {}

    def lire(chemin: str) -> tuple[set[str], list[tuple[int, str]]]:
        if chemin not in cache:
            cache[chemin] = analyser((racine / chemin).read_text(encoding="utf-8"))
        return cache[chemin]

    a_controler = [Path(f).as_posix() for f in args.fichiers] or [f for f in fichiers if f.endswith(".md")]
    nb_liens = nb_ancres = 0
    erreurs: list[str] = []
    toleres: list[str] = []
    for source in a_controler:
        for numero, cible in lire(source)[1]:
            if EXTERNE.match(cible):
                continue
            nb_liens += 1
            chemin, _, fragment = cible.partition("#")
            chemin = unquote(chemin.partition("?")[0])
            if not chemin:
                destination = source
            elif chemin.startswith("/"):
                destination = posixpath.normpath(chemin.lstrip("/"))
            else:
                destination = posixpath.normpath(posixpath.join(posixpath.dirname(source), chemin))
            probleme = None
            if destination == ".." or destination.startswith("../"):
                probleme = "sort du dépôt"
            elif destination not in existants and destination not in dossiers and destination != ".":
                if destination in args.tolerer:
                    toleres.append(f"{source}:{numero}: {cible} (annoncé, pas encore dans cette branche)")
                    continue
                probleme = "casse différente" if destination.lower() in minuscules else "introuvable"
            elif fragment:
                nb_ancres += 1
                fragment = unquote(fragment)
                if destination in dossiers or not destination.endswith(".md"):
                    if destination in dossiers or not LIGNE.match(fragment):
                        probleme = "ancre sur un fichier non Markdown ou un dossier"
                elif fragment not in lire(destination)[0]:
                    probleme = f"ancre « #{fragment} » absente de {destination}"
            if probleme:
                erreurs.append(f"{source}:{numero}: {cible} : {probleme}")

    for ligne in toleres:
        print("toléré  ", ligne)
    for ligne in erreurs:
        print("ERREUR  ", ligne)
    print(f"{len(a_controler)} fichier(s), {nb_liens} lien(s) relatif(s) dont {nb_ancres} avec ancre, "
          f"{len(erreurs)} erreur(s), {len(toleres)} toléré(s)")
    return 1 if erreurs else 0


if __name__ == "__main__":
    sys.exit(main())
