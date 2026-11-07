#!/usr/bin/env python3
"""Contrôle INDÉPENDANT des alertes officielles (vigie_alertes.hpp, vigie_vue.hpp) : relit les VRAIES réponses de Vigie avec Python seul
(json, sans reprendre une ligne du noyau C++) et écrit ce que le noyau doit produire.

    python test/host/controle_python_vigie.py             # (ré)écrit les .inc dans test/host/donnees/
    python test/host/controle_python_vigie.py --verifier  # ne modifie rien ; échoue si un .inc n'est pas à jour, ou si la table de
                                                          # caractères du noyau annonce une lettre que la police embarquée n'a pas

Écrit, pour chaque réponse `donnees/vigie_<nom>.json` :
  - `vigie_<nom>.inc`          la réponse, octet pour octet, en littéral brut C++ (R"VG(…)VG") ;
  - `attendu_vigie_<nom>.inc`  ce que `analyser()` doit lire (une valeur par champ gardé) et, pour des instants pris AUTOUR de chaque début et
                               de chaque fin de la réponse, les alertes que `composer()` doit montrer pour chaque niveau minimal.

Ce que ce fichier prouve : que le noyau C++ et une seconde lecture, écrite à part, sont d'accord sur des réponses réelles. Ce qu'il ne
prouve pas : que la seconde lecture ait raison (elle applique la MÊME règle écrite, voir l'en-tête de vigie_vue.hpp et l'ADR-0009), ni
ce que Vigie répondra demain. La capture des réponses est faite par capturer_vigie.py (à la main, appelle le réseau).
"""
import json
import re
import struct
import sys
import zlib
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[1]
DONNEES = ICI / "donnees"
ALERTES_HPP = RACINE / "components/core/include/ecran/vigie_alertes.hpp"
POLICE = RACINE / "main/assets/roboto_700.ttf"

# Mêmes limites que vigie_alertes.hpp (lues dans le fichier : une divergence ferait échouer le contrôle des .inc).
def constante(nom: str) -> int:
    m = re.search(r"inline constexpr std::size_t " + nom + r"\s*=\s*(\d+);", ALERTES_HPP.read_text(encoding="utf-8"))
    if m is None:
        raise SystemExit(f"{nom} introuvable dans {ALERTES_HPP}")
    return int(m.group(1))


ALERTES_MAX = constante("ALERTES_MAX")
TEXTES_MAX = constante("TEXTES_MAX")
CAP = {nom: constante(nom.upper() + "_CAP") for nom in
       ("langue", "titre", "description", "consigne", "emetteur", "zone", "original", "attribution", "avertissement")}
AGE_MAX_S = 6 * 3600
INSTANT_MIN, INSTANT_MAX = 946684800, 4102444800

NIVEAUX = {"GREEN": 1, "YELLOW": 2, "ORANGE": 3, "RED": 4}
PHENOMENES = ["wind", "rain", "thunderstorm", "snow-ice", "heat", "cold", "flood", "avalanche", "coastal", "fire", "fog", "other"]
ETATS = {"new": 1, "update": 2, "cancel": 3}


def plages_police():
    texte = ALERTES_HPP.read_text(encoding="utf-8")
    bloc = re.search(r"PLAGES_POLICE\[\]\s*=\s*\{(.*?)\n\};", texte, re.DOTALL)
    if bloc is None:
        raise SystemExit(f"PLAGES_POLICE introuvable dans {ALERTES_HPP}")
    return [(int(a, 16), int(b, 16)) for a, b in re.findall(r"\{0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)\}", bloc.group(1))]


PLAGES = plages_police()


def dessinable(c: str) -> bool:
    o = ord(c)
    return o == 10 or any(a <= o <= b for a, b in PLAGES)


def couper(texte, capacite):
    """(texte coupé sur une frontière de caractère à capacite-1 octets, coupé ?) ; une chaîne vide ou absente ne donne rien."""
    if not isinstance(texte, str):
        return "", False
    sortie, n = [], 0
    for c in texte:
        taille = len(c.encode("utf-8", "surrogatepass"))
        if n + taille + 1 > capacite:
            return "".join(sortie), True
        sortie.append(c)
        n += taille
    return "".join(sortie), False


def copier(valeur, capacite):
    """(texte gardé, refusé ?, coupé ?) : refusé = au moins un caractère que la police ne dessine pas."""
    texte, coupe = couper(valeur, capacite)
    if texte == "":
        return "", False, False
    if not all(dessinable(c) for c in texte):
        return "", True, False
    return texte, False, coupe


def instant(v):
    if isinstance(v, bool) or not isinstance(v, int) or v < INSTANT_MIN or v > INSTANT_MAX:
        return -1
    return v


def primaire(etiquette):
    p = re.split(r"[-_]", etiquette, maxsplit=1)[0].lower() if isinstance(etiquette, str) else ""
    return p if 0 < len(p) <= 3 else ""


def rang_langue(etiquette, preferee):
    p = primaire(etiquette)
    return 0 if p == preferee else (1 if p == "en" else 2)


def lire_textes(textes, preferee):
    gardes, rangs = [], []
    for t in textes if isinstance(textes, list) else []:
        if not isinstance(t, dict):
            continue
        titre, r1, c1 = copier(t.get("headline"), CAP["titre"])
        desc, r2, c2 = copier(t.get("description"), CAP["description"])
        cons, r3, c3 = copier(t.get("instruction"), CAP["consigne"])
        if r1 or r2 or r3 or (titre == "" and desc == "" and cons == ""):
            continue
        langue, _, _ = copier(t.get("language"), CAP["langue"])
        # la langue brute sert au rang même si elle n'est pas dessinable (le noyau la lit avant de la contrôler)
        brute, _ = couper(t.get("language"), CAP["langue"])
        rang = rang_langue(brute, preferee)
        texte = {"langue": langue, "titre": titre, "description": desc, "consigne": cons, "tronque": c1 or c2 or c3}
        if len(gardes) < TEXTES_MAX:
            gardes.append(texte)
            rangs.append(rang)
        else:
            pire = 0
            for k in range(1, TEXTES_MAX):
                if rangs[k] >= rangs[pire]:
                    pire = k
            if rang >= rangs[pire]:
                continue
            gardes[pire], rangs[pire] = texte, rang
    return gardes


def analyser_py(corps: bytes, langue: str):
    d = json.loads(corps)
    assert d["version"].startswith("1.")
    sortie: dict = {"serveur": instant(d.get("server_utc")), "alertes": [], "lues": 0, "doublons": 0, "omises": 0, "ignorees": 0}
    attribution, _, _ = copier(d.get("attribution"), CAP["attribution"])
    avertissement, _, _ = copier(d.get("disclaimer"), CAP["avertissement"])
    sortie["attribution"], sortie["avertissement"] = attribution, avertissement
    couverture = 0
    for c in d.get("coverage") or []:
        if c.get("status") == "stale":
            couverture |= 1
        elif c.get("status") == "unavailable":
            couverture |= 2
        nr = c.get("warnings_not_matchable")
        if isinstance(nr, int) and not isinstance(nr, bool) and nr > 0:
            couverture |= 4
    if d.get("uncovered"):
        couverture |= 8
    sortie["couverture"] = couverture
    gardees = sortie["alertes"]
    for v in d["vigilances"]:
        a: dict = {
            "niveau": NIVEAUX.get(v.get("level"), 0),
            "phenomene": PHENOMENES.index(v["phenomenon"]) + 1 if v.get("phenomenon") in PHENOMENES else 0,
            "etat": ETATS.get(v.get("status"), 3 if v.get("message_type") == "Cancel" else 0),
            "envoi": instant(v.get("sent_utc")),
            "effectif": instant(v.get("effective_utc")),
            "debut": instant(v.get("onset_utc")),
            "expire": instant(v.get("expires_utc")),
            "fin": instant(v.get("ends_utc")),
        }
        a["tronque"] = False
        for cle, source, cap in (("pays", "country", 4), ("niveau_original", "level_original", CAP["original"]),
                                 ("phenomene_original", "phenomenon_original", CAP["original"]),
                                 ("evenement", "event", CAP["original"]), ("emetteur", "issuer", CAP["emetteur"]),
                                 ("zone", None, CAP["zone"])):
            valeur = (v.get("area") or {}).get("description") if source is None else v.get(source)
            a[cle], _, coupe = copier(valeur, cap)
            a["tronque"] = a["tronque"] or coupe
        a["attribution"], _, a["attribution_tronquee"] = copier(v.get("attribution"), CAP["attribution"])
        a["textes"] = lire_textes(v.get("texts"), langue)
        if a["emetteur"] == "" and a["textes"]:
            a["emetteur"], _, _ = copier(v["texts"][0].get("sender_name"), CAP["emetteur"])
        a["tronque"] = a["tronque"] or any(t["tronque"] for t in a["textes"])
        a_des_mots = bool(a["textes"]) or a["evenement"] != "" or a["phenomene_original"] != ""
        if a["niveau"] == 0 and a["phenomene"] == 0 and not a_des_mots:
            sortie["ignorees"] += 1
            continue
        sortie["lues"] += 1
        if any(g == a for g in gardees):
            sortie["doublons"] += 1
            continue
        if len(gardees) < ALERTES_MAX:
            gardees.append(a)
            continue
        cle_imp = lambda x: (x["niveau"], -(x["debut"] if x["debut"] >= 0 else 0))  # noqa: E731  (plus grave, puis plus ancienne)
        pire = 0
        for k in range(1, ALERTES_MAX):
            if cle_imp(gardees[pire]) > cle_imp(gardees[k]):
                pire = k
        if cle_imp(a) > cle_imp(gardees[pire]):
            gardees[pire] = a
        sortie["omises"] += 1
    return sortie


def en_vigueur(a, maintenant, age):
    if age < 0 or age > AGE_MAX_S or a["etat"] == 3 or a["niveau"] == 1:
        return False
    debut = a["debut"] if a["debut"] >= 0 else a["effectif"]
    if debut >= 0 and maintenant < debut:
        return False
    fin = a["fin"] if a["fin"] >= 0 else a["expire"]
    if fin >= 0 and (fin + 1) % 3600 == 0:  # « hh:59:59 » : la dernière seconde de l'heure fait encore partie de l'alerte
        fin += 1
    return not (fin >= 0 and maintenant >= fin)


def suffisant(niveau, minimum):
    if minimum == 0:
        return niveau == 0 or niveau >= 2
    return niveau >= (3 if minimum == 1 else 4)


def composer_py(analyse, maintenant, recu, minimum):
    age = maintenant - recu
    retenues = [i for i, a in enumerate(analyse["alertes"]) if suffisant(a["niveau"], minimum) and en_vigueur(a, maintenant, age)]
    # plus grave d'abord, puis début le plus ancien (inconnu = 0), puis ordre de la réponse
    return sorted(retenues, key=lambda i: (-analyse["alertes"][i]["niveau"], max(analyse["alertes"][i]["debut"], 0), i))


def instants_de_controle(analyse):
    points = set()
    for a in analyse["alertes"]:
        for cle in ("debut", "effectif", "expire", "fin"):
            if a[cle] >= 0:
                points.update((a[cle] - 1, a[cle], a[cle] + 1))
    if analyse["serveur"] >= 0:
        points.add(analyse["serveur"])
    return sorted(points)


def cpp_chaine(s: str) -> str:
    assert ')VG"' not in s
    return 'R"VG(' + s + ')VG"'


def inc_reponse(corps: bytes) -> str:
    return "// FICHIER GÉNÉRÉ par test/host/controle_python_vigie.py : ne pas modifier à la main.\n" + cpp_chaine(corps.decode("utf-8")) + "\n"


def inc_attendu(analyse, langue: str) -> str:
    L = ["// FICHIER GÉNÉRÉ par test/host/controle_python_vigie.py (lecture indépendante de la réponse, langue de l'écran « %s ») : ne pas modifier." % langue,
         "{",
         f"    {analyse['serveur']}LL, // server_utc (-1 : inconnu)",
         f"    {len(analyse['alertes'])}, {analyse['lues']}, {analyse['doublons']}, {analyse['omises']}, {analyse['ignorees']}, {analyse['couverture']}, // gardées, lues, doublons, omises, ignorées, couverture (1 périmée, 2 absente, 4 non rattachables, 8 pays sans source)",
         f"    {cpp_chaine(analyse['attribution'])},",
         f"    {cpp_chaine(analyse['avertissement'])},",
         "    {"]
    for a in analyse["alertes"]:
        L.append("        {" + f"{a['niveau']}, {a['phenomene']}, {a['etat']}, {a['envoi']}LL, {a['effectif']}LL, {a['debut']}LL, {a['expire']}LL, {a['fin']}LL, "
                 f"{int(a['tronque'])}, {int(a['attribution_tronquee'])},")
        L.append("         " + ", ".join(cpp_chaine(a[c]) for c in ("pays", "niveau_original", "phenomene_original", "evenement", "emetteur", "zone")) + ",")
        L.append("         " + cpp_chaine(a["attribution"]) + f", {len(a['textes'])},")
        L.append("         {")
        for t in a["textes"]:
            L.append("             {" + ", ".join(cpp_chaine(t[c]) for c in ("langue", "titre", "description", "consigne")) + f", {int(t['tronque'])}" + "},")
        L.append("         }},")
    L.append("    },")
    L.append("    { // instants de contrôle : maintenant, réponse reçue 60 s avant, niveau minimal, indices des alertes montrées (plus grave d'abord)")
    for t in instants_de_controle(analyse):
        for minimum in (0, 1, 2):
            idx = composer_py(analyse, t, t - 60, minimum)
            L.append(f"        {{{t}LL, {minimum}, {len(idx)}, {{{', '.join(map(str, idx))}}}}},")
    L.append("    },")
    L.append("}")
    return "\n".join(L) + "\n"


def gz_decrit(gz: bytes, corps: bytes):
    """(longueur de l'en-tête gzip, taille décompressée) lues avec le module zlib de Python, flux vérifié : le flux DEFLATE qui suit
    l'en-tête redonne exactement le corps, la fin porte son CRC-32 et sa taille. Les drapeaux (nom, commentaire, extra, CRC d'en-tête) sont lus ici."""
    assert gz[:3] == bytes([0x1F, 0x8B, 8])
    drapeaux, p = gz[3], 10
    if drapeaux & 4:
        p += 2 + struct.unpack("<H", gz[p:p + 2])[0]
    for bit in (8, 16):
        if drapeaux & bit:
            p = gz.index(bytes(1), p) + 1
    if drapeaux & 2:
        p += 2
    assert zlib.decompress(gz[p:-8], -15) == corps
    crc, taille = struct.unpack("<II", gz[-8:])
    assert crc == zlib.crc32(corps) and taille == len(corps)
    return p, taille


def inc_gz(gz: bytes) -> str:
    lignes = [", ".join(f"0x{o:02x}" for o in gz[i:i + 16]) for i in range(0, len(gz), 16)]
    entete = "// FICHIER GÉNÉRÉ par test/host/controle_python_vigie.py : octets REÇUS sur le fil (gzip), ne pas modifier.\n"
    return entete + ",\n".join(lignes) + ",\n"


def urls_attendues():
    """[(nom, latitude e5, longitude e5, adresse demandée à la capture)] pour les lieux de capturer_vigie.py ; échoue si l'adresse que le
    script construit n'est pas celle qu'il a enregistrée (l'index dit avec quelle adresse chaque réponse a été reçue)."""
    sys.path.insert(0, str(ICI))
    import capturer_vigie  # noqa: PLC0415  (le script appelle le réseau seulement dans main())

    index = json.loads((DONNEES / "vigie_captures.json").read_text(encoding="utf-8"))
    sortie = []
    for nom, lat, lon, _ in capturer_vigie.LIEUX:
        voulue = capturer_vigie.adresse(lat, lon, "fr")
        assert index[nom]["url"] == voulue, f"{nom}: l'index dit {index[nom]['url']}, le script construit {voulue}"
        sortie.append((nom, lat, lon, voulue))
    return sortie


def inc_urls() -> str:
    lignes = ["// FICHIER GÉNÉRÉ par test/host/controle_python_vigie.py : lieux de capturer_vigie.py, coordonnées e5 et adresse demandée (langue « fr »)."]
    for nom, lat, lon, url in urls_attendues():
        lignes.append("{" + f"{cpp_chaine(nom)}, {lat}, {lon}, {cpp_chaine(url)}" + "},")
    return "\n".join(lignes) + "\n"


def reponses():
    index = json.loads((DONNEES / "vigie_captures.json").read_text(encoding="utf-8"))
    return sorted(index)


def controle_police():
    try:
        from fontTools.ttLib import TTFont  # noqa: PLC0415  (outil de développement, pas toujours installé)
    except ImportError:
        print("  saute   fonttools absent (pip install fonttools) : table de caractères non comparée à la police")
        return 0
    cmap = TTFont(POLICE).getBestCmap() or {}
    manque = [f"U+{c:04X}" for a, b in PLAGES for c in range(a, b + 1) if c not in cmap]
    if manque:
        print(f"  ECHEC   caractères annoncés par vigie_alertes.hpp mais absents de {POLICE.name} : {', '.join(manque[:12])}")
        return 1
    n = sum(b - a + 1 for a, b in PLAGES)
    print(f"  ok      les {n} caractères de la table du noyau sont tous dans {POLICE.name}")
    return 0


def controler(verifier: bool) -> int:
    """Écrit (verifier faux) ou compare (verifier vrai) les fichiers générés ; rend le nombre de contrôles en échec."""
    echecs = 0
    for nom in reponses():
        source = DONNEES / f"vigie_{nom}.json"
        corps = source.read_bytes()
        analyse = analyser_py(corps, "fr")
        voulus = {f"vigie_{nom}.inc": inc_reponse(corps), f"attendu_vigie_{nom}.inc": inc_attendu(analyse, "fr")}
        gz = DONNEES / f"vigie_{nom}.gz"
        if gz.exists():
            octets = gz.read_bytes()
            entete, taille = gz_decrit(octets, corps)
            voulus[f"vigie_{nom}.gz.inc"] = inc_gz(octets)
            voulus[f"attendu_vigie_{nom}.gz.inc"] = (
                "// FICHIER GÉNÉRÉ : longueur de l'en-tête gzip, taille décompressée (zlib)\n" + "{" + f"{entete}, {taille}" + "}\n"
            )
        for fichier, contenu in voulus.items():
            chemin = DONNEES / fichier
            if verifier:
                actuel = chemin.read_text(encoding="utf-8") if chemin.exists() else None
                if actuel != contenu:
                    print(f"  ECHEC   {fichier} n'est pas à jour (relancer test/host/controle_python_vigie.py)")
                    echecs += 1
            else:
                chemin.write_text(contenu, encoding="utf-8", newline="\n")
        if not verifier:
            print(f"  {nom}: {len(corps)} octets, {len(analyse['alertes'])} alerte(s) gardée(s) sur {analyse['lues']} lue(s), "
                  f"{len(instants_de_controle(analyse))} instants de contrôle")
    fichier_urls = DONNEES / "attendu_vigie_urls.inc"
    if verifier:
        if not fichier_urls.exists() or fichier_urls.read_text(encoding="utf-8") != inc_urls():
            print("  ECHEC   attendu_vigie_urls.inc n'est pas à jour")
            echecs += 1
    else:
        fichier_urls.write_text(inc_urls(), encoding="utf-8", newline="\n")
    echecs += controle_police()
    if verifier and echecs == 0:
        print("  ok      tout est à jour")
    return echecs


def controle_vigie() -> int:
    """Pour lancer.py : les fichiers générés sont à jour et la table de caractères du noyau est dans la police. Rend le nombre d'échecs."""
    print("\ncontrole_python_vigie.py :")
    return controler(True)


def main() -> int:
    return 1 if controler("--verifier" in sys.argv) else 0


if __name__ == "__main__":
    sys.exit(main())
