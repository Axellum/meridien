"""Rendu PNG de l'interface du firmware, hors carte (voir docs/outils.md, section « Rendu de l'écran sur le PC »).

Compile sur le PC (clang de llvm-mingw, sans ESP-IDF) LVGL 9.5 avec la configuration du firmware, les MÊMES fichiers
d'interface que le firmware (main/ui_*.cpp, sans les modifier) et un banc minimal (affichage en mémoire RGB565, bouchons
ESP-IDF), exécute chaque scène et écrit un PNG par scène, variante et taille d'écran.

Ce n'est PAS une photo de l'écran : rendu LVGL logiciel, pas de dalle. Voir docs/outils.md pour ce qui n'est pas couvert.

Usage (à travers tools/rendre.ps1, qui prépare le PATH) :
    python tools/rendu/rendre.py                          # toutes les scènes, 720x1280 et 1280x720
    python tools/rendu/rendre.py --scene bonjour --taille 720x1280
    python tools/rendu/rendre.py --regenerer-config <sdkconfig>   # réécrit tools/rendu/sdkconfig_lvgl.h
"""

import argparse
import glob
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path
from typing import NoReturn

RACINE = Path(__file__).resolve().parents[2]
BANC = RACINE / "tools" / "rendu"
CONFIG_LVGL = BANC / "sdkconfig_lvgl.h"
CACHE = RACINE / ".cache" / "rendu"
SORTIE_DEFAUT = RACINE / "docs" / "captures" / "ecran"
LLVM_MINGW = Path(r"H:\AuxFilsDesIdees\_outils\llvm-mingw\bin")
POLICE = RACINE / "main" / "assets" / "roboto_700.ttf"

# Une scène = un fichier d'interface du firmware (main/ui_*.cpp) + son adaptateur du banc + ce qu'il faut pour le lier.
# Les variantes sont décrites dans l'adaptateur C++ (le banc les liste avec --lister) : une seule source de vérité.
SCENES = {
    "bonjour": [
        "tools/rendu/scene_bonjour.cpp",
        "main/ui_bonjour.cpp",
        "main/horloge.cpp",
    ],
}
# Scènes optionnelles : prises en compte seulement si le fichier d'interface existe dans le dépôt.
SCENES_SI_PRESENTES = {
    "principal": ["tools/rendu/scene_principal.cpp", "main/ui_principal.cpp"],
}
FICHIERS_COMMUNS = ["tools/rendu/banc.cpp"]

TAILLES_DEFAUT = ["720x1280", "1280x720"]
INSTANT_DEFAUT = "2026-11-06T10:40:00Z"  # 11 h 40 en France (heure d'hiver)
JOBS_MAX = 4  # le PC a planté à 6 compilations parallèles (AGENTS.md)


def erreur(message: str) -> NoReturn:
    print(f"ECHEC : {message}", file=sys.stderr)
    sys.exit(1)


def lancer(commande: list, cwd: Path | None = None, capturer: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(commande, cwd=cwd, capture_output=capturer, text=True, encoding="utf-8", errors="replace")


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


def outil(nom: str) -> str:
    chemin = shutil.which(nom) or str(LLVM_MINGW / f"{nom}.exe")
    if not Path(chemin).exists() and not shutil.which(nom):
        erreur(f"{nom} introuvable. Lancer `. H:\\AuxFilsDesIdees\\_outils\\env_hote.ps1` (voir docs/outils.md).")
    return chemin


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


def construire_scene(nom: str, sources: list, lvgl: Path, archive: Path, jobs: int) -> Path:
    dossier = CACHE / "scenes" / nom
    dossier.mkdir(parents=True, exist_ok=True)
    clangxx = outil("clang++")
    clang = outil("clang")

    # La police est dans le firmware par EMBED_FILES (symboles _binary_roboto_700_ttf_start/end) : même fichier, même
    # symboles, par un .s qui l'inclut.
    police_s = dossier / "police.s"
    police_s.write_text(
        '    .section .rdata,"dr"\n'
        "    .global _binary_roboto_700_ttf_start\n"
        "    .global _binary_roboto_700_ttf_end\n"
        "_binary_roboto_700_ttf_start:\n"
        f'    .incbin "{POLICE.as_posix()}"\n'
        "_binary_roboto_700_ttf_end:\n",
        encoding="utf-8",
    )
    police_o = dossier / "police.o"
    resultat = lancer([clang, "-c", str(police_s), "-o", str(police_o)])
    if resultat.returncode != 0:
        erreur("police : " + resultat.stderr)

    drapeaux = [
        "-std=gnu++23",
        "-O1",
        "-Wall",
        "-Wextra",
        f'-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="{CONFIG_LVGL.name}"',
        f"-I{BANC}",
        f"-I{BANC / 'bouchons'}",
        f"-I{RACINE / 'main'}",
        f"-I{RACINE / 'components' / 'board_api' / 'include'}",
        f"-I{RACINE / 'components' / 'core' / 'include'}",
        f"-I{lvgl}",
        f"-I{lvgl / 'src'}",
    ]
    objets, taches = [], []
    for relatif in [*FICHIERS_COMMUNS, *sources]:
        source = RACINE / relatif
        if not source.is_file():
            erreur(f"{relatif} introuvable (scène {nom})")
        objet = dossier / (relatif.replace("/", "__") + ".o")
        objets.append(objet)
        taches.append(([clangxx, "-c", *drapeaux, str(source), "-o", str(objet)], relatif))
    compiler_en_parallele(taches, jobs)

    exe = dossier / f"rendu_{nom}.exe"
    # -static : sans lui l'exécutable cherche libc++.dll dans le PATH et refuse de démarrer hors d'une session env_hote.ps1.
    resultat = lancer([clangxx, "-static", *map(str, objets), str(police_o), str(archive), "-o", str(exe)])
    if resultat.returncode != 0:
        print(resultat.stdout + resultat.stderr, file=sys.stderr)
        erreur(f"édition de liens de la scène {nom}")
    return exe


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


# --- orchestration ------------------------------------------------------------------------------------------------------------


def instant_en_epoch(texte: str) -> int:
    return int(datetime.fromisoformat(texte.replace("Z", "+00:00")).astimezone(timezone.utc).timestamp())


def main() -> None:
    parseur = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parseur.add_argument("--scene", action="append", help="scène à rendre (défaut : toutes) ; répétable")
    parseur.add_argument("--taille", action="append", help="LxH (défaut : 720x1280 et 1280x720) ; répétable")
    parseur.add_argument("--variante", action="append", help="variante à rendre (défaut : toutes) ; répétable")
    parseur.add_argument("--instant", default=INSTANT_DEFAUT, help=f"instant UTC fixé (défaut : {INSTANT_DEFAUT})")
    parseur.add_argument("--sortie", type=Path, default=SORTIE_DEFAUT, help="dossier des PNG")
    parseur.add_argument("--jobs", type=int, default=JOBS_MAX, help=f"compilations parallèles (au plus {JOBS_MAX})")
    parseur.add_argument("--nettoyer", action="store_true", help="oublier le cache de compilation (LVGL compris)")
    parseur.add_argument("--regenerer-config", type=Path, metavar="SDKCONFIG", help="réécrire sdkconfig_lvgl.h")
    args = parseur.parse_args()

    if args.regenerer_config:
        ecrire_config_lvgl(args.regenerer_config)
        return
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

    scenes = {**SCENES}
    for nom, sources in SCENES_SI_PRESENTES.items():
        if all((RACINE / s).is_file() for s in sources):
            scenes[nom] = sources
    demandees = args.scene or list(scenes)
    for nom in demandees:
        if nom not in scenes:
            erreur(f"scène inconnue : {nom} (connues ici : {', '.join(scenes)})")
    tailles = args.taille or TAILLES_DEFAUT
    epoch = instant_en_epoch(args.instant)
    version = version_lvgl(lvgl)

    ecrits = []
    for nom in demandees:
        exe = construire_scene(nom, scenes[nom], lvgl, archive, jobs)
        liste = lancer([str(exe), "--lister"])
        if liste.returncode != 0:
            erreur(f"{exe.name} --lister : {liste.stderr}")
        variantes = liste.stdout.split()
        for variante in args.variante or variantes:
            if variante not in variantes:
                erreur(f"variante inconnue pour {nom} : {variante} (connues : {', '.join(variantes)})")
            for taille in tailles:
                if not re.fullmatch(r"\d+x\d+", taille):
                    erreur(f"taille invalide : {taille} (attendu LxH, par exemple 720x1280)")
                ppm = CACHE / "scenes" / nom / f"{variante}-{taille}.ppm"
                resultat = lancer(
                    [str(exe), "--variante", variante, "--taille", taille, "--instant", str(epoch), "--sortie", str(ppm)]
                )
                if resultat.returncode != 0:
                    print(resultat.stdout + resultat.stderr, file=sys.stderr)
                    erreur(f"rendu de {nom}/{variante} {taille}")
                if resultat.stderr.strip():
                    print("  journal du firmware :", resultat.stderr.strip().replace("\n", "\n    "))
                lu_l, lu_h, pixels = lire_ppm(ppm)
                description = (
                    f"Rendu LVGL {version} logiciel sur PC (RGB565 puis RGB888), pas une photo de la carte. "
                    f"Scène {nom}/{variante}, {lu_l}x{lu_h}, instant fixé {args.instant}."
                )
                png = args.sortie / f"{nom}-{variante}-{taille}.png"
                ecrire_png(png, lu_l, lu_h, pixels, description)
                ecrits.append(png)
                print(f"-> {png.relative_to(RACINE) if png.is_relative_to(RACINE) else png}  ({lu_l}x{lu_h})")
    print(f"{len(ecrits)} PNG écrit(s) dans {args.sortie}")


if __name__ == "__main__":
    main()
