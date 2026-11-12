"""Mutations de tools/essai_7b.py : casse le script de onze façons (sur des copies jetables) et vérifie que `autotest` le voit.

Pourquoi : l'autotest rejoue sauvegarde, flash et RESTAURATION contre une carte simulée ; ce fichier prouve que ces tests ont des dents
(une vérification retirée, une restauration oubliée, une MAC non comparée... doivent faire échouer l'autotest).
Usage, depuis n'importe où : python test/host/mutations_essai_7b.py   (code de sortie 0 = toutes détectées)
"""

import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "tools" / "essai_7b.py"
MUTATIONS = [
    ("pas de restauration", "        if ecrit:\n            print(\"[essai] RESTAURATION\")", "        if False:\n            print(\"[essai] RESTAURATION\")"),
    ("restauration au mauvais decalage", '"keep", "0x0", str(flash)', '"keep", "0x10000", str(flash)'),
    # Texte suivi depuis 014f608 (sauvegarde à portée limitée : la ligne qui suit le contrôle a changé, pas le contrôle).
    ("restauration sans verify-flash", "    if not verifier_puce(port, flash):\n        return False\n    if relire and marque.get(\"portee\"):",
     "    if relire and marque.get(\"portee\"):"),
    ("lectures differentes acceptees", "    if not identiques:\n        raise Refus", "    if False:\n        raise Refus"),
    ("MAC non comparee", "    if lue != attendue:", "    if False:"),
    ("pas de verify-flash de la sauvegarde", "    if not verifier_puce(port, f1):\n        raise Refus", "    if False:\n        raise Refus"),
    ("heure limite ignoree", "        if dt.datetime.now() >= limite:\n            raise Refus(\"heure limite deja passee", "        if False:\n            raise Refus(\"heure limite deja passee"),
    ("flasher sans marque", "    marque = charger_marque(dossier)\n    plages = plages_ecrites(build)", "    marque = {\"table\": []}\n    plages = plages_ecrites(build)"),
    ("code 3 oublie", "                code = 3", "                code = 1"),
    ("decoupage sans reessai sans stub", "        return lire_morceau(port, adresse, longueur, compteur, sans_stub=True)", "        raise Refus('abandon')"),
    ("otadata: CRC non controle", "        valide = crc == crc_otadata(sequence)", "        valide = True"),
]

source = SOURCE.read_text(encoding="utf-8")
non_detectees = []
for nom, ancien, nouveau in MUTATIONS:
    if source.count(ancien) != 1:
        print(f"INAPPLICABLE ({source.count(ancien)} occurrences) : {nom}")
        non_detectees.append(nom)
        continue
    with tempfile.TemporaryDirectory() as tmp:
        copie = Path(tmp) / "essai_7b_mutant.py"
        copie.write_text(source.replace(ancien, nouveau), encoding="utf-8")
        r = subprocess.run([sys.executable, "-I", str(copie), "autotest"], capture_output=True, text=True, timeout=300)
    detectee = r.returncode != 0
    print(("detectee   : " if detectee else "NON DETECTEE : ") + nom)
    if not detectee:
        non_detectees.append(nom)
print(f"{len(MUTATIONS) - len(non_detectees)} detectee(s) sur {len(MUTATIONS)}")
sys.exit(1 if non_detectees else 0)
