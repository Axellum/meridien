"""Contrôle de la bascule de thème à chaud de l'écran principal (ui_principal_changer_theme).

Une bascule (écran construit avec un thème, rendu, puis passé à un autre) doit donner la même image, pixel pour pixel,
que l'écran construit directement avec le thème d'arrivée. Une bascule refusée (vers « horloge », autre composition) doit
laisser l'écran du départ intact. Usage, après avoir rendu les variantes voulues (voir docs/outils.md) :

    .\tools\rendre.ps1 -Scene principal-themes -Taille 1280x720,720x1280 -Langue fr -Sortie <dossier> `
        -Variante bascule-ciel-clair,bascule-clair-ciel,bascule-ciel-tableau,bascule-tableau-ciel,bascule-refusee,ciel-soleil,clair-soleil,tableau-soleil
    python tools\rendu\verifier_bascule.py <dossier>

Rend 0 si toutes les paires sont identiques, 1 sinon. Les métadonnées des PNG (variante, légende) diffèrent : seuls les pixels comptent.
"""

import sys
from pathlib import Path

from PIL import Image, ImageChops

# (variante rendue après bascule, variante construite directement avec le thème d'arrivée)
PAIRES = [
    ("bascule-ciel-clair", "clair-soleil"),
    ("bascule-clair-ciel", "ciel-soleil"),
    ("bascule-ciel-tableau", "tableau-soleil"),
    ("bascule-tableau-ciel", "ciel-soleil"),
    ("bascule-refusee", "ciel-soleil"),
]
TAILLES = ("1280x720", "720x1280")


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    dossier = Path(sys.argv[1])
    echecs = 0
    for taille in TAILLES:
        for apres, direct in PAIRES:
            fichiers = [dossier / f"principal_themes-{nom}-{taille}.png" for nom in (apres, direct)]
            manquants = [f.name for f in fichiers if not f.is_file()]
            if manquants:
                print(f"{taille} {apres} : fichier absent {manquants}")
                echecs += 1
                continue
            images = [Image.open(f).convert("RGB") for f in fichiers]
            ecart = ImageChops.difference(images[0], images[1]).getbbox()
            etat = "identique" if ecart is None else f"DIFFERENT dans {ecart}"
            print(f"{taille} {apres:22s} = {direct:15s} : {etat}")
            echecs += ecart is not None
    print("bascule à chaud : " + ("conforme" if echecs == 0 else f"{echecs} écart(s)"))
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
