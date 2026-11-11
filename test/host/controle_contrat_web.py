"""Contrôle croisé : le faux appareil de la page (web/faux-serveur/serveur.mjs) contre le vrai contrat (api_reglages.hpp).

La page de réglages n'est jouée par Playwright que contre le faux serveur ; s'il dérive du firmware, les parcours passent alors
que la page casserait sur l'appareil. Ce contrôle lit les deux sources, route par route, et compare :

1. les routes : chaque route du faux serveur existe dans le noyau (faite, ou « 501 non_disponible ») et inversement ;
2. pour chaque route que le firmware fait vraiment (pas 501) : les erreurs qu'elle peut rendre, triplets (statut HTTP, `id`,
   `champ`), doivent être les mêmes des deux côtés ;
3. pour ces mêmes routes : chaque champ JSON que le noyau écrit dans la réponse doit être écrit aussi par le faux serveur.

Comment c'est lu (aucune exécution) : pour une route, le corps de sa branche dans `Api::traiter` (C++) ou dans `api()` (JS), plus le
corps de chaque fonction du même fichier qu'il appelle, de proche en proche. Côté C++ : `erreur(t, statut, "id", "phrase"[, "champ"])`,
`ecrire_erreur` suivi de `.statut = N`, et les noms de `e.champ…("nom", …)` / `e.cle("nom")`. Côté JS : `new Refus(statut, 'id',
phrase[, 'champ'])` et les clés des objets écrits (`clé:` et la forme courte `{ jeton }`).

Les écarts connus et voulus sont dans ECARTS_CONNUS, chacun avec sa raison : un écart nouveau, ou un écart connu qui a disparu,
fait échouer. Ne prouve pas que les valeurs sont les mêmes (bornes, phrases) : les tests C++ et le parcours s'en chargent.

Code de sortie non nul = un contrôle a échoué. Utilisable seul : python test/host/controle_contrat_web.py
"""

import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
NOYAU = RACINE / "components" / "core" / "include" / "ecran"
API_CPP = NOYAU / "api_reglages.hpp"
FAUX = RACINE / "web" / "faux-serveur" / "serveur.mjs"

# Écarts voulus entre le faux serveur et le noyau. Clé : (méthode, route) ; valeur : {genre: {élément: raison}}.
# genre « erreur_faux_seul » = (statut, id, champ) que seul le faux rend ; « erreur_noyau_seul » l'inverse ; « champ_noyau_seul » =
# champ de réponse écrit par le noyau et absent du faux.
_V1 = "le firmware sert le sous-ensemble V1 du contrat (docs/api-reglages.md, encadré « Firmware status ») ; le faux en sert tout"
ECARTS_CONNUS = {
    ("PUT", "/api/location"): {
        "erreur_faux_seul": {(422, "zone_invalide", "nom"): "atteinte seulement par l'import (« zone » qui n'est pas un objet), "
                                                             "route 501 sur le firmware ; le PUT reçoit toujours un objet (400 sinon)"},
    },
    ("GET", "/api/presets"): {
        "champ_faux_seul": {c: _V1 + " : aucun préréglage, trois listes vides" for c in ("aide", "cle_requise", "id", "local", "modele", "nom", "url")},
    },
    ("GET", "/api/update"): {
        "champ_faux_seul": {c: _V1 + " : pas encore de retour arrière" for c in ("dernier_resultat", "disponible", "precedente")},
    },
}

# Erreurs communes à toutes les routes, écrites hors des branches : comparées une fois.
COMMUNES_NOYAU = {(413, "trop_gros", None), (401, "non_appaire", None), (404, "inconnue", None), (500, "interne", None)}


# ---------------------------------------------------------------------------------------------------------------------------
# Lecture : un petit lecteur de code qui saute chaînes, commentaires (et expressions régulières en JS) pour apparier les accolades.


def _fin_chaine(texte, i, guillemet):
    i += 1
    while i < len(texte):
        c = texte[i]
        if c == "\\":
            i += 2
            continue
        if c == guillemet:
            return i + 1
        i += 1
    return i


def masquer(texte, js):
    """Le texte où chaînes, commentaires et expressions régulières sont remplacés par des espaces (mêmes positions).

    Les accolades ou parenthèses qu'ils contiennent (« {n} » dans une phrase) ne comptent plus. Les guillemets restent : un
    littéral garde ses bornes, son contenu se relit dans le texte d'origine aux mêmes positions."""
    sortie = list(texte)
    i, n = 0, len(texte)
    precedent = ""  # dernier caractère significatif : décide si « / » ouvre une expression régulière (JS)
    while i < n:
        c = texte[i]
        if texte.startswith("//", i):
            j = texte.find("\n", i)
            j = n if j < 0 else j
        elif texte.startswith("/*", i):
            j = texte.find("*/", i + 2)
            j = n if j < 0 else j + 2
        elif c in "\"'" or (js and c == "`"):
            j = _fin_chaine(texte, i, c)
            for k in range(i + 1, j - 1):
                if sortie[k] != "\n":
                    sortie[k] = " "
            precedent = c
            i = j
            continue
        elif js and c == "/" and (precedent == "" or precedent in "(,=:[!&|?{};+-*%<>~^" or texte[max(0, i - 7):i].rstrip().endswith("return")):
            j = i + 1
            classe = False
            while j < n and texte[j] != "\n":
                if texte[j] == "\\":
                    j += 2
                    continue
                if texte[j] == "[":
                    classe = True
                elif texte[j] == "]":
                    classe = False
                elif texte[j] == "/" and not classe:
                    break
                j += 1
            for k in range(i + 1, j):
                sortie[k] = " "
            precedent = "/"
            i = j + 1
            continue
        else:
            if not c.isspace():
                precedent = c
            i += 1
            continue
        for k in range(i, j):
            if sortie[k] != "\n":
                sortie[k] = " "
        i = j
    return "".join(sortie)


def fin_bloc(masque, ouvrante):
    """Position juste après l'accolade (ou parenthèse) qui ferme celle de `ouvrante`."""
    paire = {"{": "}", "(": ")", "[": "]"}[masque[ouvrante]]
    pile = [paire]
    i = ouvrante + 1
    while i < len(masque) and pile:
        c = masque[i]
        if c in "{([":
            pile.append({"{": "}", "(": ")", "[": "]"}[c])
        elif c in "})]":
            if c != pile[-1]:
                raise ValueError(f"accolades déséquilibrées vers la position {i}")
            pile.pop()
        i += 1
    return i


def arguments(texte, masque, ouvrante):
    """Les arguments (texte d'origine, sans espaces autour) de l'appel dont la parenthèse ouvrante est à `ouvrante`."""
    fin = fin_bloc(masque, ouvrante) - 1
    args, debut, prof = [], ouvrante + 1, 0
    for i in range(ouvrante + 1, fin):
        c = masque[i]
        if c in "{([":
            prof += 1
        elif c in "})]":
            prof -= 1
        elif c == "," and prof == 0:
            args.append(texte[debut:i].strip())
            debut = i + 1
    dernier = texte[debut:fin].strip()
    if dernier:
        args.append(dernier)
    return args


def litteral(arg):
    """« "nom" » ou « 'nom' » -> nom ; autre chose -> None."""
    m = re.fullmatch(r"""(["'])(\w*)\1""", arg or "")
    return m.group(2) if m else None


# ---------------------------------------------------------------------------------------------------------------------------
# Le noyau (C++)


class Source:
    def __init__(self, chemin, js):
        self.chemin = chemin
        self.texte = chemin.read_text(encoding="utf-8")
        self.masque = masquer(self.texte, js)
        self.js = js
        self.fonctions = {}  # nom -> (début, fin) du corps

    def appels(self, debut, fin):
        # « hote_.wifi() », « fm::trouver() », « etat.zone() » : la méthode d'un autre objet, pas une fonction de ce fichier
        return set(re.findall(r"(?<![\w.>:])(\w+)\s*\(", self.masque[debut:fin])) & set(self.fonctions)

    def fermeture(self, debut, fin):
        """[(début, fin)] : le morceau donné et le corps de chaque fonction du fichier qu'il appelle, de proche en proche."""
        morceaux, vues, a_voir = [(debut, fin)], set(), self.appels(debut, fin)
        while a_voir:
            nom = a_voir.pop()
            if nom in vues:
                continue
            vues.add(nom)
            d, f = self.fonctions[nom]
            morceaux.append((d, f))
            a_voir |= self.appels(d, f) - vues
        return morceaux


def lire_noyau():
    """{(méthode, route): {"erreurs": set, "champs": set}} pour les routes faites ; {(méthode, route)} pour les 501 ; source."""
    src = Source(API_CPP, js=False)
    t, m = src.texte, src.masque
    # Fonctions membres et libres du fichier : « … nom(…) [const] noexcept {».
    for d in re.finditer(r"^\s*(?:template\s*<[^>]*>\s*)?(?:static\s+)?constexpr\s+[\w:<>&*\s]+?\b(\w+)\s*\(", m, re.M):
        ouvrante = d.end() - 1
        apres = fin_bloc(m, ouvrante)
        accolade = re.match(r"[^;{]*\{", m[apres:])
        if accolade:
            debut = apres + accolade.end() - 1
            src.fonctions.setdefault(d.group(1), (debut, fin_bloc(m, debut)))
    if "traiter" not in src.fonctions or "hors_perimetre" not in src.fonctions:
        raise ValueError("Api::traiter ou hors_perimetre introuvable : api_reglages.hpp a changé de forme ?")
    debut, fin = src.fonctions["traiter"]
    # les littéraux sont blancs dans le masque : les branches se cherchent dans le texte, aux mêmes positions
    branches = list(re.finditer(r"if \(est\(rq, Methode::(\w+), \"(/api/[^\"]+)\"\)\)", t[debut:fin]))
    branches_501 = re.search(r"if \(hors_perimetre\(rq\)\)", t[debut:fin])
    if not branches or not branches_501:
        raise ValueError("branches de Api::traiter introuvables")
    reelles = {}
    for k, b in enumerate(branches):
        d = debut + b.end()
        f = debut + (branches[k + 1].start() if k + 1 < len(branches) else branches_501.start())
        cle = (b.group(1).upper(), b.group(2))
        reelles[cle] = analyser_noyau(src, d, f)

    hd, hf = src.fonctions["hors_perimetre"]
    non_dispo = set()
    for bloc in re.finditer(r"if \(((?:c == \"[^\"]+\"(?:\s*\|\|\s*)?)+)\)\s*return ([^;]+);", t[hd:hf]):
        routes = re.findall(r"\"([^\"]+)\"", bloc.group(1))
        methodes = [x.upper() for x in re.findall(r"Methode::(\w+)", bloc.group(2))]
        non_dispo |= {(me, r) for r in routes for me in methodes}
    if not non_dispo:
        raise ValueError("hors_perimetre illisible")
    return reelles, non_dispo


def analyser_noyau(src, debut, fin):
    t, m = src.texte, src.masque
    erreurs, champs = set(), set()
    enveloppe = src.fonctions["ecrire_erreur"]  # {"erreur":{"id","message","champ","reessayer_dans"}} : la forme d'erreur, pas un champ de route
    for d, f in src.fermeture(debut, fin):
        if (d, f) == enveloppe:
            continue
        for appel in re.finditer(r"\berreur\s*\(", m[d:f]):
            a = arguments(t, m, d + appel.end() - 1)
            if len(a) >= 4 and a[1].isdigit():
                champ = litteral(a[4]) if len(a) > 4 else None
                if len(a) > 4 and champ is None:  # champ calculé (« a ? "x" : "y" ») : les deux littéraux possibles
                    for c in re.findall(r"\"(\w+)\"", a[4]):
                        erreurs.add((int(a[1]), litteral(a[2]), c))
                else:
                    erreurs.add((int(a[1]), litteral(a[2]), champ))
        for appel in re.finditer(r"\becrire_erreur\s*\(", m[d:f]):
            a = arguments(t, m, d + appel.end() - 1)
            ident = litteral(a[1]) if len(a) > 1 else None
            statut = re.search(r"\.statut\s*=\s*(\d+)\s*;", m[d + appel.end():f])
            if ident and ident != "interne" and statut:
                erreurs.add((int(statut.group(1)), ident, None))
        # l'écrivain de la réponse s'appelle toujours « e » ; « objet.champ("code") » est une LECTURE du corps reçu
        champs |= set(re.findall(r"\be\.(?:champ|champ_entier|champ_booleen|champ_decimal)\(\s*\"(\w+)\"", t[d:f]))
        champs |= set(re.findall(r"\be\.cle\(\s*\"(\w+)\"\s*\)", t[d:f]))
        # « for (std::string_view x : {"a", "b"}) … e.cle(x) » : les noms de la liste sont des clés
        for boucle in re.finditer(r"for \(std::string_view (\w+) : \{([^}]*)\}\)", t[d:f]):
            if re.search(r"\be\.cle\(\s*" + boucle.group(1) + r"\s*\)", t[d:f]):
                champs |= set(re.findall(r"\"(\w+)\"", boucle.group(2)))
    return {"erreurs": erreurs, "champs": champs}


# ---------------------------------------------------------------------------------------------------------------------------
# Le faux serveur (JS)


def lire_faux():
    """{(méthode, route): {"erreurs": set}} et l'ensemble des erreurs (statut, id, None) écrites dans tout le fichier."""
    src = Source(FAUX, js=True)
    t, m = src.texte, src.masque
    for d in re.finditer(r"^(?:async\s+)?function\s+(\w+)\s*\(", m, re.M):
        ouvrante = d.end() - 1
        corps = m.index("{", fin_bloc(m, ouvrante))
        src.fonctions[d.group(1)] = (corps, fin_bloc(m, corps))
    for d in re.finditer(r"^const\s+(\w+)\s*=\s*(?:async\s*)?(?:\([^)]*\)|\w+)\s*=>", m, re.M):  # premier niveau seulement
        # fonction fléchée : jusqu'au « ; » de fin d'instruction, hors parenthèses et accolades
        i, prof = d.end(), 0
        while i < len(m) and not (m[i] == ";" and prof == 0):
            prof += m[i] in "{([" and 1 or (-1 if m[i] in "})]" else 0)
            i += 1
        src.fonctions.setdefault(d.group(1), (d.end(), i))
    if "api" not in src.fonctions:
        raise ValueError("function api() introuvable : serveur.mjs a changé de forme ?")
    debut, fin = src.fonctions.pop("api")
    for inutile in ("json", "T"):
        src.fonctions.pop(inutile, None)

    corps_api = t[debut:fin]
    branches = []
    for b in re.finditer(r"if \(m === '(\w+)' && p === '(/api/[^']+)'\)", corps_api):
        branches.append((b.start(), b.end(), [(b.group(1), b.group(2))]))
    # les services : « const sv = p.match(/^\/api\/services\/(stt|llm|tts)(\/test)?$/) » puis trois branches sur sv[2] et m
    sv = re.search(r"const sv = p\.match\(", corps_api)
    if not sv:
        raise ValueError("bloc des services introuvable dans api()")
    ids = re.search(r"\((\w+(?:\|\w+)+)\)", corps_api[sv.end():corps_api.index("\n", sv.end())])
    if not ids:
        raise ValueError("liste des services illisible")
    bloc_sv = debut + sv.start()
    fin_sv = fin_bloc(m, m.index("{", m.index("if (sv)", bloc_sv)))
    sous = []
    for s in re.finditer(r"if \((!?)sv\[2\] && m === '(\w+)'\)", t[bloc_sv:fin_sv]):
        suffixe = "/test" if s.group(1) == "" else ""
        sous.append((bloc_sv + s.start(), bloc_sv + s.end(), [(s.group(2), f"/api/services/{i}{suffixe}") for i in ids.group(1).split("|")]))
    if len(sous) != 3:
        raise ValueError(f"branches des services : {len(sous)} au lieu de 3")
    # chaque branche va jusqu'à la suivante (ou à la fin du bloc qui la contient)
    toutes = sorted([(debut + d, debut + f, r) for d, f, r in branches] + sous)
    routes = {}
    for k, (d, f, routes_de) in enumerate(toutes):
        suite = toutes[k + 1][0] if k + 1 < len(toutes) else fin
        borne = min(suite, fin_sv) if bloc_sv <= d < fin_sv else suite
        if d < bloc_sv and borne > bloc_sv:
            borne = bloc_sv
        analyse = analyser_faux(src, f, borne)
        for route in routes_de:
            routes[route] = analyse
    # les erreurs communes (401 sans jeton, 404, 413…) sont écrites hors des branches : cherchées dans tout le fichier
    tout = {"erreurs": set()}
    for appel in re.finditer(r"\bnew Refus\s*\(", m):
        a = arguments(t, m, appel.end() - 1)
        if len(a) >= 2 and a[0].isdigit():
            tout["erreurs"].add((int(a[0]), litteral(a[1]), None))
    return routes, tout["erreurs"]


def champs_possibles(texte, arg, d, f):
    """Les valeurs littérales possibles d'un argument `champ` : un littéral, `undefined` (aucun), ou une constante du même corps
    (« const champVent = c.x !== undefined ? 'vent_min_kn' : 'vent_max_kn'; ») dont on garde chaque littéral."""
    if arg is None or arg == "undefined":
        return [None]
    if litteral(arg) is not None:
        return [litteral(arg)]
    if re.fullmatch(r"\w+", arg):
        defn = re.search(r"\bconst " + arg + r"\s*=\s*([^;]+);", texte[d:f])
        if defn:
            return re.findall(r"'(\w+)'", defn.group(1)) or [None]
    return [None]


def analyser_faux(src, debut, fin):
    """Les erreurs (statut, id, champ) qu'une branche de api() peut rendre : `new Refus(…)` dans la branche et dans les fonctions
    qu'elle appelle, plus les appels d'une aide locale « const refus = (id, phrase, champ) => new Refus(422, id, phrase, champ) »."""
    t, m = src.texte, src.masque
    erreurs = set()
    morceaux = [(debut, fin)] if fin <= debut or debut >= len(m) else src.fermeture(debut, fin)
    for d, f in morceaux:
        for appel in re.finditer(r"\bnew Refus\s*\(", m[d:f]):
            a = arguments(t, m, d + appel.end() - 1)
            if len(a) >= 3 and a[0].isdigit() and litteral(a[1]) is not None:
                for champ in champs_possibles(t, a[3] if len(a) > 3 else None, d, f):
                    erreurs.add((int(a[0]), litteral(a[1]), champ))
        for aide in re.finditer(r"const (\w+) = \((\w+), \w+, (\w+)\) => new Refus\((\d+), \2,", t[d:f]):
            for appel in re.finditer(r"\b" + aide.group(1) + r"\s*\(", m[d:f]):
                a = arguments(t, m, d + appel.end() - 1)
                if a and litteral(a[0]) is not None:
                    for champ in champs_possibles(t, a[2] if len(a) > 2 else None, d, f):
                        erreurs.add((int(aide.group(4)), litteral(a[0]), champ))
    return {"erreurs": erreurs}


def cles(valeur):
    """Toutes les clés d'objet d'une réponse JSON, à toute profondeur (éléments des listes compris)."""
    if isinstance(valeur, dict):
        return set(valeur) | {c for v in valeur.values() for c in cles(v)}
    if isinstance(valeur, list):
        return {c for v in valeur for c in cles(v)}
    return set()


# Ce que le contrôle demande au faux serveur, dans l'ordre, pour voir chaque champ qu'il écrit (aucun appel à Internet : la météo
# et la recherche de lieu, qui en demandent, ne sont pas lues ici). Corps envoyés : des valeurs valides du contrat.
LYON = {"nom": "Lyon", "region": "Auvergne-Rhône-Alpes", "pays": "France", "code_pays": "FR", "latitude": 45.75, "longitude": 4.85,
        "fuseau": "Europe/Paris"}
DEMANDES = [
    ("GET", "/api/location", None),        # sans ville : « definie: false » et les sources mondiales
    ("PUT", "/api/location", {"source_meteo": "auto"}),  # la source seule, sans ville : la réponse dit aussi « definie: false »
    ("PUT", "/api/location", LYON),
    ("GET", "/api/location", None),
    ("PUT", "/api/sea", {"spots": [{"id": "essai", "nom": "Essai"}], "choisi": "essai"}),
    ("GET", "/api/sea", None),
    ("PUT", "/api/display", {"theme": "auto"}),
    ("GET", "/api/display", None),
    ("PUT", "/api/alerts", {"actives": True}),
    ("GET", "/api/alerts", None),
    ("GET", "/api/status", None),
    ("GET", "/api/system", None),
    ("GET", "/api/wifi", None),
    ("GET", "/api/services", None),
    ("GET", "/api/presets", None),
    ("GET", "/api/update", None),
]
SANS_LECTURE = {("GET", "/api/weather"): "Open-Meteo (Internet)", ("GET", "/api/geocode"): "Open-Meteo (Internet)"}


def champs_du_faux():
    """{(méthode, route): {clés}} : les réponses réelles du faux serveur, lancé sur un port libre le temps de la lecture."""
    import json  # noqa: PLC0415
    import os  # noqa: PLC0415
    import shutil  # noqa: PLC0415
    import socket  # noqa: PLC0415
    import subprocess  # noqa: PLC0415
    import time  # noqa: PLC0415
    import urllib.request  # noqa: PLC0415

    node = shutil.which("node")
    if not node:
        raise OSError("Node.js introuvable : le faux serveur ne peut pas être lancé")
    ports = []
    for _ in range(2):  # la page et l'API, puis le faux service OpenAI (inutilisé ici, mais il écoute)
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            ports.append(s.getsockname()[1])
    port = ports[0]
    code = "135790"
    env = {**os.environ, "PORT": str(port), "PORT_SERVICE": str(ports[1]), "CODE_APPAIRAGE": code}
    proc = subprocess.Popen([node, str(FAUX)], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
                            encoding="utf-8", errors="replace")
    base = f"http://127.0.0.1:{port}"

    def appel(methode, chemin, corps=None, jeton=None):
        rq = urllib.request.Request(base + chemin, method=methode, data=None if corps is None else json.dumps(corps).encode())
        rq.add_header("Content-Type", "application/json")
        if jeton:
            rq.add_header("Authorization", "Bearer " + jeton)
        with urllib.request.urlopen(rq, timeout=5) as r:
            texte = r.read()
            return json.loads(texte) if texte else None

    try:
        for _ in range(80):
            try:
                appel("GET", "/api/status")
                break
            except OSError:
                if proc.poll() is not None:
                    raise OSError("le faux serveur s'est arrêté : " + ((proc.stderr.read() if proc.stderr else "") or "").strip()[:300]) from None
                time.sleep(0.1)
        else:
            raise OSError("le faux serveur ne répond pas")
        reponse = appel("POST", "/api/pair", {"code": code}) or {}
        jeton = reponse.get("jeton")
        if not jeton:
            raise OSError("le faux serveur n'a pas rendu de jeton d'appairage")
        vus = {("POST", "/api/pair"): cles(reponse), ("DELETE", "/api/pair"): set()}
        for methode, chemin, corps in DEMANDES:
            vus.setdefault((methode, chemin), set()).update(cles(appel(methode, chemin, corps, jeton)))
        return vus
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


# ---------------------------------------------------------------------------------------------------------------------------


def controle_contrat_web():
    """Nombre d'échecs (0 = le faux serveur suit le contrat du noyau)."""
    print("\ncontrat de l'API : le faux serveur de la page (web/faux-serveur) contre le noyau (api_reglages.hpp) :")
    try:
        reelles, non_dispo = lire_noyau()
        faux, communes_faux = lire_faux()
        champs_faux = champs_du_faux()
    except (ValueError, OSError) as erreur:
        print(f"  ÉCHEC   lecture impossible : {erreur}")
        return 1
    echecs = 0

    def echec(texte):
        nonlocal echecs
        echecs += 1
        print(f"  ÉCHEC   {texte}")

    noyau = set(reelles) | non_dispo
    for route in sorted(set(faux) - noyau):
        echec(f"{route[0]} {route[1]} : route du faux serveur inconnue du noyau (ni faite, ni « 501 non_disponible »)")
    for route in sorted(noyau - set(faux)):
        echec(f"{route[0]} {route[1]} : route du noyau absente du faux serveur")
    # 500 « interne » : le faux ne le rend que sur une exception imprévue (écrit à part, hors de Refus)
    for commune in sorted(COMMUNES_NOYAU - communes_faux - {(500, "interne", None)}, key=str):
        echec(f"erreur commune {commune} du noyau : le faux serveur ne la rend nulle part")
    for route in sorted(set(reelles) - set(champs_faux) - set(SANS_LECTURE)):
        echec(f"{route[0]} {route[1]} : route faite par le firmware, jamais lue dans le faux serveur (DEMANDES à compléter)")

    vus, erreurs_vues, champs_vus = set(), 0, 0
    for route in sorted(reelles):
        if route not in faux:
            continue
        cpp, js = reelles[route], faux[route]
        connus = ECARTS_CONNUS.get(route, {})
        comparaisons = [
            ("erreur_faux_seul", js["erreurs"] - cpp["erreurs"] - COMMUNES_NOYAU, "erreur rendue par le faux serveur seul"),
            ("erreur_noyau_seul", cpp["erreurs"] - js["erreurs"] - COMMUNES_NOYAU, "erreur rendue par le noyau seul"),
        ]
        if route in champs_faux:
            comparaisons += [
                ("champ_noyau_seul", cpp["champs"] - champs_faux[route], "champ écrit par le noyau, absent de la réponse du faux serveur"),
                ("champ_faux_seul", champs_faux[route] - cpp["champs"], "champ de la réponse du faux serveur que le noyau n'écrit pas"),
            ]
            champs_vus += len(cpp["champs"])
        erreurs_vues += len(cpp["erreurs"] - COMMUNES_NOYAU)
        for genre, ecart, phrase in comparaisons:
            for element in sorted(ecart, key=str):
                vus.add((route, genre, element))
                if element not in connus.get(genre, {}):
                    echec(f"{route[0]} {route[1]} : {phrase} : {element}")
    for route, genres in ECARTS_CONNUS.items():
        for genre, elements in genres.items():
            for element in elements:
                if (route, genre, element) not in vus:
                    echec(f"{route[0]} {route[1]} : l'écart connu {genre} {element} a disparu : le retirer de ECARTS_CONNUS")
    if not echecs:
        nb_connus = sum(len(x) for g in ECARTS_CONNUS.values() for x in g.values())
        print(f"  ok      mêmes {len(faux)} routes des deux côtés ({len(reelles)} faites par le firmware, {len(non_dispo)} en « 501 »)")
        print(f"  ok      routes faites : {erreurs_vues} erreurs et {champs_vus} champs de réponse identiques ({nb_connus} écarts connus et "
              f"expliqués ; champs de {', '.join(r[1] for r in SANS_LECTURE)} non lus : {', '.join(sorted(set(SANS_LECTURE.values())))})")
    return echecs


if __name__ == "__main__":
    sys.exit(1 if controle_contrat_web() else 0)
