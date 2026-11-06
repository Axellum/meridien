"""Contrôles croisés de la traduction, lancés par lancer.py après les tests C++ (rien à compiler).

1. `generer_i18n.py --verifier` : les fichiers i18n/*.yaml sont cohérents (mêmes clés, mêmes marqueurs, indices figés) et
   les sorties versionnées (i18n_donnees.inc, web/app/lang/*.json) sont à jour.
2. Les textes français que l'écran « Bonjour » affiche (main/ui_bonjour.cpp, main/boot_info.cpp) sont exactement la section
   « Écran : « Bonjour » » de i18n/en.yaml, et la copie de cette liste dans test_i18n.cpp (BONJOUR[]) est la même. Si
   quelqu'un change un texte de ces deux fichiers sans toucher la table, le contrôle échoue ici (main/ n'est pas modifié).

Code de sortie non nul = un contrôle a échoué. Utilisable seul : python test/host/controle_i18n.py
"""

import importlib.util
import re
import subprocess
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
GENERATEUR = RACINE / "components" / "core" / "outils" / "generer_i18n.py"
UI_BONJOUR = RACINE / "main" / "ui_bonjour.cpp"
BOOT_INFO = RACINE / "main" / "boot_info.cpp"
TEST_I18N = RACINE / "test" / "host" / "test_i18n.cpp"

LITTERAL = r'"((?:[^"\\]|\\.)*)"'
CONSTANTES_UI = re.compile(r"\bk[A-Z]\w*\s*=\s*" + LITTERAL + r"\s*;")
RETOURS_BOOT = re.compile(r"\breturn\s+" + LITTERAL + r"\s*;")
LISTE_TEST = re.compile(r"BONJOUR\[\]\s*=\s*\{(.*?)\};", re.DOTALL)
LITTERAUX = re.compile(LITTERAL)


def _generateur():
    spec = importlib.util.spec_from_file_location("generer_i18n", GENERATEUR)
    if spec is None or spec.loader is None:
        raise ImportError(f"impossible de charger {GENERATEUR}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _sans_echappements(texte):
    return texte.replace('\\"', '"').replace("\\\\", "\\")


def textes_du_firmware():
    """Textes français de l'écran « Bonjour », lus dans le code C++ de main/ (ensemble)."""
    sources = [(UI_BONJOUR, CONSTANTES_UI), (BOOT_INFO, RETOURS_BOOT)]
    trouves = set()
    for chemin, motif in sources:
        texte = chemin.read_text(encoding="utf-8")
        trouves |= {_sans_echappements(m) for m in motif.findall(texte)}
    return trouves


def textes_du_test():
    """Copie de la liste dans test_i18n.cpp."""
    m = LISTE_TEST.search(TEST_I18N.read_text(encoding="utf-8"))
    return {_sans_echappements(t) for t in LITTERAUX.findall(m.group(1))} if m else set()


def controle_i18n():
    """Retourne le nombre de contrôles en échec (0 = tout passe) ; affiche ce qui a été vérifié."""
    echecs = 0
    print("\ncontrole_i18n.py :")

    r = subprocess.run([sys.executable, str(GENERATEUR), "--verifier"], capture_output=True, text=True, encoding="utf-8")
    if r.returncode == 0:
        print("  ok      " + r.stdout.strip())
    else:
        print("  ECHEC   generer_i18n.py --verifier :\n" + (r.stdout + r.stderr).strip())
        return 1

    if not UI_BONJOUR.exists() or not BOOT_INFO.exists():
        print("  saute   main/ui_bonjour.cpp ou main/boot_info.cpp absent : textes de l'écran « Bonjour » non comparés")
        return echecs

    _, _, cles, _ = _generateur().sorties_attendues()
    table = {c for c, section in cles if section.startswith("Écran : « Bonjour »")}
    firmware = textes_du_firmware()
    copie = textes_du_test()

    for libelle, a, b in (
        ("le firmware (main/) affiche un texte absent de la table", firmware, table),
        ("la table a un texte que main/ n'affiche plus", table, firmware),
        ("test_i18n.cpp BONJOUR[] n'est plus la liste de main/", copie, firmware),
        ("main/ affiche un texte absent de test_i18n.cpp BONJOUR[]", firmware, copie),
    ):
        ecart = sorted(a - b)
        if ecart:
            print(f"  ECHEC   {libelle} : {ecart[:3]}")
            echecs += 1
    if echecs == 0:
        print(f"  ok      {len(firmware)} textes de l'écran « Bonjour » : main/, table et test_i18n.cpp identiques")
    return echecs


if __name__ == "__main__":
    sys.exit(1 if controle_i18n() else 0)
