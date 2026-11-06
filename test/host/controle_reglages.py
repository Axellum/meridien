"""Contrôles croisés des réglages sur l'écran, lancés par lancer.py après les tests C++ (rien à compiler).

Le test C++ test_registre_reglages.cpp prouve que chaque réglage du registre a un chemin dans l'API (CONTRAT_API) et une rangée
de page. Ce qu'il ne peut pas lire, ce fichier le compare :

1. CONTRAT_API (copie écrite dans le test) = les champs que docs/api-reglages.md dit accepter, route par route, dans la moitié
   anglaise et dans la moitié française du document. Un champ ajouté au document sans réglage de l'écran fait échouer ici ; un
   champ du test que le document ne connaît pas aussi.
2. ACTIONS_ET_INFOS (copie dans le test) = les rangées « action » et « info » des pages (ecrans_reglages.hpp).
3. Dans le dessin (main/ui_reglages*.cpp) : chaque éditeur utilisé par une page a un `case Editeur::…`, et chaque action ou
   information y figure. Cela prouve qu'une rangée est dessinée, pas qu'elle se voit bien ni qu'elle répond au toucher : les
   parcours de tools/rendu/scenes/scene_parcours.cpp (rendu PC) rejouent les gestes, la carte reste à essayer. Si le fichier est
   absent, le contrôle le dit (« saute ») et ne prétend rien.
4. Chaque caractère des dispositions du clavier est dans la police embarquée (sans lui, une touche resterait blanche). Une présence
   dans la table cmap ne prouve pas le rendu de TinyTTF sur l'écran : cela reste à constater sur carte.

Code de sortie non nul = un contrôle a échoué. Utilisable seul : python test/host/controle_reglages.py
"""

import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
TEST_REGISTRE = RACINE / "test" / "host" / "test_registre_reglages.cpp"
DOC_API = RACINE / "docs" / "api-reglages.md"
ECRANS = RACINE / "components" / "core" / "include" / "ecran" / "ecrans_reglages.hpp"
CLAVIER = RACINE / "components" / "core" / "include" / "ecran" / "clavier.hpp"
POLICE = RACINE / "main" / "assets" / "roboto_700.ttf"

# Section du document pour chaque route.
SECTIONS = {"/api/wifi": 4, "/api/location": 5, "/api/display": 6, "/api/services/{id}": 7}


def contrat_du_test():
    """{route: {champ, …}} lu dans CONTRAT_API (test_registre_reglages.cpp)."""
    texte = TEST_REGISTRE.read_text(encoding="utf-8")
    bloc = re.search(r"CONTRAT_API\[\]\s*=\s*\{(.*?)\n\};", texte, re.DOTALL)
    if not bloc:
        return None
    contrat = {}
    for route, champ in re.findall(r'\{"(/api/[^"]+)",\s*"(\w+)",\s*Couverture::\w+\}', bloc.group(1)):
        contrat.setdefault(route, set()).add(champ)
    return contrat


def moities_du_document():
    """(anglais, français) : le document commence par « # English » et passe au français à « # Français »."""
    texte = DOC_API.read_text(encoding="utf-8")
    coupe = texte.find("\n# Français")
    return (texte[:coupe], texte[coupe:]) if coupe > 0 else (None, None)


def section(moitie, numero):
    m = re.search(rf"^## {numero}\. .*?(?=^## \d+\. |\Z)", moitie, re.DOTALL | re.MULTILINE)
    return m.group(0) if m else ""


def mots_entre_accents(texte):
    return set(re.findall(r"`(\w+)`", texte))


def champs_du_document(moitie, route):
    """Champs qu'un client peut envoyer à `route`, lus dans le texte du document ; None si le format n'est pas reconnu."""
    texte = section(moitie, SECTIONS[route])
    if route == "/api/wifi":
        m = re.search(r"PUT /api/wifi`\s*\|\s*(?:Body|Corps)\s*`(\{[^`]*\})`", texte)
        return set(re.findall(r'"(\w+)"\s*:', m.group(1))) if m else None
    if route == "/api/location":
        m = re.search(r"PUT /api/location` (?:receives|reçoit)[^(]*\(([^)]*)\)", texte)
        return mots_entre_accents(m.group(1)) if m else None
    if route == "/api/display":
        m = re.search(r"PUT /api/display` (?:accepts|accepte)[^(]*\(([^)]*)\)", texte)
        return mots_entre_accents(m.group(1)) if m else None
    if route == "/api/services/{id}":
        debut = re.search(r"^`PUT /api/services/\{id\}`", texte, re.MULTILINE)
        if not debut:
            return None
        champs, dans_le_tableau = set(), False
        for ligne in texte[debut.end():].splitlines():
            if ligne.startswith("|"):
                dans_le_tableau = True
                premiere_cellule = ligne.strip().strip("|").split("|")[0]
                champs |= mots_entre_accents(premiere_cellule)
            elif dans_le_tableau:
                break
        return champs or None
    return None


def controle_contrat():
    echecs = 0
    contrat = contrat_du_test()
    if not contrat:
        print("  ECHEC   CONTRAT_API introuvable dans test_registre_reglages.cpp (format changé ?)")
        return 1
    anglais, francais = moities_du_document()
    if anglais is None:
        print("  ECHEC   docs/api-reglages.md : séparation « # Français » introuvable")
        return 1
    for nom, moitie in (("anglais", anglais), ("français", francais)):
        for route in SECTIONS:
            attendu = contrat.get(route, set())
            lu = champs_du_document(moitie, route)
            if lu is None:
                print(f"  ECHEC   docs/api-reglages.md ({nom}) : champs de {route} non reconnus, adapter controle_reglages.py")
                echecs += 1
            elif lu != attendu:
                print(f"  ECHEC   {route} ({nom}) : le document accepte {sorted(lu - attendu)} que l'écran ne règle pas, "
                      f"le test connaît {sorted(attendu - lu)} que le document n'accepte pas")
                echecs += 1
    if echecs == 0:
        total = sum(len(c) for c in contrat.values())
        print(f"  ok      {total} champs de l'API sur {len(SECTIONS)} routes : le test et le document (anglais et français) sont d'accord")
    return echecs


def controle_actions_et_infos():
    texte = TEST_REGISTRE.read_text(encoding="utf-8")
    bloc = re.search(r"ACTIONS_ET_INFOS\[\]\s*=\s*\{(.*?)\};", texte, re.DOTALL)
    copie = set(re.findall(r'"([\w.]+)"', bloc.group(1))) if bloc else set()
    pages = set(re.findall(r'\{Genre::(?:Action|Info),\s*"([\w.]+)"', ECRANS.read_text(encoding="utf-8")))
    if not copie or copie != pages:
        print(f"  ECHEC   ACTIONS_ET_INFOS du test et rangées action/info des pages diffèrent : {sorted(copie ^ pages)}")
        return 1
    print(f"  ok      {len(pages)} actions et informations : la liste du test est celle des pages")
    return 0


def controle_dessin():
    sources = sorted((RACINE / "main").glob("ui_reglages*.cpp"))
    if not sources:
        print("  saute   main/ui_reglages*.cpp absent : éditeurs, actions et informations non comparés au dessin")
        return 0
    dessin = "\n".join(s.read_text(encoding="utf-8") for s in sources)
    pages = ECRANS.read_text(encoding="utf-8")
    editeurs = set(re.findall(r"Editeur::(\w+),\s*(?:true|false)\}", pages)) - {"Aucun"}
    ids = set(re.findall(r'\{Genre::(?:Action|Info),\s*"([\w.]+)"', pages))
    # « case Editeur::X » ou « case registre::Editeur::X » (le dessin est hors de l'espace de noms du registre).
    manque = sorted(e for e in editeurs if not re.search(rf"\bcase\s+(?:\w+::)*Editeur::{e}\b", dessin))
    # Les trois essais de service (stt, llm, tts) sont reconnus par leur préfixe commun : l'écran les dessine en boucle sur kServices.
    prefixes = {i: "action.tester_" for i in ids if i.startswith("action.tester_")}
    oublie = sorted(i for i in ids if f'"{i}"' not in dessin and f'"{prefixes.get(i, i)}"' not in dessin)
    if manque or oublie:
        print(f"  ECHEC   le dessin ne couvre pas : éditeurs {manque}, actions/informations {oublie}")
        return 1
    print(f"  ok      le dessin couvre {len(editeurs)} éditeurs et {len(ids)} actions ou informations")
    return 0


def controle_glyphes_du_clavier():
    try:
        from fontTools.ttLib import TTFont  # noqa: PLC0415  (outil de développement, pas toujours installé)
    except ImportError:
        print("  saute   fonttools absent (pip install fonttools) : caractères du clavier non comparés à la police")
        return 0
    if not POLICE.exists():
        print(f"  saute   {POLICE.name} absente : caractères du clavier non comparés à la police")
        return 0
    cmap = TTFont(POLICE).getBestCmap() or {}
    source = CLAVIER.read_text(encoding="utf-8")
    absents = set()
    nombre = 0
    for litteral in re.findall(r'"((?:[^"\\]|\\.)*)"', source):
        for touche in litteral.replace('\\"', '"').replace("\\\\", "\\").split(" "):
            if not touche or touche.startswith("#"):
                continue
            for c in touche:
                nombre += 1
                if ord(c) not in cmap:
                    absents.add(c)
    if absents:
        print("  ECHEC   caractères du clavier absents de la police : " + ", ".join(f"U+{ord(c):04X} {c}" for c in sorted(absents)))
        return 1
    print(f"  ok      les caractères du clavier ({nombre} occurrences) sont tous dans la police")
    return 0


def controle_reglages():
    """Retourne le nombre de contrôles en échec (0 = tout passe) ; affiche ce qui a été vérifié."""
    print("\ncontrole_reglages.py :")
    return controle_contrat() + controle_actions_et_infos() + controle_dessin() + controle_glyphes_du_clavier()


if __name__ == "__main__":
    sys.exit(1 if controle_reglages() else 0)
