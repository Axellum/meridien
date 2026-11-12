"""Endurance de l'écran principal sur le PC : la mémoire de LVGL ne grandit pas quand l'écran tourne des jours.

Compile tools/rendu/endurance_principal.cpp avec les mêmes fichiers du firmware que la scène « principal » du banc de rendu
(main/ui_principal.cpp et ses voisins, compilés tels quels) et la même bibliothèque LVGL (tools/rendu/rendre.py, cache .cache/rendu),
puis le lance sur la taille du Tab5 (1280x720) et celle de la 7B (1024x600) : la même journée simulée jouée trois fois, minute par
minute, une météo neuve tous les quarts d'heure. Réussit si la troisième tient exactement la mémoire de LVGL de la deuxième, minute
par minute (détail et limites : en tête de
endurance_principal.cpp, docs/depannage.md « La PSRAM libre baisse d'environ 200 o par minute »).

Le contrôle se vérifie lui-même : il est relancé avec --injecter-fuite (108 o par minute jamais rendus) et DOIT alors échouer ;
sinon il ne prouverait rien et le script échoue.

Usage (outils du PC dans le PATH : `. <dossier des outils>\\env_hote.ps1`, voir docs/outils.md ; une seule compilation à la fois) :
    python tools/rendu/endurance.py
"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import rendre  # noqa: E402  (le banc de rendu : LVGL, police, fichiers du firmware)

SOURCE = rendre.BANC / "endurance_principal.cpp"
TAILLES = ["1280x720", "1024x600"]


def construire() -> Path:
    lvgl = rendre.chercher_lvgl()
    archive = rendre.construire_lvgl(lvgl, rendre.JOBS_MAX)
    scenes = rendre.choisir_scenes(rendre.decouvrir_scenes(), ["principal"])
    rendre.construire_scenes(scenes, lvgl, archive, rendre.JOBS_MAX)  # objets de main/ui_principal.cpp & co, police.o
    scene = scenes[0]
    objet = rendre.CACHE / "obj" / "endurance_principal.o"
    drapeaux = [
        "-std=gnu++23",
        "-O1",
        "-Wall",
        "-Wextra",
        f'-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="{rendre.CONFIG_LVGL.name}"',
        f"-I{rendre.BANC}",
        f"-I{rendre.BANC / 'bouchons'}",
        f"-I{rendre.RACINE / 'main'}",
        f"-I{rendre.RACINE / 'components' / 'board_api' / 'include'}",
        f"-I{rendre.RACINE / 'components' / 'core' / 'include'}",
        f"-I{lvgl}",
        f"-I{lvgl / 'src'}",
    ]
    clangxx = rendre.outil("clang++")
    resultat = rendre.lancer([clangxx, "-c", *drapeaux, str(SOURCE), "-o", str(objet)])
    if resultat.returncode != 0:
        print(resultat.stdout + resultat.stderr, file=sys.stderr)
        rendre.erreur("compilation de endurance_principal.cpp")
    exe = rendre.CACHE / "scenes" / "endurance_principal.exe"
    # L'objet de l'endurance passe AVANT la bibliothèque : il définit lv_malloc_core & co, le membre de LVGL qui les définit
    # n'est donc pas lié (sinon : symboles en double à l'édition de liens).
    resultat = rendre.lancer(
        [
            clangxx,
            "-static",
            str(objet),
            # les fichiers communs du banc (le registre des polices partagées...), sauf banc.cpp et son main
            *(str(rendre.objet_de(f)) for f in rendre.FICHIERS_COMMUNS if not f.endswith("banc.cpp")),
            *(str(rendre.objet_de(s)) for s in scene.sources),
            str(rendre.CACHE / "scenes" / "police.o"),
            str(archive),
            "-o",
            str(exe),
        ]
    )
    if resultat.returncode != 0:
        print(resultat.stdout + resultat.stderr, file=sys.stderr)
        rendre.erreur("édition de liens de l'endurance")
    return exe


def main() -> int:
    exe = construire()
    echec = False
    for taille in TAILLES:
        t0 = time.monotonic()
        normal = rendre.lancer([str(exe), "--taille", taille])
        print(normal.stdout.rstrip() + f"  ({time.monotonic() - t0:.1f} s)")
        if normal.returncode != 0:
            print(normal.stderr, file=sys.stderr)
            echec = True
        temoin = rendre.lancer([str(exe), "--taille", taille, "--injecter-fuite"])
        if temoin.returncode == 1:
            print(f"  témoin {taille} : une fuite injectée de 108 o/min est bien détectée")
        else:
            print(
                f"ECHEC : le témoin {taille} (fuite injectée) n'a pas échoué (code {temoin.returncode}) : le contrôle ne prouve rien"
            )
            echec = True
    print("endurance : ECHEC" if echec else "endurance : OK")
    return 1 if echec else 0


if __name__ == "__main__":
    sys.exit(main())
