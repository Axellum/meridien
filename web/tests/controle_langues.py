"""Contrôles sans navigateur des textes de la page et du faux serveur (appelés aussi par test/host/controle_i18n.py).

1. web/app/app.js n'écrit aucun texte d'écran : chaque identifiant `t('…')` est dans i18n/page.yaml, chaque identifiant de
   page.yaml est utilisé, et aucune chaîne du code ne ressemble à du français (lettres accentuées, mots courants).
2. web/faux-serveur/serveur.mjs : chaque texte français passé à `T('…')` est une clé de i18n/en.yaml (donc de toutes les
   langues : le générateur impose les mêmes clés), et aucune phrase traduisible n'est écrite en dur hors de `T()`.
3. La carte « La mer » (08/11/2026) : la section « Page de réglages : mer » existe dans les six langues avec les mêmes clés,
   dans le même ordre que i18n/page.yaml ; chaque texte est traduit (≠ français), garde ses marqueurs `{noms}`, suit le glossaire
   (« spot » en fr/en/nl/es/it, « Spot » en allemand, « nokta » en turc, « kite » toujours « kite ») et tutoie (allemand, néerlandais,
   espagnol). Le glossaire est contrôlé aussi sur la section « Appareil : messages de l'API, mer ».

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
SECTION_MER = "Page de réglages : mer (08/11/2026)"
SECTION_MER_API = "Appareil : messages de l'API, mer (08/11/2026)"
LANGUES_MER = ["en", "de", "nl", "es", "it", "tr"]
SPOT = {"en": "spot", "de": "Spot", "nl": "spot", "es": "spot", "it": "spot", "tr": "nokta"}  # glossaire (« Spot » : nom commun allemand)
POLITESSE = {"de": r"\b(Sie|Ihnen|Ihre[nmrs]?|Ihr)\b", "nl": r"\b(u|uw)\b", "es": r"\b(usted|ustedes)\b"}  # on tutoie
MARQUEURS = re.compile(r"\{[^}]*\}")

LIGNE = re.compile(r'^"((?:[^"\\]|\\["\\])*)": "((?:[^"\\]|\\["\\])*)"$')
ID_PAGE = re.compile(r"^[a-z]+(\.[a-z0-9_]+)+$")
PREFIXE_ID = re.compile(r"^[a-z]+(\.[a-z0-9]+)*[._]$")  # « nav. », « theme. », « wifi.etat_ » : début d'un identifiant construit
APPEL_T = re.compile(r"\bT\(\s*(['\"`])((?:\\.|(?!\1).)*)\1")
# Noms de champs de l'API (clés de l'import), pas des textes : ils ressemblent à un identifiant de page.
CHAMPS_API = {"services.stt", "services.llm", "services.tts"}
# Mot d'essai envoyé tel quel au service de synthèse vocale pour le tester (API §7) : ce n'est pas un texte d'écran.
MOTS_D_ESSAI = {"Bonjour", "OK"}  # « OK » : réponse du faux service de langage (donnée d'essai), et aussi un bouton de l'écran
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


def sections_yaml(chemin):
    """Les sections `# --- Titre ---` d'un fichier de langue : {titre: [(clé, valeur), …]} dans l'ordre du fichier."""
    sortie, courante = {}, None
    for ligne in chemin.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^# --- (.+) ---$", ligne)
        if m:
            courante = m.group(1)
            sortie.setdefault(courante, [])
            continue
        m = LIGNE.match(ligne)
        if m and courante is not None:
            sortie[courante].append((sans_echappements(m.group(1)), sans_echappements(m.group(2))))
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


def controler_mer():
    """La carte « La mer » : mêmes clés, même ordre, vraiment traduit, marqueurs, glossaire, tutoiement dans les six langues."""
    ecarts = []
    page = sections_yaml(PAGE_YAML).get(SECTION_MER)
    if not page:
        return [f"page.yaml : section « {SECTION_MER} » absente"], 0
    ids_section = [i for i, _ in page]
    ids_mer = [i for i in lire_yaml(PAGE_YAML) if i.startswith("mer.") or i == "nav.mer"]
    if sorted(ids_section) != sorted(ids_mer):
        ecarts.append(f"page.yaml : les identifiants mer.* / nav.mer ne sont pas tous dans la section « {SECTION_MER} » : {sorted(set(ids_mer) ^ set(ids_section))[:5]}")
    cles_fr = list(dict.fromkeys(v for _, v in page))  # la clé d'un fichier de langue est le texte français ; nav.mer et mer.titre disent la même chose
    cles_api = None
    for code in LANGUES_MER:
        sections = sections_yaml(RACINE / "i18n" / f"{code}.yaml")
        sec, api = sections.get(SECTION_MER), sections.get(SECTION_MER_API)
        if sec is None or api is None:
            ecarts.append(f"{code}.yaml : section « {SECTION_MER if sec is None else SECTION_MER_API} » absente")
            continue
        cles = [k for k, _ in sec]
        if cles != cles_fr:
            ecarts.append(f"{code}.yaml : section « {SECTION_MER} » : clés ou ordre différents de page.yaml (manque {[k for k in cles_fr if k not in cles][:2]}, en trop {[k for k in cles if k not in cles_fr][:2]})")
        if cles_api is None:
            cles_api = [k for k, _ in api]
        elif [k for k, _ in api] != cles_api:
            ecarts.append(f"{code}.yaml : section « {SECTION_MER_API} » : clés ou ordre différents des autres langues")
        for k, v in sec + api:
            if v == k:
                ecarts.append(f"{code}.yaml : texte non traduit : {k[:50]!r}")
            if sorted(MARQUEURS.findall(k)) != sorted(MARQUEURS.findall(v)):
                ecarts.append(f"{code}.yaml : marqueurs {{…}} différents : {k[:50]!r}")
            if "spot" in k.lower() and SPOT[code] not in (v if code == "de" else v.lower()):
                ecarts.append(f"{code}.yaml : glossaire — « {SPOT[code]} » attendu pour « spot » : {v[:60]!r}")
            if "kite" in k.lower() and "kite" not in v.lower():
                ecarts.append(f"{code}.yaml : glossaire — « kite » reste « kite » : {v[:60]!r}")
            if code in POLITESSE and re.search(POLITESSE[code], v):
                ecarts.append(f"{code}.yaml : vouvoiement (on tutoie) : {v[:60]!r}")
    return ecarts, len(cles_fr)


def main():
    ecarts_page, nb_ids = controler_page()
    ecarts_serveur, nb_t = controler_serveur()
    ecarts_mer, nb_mer = controler_mer()
    for e in ecarts_page + ecarts_serveur + ecarts_mer:
        print("ECART", e)
    if ecarts_page or ecarts_serveur or ecarts_mer:
        return 1
    print(
        f"ok : app.js sans texte français ({nb_ids} identifiants, tous cités, aucun inconnu) ; "
        f"serveur.mjs : {nb_t} textes passés à T(), tous dans les fichiers de langue ; "
        f"carte « La mer » : {nb_mer} textes, mêmes clés et même ordre dans les six langues, traduits, glossaire respecté"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
