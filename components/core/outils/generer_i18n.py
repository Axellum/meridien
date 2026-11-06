#!/usr/bin/env python3
"""Génère les tables de traduction depuis i18n/*.yaml (sept langues, ADR-0006).

Le texte FRANÇAIS du code est la clé (façon gettext, comme sur le Tab5) : `tr("Dimanche")`. Un fichier `i18n/<code>.yaml`
par langue, `"texte français": "traduction"`, plat ; `i18n/fr.yaml` ne porte que les métadonnées (le français est la
clé) ; `i18n/page.yaml` associe un identifiant stable de la page de réglages (ce qu'écrit app.js) au texte français.

Sorties (jamais modifiées à la main) :
  components/core/include/ecran/i18n_donnees.inc   lignes de la table C++ du noyau pur (lue par ecran/i18n.hpp)
  web/app/lang/<code>.json                         un petit fichier par langue pour la page (chargé à la demande)

Usage :
  python components/core/outils/generer_i18n.py              réécrit les sorties
  python components/core/outils/generer_i18n.py --verifier   ne modifie rien ; code 1 si une source est incohérente ou si
                                                             une sortie n'est plus à jour

Contrôles des sources (tous bloquants) : format strict ; ordre figé des langues (l'indice est enregistré en NVS) ;
mêmes marqueurs dans le même ordre entre la clé et chaque traduction (%d %s %u…, {noms}) ; une langue marquée
`_statut: complet` a toutes les clés, dans le même ordre que les autres ; aucun caractère de contrôle ; aucun doublon.
Repli : une clé absente d'une langue partielle prend l'anglais, puis le français. Dans la table C++ la case reste vide
(le repli est fait à l'exécution par ecran/i18n.hpp et testé) ; dans les fichiers JSON il est écrit à la génération.
"""
import json
import re
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[3]
DOSSIER_LANGUES = RACINE / "i18n"
SORTIE_INC = RACINE / "components" / "core" / "include" / "ecran" / "i18n_donnees.inc"
SORTIE_JSON = RACINE / "web" / "app" / "lang"

# Ordre FIGÉ des langues : l'indice est mémorisé en NVS, une langue ajoutée prend le numéro suivant (à la fin).
# Même ordre que le sélecteur « Langue » du Tab5 (Tab5/lang/*.yaml, champ _index, lu le 06/11/2026).
ORDRE_FIGE = ("fr", "en", "de", "nl", "es", "it", "tr")
REPLI = "en"

LIGNE_ENTREE = re.compile(r'^"((?:[^"\\]|\\["\\])*)": "((?:[^"\\]|\\["\\])*)"$')
LIGNE_META = re.compile(r"^_([a-z]+): ([^#]*?)\s*(?:#.*)?$")
LIGNE_SECTION = re.compile(r"^# --- (.+) ---$")
# Marqueurs à conserver : %d %s %u %02d %lu… (jamais « % » suivi d'une espace) et {noms}.
MARQUEUR = re.compile(r"%%|%[-+#0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z)?[diuxXsc]|\{[a-z_][a-z0-9_]*\}")
INVISIBLES = re.compile("[ ­ -‏ -  -⁯﻿]")


class ErreurSource(Exception):
    pass


def marqueurs(texte):
    """Marqueurs du texte, dans l'ordre (« %% » n'est pas un marqueur)."""
    return [m for m in MARQUEUR.findall(texte) if m != "%%"]


def lire_yaml(chemin):
    """Lit un fichier de langue ou de catalogue. Rend (métadonnées, [(clé, valeur, section)]). Format strict."""
    meta, entrees, vues = {}, [], set()
    section = ""
    for n, ligne in enumerate(chemin.read_text(encoding="utf-8").split("\n"), 1):
        ligne = ligne.rstrip("\r")
        if not ligne.strip():
            continue
        if ligne.startswith("#"):
            m = LIGNE_SECTION.match(ligne)
            if m:
                section = m.group(1)
            continue
        m = LIGNE_ENTREE.match(ligne)
        if m:
            cle, valeur = (s.replace('\\"', '"').replace("\\\\", "\\") for s in m.groups())
            if cle in vues:
                raise ErreurSource(f"{chemin.name}:{n} : clé en double {cle!r}")
            vues.add(cle)
            entrees.append((cle, valeur, section))
            continue
        m = LIGNE_META.match(ligne)
        if m:
            meta[m.group(1)] = m.group(2)
            continue
        raise ErreurSource(f"{chemin.name}:{n} : ligne illisible (format strict) : {ligne[:70]!r}")
    return meta, entrees


def charger():
    """Toutes les sources. Rend (langues, cles, page) ; langues = liste ordonnée par indice de dicts
    {code, nom, indice, complet, entrees: {clé: traduction}}, cles = [(clé, section)] dans l'ordre des fichiers,
    page = [(identifiant, texte français)]."""
    langues = []
    for chemin in sorted(DOSSIER_LANGUES.glob("*.yaml")):
        if chemin.name == "page.yaml":
            continue
        meta, entrees = lire_yaml(chemin)
        for champ in ("langue", "code", "index"):
            if champ not in meta:
                raise ErreurSource(f"{chemin.name} : métadonnée _{champ} manquante")
        if meta["code"] != chemin.stem:
            raise ErreurSource(f"{chemin.name} : _code vaut {meta['code']!r}, le nom du fichier dit {chemin.stem!r}")
        langues.append({"code": meta["code"], "nom": meta["langue"], "indice": int(meta["index"]),
                        "complet": meta.get("statut") == "complet", "entrees": {c: v for c, v, _ in entrees},
                        "ordre": [(c, s) for c, _, s in entrees]})
    langues.sort(key=lambda lg: lg["indice"])
    codes = tuple(lg["code"] for lg in langues)
    if [lg["indice"] for lg in langues] != list(range(len(langues))):
        raise ErreurSource(f"indices de langue : 0, 1, 2… sans trou ni doublon (vu {[lg['indice'] for lg in langues]})")
    if codes[: len(ORDRE_FIGE)] != ORDRE_FIGE[: len(codes)] or len(codes) < len(ORDRE_FIGE):
        raise ErreurSource(f"ordre des langues {codes} : l'ordre figé est {ORDRE_FIGE} (une langue nouvelle va à la fin)")
    if langues[0]["entrees"]:
        raise ErreurSource("fr.yaml ne contient que des métadonnées : le français est la clé")

    traduites = langues[1:]
    cles, deja = [], set()
    for lg in traduites:
        for cle, section in lg["ordre"]:
            if cle not in deja:
                deja.add(cle)
                cles.append((cle, section))
    for lg in traduites:
        for cle, valeur in lg["entrees"].items():
            if not cle.strip() or cle != cle.strip():
                raise ErreurSource(f"{lg['code']}.yaml : clé vide ou entourée d'espaces {cle!r}")
            if re.search(r"[\x00-\x1f\x7f�]", cle + valeur):
                raise ErreurSource(f"{lg['code']}.yaml : caractère de contrôle ou U+FFFD dans {cle!r}")
            if valeur != valeur.strip():
                raise ErreurSource(f"{lg['code']}.yaml : traduction entourée d'espaces pour {cle!r}")
            if not valeur:
                raise ErreurSource(f"{lg['code']}.yaml : traduction vide pour {cle!r} (supprimer la ligne plutôt)")
            if marqueurs(cle) != marqueurs(valeur):
                raise ErreurSource(f"{lg['code']}.yaml : marqueurs différents pour {cle!r} : {marqueurs(cle)} contre "
                                   f"{marqueurs(valeur)} (même liste, même ordre)")
        if lg["complet"]:
            absentes = [c for c, _ in cles if c not in lg["entrees"]]
            if absentes:
                raise ErreurSource(f"{lg['code']}.yaml est « complet » mais il manque {len(absentes)} clé(s) : {absentes[:3]}")
            if [c for c, _ in lg["ordre"]] != [c for c, _ in cles]:
                raise ErreurSource(f"{lg['code']}.yaml : les clés ne sont pas dans le même ordre que {traduites[0]['code']}.yaml")
    if REPLI not in codes:
        raise ErreurSource(f"la langue de repli {REPLI} manque")
    if not next(lg for lg in langues if lg["code"] == REPLI)["complet"]:
        raise ErreurSource(f"la langue de repli {REPLI} doit être complète")

    meta, entrees_page = lire_yaml(DOSSIER_LANGUES / "page.yaml")
    if meta:
        raise ErreurSource("page.yaml : pas de métadonnées")
    page = [(i, f) for i, f, _ in entrees_page]
    cles_connues = {c for c, _ in cles}
    for ident, fr in page:
        if not re.fullmatch(r"[a-z]+(?:\.[a-z0-9_]+)+", ident):
            raise ErreurSource(f"page.yaml : identifiant {ident!r} (forme attendue : domaine.nom_en_minuscules)")
        if fr not in cles_connues:
            raise ErreurSource(f"page.yaml : {ident} → le texte {fr!r} n'est pas une clé des fichiers de langue")
    return langues, cles, page


def litteral_cpp(texte):
    """Littéral C++ UTF-8 : guillemet et antislash échappés, caractères invisibles en octal (3 chiffres : jamais ambigu)."""
    sortie = []
    for car in texte:
        if car in '"\\':
            sortie.append("\\" + car)
        elif INVISIBLES.match(car):
            sortie.append("".join(f"\\{b:03o}" for b in car.encode("utf-8")))
        else:
            sortie.append(car)
    return '"' + "".join(sortie) + '"'


def produire_inc(langues, cles):
    traduites = langues[1:]
    lignes = [
        "// FICHIER GÉNÉRÉ par components/core/outils/generer_i18n.py depuis i18n/*.yaml : ne pas modifier à la main.",
        "// {clé française, {" + ", ".join(lg["code"] for lg in traduites) + "}}, trié par octets UTF-8 sur la clé",
        '// (recherche dichotomique de ecran/i18n.hpp). "" = pas de traduction : repli sur l\'anglais, puis sur le français.',
        "// clang-format off",
    ]
    for cle in sorted((c for c, _ in cles), key=lambda c: c.encode("utf-8")):
        cases = ", ".join(litteral_cpp(lg["entrees"].get(cle, "")) for lg in traduites)
        lignes.append(f"{{{litteral_cpp(cle)}, {{{cases}}}}},")
    # Pas de « clang-format on » à la fin : clang-format 23 trouve alors un écart sur la dernière ligne (constaté).
    return "\n".join(lignes) + "\n"


def produire_json(langues, page):
    """Rend {code: texte JSON}. Pour une langue : identifiant → traduction, repli anglais puis français écrit ici."""
    repli = next(lg for lg in langues if lg["code"] == REPLI)["entrees"]
    sorties = {}
    for lg in langues:
        contenu = {}
        for ident, fr in page:
            contenu[ident] = fr if lg["indice"] == 0 else lg["entrees"].get(fr) or repli.get(fr) or fr
        sorties[lg["code"]] = json.dumps(dict(sorted(contenu.items())), ensure_ascii=False, separators=(",", ":")) + "\n"
    return sorties


def sorties_attendues():
    langues, cles, page = charger()
    attendues = {SORTIE_INC: produire_inc(langues, cles)}
    for code, texte in produire_json(langues, page).items():
        attendues[SORTIE_JSON / f"{code}.json"] = texte
    return attendues, langues, cles, page


def main(argv):
    verifier = "--verifier" in argv
    try:
        attendues, langues, cles, page = sorties_attendues()
    except ErreurSource as e:
        print(f"ÉCHEC : {e}", file=sys.stderr)
        return 1
    resume = f"{len(langues)} langues, {len(cles)} clés, {len(page)} identifiants de page"
    if verifier:
        perimes = [p for p, t in attendues.items() if not p.exists() or p.read_text(encoding="utf-8") != t]
        # un fichier de langue de la page qui n'a plus de source doit disparaître
        orphelins = [p for p in SORTIE_JSON.glob("*.json") if p not in attendues] if SORTIE_JSON.exists() else []
        if perimes or orphelins:
            for p in perimes + orphelins:
                print(f"ÉCHEC : {p.relative_to(RACINE)} n'est pas à jour (relancer generer_i18n.py)", file=sys.stderr)
            return 1
        print(f"i18n : sources cohérentes et sorties à jour ({resume})")
        return 0
    for chemin, texte in attendues.items():
        chemin.parent.mkdir(parents=True, exist_ok=True)
        chemin.write_text(texte, encoding="utf-8", newline="\n")
    print(f"écrit {len(attendues)} fichiers ({resume})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
