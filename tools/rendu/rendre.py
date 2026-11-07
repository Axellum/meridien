"""Rendu PNG de l'interface du firmware, hors carte (voir docs/outils.md, section « Rendu de l'écran sur le PC »).

Compile sur le PC (clang de llvm-mingw, sans ESP-IDF) LVGL 9.5 avec la configuration du firmware, les MÊMES fichiers
d'interface que le firmware (main/ui_*.cpp, sans les modifier) et un banc minimal (affichage en mémoire RGB565, bouchons
ESP-IDF), exécute chaque scène et écrit un PNG par scène, variante, langue et taille d'écran ; sur demande, une PLANCHE qui
réunit les écrans d'une séquence dans une seule image légendée.

Une scène = UN fichier tools/rendu/scenes/scene_<nom>.cpp, découvert tout seul : ses directives en commentaire disent quels
fichiers du firmware lier, quelles langues et quelles tailles. Ajouter une scène ne touche ni au banc ni à ce script.

Ce n'est PAS une photo de l'écran : rendu LVGL logiciel, pas de dalle. Voir docs/outils.md pour ce qui n'est pas couvert.

Usage (à travers tools/rendre.ps1, qui prépare le PATH) :
    python tools/rendu/rendre.py                                   # toutes les scènes, langues et tailles par défaut
    python tools/rendu/rendre.py --scene bonjour --taille 720x1280
    python tools/rendu/rendre.py --scene temoin --langue fr --langue de --langue tr --planche
    python tools/rendu/rendre.py --lister                          # scènes, variantes, langues, tailles, séquences
    python tools/rendu/rendre.py --regenerer-config <sdkconfig>    # réécrit tools/rendu/sdkconfig_lvgl.h
"""

import argparse
import glob
import hashlib
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import NoReturn

RACINE = Path(__file__).resolve().parents[2]
BANC = RACINE / "tools" / "rendu"
SCENES_DIR = BANC / "scenes"
CONFIG_LVGL = BANC / "sdkconfig_lvgl.h"
CACHE = RACINE / ".cache" / "rendu"
SORTIE_DEFAUT = RACINE / "docs" / "captures" / "ecran"
POLICE = RACINE / "main" / "assets" / "roboto_700.ttf"
ICONES = RACINE / "main" / "assets" / "icones_meteo.bin"
FICHIERS_COMMUNS = ["tools/rendu/banc.cpp"]

# Les sept langues de l'écran (ADR-0006) : code -> nom dans sa langue, dans l'ordre des indices du noyau pur.
LANGUES = {
    "fr": "Français",
    "en": "English",
    "de": "Deutsch",
    "nl": "Nederlands",
    "es": "Español",
    "it": "Italiano",
    "tr": "Türkçe",
}
TAILLES_DEFAUT = ["720x1280", "1280x720"]
INSTANT_DEFAUT = "2026-11-06T10:40:00Z"  # 11 h 40 en France (heure d'hiver)
JOBS_MAX = 4  # le PC a planté à 6 compilations parallèles (AGENTS.md)


def erreur(message: str) -> NoReturn:
    print(f"ECHEC : {message}", file=sys.stderr)
    sys.exit(1)


def lancer(commande: list, cwd: Path | None = None, capturer: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(commande, cwd=cwd, capture_output=capturer, text=True, encoding="utf-8", errors="replace")


# --- scènes : un fichier chacune, découvert par ses directives ---------------------------------------------------------------

DIRECTIVE = re.compile(r"^//\s*rendu-([a-z-]+):\s*(.*?)\s*$")


@dataclass
class Scene:
    nom: str
    fichier: Path
    sources: list  # fichiers du dépôt à compiler avec la scène (main/ui_*.cpp, main/horloge.cpp...)
    langues: list  # langues que la scène sait afficher
    langues_defaut: list  # celles rendues quand --langue n'est pas donné
    tailles: list  # tailles rendues par défaut
    sequence: str  # nom de la séquence (planche) ; défaut : le nom de la scène
    rang: int  # place de la scène dans sa séquence (les variantes d'un même fichier gardent leur ordre)
    variantes: list = field(default_factory=list)  # [(variante, légende)], lues dans l'exécutable de la scène

    def manquants(self) -> list:
        return [s for s in self.sources if not (RACINE / s).is_file()]


def lire_scene(fichier: Path) -> Scene:
    """Lit les directives `// rendu-<clé>: valeurs` du fichier (les 80 premières lignes)."""
    directives: dict = {}
    for ligne in fichier.read_text(encoding="utf-8").splitlines()[:80]:
        trouve = DIRECTIVE.match(ligne)
        if trouve:
            directives.setdefault(trouve.group(1), []).extend(trouve.group(2).split())
    inconnues = set(directives) - {"sources", "langues", "langues-defaut", "tailles", "sequence"}
    if inconnues:
        erreur(f"{fichier.name} : directive(s) inconnue(s) : rendu-{', rendu-'.join(sorted(inconnues))}")

    langues = directives.get("langues", ["fr"])
    langues = list(LANGUES) if langues == ["toutes"] else langues
    defaut = directives.get("langues-defaut", langues)
    tailles = directives.get("tailles", TAILLES_DEFAUT)
    for code in [*langues, *defaut]:
        if code not in LANGUES:
            erreur(f"{fichier.name} : langue inconnue « {code} » (connues : {', '.join(LANGUES)}, ou « toutes »)")
    for taille in tailles:
        if not re.fullmatch(r"\d+x\d+", taille):
            erreur(f"{fichier.name} : taille invalide « {taille} » (attendu LxH)")
    sequence = directives.get("sequence", [fichier.stem.removeprefix("scene_")])
    rang = int(sequence[1]) if len(sequence) > 1 else 0
    return Scene(
        nom=fichier.stem.removeprefix("scene_"),
        fichier=fichier,
        sources=directives.get("sources", []),
        langues=langues,
        langues_defaut=defaut,
        tailles=tailles,
        sequence=sequence[0],
        rang=rang,
    )


def decouvrir_scenes() -> dict:
    scenes = {}
    for fichier in sorted(SCENES_DIR.glob("scene_*.cpp")):
        scene = lire_scene(fichier)
        scenes[scene.nom] = scene
    return scenes


# --- configuration LVGL : copie de celle du firmware --------------------------------------------------------------------


def lire_config_lvgl(sdkconfig: Path) -> dict:
    """Les options CONFIG_LV_* activées (ou valorisées) d'un sdkconfig, dans l'ordre du fichier."""
    options = {}
    for ligne in sdkconfig.read_text(encoding="utf-8", errors="replace").splitlines():
        trouve = re.match(r"^(CONFIG_LV_[A-Z0-9_]+)=(.*)$", ligne)
        if trouve and trouve.group(2) != "n":
            options[trouve.group(1)] = "1" if trouve.group(2) == "y" else trouve.group(2)
    return options


def ecrire_config_lvgl(sdkconfig: Path) -> None:
    options = lire_config_lvgl(sdkconfig)
    lignes = [
        "// Configuration LVGL du firmware, recopiée pour le rendu sur le PC (tools/rendu/rendre.py --regenerer-config).",
        "// Source : les lignes CONFIG_LV_* d'un sdkconfig issu de sdkconfig.defaults + valeurs par défaut du Kconfig de LVGL 9.5.0.",
        "// Ne pas éditer à la main : régénérer après un changement de sdkconfig.defaults (le banc compare ce fichier au sdkconfig",
        "// du dépôt s'il existe et avertit d'un écart). Inclus par LVGL à la place de sdkconfig.h",
        "// (LV_CONF_KCONFIG_EXTERNAL_INCLUDE).",
        "#pragma once",
        "",
    ]
    lignes += [f"#define {nom} {valeur}" for nom, valeur in options.items()]
    CONFIG_LVGL.write_text("\n".join(lignes) + "\n", encoding="utf-8", newline="\n")
    print(f"{CONFIG_LVGL.relative_to(RACINE)} : {len(options)} options")


def comparer_config(sdkconfig: Path) -> None:
    attendu = lire_config_lvgl(sdkconfig)
    mirroir = {}
    for ligne in CONFIG_LVGL.read_text(encoding="utf-8").splitlines():
        trouve = re.match(r"^#define (CONFIG_LV_[A-Z0-9_]+) (.*)$", ligne)
        if trouve:
            mirroir[trouve.group(1)] = trouve.group(2)
    # Les options sans effet sur le rendu (démos, exemples) ne comptent pas.
    ignorees = ("CONFIG_LV_BUILD_", "CONFIG_LV_USE_DEMO")
    ecarts = [
        nom
        for nom in sorted(set(attendu) | set(mirroir))
        if attendu.get(nom) != mirroir.get(nom) and not nom.startswith(ignorees)
    ]
    if ecarts:
        print(f"AVERTISSEMENT : la config LVGL du banc diffère de {sdkconfig} : {', '.join(ecarts[:8])}")
        print("  -> python tools/rendu/rendre.py --regenerer-config", sdkconfig)
    else:
        print(f"config LVGL : identique à {sdkconfig.name} ({len(attendu)} options)")


# --- sources de LVGL ------------------------------------------------------------------------------------------------------


def chercher_lvgl() -> Path:
    candidats = []
    if os.environ.get("MERIDIEN_LVGL"):
        candidats.append(Path(os.environ["MERIDIEN_LVGL"]))
    candidats.append(RACINE / "managed_components" / "lvgl__lvgl")
    commun = lancer(["git", "rev-parse", "--git-common-dir"], cwd=RACINE)
    if commun.returncode == 0:
        principal = (RACINE / commun.stdout.strip()).resolve().parent
        candidats.append(principal / "managed_components" / "lvgl__lvgl")
        candidats += [Path(p) for p in sorted(glob.glob(str(principal / ".cache" / "composants" / "*" / "lvgl__lvgl_*")))]
    for dossier in candidats:
        if (dossier / "lv_version.h").is_file():
            return dossier
    erreur(
        "sources de LVGL introuvables. Les obtenir par `.\\tools\\idf.ps1 reconfigure` (sous le verrou `build`) ou donner le "
        "dossier dans la variable MERIDIEN_LVGL. Cherché : " + ", ".join(str(c) for c in candidats)
    )


def version_lvgl(dossier: Path) -> str:
    texte = (dossier / "lv_version.h").read_text(encoding="utf-8")
    parties = [re.search(rf"#define LVGL_VERSION_{n}\s+(\d+)", texte) for n in ("MAJOR", "MINOR", "PATCH")]
    if not all(parties):
        erreur(f"version illisible dans {dossier / 'lv_version.h'}")
    return ".".join(p.group(1) for p in parties if p)


def version_verrouillee() -> str | None:
    """La version de lvgl/lvgl dans dependencies.lock (celle que le firmware compile)."""
    lock = RACINE / "dependencies.lock"
    if not lock.is_file():
        return None
    trouve = re.search(r"^  lvgl/lvgl:\n(?:.*\n)*?    version: (\S+)", lock.read_text(encoding="utf-8"), re.M)
    return trouve.group(1) if trouve else None


# --- compilation ------------------------------------------------------------------------------------------------------


def dossier_outils() -> Path | None:
    """Le dossier des outils du PC : MERIDIEN_OUTILS (posée par tools/rendre.ps1), sinon un dossier `_outils` dans un parent du
    dépôt. Même règle que tools/trouver_outils.ps1, moins le `local.ps1` que seul PowerShell sait lire."""
    depuis_env = os.environ.get("MERIDIEN_OUTILS")
    candidats = [Path(depuis_env)] if depuis_env else []
    candidats += [parent / "_outils" for parent in RACINE.parents]
    return next((c for c in candidats if (c / "env_hote.ps1").is_file()), None)


def outil(nom: str) -> str:
    dans_le_path = shutil.which(nom)
    if dans_le_path:
        return dans_le_path
    dossier = dossier_outils()
    chemin = dossier / "llvm-mingw" / "bin" / f"{nom}.exe" if dossier else None
    if not chemin or not chemin.exists():
        erreur(f"{nom} introuvable. Lancer `.\\tools\\rendre.ps1` (il trouve le dossier des outils), voir docs/outils.md.")
    return str(chemin)


def empreinte(*morceaux: str) -> str:
    h = hashlib.sha256()
    for m in morceaux:
        h.update(m.encode("utf-8"))
        h.update(b"\0")
    return h.hexdigest()[:16]


def compiler_en_parallele(taches: list, jobs: int) -> None:
    """taches : liste de (commande, description). S'arrête à la première erreur et montre sa sortie."""
    if not taches:
        return

    def une(tache):
        commande, description = tache
        resultat = lancer(commande)
        return description, resultat

    echecs = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for fait, (description, resultat) in enumerate(pool.map(une, taches), 1):
            if resultat.returncode != 0:
                echecs.append((description, resultat))
            if fait % 100 == 0:
                print(f"  {fait}/{len(taches)}")
    if echecs:
        for description, resultat in echecs[:3]:
            print(f"--- {description}\n{resultat.stdout}{resultat.stderr}", file=sys.stderr)
        erreur(f"{len(echecs)} fichier(s) n'ont pas compilé")


def construire_lvgl(lvgl: Path, jobs: int) -> Path:
    """Bibliothèque statique LVGL, recompilée seulement si la config, la version ou le compilateur changent."""
    dossier = CACHE / "lvgl"
    dossier.mkdir(parents=True, exist_ok=True)
    clang = outil("clang")
    version_clang = lancer([clang, "--version"]).stdout.splitlines()[0]
    sources = sorted((lvgl / "src").rglob("*.c"))
    cle = empreinte(str(lvgl), version_lvgl(lvgl), version_clang, CONFIG_LVGL.read_text(encoding="utf-8"), "O1")
    jeton = dossier / "cle.txt"
    archive = dossier / "liblvgl.a"
    if archive.is_file() and jeton.is_file() and jeton.read_text() == cle:
        print(f"LVGL {version_lvgl(lvgl)} : bibliothèque en cache ({archive.relative_to(RACINE)})")
        return archive

    print(f"LVGL {version_lvgl(lvgl)} : compilation de {len(sources)} fichiers (jobs={jobs}), une seule fois...")
    objets_dir = dossier / "obj"
    if objets_dir.exists():
        shutil.rmtree(objets_dir)
    objets_dir.mkdir(parents=True)
    drapeaux = [
        "-O1",
        "-w",  # code tiers : les avertissements de LVGL ne sont pas à nous
        f'-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="{CONFIG_LVGL.name}"',
        f"-I{BANC}",
        f"-I{lvgl}",
        f"-I{lvgl / 'src'}",
    ]
    taches, objets = [], []
    for source in sources:
        relatif = source.relative_to(lvgl / "src")
        objet = objets_dir / (str(relatif).replace("\\", "__").replace("/", "__") + ".o")
        objets.append(objet)
        taches.append(([clang, "-c", *drapeaux, str(source), "-o", str(objet)], str(relatif)))
    compiler_en_parallele(taches, jobs)
    if archive.exists():
        archive.unlink()
    # 463 chemins dépassent la longueur maximale d'une commande Windows : llvm-ar lit la liste dans un fichier (@).
    liste = dossier / "objets.rsp"
    liste.write_text("\n".join(o.as_posix() for o in objets) + "\n", encoding="utf-8")
    resultat = lancer([outil("llvm-ar"), "rcs", str(archive), f"@{liste}"])
    if resultat.returncode != 0:
        erreur("llvm-ar : " + resultat.stderr)
    jeton.write_text(cle)
    return archive


def objet_de(relatif: str) -> Path:
    return CACHE / "obj" / (relatif.replace("/", "__") + ".o")


def construire_scenes(scenes: list, lvgl: Path, archive: Path, jobs: int) -> dict:
    """Compile (une fois chacun) les fichiers communs, ceux des scènes et ceux du firmware qu'elles lient ; un exécutable
    par scène. Rend {nom de scène: exécutable}."""
    dossier = CACHE / "scenes"
    dossier.mkdir(parents=True, exist_ok=True)
    (CACHE / "obj").mkdir(parents=True, exist_ok=True)
    clangxx = outil("clang++")
    clang = outil("clang")

    # La police est dans le firmware par EMBED_FILES (symboles _binary_roboto_700_ttf_start/end) : même fichier, mêmes
    # symboles, par un .s qui l'inclut.
    # Le binaire des icônes météo (main/assets/icones_meteo.bin, EMBED_FILES aussi) est posé de la même façon, dans le même .o :
    # sans lui, les scènes qui lient main/ui_meteo_icones.cpp n'ont pas de pixels à dessiner.
    police_s = dossier / "police.s"
    police_s.write_text(
        '    .section .rdata,"dr"\n'
        "    .global _binary_roboto_700_ttf_start\n"
        "    .global _binary_roboto_700_ttf_end\n"
        "_binary_roboto_700_ttf_start:\n"
        f'    .incbin "{POLICE.as_posix()}"\n'
        "_binary_roboto_700_ttf_end:\n"
        "    .global _binary_icones_meteo_bin_start\n"
        "    .global _binary_icones_meteo_bin_end\n"
        "_binary_icones_meteo_bin_start:\n"
        f'    .incbin "{ICONES.as_posix()}"\n'
        "_binary_icones_meteo_bin_end:\n",
        encoding="utf-8",
    )
    police_o = dossier / "police.o"
    resultat = lancer([clang, "-c", str(police_s), "-o", str(police_o)])
    if resultat.returncode != 0:
        erreur("police : " + resultat.stderr)

    # Contrat météo (main/meteo_etat.h, session meteo-appareil) : tant que le vrai fichier n'est pas dans l'arbre, main/bouchons/
    # (s'il existe) donne la même forme, comme le fait main/CMakeLists.txt. Dès que le vrai fichier est là, le bouchon est ignoré.
    bouchons_firmware = RACINE / "main" / "bouchons"
    repli_meteo = (
        [f"-I{bouchons_firmware}"]
        if (bouchons_firmware / "meteo_etat.h").exists() and not (RACINE / "main" / "meteo_etat.h").exists()
        else []
    )
    drapeaux = [
        "-std=gnu++23",
        "-O1",
        "-Wall",
        "-Wextra",
        f'-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="{CONFIG_LVGL.name}"',
        f"-I{BANC}",
        f"-I{BANC / 'bouchons'}",
        f"-I{RACINE / 'main'}",
        *repli_meteo,
        f"-I{RACINE / 'components' / 'board_api' / 'include'}",
        f"-I{RACINE / 'components' / 'core' / 'include'}",
        f"-I{lvgl}",
        f"-I{lvgl / 'src'}",
    ]
    a_compiler = list(dict.fromkeys([*FICHIERS_COMMUNS, *(str(s.fichier.relative_to(RACINE).as_posix()) for s in scenes)]))
    for scene in scenes:
        a_compiler += [s for s in scene.sources if s not in a_compiler]
    taches = [([clangxx, "-c", *drapeaux, str(RACINE / r), "-o", str(objet_de(r))], r) for r in a_compiler]
    compiler_en_parallele(taches, jobs)

    executables = {}
    for scene in scenes:
        fichiers = [*FICHIERS_COMMUNS, scene.fichier.relative_to(RACINE).as_posix(), *scene.sources]
        exe = dossier / f"rendu_{scene.nom}.exe"
        # -static : sans lui l'exécutable cherche libc++.dll dans le PATH et refuse de démarrer hors d'une session
        # env_hote.ps1.
        resultat = lancer(
            [clangxx, "-static", *(str(objet_de(f)) for f in fichiers), str(police_o), str(archive), "-o", str(exe)]
        )
        if resultat.returncode != 0:
            print(resultat.stdout + resultat.stderr, file=sys.stderr)
            erreur(f"édition de liens de la scène {scene.nom}")
        executables[scene.nom] = exe
        liste = lancer([str(exe), "--lister"])
        if liste.returncode != 0:
            erreur(f"{exe.name} --lister : code {liste.returncode} {liste.stderr}")
        scene.variantes = [tuple(ligne.split("\t", 1)) for ligne in liste.stdout.splitlines() if ligne.strip()]
    return executables


# --- PNG ----------------------------------------------------------------------------------------------------------------------


def lire_ppm(chemin: Path) -> tuple:
    brut = chemin.read_bytes()
    entete = re.match(rb"P6\n(\d+) (\d+)\n255\n", brut)
    if not entete:
        erreur(f"{chemin} n'est pas un PPM P6 attendu")
    largeur, hauteur = int(entete.group(1)), int(entete.group(2))
    pixels = brut[entete.end() :]
    if len(pixels) != largeur * hauteur * 3:
        erreur(f"{chemin} : {len(pixels)} octets de pixels au lieu de {largeur * hauteur * 3}")
    return largeur, hauteur, pixels


def morceau(type_: bytes, donnees: bytes) -> bytes:
    contenu = type_ + donnees
    return struct.pack(">I", len(donnees)) + contenu + struct.pack(">I", zlib.crc32(contenu) & 0xFFFFFFFF)


def ecrire_png(chemin: Path, largeur: int, hauteur: int, pixels: bytes, description: str) -> None:
    """PNG RGB 8 bits, déterministe (même image = mêmes octets : pas de date), description en iTXt (UTF-8)."""
    lignes = b"".join(b"\0" + pixels[y * largeur * 3 : (y + 1) * largeur * 3] for y in range(hauteur))
    itxt = b"Description\0\0\0\0\0" + description.encode("utf-8")
    png = (
        b"\x89PNG\r\n\x1a\n"
        + morceau(b"IHDR", struct.pack(">IIBBBBB", largeur, hauteur, 8, 2, 0, 0, 0))
        + morceau(b"iTXt", itxt)
        + morceau(b"IDAT", zlib.compress(lignes, 9))
        + morceau(b"IEND", b"")
    )
    chemin.parent.mkdir(parents=True, exist_ok=True)
    chemin.write_bytes(png)


# --- planche : les écrans d'une séquence en une seule image légendée ------------------------------------------------------------


def lignes_de_texte(dessin, texte: str, police_pil, largeur_max: int, maximum: int = 2) -> list:
    """Coupe `texte` en au plus `maximum` lignes qui tiennent dans `largeur_max` (la dernière finit par « … » si besoin)."""
    mots, lignes, courante = texte.split(), [], ""
    for mot in mots:
        essai = f"{courante} {mot}".strip()
        if dessin.textlength(essai, font=police_pil) <= largeur_max or not courante:
            courante = essai
        else:
            lignes.append(courante)
            courante = mot
    lignes.append(courante)
    if len(lignes) > maximum:
        lignes = [*lignes[: maximum - 1], " ".join(lignes[maximum - 1 :])]
    while dessin.textlength(lignes[-1], font=police_pil) > largeur_max and len(lignes[-1]) > 1:
        lignes[-1] = lignes[-1][:-2].rstrip() + "…"
    return lignes


def assembler_planche(sequence: str, langue: str, taille: str, tuiles: list, sortie: Path, colonnes: int = 0) -> None:
    """tuiles : [(légende, chemin du PNG de l'écran)] dans l'ordre de la séquence ; la légende est écrite telle quelle sous
    l'écran (« Étape 3 : Lieu »). `colonnes` 0 = au choix : 3 colonnes de 560 px en paysage (4 de 440 px à partir de sept
    écrans), 4 de 330 px en portrait. Pillow n'est requis que pour cela."""
    try:
        from PIL import Image, ImageDraw, ImageFont, PngImagePlugin
    except ImportError:
        erreur("Pillow est nécessaire aux planches : python -m pip install --user pillow")
    largeur, hauteur = (int(v) for v in taille.split("x"))
    paysage = largeur >= hauteur
    nombre = len(tuiles)
    if paysage:
        colonnes = colonnes or (4 if nombre >= 7 else 3)
        cellule_l = 440 if colonnes >= 4 else 560
    else:
        colonnes = colonnes or 4
        cellule_l = 330
    cellule_h = round(hauteur * cellule_l / largeur)
    colonnes = max(1, min(nombre, colonnes))
    rangees = math.ceil(nombre / colonnes)
    marge, ecart, haut_titre, haut_legende = 28, 28, 104, 66
    # Au moins 1100 px de large pour que le titre et le sous-titre tiennent ; la grille est centrée.
    largeur_planche = max(1100, 2 * marge + colonnes * cellule_l + (colonnes - 1) * ecart)
    decalage = (largeur_planche - (colonnes * cellule_l + (colonnes - 1) * ecart)) // 2
    hauteur_planche = haut_titre + rangees * (cellule_h + haut_legende) + (rangees - 1) * ecart + marge

    fond, cadre, texte, discret, accent = (22, 26, 31), (74, 82, 92), (236, 240, 243), (154, 167, 180), (61, 220, 151)
    planche = Image.new("RGB", (largeur_planche, hauteur_planche), fond)
    dessin = ImageDraw.Draw(planche)
    f_titre = ImageFont.truetype(str(POLICE), 34)
    f_sous = ImageFont.truetype(str(POLICE), 20)
    f_legende = ImageFont.truetype(str(POLICE), 22)
    titre = f"{sequence} · {LANGUES[langue]} · {largeur}×{hauteur}"
    dessin.text((marge, 22), titre, font=f_titre, fill=accent)
    sous = f"{len(tuiles)} écran(s) · rendu LVGL logiciel sur PC, pas une photo de la carte · données d'exemple"
    dessin.text((marge, 68), sous, font=f_sous, fill=discret)

    for i, (legende, chemin) in enumerate(tuiles):
        colonne, rangee = i % colonnes, i // colonnes
        x = decalage + colonne * (cellule_l + ecart)
        y = haut_titre + rangee * (cellule_h + haut_legende + ecart)
        with Image.open(chemin) as image:
            vignette = image.convert("RGB").resize((cellule_l, cellule_h), Image.Resampling.LANCZOS)
        planche.paste(vignette, (x, y))
        dessin.rectangle((x - 2, y - 2, x + cellule_l + 1, y + cellule_h + 1), outline=cadre, width=2)
        for n, ligne in enumerate(lignes_de_texte(dessin, legende, f_legende, cellule_l)):
            dessin.text((x, y + cellule_h + 10 + n * 28), ligne, font=f_legende, fill=texte)

    infos = PngImagePlugin.PngInfo()
    infos.add_itxt("Description", f"Planche {titre} : rendu LVGL logiciel sur PC, pas une photo de la carte.")
    sortie.parent.mkdir(parents=True, exist_ok=True)
    planche.save(sortie, pnginfo=infos, compress_level=9)


# --- orchestration ------------------------------------------------------------------------------------------------------------


def instant_en_epoch(texte: str) -> int:
    return int(datetime.fromisoformat(texte.replace("Z", "+00:00")).astimezone(timezone.utc).timestamp())


def chemin_relatif(chemin: Path) -> Path:
    return chemin.relative_to(RACINE) if chemin.is_relative_to(RACINE) else chemin


def choisir_scenes(toutes: dict, demandees: list | None) -> list:
    """Les scènes à rendre : celles demandées (par nom de scène ou de séquence) ou toutes celles dont les fichiers du
    firmware existent. Une scène dont un fichier du firmware n'existe pas encore est ignorée avec un avis (ui_principal)."""
    if demandees:
        choisies = []
        for jeton in demandees:
            trouvees = [s for s in toutes.values() if jeton in (s.nom, s.sequence)]
            if not trouvees:
                erreur(f"scène ou séquence inconnue : {jeton} (connues : {', '.join(sorted(toutes))})")
            for scene in trouvees:
                if scene.manquants():
                    erreur(f"scène {scene.nom} : fichier(s) du firmware absent(s) : {', '.join(scene.manquants())}")
                if scene not in choisies:
                    choisies.append(scene)
        return choisies
    choisies = []
    for scene in toutes.values():
        if scene.manquants():
            print(f"scène {scene.nom} ignorée : {', '.join(scene.manquants())} n'existe pas (encore) dans ce dépôt")
        else:
            choisies.append(scene)
    return choisies


def main() -> None:
    parseur = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parseur.add_argument("--scene", action="append", help="scène ou séquence à rendre (défaut : toutes) ; répétable")
    parseur.add_argument("--taille", action="append", help="LxH, par exemple 1280x720 (défaut : celles de la scène) ; répétable")
    parseur.add_argument("--variante", action="append", help="variante à rendre (défaut : toutes) ; répétable")
    parseur.add_argument("--langue", action="append", help=f"code de langue ({' '.join(LANGUES)}) ; répétable")
    parseur.add_argument("--planche", action="store_true", help="assembler une planche par séquence, langue et taille")
    parseur.add_argument("--colonnes", type=int, default=0, help="colonnes de la planche (défaut : 3, ou 4 à partir de 7 écrans)")
    parseur.add_argument("--lister", action="store_true", help="lister scènes, variantes, langues, tailles et séquences")
    parseur.add_argument("--instant", default=INSTANT_DEFAUT, help=f"instant UTC fixé (défaut : {INSTANT_DEFAUT})")
    parseur.add_argument("--sortie", type=Path, default=SORTIE_DEFAUT, help="dossier des PNG")
    parseur.add_argument("--jobs", type=int, default=JOBS_MAX, help=f"compilations parallèles (au plus {JOBS_MAX})")
    parseur.add_argument("--nettoyer", action="store_true", help="oublier le cache de compilation (LVGL compris)")
    parseur.add_argument("--regenerer-config", type=Path, metavar="SDKCONFIG", help="réécrire sdkconfig_lvgl.h")
    args = parseur.parse_args()

    if args.regenerer_config:
        ecrire_config_lvgl(args.regenerer_config)
        return
    for code in args.langue or []:
        if code not in LANGUES:
            erreur(f"langue inconnue : {code} (connues : {', '.join(LANGUES)})")
    for taille in args.taille or []:
        if not re.fullmatch(r"\d+x\d+", taille):
            erreur(f"taille invalide : {taille} (attendu LxH, par exemple 1280x720)")
    if args.nettoyer and CACHE.exists():
        shutil.rmtree(CACHE)
    jobs = max(1, min(args.jobs, JOBS_MAX))

    sdkconfig = Path(os.environ.get("MERIDIEN_SDKCONFIG", RACINE / "sdkconfig"))
    if sdkconfig.is_file():
        comparer_config(sdkconfig)
    else:
        print("config LVGL : non comparée au sdkconfig du firmware (aucun sdkconfig ici ; copie du 06/11/2026 utilisée)")

    lvgl = chercher_lvgl()
    print(f"sources de LVGL : {lvgl}")
    verrouillee = version_verrouillee()
    if verrouillee and verrouillee != version_lvgl(lvgl):
        erreur(f"LVGL {version_lvgl(lvgl)} dans {lvgl} mais dependencies.lock verrouille {verrouillee}")
    archive = construire_lvgl(lvgl, jobs)

    scenes = choisir_scenes(decouvrir_scenes(), args.scene)
    if not scenes:
        erreur("aucune scène à rendre")
    executables = construire_scenes(scenes, lvgl, archive, jobs)

    if args.lister:
        for scene in scenes:
            print(f"{scene.nom}  (séquence {scene.sequence}, rang {scene.rang})")
            print(f"    langues : {' '.join(scene.langues)}  (par défaut : {' '.join(scene.langues_defaut)})")
            print(f"    tailles : {' '.join(scene.tailles)}")
            for variante, legende in scene.variantes:
                print(f"    - {variante} : {legende}")
        return

    epoch = instant_en_epoch(args.instant)
    version = version_lvgl(lvgl)
    ecrits, groupes = [], {}
    for scene in scenes:
        variantes = [v for v in scene.variantes if not args.variante or v[0] in args.variante]
        if args.variante and not variantes:
            print(f"scène {scene.nom} : aucune variante parmi {', '.join(args.variante)}")
        if args.langue:
            langues = [c for c in args.langue if c in scene.langues]
            for code in args.langue:
                if code not in scene.langues:
                    print(f"scène {scene.nom} : langue {code} non prise en charge (prises en charge : {' '.join(scene.langues)})")
        else:
            langues = scene.langues_defaut
        tailles = args.taille or scene.tailles
        for ordre, (variante, legende) in enumerate(variantes):
            for langue in langues:
                for taille in tailles:
                    suffixe = "" if langue == "fr" else f"-{langue}"
                    ppm = CACHE / "scenes" / f"{scene.nom}-{variante}-{langue}-{taille}.ppm"
                    resultat = lancer(
                        [str(executables[scene.nom]), "--variante", variante, "--taille", taille, "--langue", langue]
                        + ["--instant", str(epoch), "--sortie", str(ppm)]
                    )
                    if resultat.returncode != 0:
                        print(resultat.stdout + resultat.stderr, file=sys.stderr)
                        erreur(f"rendu de {scene.nom}/{variante} {langue} {taille}")
                    if resultat.stderr.strip():
                        print("  journal du firmware :", resultat.stderr.strip().replace("\n", "\n    "))
                    lu_l, lu_h, pixels = lire_ppm(ppm)
                    description = (
                        f"Rendu LVGL {version} logiciel sur PC (RGB565 puis RGB888), pas une photo de la carte. "
                        f"Scène {scene.nom}/{variante}, langue {langue}, {lu_l}x{lu_h}, instant fixé {args.instant}."
                    )
                    png = args.sortie / f"{scene.nom}-{variante}{suffixe}-{taille}.png"
                    ecrire_png(png, lu_l, lu_h, pixels, description)
                    ecrits.append(png)
                    print(f"-> {chemin_relatif(png)}  ({lu_l}x{lu_h})")
                    groupes.setdefault((scene.sequence, langue, taille), []).append(
                        (scene.rang, scene.nom, ordre, legende, png)
                    )

    if args.planche:
        for (sequence, langue, taille), tuiles in sorted(groupes.items()):
            tuiles.sort(key=lambda t: t[:3])
            sortie = args.sortie / f"planche-{sequence}{'' if langue == 'fr' else '-' + langue}-{taille}.png"
            assembler_planche(sequence, langue, taille, [(t[3], t[4]) for t in tuiles], sortie, args.colonnes)
            ecrits.append(sortie)
            print(f"-> {chemin_relatif(sortie)}  (planche de {len(tuiles)} écran(s))")
    if not ecrits:
        erreur("aucun PNG écrit : voir les avis ci-dessus (variante, langue ou scène sans correspondance)")
    print(f"{len(ecrits)} PNG écrit(s) dans {args.sortie}")


if __name__ == "__main__":
    main()
