#!/usr/bin/env python3
"""Rejoue d'un coup les contrôles « indépendants » rapides : ceux dont la valeur attendue vient de Python, pas du noyau.

    python test/host/controles_independants.py                 # sur le PC (un contrôle impossible ici est dit SAUTÉ)
    python test/host/controles_independants.py --exiger-tout   # la CI : un contrôle sauté est un échec

Chaque script a un mode `--verifier` qui recalcule ses fichiers générés (attendu_*.inc, captures relues, icônes, polices)
et sort en erreur si ceux du dépôt ne sont plus à jour : un attendu retouché à la main pour faire passer un test, ou une
capture changée sans régénérer son littéral, est alors vu. S'y ajoute le contrôle des liens Markdown (tools/controle_liens.py).
S'y ajoutent l'autotest de tools/essai_7b.py (carte simulée, aucun port) et ses mutations (mutations_essai_7b.py). Aucun ne
demande le réseau, un compilateur ni une carte ; chacun dure moins d'une minute.

Hors de cette liste, exprès : controle_json.py (g++ croisé d'ESP-IDF), les mutations (compilation, voir lancer.py
--mutations et mutations_horloge.py) et les capturer_* sans --verifier (ils vont sur le réseau).

Sauts possibles sur un PC (jamais en CI) : `zoneinfo` sans base des fuseaux (Python de Windows sans le paquet tzdata) pour
controle_python.py et controle_python_fuseaux.py ; Pillow absent pour generer_icones.py ; fontTools absent pour les polices
(le script le dit lui-même par une ligne « saute »). Code de sortie non nul = au moins un contrôle en échec (ou sauté avec
--exiger-tout).
"""

import argparse
import importlib.util
import io
import subprocess
import sys
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]

# (script depuis la racine, arguments, modules Python dont il a besoin, raison si l'un manque)
CONTROLES = [
    ("test/host/controle_python.py", ["--verifier"], [], "tzdata"),
    ("test/host/controle_python_fuseaux.py", ["--verifier"], [], "tzdata"),
    ("test/host/controle_python_fournisseurs.py", ["--verifier"], [], None),
    ("test/host/controle_python_gzip.py", ["--verifier"], [], None),
    ("test/host/controle_python_modeles.py", ["--verifier"], [], None),
    ("test/host/controle_python_riche.py", ["--verifier"], [], None),
    ("test/host/controle_python_surf.py", ["--verifier"], [], None),
    ("test/host/capturer_nvs.py", ["--verifier"], [], None),
    ("test/host/capturer_geocodage.py", ["--verifier"], [], None),
    ("tools/icones/generer_icones.py", ["--verifier"], ["PIL"], None),
    ("tools/icones/generer_police_texte.py", ["--verifier"], ["fontTools"], None),
    ("tools/icones/generer_police_roue.py", ["--verifier"], ["fontTools"], None),
    ("tools/controle_liens.py", [], [], None),
    # Le script d'essai physique de la 7B, rejoué contre une carte simulée (aucun port), et ses onze mutations.
    ("tools/essai_7b.py", ["autotest"], [], None),
    ("test/host/mutations_essai_7b.py", [], [], None),
]


def base_des_fuseaux() -> bool:
    """Vrai si zoneinfo connaît Europe/Paris (base du système sous Linux, paquet tzdata sous Windows)."""
    try:
        from zoneinfo import ZoneInfo  # noqa: PLC0415

        ZoneInfo("Europe/Paris")
        return True
    except Exception:  # noqa: BLE001  (ZoneInfoNotFoundError ou module absent : même conclusion)
        return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exiger-tout", action="store_true", help="un contrôle sauté (module absent) compte comme un échec")
    a = ap.parse_args()
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", line_buffering=True)

    echecs = sautes = 0
    for script, arguments, modules, besoin in CONTROLES:
        manque = [m for m in modules if importlib.util.find_spec(m) is None]
        if besoin == "tzdata" and not base_des_fuseaux():
            manque.append("tzdata (base des fuseaux de zoneinfo)")
        if manque:
            sautes += 1
            print(f"SAUTÉ   {script} : {', '.join(manque)} absent(s)")
            continue
        debut = time.perf_counter()
        r = subprocess.run([sys.executable, "-u", script, *arguments], cwd=RACINE, capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        duree = time.perf_counter() - debut
        sortie = (r.stdout + r.stderr).strip().splitlines()
        saute_dedans = [ligne.strip() for ligne in sortie if ligne.lstrip().startswith("saute ")]
        if r.returncode != 0:
            echecs += 1
            print(f"ÉCHEC   {script} ({duree:.1f} s, code {r.returncode})")
            for ligne in sortie[-15:]:
                print(f"          {ligne}")
        elif saute_dedans:
            sautes += 1
            print(f"SAUTÉ   {script} ({duree:.1f} s) : {saute_dedans[0]}")
        else:
            print(f"OK      {script} ({duree:.1f} s) : {sortie[-1] if sortie else '(aucune sortie)'}")

    print(f"\n{len(CONTROLES)} contrôle(s) indépendant(s) : {len(CONTROLES) - echecs - sautes} OK, {echecs} en échec, "
          f"{sautes} sauté(s)")
    if a.exiger_tout and sautes:
        print("--exiger-tout : un contrôle sauté est un échec (installer le module qui manque)")
        return 1
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
