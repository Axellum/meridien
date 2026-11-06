"""Contrôles sans navigateur des textes de la page et du faux serveur (appelés aussi par test/host/controle_i18n.py).

1. web/app/app.js n'écrit aucun texte d'écran : chaque identifiant `t('…')` est dans i18n/page.yaml, chaque identifiant de
   page.yaml est utilisé, et aucune chaîne du code ne ressemble à du français (lettres accentuées, mots courants).
2. web/faux-serveur/serveur.mjs : chaque texte français passé à `T('…')` est une clé de i18n/en.yaml (donc de toutes les
   langues : le générateur impose les mêmes clés), et aucune phrase traduisible n'est écrite en dur hors de `T()`.

Utilisable seul : python web/tests/controle_langues.py   (code de sortie non nul = au moins un écart).
"""

import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
APP_JS = RACINE / "web" / "app" / "app.js"
SERVEUR = RACINE / "web" / "faux-serveur" / "serveur.mjs"
PAGE_YAML = RACINE / "i18n" / "page.yaml"
EN_YAML = RACINE / "i18n" / "en.yaml"

LIGNE = re.compile(r'^"((?:[^"\\]|\\["\\])*)": "((?:[^"\\]|\\["\\])*)"$')
ID_PAGE = re.compile(r"^[a-z]+(\.[a-z0-9_]+)+$")
PREFIXE_ID = re.compile(r"^[a-z]+(\.[a-z0-9]+)*[._]$")  # « nav. », « theme. », « wifi.etat_ » : début d'un identifiant construit
APPEL_T = re.compile(r"\bT\(\s*(['\"`])((?:\\.|(?!\1).)*)\1")
# Noms de champs de l'API (clés de l'import), pas des textes : ils ressemblent à un identifiant de page.
CHAMPS_API = {"services.stt", "services.llm", "services.tts"}
# Mot d'essai envoyé tel quel au service de synthèse vocale pour le tester (API §7) : ce n'est pas un texte d'écran.
MOTS_D_ESSAI = {"Bonjour"}
ACCENTS = re.compile("[àâäçéèêëîïôöùûüÿœÀÂÇÉÈÊËÎÏÔÙÛÜŒ«»’]")
# Mots français courts et fréquents qui n'existent ni en anglais ni dans un identifiant : un seul suffit à signaler.
MOTS_FRANCAIS = re.compile(
    r"\b(le|la|les|des|du|une|votre|vos|vous|pas|est|sur|pour|avec|dans|ce|cette|ces|ou|et|mon|ma|mes|nous|aucun|aucune|"
    r"choisir|enregistrer|réessayer|erreur|écran|réglages?)\b",
    re.IGNORECASE,
)


def sans_echappements(texte):
    return re.sub(r"\\(.)", r"\1", texte)


def lire_yaml(chemin):
    sortie = {}
    for ligne in chemin.read_text(encoding="utf-8").splitlines():
        m = LIGNE.match(ligne)
        if m:
            sortie[sans_echappements(m.group(1))] = sans_echappements(m.group(2))
    return sortie


def litteraux_js(source):
    """Chaînes de caractères d'un source JavaScript, hors commentaires et hors expressions régulières.

    Rend [(texte, numéro de ligne, offset du guillemet ouvrant)]. Les expressions d'un gabarit `…${x}…` sont lues aussi
    (les chaînes qu'elles contiennent sont rendues) ; le gabarit lui-même est rendu sans ses parties dynamiques.
    Suffisant pour app.js et serveur.mjs ; ce n'est pas un analyseur complet de JavaScript."""
    sortie = []
    n = len(source)

    def balayer(i, dans_gabarit):
        """Lit à partir de i ; s'arrête à l'accolade qui clôt un `${…}` (si dans_gabarit) ou à la fin du source."""
        dernier = ""  # dernier caractère significatif : distingue une division d'un début d'expression régulière
        profondeur = 0
        while i < n:
            c = source[i]
            if c in " \t\r\n":
                i += 1
            elif source.startswith("//", i):
                while i < n and source[i] != "\n":
                    i += 1
            elif source.startswith("/*", i):
                fin = source.find("*/", i + 2)
                i = n if fin < 0 else fin + 2
            elif c in "'\"":
                debut, morceaux = i, []
                i += 1
                while i < n and source[i] != c:
                    pas = 2 if source[i] == "\\" else 1
                    morceaux.append(source[i : i + pas])
                    i += pas
                i += 1
                sortie.append(("".join(morceaux), debut))
                dernier = c
            elif c == "`":
                debut, morceaux = i, []
                i += 1
                while i < n and source[i] != "`":
                    if source.startswith("${", i):
                        i = balayer(i + 2, True)
                        morceaux.append("#")
                    else:
                        pas = 2 if source[i] == "\\" else 1
                        morceaux.append(source[i : i + pas])
                        i += pas
                i += 1
                sortie.append(("".join(morceaux), debut))
                dernier = "`"
            elif c == "/" and dernier in "(,=:[!&|?{};":
                i += 1  # expression régulière : on la saute (classes [...] comprises)
                en_classe = False
                while i < n and (source[i] != "/" or en_classe) and source[i] != "\n":
                    if source[i] == "\\":
                        i += 1
                    elif source[i] == "[":
                        en_classe = True
                    elif source[i] == "]":
                        en_classe = False
                    i += 1
                i += 1
                while i < n and source[i].isalpha():  # drapeaux
                    i += 1
                dernier = "/"
            else:
                if dans_gabarit and c == "}" and profondeur == 0:
                    return i + 1
                profondeur += (c == "{") - (c == "}")
                dernier = c
                i += 1
        return i

    balayer(0, False)
    return [(texte, source.count("\n", 0, debut) + 1, debut) for texte, debut in sortie]


def controler_page():
    ecarts = []
    ids = set(lire_yaml(PAGE_YAML))
    litteraux = litteraux_js(APP_JS.read_text(encoding="utf-8"))
    prefixes = {t for t, _, _ in litteraux if PREFIXE_ID.match(t)}
    cites = {t for t, _, _ in litteraux if ID_PAGE.match(t) and t not in prefixes and t not in CHAMPS_API}
    inconnus = sorted(c for c in cites if c not in ids)
    if inconnus:
        ecarts.append(f"app.js cite des identifiants absents de page.yaml : {inconnus[:5]}")
    inutiles = sorted(i for i in ids if i not in cites and not any(i.startswith(p) for p in prefixes))
    if inutiles:
        ecarts.append(f"page.yaml a des identifiants que app.js n'utilise pas : {inutiles[:5]}")
    for texte, ligne, _ in litteraux:
        t = sans_echappements(texte)
        if ID_PAGE.match(t) or t in prefixes:
            continue
        if ACCENTS.search(t) or MOTS_FRANCAIS.search(t):
            ecarts.append(f"app.js:{ligne} : chaîne qui ressemble à du français : {t[:50]!r}")
    return ecarts, len(ids)


def controler_serveur():
    ecarts = []
    cles = set(lire_yaml(EN_YAML))
    source = SERVEUR.read_text(encoding="utf-8")
    trouves = 0
    for m in APPEL_T.finditer(source):
        trouves += 1
        t = sans_echappements(m.group(2))
        if t not in cles:
            ecarts.append(f"serveur.mjs : T() d'un texte absent des fichiers de langue : {t[:60]!r}")
    # un texte qui est une clé de traduction doit être passé à T() : il est alors sur la même ligne, après « T( »
    for texte, ligne, debut in litteraux_js(source):
        t = sans_echappements(texte)
        if t not in cles or t in MOTS_D_ESSAI:
            continue
        avant = source[source.rfind("\n", 0, debut) + 1 : debut]
        if not re.search(r"\bT\([^;]*$", avant):
            ecarts.append(f"serveur.mjs:{ligne} : texte traduisible écrit hors de T() : {t[:50]!r}")
    return ecarts, trouves


def main():
    ecarts_page, nb_ids = controler_page()
    ecarts_serveur, nb_t = controler_serveur()
    for e in ecarts_page + ecarts_serveur:
        print("ECART", e)
    if ecarts_page or ecarts_serveur:
        return 1
    print(
        f"ok : app.js sans texte français ({nb_ids} identifiants, tous cités, aucun inconnu) ; "
        f"serveur.mjs : {nb_t} textes passés à T(), tous dans les fichiers de langue"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
