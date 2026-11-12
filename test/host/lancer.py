#!/usr/bin/env python3
"""Lance les tests du noyau pur (components/core/) sur le PC.

    python test/host/lancer.py               # compile (et lance, si possible) chaque test_*.cpp
    python test/host/lancer.py --mutations   # prouve que chaque test échoue sans le code qu'il vérifie
    python test/host/lancer.py --cxx CHEMIN  # force un compilateur
    python test/host/lancer.py --seulement test_api,test_appairage [--mutations]   # un sujet à la fois
    python test/host/lancer.py --jobs 4 [--durees]   # quatre fichiers à la fois (même sortie, même code de retour) ; temps mesurés

--jobs vaut 1 par défaut (le PC d'Axel a déjà planté à six compilations en parallèle) ; la CI en prend quatre, autant que de
cœurs du runner. Il ne change que la vitesse : la sortie reste dans l'ordre alphabétique des fichiers. Sans effet avec --mutations.

Compilateur : --cxx, sinon la variable CXX, sinon g++ / clang++ / c++ du PATH (mode « complet » : compile,
lie, EXÉCUTE). À défaut, le g++ croisé RISC-V d'ESP-IDF (dossier donné par tools/local.ps1 ou MERIDIEN_IDF_RACINE,
voir racine_idf.py ; utilisé en lecture seule, sans projet ESP-IDF ni IDF_PATH) avec -fsyntax-only (mode « syntaxe ») : le programme n'est pas
lancé, MAIS chaque test est aussi évalué à la compilation par un static_assert (voir mini.hpp), donc une
régression fait échouer la compilation. Le mode utilisé est affiché ; aucun test n'est déclaré « exécuté »
quand il ne l'est pas.
"""
import argparse
import collections
import concurrent.futures
import functools
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
TESTS = sorted((RACINE / "test" / "host").glob("test_*.cpp"))
INCLUDE_NOYAU = RACINE / "components" / "core" / "include"
# Parties pures des couches carte (components/bsp_<carte>/include/ : aucun include ESP-IDF, testables sur le PC).
INCLUDES_CARTES = sorted((RACINE / "components").glob("bsp_*/include"))
DRAPEAUX = ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic"]


@functools.lru_cache(maxsize=None)
def drapeaux_constexpr(cxx):
    """Les tests qui relisent une vraie réponse JSON dépassent la limite par défaut de GCC (33 554 432 opérations
    constexpr) : on la relève. Clang l'appelle autrement (-fconstexpr-steps, non essayé ici : pas de clang sur ce PC)."""
    try:
        version = subprocess.run([cxx, "--version"], capture_output=True, text=True, timeout=60).stdout.lower()
    except (OSError, subprocess.SubprocessError):
        version = ""
    return ["-fconstexpr-steps=2000000000"] if "clang" in version else ["-fconstexpr-ops-limit=2000000000"]


def trouver_compilateur(force=None):
    """Retourne (chemin, mode) avec mode = "complet" ou "syntaxe"."""
    if force:
        return force, ("syntaxe" if "riscv32" in force.lower() else "complet")
    if os.environ.get("CXX"):
        return os.environ["CXX"], "complet"
    for nom in ("g++", "clang++", "c++"):
        chemin = shutil.which(nom)
        if chemin:
            return chemin, "complet"
    croise = outil_croise("g++")
    if croise:
        return croise, "syntaxe"
    return None, None


def options_inclusion(include_noyau):
    """Les « -I » communs à la compilation et à la liste des dépendances (dependances_des_tests)."""
    options = ["-I", str(include_noyau), "-I", str(RACINE / "test" / "host")]
    for dossier in INCLUDES_CARTES:
        options += ["-I", str(dossier)]
    return options


def compiler(cxx, mode, source, include_noyau, sortie_exe=None):
    cmd = [cxx, *DRAPEAUX, *drapeaux_constexpr(cxx), *options_inclusion(include_noyau)]
    if mode == "syntaxe":
        cmd += ["-fsyntax-only", str(source)]
    else:
        cmd += [str(source), "-o", str(sortie_exe)]
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")


def traiter_un_test(cxx, mode, t, tmp):
    """Compile puis, en mode complet, exécute UN fichier de test. Retourne (texte à afficher tel quel, nombre d'échecs,
    secondes de compilation, secondes d'exécution). Aucun état partagé : l'exécutable porte le nom du test dans le
    dossier temporaire, donc plusieurs fichiers peuvent être traités en même temps (aucun test ne lit ni n'écrit un
    fichier, un port ou une variable d'environnement)."""
    exe = tmp / (t.stem + (".exe" if os.name == "nt" else ""))
    debut = time.perf_counter()
    r = compiler(cxx, mode, t, INCLUDE_NOYAU, exe)
    compil = time.perf_counter() - debut
    if r.returncode != 0:
        return f"\n{t.name} : ÉCHEC de compilation\n{r.stdout}{r.stderr}\n", 1, compil, 0.0
    if mode == "complet":
        debut = time.perf_counter()
        p = subprocess.run([str(exe)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        return f"\n{t.name} :\n{p.stdout}", int(p.returncode != 0), compil, time.perf_counter() - debut
    if executee_seulement(t.name):
        return f"\n{t.name} : compilation OK, MAIS NON EXÉCUTÉ (aucun static_assert : lancer en mode complet)\n", 0, compil, 0.0
    return f"\n{t.name} : compilation OK (tests évalués par static_assert)\n", 0, compil, 0.0


def afficher_durees(mesures, jobs, reel):
    """Sur la sortie d'erreur (la sortie standard reste identique) : temps cumulé, temps réel, fichiers les plus longs et
    mémoire du plus gros processus fils, pour savoir si l'on peut encore paralléliser."""
    compil = sum(m[1] for m in mesures)
    execution = sum(m[2] for m in mesures)
    print(f"\ndurées : {len(mesures)} fichier(s), compilation {compil:.0f} s + exécution {execution:.0f} s cumulées, "
          f"{reel:.0f} s réelles avec {jobs} processus (gain x{(compil + execution) / max(reel, 0.001):.1f})", file=sys.stderr)
    for nom, c, x in sorted(mesures, key=lambda m: m[1] + m[2], reverse=True)[:8]:
        print(f"  {nom:40} compilation {c:6.1f} s  exécution {x:5.1f} s", file=sys.stderr)
    try:
        import resource  # noqa: PLC0415  (Linux et macOS seulement)

        print(f"  mémoire du plus gros processus fils : {resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss / 1024:.0f} Mo",
              file=sys.stderr)
    except ImportError:
        pass


def controles_croises():
    """Les contrôles Python (sans compilation) qui recoupent le noyau avec main/, web/, boards/ et docs/ : retourne le nombre d'échecs."""
    from controle_i18n import controle_i18n  # noqa: PLC0415  (contrôles croisés de la traduction, sans compilation)

    echecs = controle_i18n()
    from controle_reglages import controle_reglages  # noqa: PLC0415  (contrat de l'API, pages et clavier contre leurs sources)

    echecs += controle_reglages()
    from controle_contrat_web import controle_contrat_web  # noqa: PLC0415  (faux serveur de la page contre le noyau : routes, erreurs, champs)

    echecs += controle_contrat_web()
    from controle_python_vigie import controle_vigie  # noqa: PLC0415  (alertes : fichiers générés à jour, table de caractères dans la police)

    echecs += controle_vigie()

    from controle_cartes import controle_cartes  # noqa: PLC0415  (cartes : partitions, broches du C6, hygiène des fichiers de la 7B)

    echecs += controle_cartes()

    from controle_python_mer import controle_mer  # noqa: PLC0415  (mer : trames enregistrées relues par un lecteur Python indépendant)

    echecs += controle_mer()
    return echecs


def lancer(cxx, mode, jobs=1, durees=False, controles_seuls=False):
    if controles_seuls:
        # Aucun test C++ : la CI n'en demande pas quand rien de ce que les tests lisent n'a changé (voir --concerne).
        # Cette ligne ne contient volontairement pas « mode        : complet », que la CI exige quand elle compile.
        print("mode        : contrôles seuls  (aucun test C++ compilé ni exécuté)")
        TESTS[:] = []
    else:
        print(f"compilateur : {cxx}\nmode        : {mode}" + (
            "  (compilé + exécuté)" if mode == "complet" else "  (static_assert évalués à la compilation ; main() NON exécuté)"))
    echecs = 0
    mesures = []
    drapeaux_constexpr(cxx)  # lu une fois ici, avant de lancer les fils
    debut = time.perf_counter()
    with tempfile.TemporaryDirectory() as tmp, concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        # Les plus gros fichiers partent en premier (ils sont les plus longs à compiler : la file se vide plus régulièrement) ;
        # l'AFFICHAGE, lui, reste dans l'ordre alphabétique, comme avec un seul processus.
        futurs = {t: pool.submit(traiter_un_test, cxx, mode, t, Path(tmp))
                  for t in sorted(TESTS, key=lambda f: f.stat().st_size, reverse=True)}
        for t in TESTS:
            texte, echec, compil, execution = futurs[t].result()
            print(texte, end="")
            echecs += echec
            mesures.append((t.name, compil, execution))
    reel_tests = time.perf_counter() - debut
    debut = time.perf_counter()
    echecs += controles_croises()
    if durees:
        if mesures:
            afficher_durees(mesures, max(1, jobs), reel_tests)
        print(f"  contrôles croisés (Python, sans compilation) : {time.perf_counter() - debut:.1f} s", file=sys.stderr)
    print("\nRÉSULTAT :", "tout passe" if echecs == 0 else f"{echecs} fichier(s) en échec")
    return 1 if echecs else 0


def dependances_des_tests(cxx, jobs=1):
    """(dossiers, fichiers) dont le contenu peut changer le résultat de la compilation d'un test, en chemins relatifs à la
    racine avec « / ». Les fichiers : chaque test et, par le préprocesseur du compilateur (-MM), tous les en-têtes qu'il
    inclut, directement ou non, y compris ceux de main/ (test_horloge inclut ../../main/horloge.cpp). Les dossiers : tous
    ceux des « -I », en entier, parce qu'un fichier AJOUTÉ dans l'un d'eux peut masquer un en-tête trouvé plus loin. Calculé
    à chaque fois : aucune liste écrite à la main qui pourrait rester en arrière quand un test inclut un nouveau fichier."""
    dossiers = {Path(INCLUDE_NOYAU).resolve(), (RACINE / "test" / "host").resolve(), *(d.resolve() for d in INCLUDES_CARTES)}

    def un_test(t):
        cmd = [cxx, "-std=c++17", "-MM", "-MT", "cible", *options_inclusion(INCLUDE_NOYAU), str(t)]
        r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if r.returncode != 0:
            raise RuntimeError(f"{t.name} : le préprocesseur a échoué\n{r.stderr}")
        fichiers = set()
        for mot in r.stdout.replace("\\\n", " ").replace("\\\r\n", " ").split()[1:]:  # le premier mot est « cible: »
            chemin = Path(mot)
            if not chemin.is_file():
                raise RuntimeError(f"{t.name} : « {mot} » n'est pas un fichier (chemin à espaces ?)")
            try:
                fichiers.add(chemin.resolve().relative_to(RACINE).as_posix())
            except ValueError:
                pass  # hors du dépôt : en-tête du système, sans lien avec une modification du dépôt
        return fichiers

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        tous = set().union(*pool.map(un_test, TESTS)) if TESTS else set()
    return sorted(d.relative_to(RACINE).as_posix() + "/" for d in dossiers), sorted(tous)


def concerne(cxx, modifies, jobs=1):
    """Premier fichier modifié que lisent les tests C++ (avec sa raison), ou None. Le message dit toujours ce qui a été regardé."""
    dossiers, fichiers = dependances_des_tests(cxx, jobs)
    exacts = set(fichiers)
    for brut in modifies:
        p = brut.strip().replace("\\", "/")
        if not p:
            continue
        if p in exacts:
            return f"oui : {p} est inclus par un test ({len(fichiers)} fichiers suivis)"
        for d in dossiers:
            if p.startswith(d):
                return f"oui : {p} est dans {d}, dossier d'inclusion des tests"
    return None


# (fichier du noyau, texte exact à remplacer, remplacement, test qui doit le détecter ; None = « n'importe lequel »)
MUTATIONS = [
    # --- météo WMO ---
    ("meteo_wmo.hpp", "constexpr Condition decrire(", "constexpr Condition decrire_absente(", "test_meteo_wmo.cpp", None,
     "la fonction decrire n'existe pas"),
    ("meteo_wmo.hpp", '{"Ciel dégagé", jour', '{"Ciel degage", jour', "test_meteo_wmo.cpp", "libelles_francais_exacts_avec_accents",
     "accents perdus dans un libellé"),
    ("meteo_wmo.hpp", 'jour ? Icone::Soleil : Icone::Lune', 'jour ? Icone::Soleil : Icone::Soleil', "test_meteo_wmo.cpp",
     "seuls_les_codes_0_a_2_changent_entre_jour_et_nuit", "la nuit montre le soleil"),
    ("meteo_wmo.hpp", "case 97:", "case 98:", "test_meteo_wmo.cpp", "tous_les_codes_sont_connus_jour_et_nuit",
     "le code 97 est devenu 98"),
    ("meteo_wmo.hpp", 'Icone::Inconnue, false};', 'Icone::Inconnue, true};', "test_meteo_wmo.cpp",
     "codes_inconnus_donnent_meteo_inconnue", "un code inconnu est déclaré connu"),
    ("meteo_wmo.hpp", '"Orage avec petite grêle", Icone::OrageGrele', '"Orage avec petite grêle", Icone::Orage',
     "test_meteo_wmo.cpp", "regroupements_d_icones", "la grêle prend l'icône d'orage"),
    ("meteo_wmo.hpp", '"Bruine verglaçante légère"', '"Bruine verglaçante très très légère"', "test_meteo_wmo.cpp",
     "libelles_courts_distincts_et_sans_chiffre", "un libellé trop long pour l'écran"),
    # --- fuseaux ---
    ("fuseaux.hpp", "constexpr const Entree *trouver(", "constexpr const Entree *trouver_absente(", "test_fuseaux.cpp", None,
     "la fonction trouver n'existe pas"),
    ("fuseaux.hpp", "if (c < 0) {\n            bas = milieu + 1;\n        } else {\n            haut = milieu;\n        }",
     "if (c > 0) {\n            bas = milieu + 1;\n        } else {\n            haut = milieu;\n        }", "test_fuseaux.cpp",
     "chaque_entree_se_retrouve_par_la_dichotomie", "dichotomie à l'envers"),
    ("fuseaux_donnees.inc", '{"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"}', '{"Europe/Paris", "CET-1"}', "test_fuseaux.cpp",
     "valeurs_connues_lues_dans_posix_tz_db", "Paris sans heure d'été"),
    ("fuseaux_donnees.inc", '{"Africa/Abidjan", "GMT0"}', '{"Zzz/Abidjan", "GMT0"}', "test_fuseaux.cpp",
     "la_table_est_strictement_triee_donc_sans_doublon", "table mal triée"),
    ("fuseaux_donnees.inc", '{"Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4"},  // complément\n', "", "test_fuseaux.cpp",
     "complements_de_la_base_tz", "Europe/Kyiv retiré"),
    ("fuseaux_donnees.inc", '{"Asia/Tokyo", "JST-9"}', '{"Asia/Tokyo", "JST -9"}', "test_fuseaux.cpp",
     "chaque_chaine_posix_est_plausible", "chaîne POSIX avec une espace"),
    ("fuseaux.hpp", "if (c >= 'A' && c <= 'Z')\n        return static_cast<char>(c - 'A' + 'a');", "if (false) return c;", "test_fuseaux.cpp",
     "recherche_par_ville_sans_casse", "la recherche tient compte de la casse"),
    ("fuseaux.hpp", "return c == ' ' ? '_' : c;", "return c;", "test_fuseaux.cpp",
     "recherche_espace_vaut_souligne", "l'espace ne vaut plus « _ »"),
    ("fuseaux.hpp", "return p == std::string_view::npos ? 0 : p + 1;", "return 0;", "test_fuseaux.cpp",
     "recherche_par_ville_sans_casse", "la ville n'est plus reconnue"),
    ("fuseaux.hpp", "if (!detail::commence_par(TABLE[i].iana, saisie) && detail::contient(TABLE[i].iana, saisie)) {",
     "if (detail::contient(TABLE[i].iana, saisie)) {", "test_fuseaux.cpp",
     "recherche_les_debuts_passent_avant_les_sous_chaines", "doublons et ordre cassé dans la recherche"),
    ("fuseaux.hpp", "if (saisie.empty() || resultats == nullptr)\n        return 0;", "if (resultats == nullptr) return 0;", "test_fuseaux.cpp",
     "recherche_vide_ou_sans_resultat_ou_sans_place", "la saisie vide renvoie tout"),
]

# Mutations des pistes « météo et heure » (json_mini, meteo_openmeteo, heure_fr) et « assistant vocal » : un fichier chacune.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from mutations_accueil_auto import MUTATIONS_ACCUEIL_AUTO  # noqa: E402  (mode automatique de l'accueil : choix du profil, raisons, garde-fous)
from mutations_accueil_bande_pluie import MUTATIONS_ACCUEIL_BANDE_PLUIE  # noqa: E402  (la bande de pluie de la ligne centrale : quarts d'heure, fin, repères, hauteur)
from mutations_accueil_cartes import MUTATIONS_ACCUEIL_CARTES  # noqa: E402  (mode automatique : composition de l'écran, carte pluie, bandeau d'alerte agrandi)
from mutations_accueil_epinglage import MUTATIONS_ACCUEIL_EPINGLAGE  # noqa: E402  (réglage « Profil » : Auto ou épinglé, de la page au pilote)
from mutations_accueil_pilote import MUTATIONS_ACCUEIL_PILOTE  # noqa: E402  (mode automatique : mesures, gel sous le doigt, lieu, phrases de la pastille)
from mutations_assistant import MUTATIONS_ASSISTANT  # noqa: E402  (noyau assistant vocal : voir ce fichier)
from mutations_fournisseurs import MUTATIONS_FOURNISSEURS  # noqa: E402  (sources météo : table, liste par pays, lien avec le lecteur)
from mutations_fuseaux import MUTATIONS_FUSEAUX  # noqa: E402  (heure du lieu et lever/coucher du soleil : ADR-0012)
from mutations_fournisseurs_natifs import MUTATIONS_FOURNISSEURS_NATIFS  # noqa: E402  (MET Norway et NWS : lecteurs, dates, rythme, requêtes, soleil)
from mutations_geocodage import MUTATIONS_GEOCODAGE  # noqa: E402  (recherche de lieu : lecture, ville choisie, état)
from mutations_grandeurs import MUTATIONS_GRANDEURS  # noqa: E402  (page « Détails » : les sept grandeurs, l'état manquant, la mise en page)
from mutations_gzip import MUTATIONS_GZIP  # noqa: E402  (enveloppe gzip des réponses HTTP : en-tête, pied, CRC32, Content-Encoding)
from mutations_hote_appareil import MUTATIONS_HOTE_APPAREIL  # noqa: E402  (hôte de l'appareil et lieux de la recherche : voir ce fichier)
from mutations_i18n import MUTATIONS_I18N  # noqa: E402  (traduction en sept langues : voir ce fichier)
from mutations_materiel_tab5 import MUTATIONS_MATERIEL_TAB5  # noqa: E402  (matériel du Tab5 : luminosité, veille, RTC, volume…)
from mutations_meteo_appareil import MUTATIONS_METEO_APPAREIL  # noqa: E402  (météo sur l'appareil : requête, zone, cache)
from mutations_meteo_detail import MUTATIONS_METEO_DETAIL  # noqa: E402  (météo riche -> contrat MeteoDetail)
from mutations_meteo_heure import MUTATIONS_METEO_HEURE  # noqa: E402
from mutations_meteo_riche import MUTATIONS_METEO_RICHE  # noqa: E402  (météo riche : heures, quarts d'heure, pluie proche, cache v2)
from mutations_meteo_tendance import MUTATIONS_METEO_TENDANCE  # noqa: E402  (tendance de pression sur 3 h)
from mutations_nvs_securite import MUTATIONS_NVS_SECURITE  # noqa: E402  (sécurité de la NVS : classement, décision, migration)
from mutations_reglages import MUTATIONS_REGLAGES  # noqa: E402  (page de réglages : JSON, valeurs, appairage, API)
from mutations_wifi import MUTATIONS_WIFI  # noqa: E402  (Wi-Fi : essai avec retour arrière, classement des réseaux)
from mutations_sntp import MUTATIONS_SNTP  # noqa: E402  (demande d'heure SNTP : lecture du paquet, relance après une IP ou un échec)
from mutations_ordonnanceur import MUTATIONS_ORDONNANCEUR  # noqa: E402  (tâche réseau unique : qui passe et quand)
from mutations_vigie import MUTATIONS_VIGIE  # noqa: E402  (alertes officielles de Vigie : requête, planning, gzip, lecture, vue, textes)
from mutations_visuel import MUTATIONS_VISUEL  # noqa: E402  (écran principal : icônes, lune et soleil, échelles)
from mutations_accueil import MUTATIONS_ACCUEIL  # noqa: E402  (accueil : pluie annoncée, courbe monotone, gélules, bandeau, mise en page)
from mutations_reglages_ecran import MUTATIONS_REGLAGES_ECRAN  # noqa: E402  (réglages sur l'écran et installation guidée : voir ce fichier)
from mutations_roue_geometrie import MUTATIONS_ROUE_GEOMETRIE  # noqa: E402  (roue d'appui long : mesures, pivotement, mots, toucher)
from mutations_roue_commande import MUTATIONS_ROUE_COMMANDE  # noqa: E402  (roue d'appui long : écran, demande, dépliement, décision du toucher)
from mutations_roue_entrees import MUTATIONS_ROUE_ENTREES  # noqa: E402  (roue d'appui long : quelles entrées au premier anneau, dans quel ordre)
from mutations_surf import MUTATIONS_SURF  # noqa: E402  (noyau surf : spot, requêtes, lecture, vent, marée, note, créneau)
from mutations_mer import MUTATIONS_MER  # noqa: E402  (noyau mer : CRC, trame SEA/ACT/CRIT, noms du contrat, fraîcheur, courbe de marée)
from mutations_mer_client import MUTATIONS_MER_CLIENT  # noqa: E402  (client de la mer : requête, rythme, registre des spots, réparation de la NVS)
from mutations_mer_textes import MUTATIONS_MER_TEXTES  # noqa: E402  (mots de la page mer : heures, âges, unités, vigilance, bandeau)
from mutations_mer_page import MUTATIONS_MER_PAGE  # noqa: E402  (page mer composée : verdicts montrés, barres, courbe de marée, bandeau)
from mutations_mer_geometrie import MUTATIONS_MER_GEOMETRIE  # noqa: E402  (mise en page de la page mer : zones, échelle, cartes, détail)
from mutations_mer_api import MUTATIONS_MER_API  # noqa: E402  (écriture du registre des spots par la page de réglages : liste, choix, plage de vent, routes)
from mutations_mer_liste import MUTATIONS_MER_LISTE  # noqa: E402  (liste des spots de Vigie lue par l'appareil : lecture, repli des accents, recherche, ajout)
from mutations_mer_reglage import MUTATIONS_MER_REGLAGE  # noqa: E402  (boutons de la plage de vent de la page La mer de l'écran)
from racine_idf import outil_croise  # noqa: E402  (compilateur croisé d'ESP-IDF, dossier propre à chaque PC)

MUTATIONS += MUTATIONS_METEO_HEURE
MUTATIONS += MUTATIONS_METEO_APPAREIL
MUTATIONS += MUTATIONS_FOURNISSEURS
MUTATIONS += MUTATIONS_FOURNISSEURS_NATIFS
MUTATIONS += MUTATIONS_METEO_RICHE
MUTATIONS += MUTATIONS_METEO_DETAIL
MUTATIONS += MUTATIONS_METEO_TENDANCE
MUTATIONS += MUTATIONS_ASSISTANT
MUTATIONS += MUTATIONS_I18N
MUTATIONS += MUTATIONS_REGLAGES
MUTATIONS += MUTATIONS_WIFI
MUTATIONS += MUTATIONS_SNTP
MUTATIONS += MUTATIONS_ORDONNANCEUR
MUTATIONS += MUTATIONS_VISUEL
MUTATIONS += MUTATIONS_ACCUEIL
MUTATIONS += MUTATIONS_GEOCODAGE
MUTATIONS += MUTATIONS_REGLAGES_ECRAN
MUTATIONS += MUTATIONS_ROUE_GEOMETRIE
MUTATIONS += MUTATIONS_ROUE_COMMANDE
MUTATIONS += MUTATIONS_ROUE_ENTREES
MUTATIONS += MUTATIONS_HOTE_APPAREIL
MUTATIONS += MUTATIONS_MATERIEL_TAB5
MUTATIONS += MUTATIONS_VIGIE
MUTATIONS += MUTATIONS_GZIP
MUTATIONS += MUTATIONS_FUSEAUX
MUTATIONS += MUTATIONS_SURF
MUTATIONS += MUTATIONS_MER
MUTATIONS += MUTATIONS_MER_CLIENT
MUTATIONS += MUTATIONS_MER_TEXTES
MUTATIONS += MUTATIONS_MER_PAGE
MUTATIONS += MUTATIONS_MER_GEOMETRIE
MUTATIONS += MUTATIONS_MER_API
MUTATIONS += MUTATIONS_MER_LISTE
MUTATIONS += MUTATIONS_MER_REGLAGE
MUTATIONS += MUTATIONS_ACCUEIL_AUTO
MUTATIONS += MUTATIONS_ACCUEIL_PILOTE
MUTATIONS += MUTATIONS_ACCUEIL_CARTES
MUTATIONS += MUTATIONS_ACCUEIL_BANDE_PLUIE
MUTATIONS += MUTATIONS_ACCUEIL_EPINGLAGE
MUTATIONS += MUTATIONS_NVS_SECURITE
MUTATIONS += MUTATIONS_GRANDEURS


@functools.lru_cache(maxsize=None)
def executee_seulement(test):
    """Vrai si le test n'a pas de static_assert (TESTS_A_LA_COMPILATION) : seul son exécution le fait échouer."""
    return "TESTS_A_LA_COMPILATION(" not in (RACINE / "test" / "host" / test).read_text(encoding="utf-8")


def mutations(cxx, mode):
    """Pour chaque mutation : copie du noyau, une seule modification, le test visé DOIT échouer à la compilation
    et son nom doit figurer dans le message ; la copie non modifiée doit, elle, compiler."""
    mauvais = 0
    with tempfile.TemporaryDirectory() as tmp:
        for t in TESTS:  # base saine : sinon une « détection » pourrait venir d'ailleurs
            r = compiler(cxx, mode, t, INCLUDE_NOYAU, Path(tmp) / (t.stem + ".exe"))
            if r.returncode != 0:
                print(f"BASE NON SAINE : {t.name} ne compile pas sans mutation\n{r.stderr[:800]}")
                return 1
        for i, (fichier, ancien, nouveau, test, attendu, quoi) in enumerate(MUTATIONS, 1):
            copie = Path(tmp) / f"noyau_{i}"
            shutil.copytree(INCLUDE_NOYAU, copie)
            cible = copie / "ecran" / fichier
            texte = cible.read_text(encoding="utf-8")
            if texte.count(ancien) != 1:
                print(f"[{i:2}] MUTATION INAPPLICABLE ({texte.count(ancien)} occurrence(s) de son texte) : {quoi}")
                mauvais += 1
                continue
            cible.write_text(texte.replace(ancien, nouveau), encoding="utf-8", newline="\n")
            r = compiler(cxx, mode, RACINE / "test" / "host" / test, copie, Path(tmp) / f"m{i}.exe")
            sortie = r.stdout + r.stderr
            if r.returncode == 0 and executee_seulement(test):
                # Un test à méthodes virtuelles (l'hôte de l'appareil) n'est pas évaluable à la compilation : la mutation
                # n'est vue qu'en LANÇANT le programme. En mode « syntaxe » rien ne tourne : on le dit, on ne le prétend pas.
                if mode != "complet":
                    print(f"[{i:2}] NON ÉVALUABLE en mode « {mode} » (test exécuté seulement) : {quoi}")
                    mauvais += 1
                    continue
                r = subprocess.run([str(Path(tmp) / f"m{i}.exe")], capture_output=True, text=True, encoding="utf-8",
                                   errors="replace")
                # Seules les lignes « ECHEC » comptent : le nom d'un test qui PASSE figure aussi dans la sortie (« ok ... »).
                echecs_vus = [ligne for ligne in (r.stdout + r.stderr).splitlines() if ligne.lstrip().startswith("ECHEC")]
                sortie = chr(10).join(echecs_vus)
                sortie = sortie or "(aucun test en échec : arrêt anormal du programme)"
            if r.returncode == 0:
                print(f"[{i:2}] NON DÉTECTÉE : {quoi}  ({test})")
                mauvais += 1
            elif attendu and attendu not in sortie:
                print(f"[{i:2}] DÉTECTÉE PAR AUTRE CHOSE (le test {attendu} n'est pas en cause) : {quoi}")
                mauvais += 1
            else:
                print(f"[{i:2}] détectée par {attendu or 'une erreur de compilation'} : {quoi}")
    print(f"\n{len(MUTATIONS)} mutations, {mauvais} problème(s)")
    return 1 if mauvais else 0


def tranche_des_tests(numero, total):
    """Noms des fichiers de test de la tranche `numero` (1..total) : chaque fichier qui porte des mutations va, du plus coûteux
    au moins coûteux, dans la tranche la moins chargée (à égalité, la première). Le coût estimé d'un fichier est son nombre de
    mutations multiplié par sa taille (la compilation du test domine le temps d'une mutation) : une estimation, pas une mesure.
    Calcul déterministe : les N appels rendent une partition des fichiers, donc de toutes les mutations."""
    from mutations_carte_7b import MUTATIONS_CARTE_7B  # noqa: PLC0415

    nombre = collections.Counter(m[3] for m in MUTATIONS)
    if any(t.name == "test_carte_7b.cpp" for t in TESTS):
        nombre["test_carte_7b.cpp"] += len(MUTATIONS_CARTE_7B)
    cout = {nom: n * (RACINE / "test" / "host" / nom).stat().st_size for nom, n in nombre.items()}
    charges = [0] * total
    tranches = [set() for _ in range(total)]
    for nom in sorted(cout, key=lambda k: (-cout[k], k)):
        j = min(range(total), key=lambda k: (charges[k], k))
        charges[j] += cout[nom]
        tranches[j].add(nom)
    return tranches[numero - 1]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mutations", action="store_true", help="prouve que les tests échouent sans le code")
    ap.add_argument("--cxx", help="compilateur à utiliser")
    ap.add_argument("--jobs", "-j", type=int, default=1, help="nombre de fichiers de test compilés et exécutés en même temps "
                    "(1 par défaut ; sans effet avec --mutations)")
    ap.add_argument("--durees", action="store_true", help="écrit sur la sortie d'erreur le temps cumulé, le temps réel, les "
                    "fichiers les plus longs et la mémoire du plus gros fils")
    ap.add_argument("--controles-seuls", action="store_true", help="ne compile aucun test C++ : seulement les contrôles Python "
                    "qui recoupent le noyau avec main/, web/, boards/ et docs/ (la CI s'en contente quand --concerne répond non)")
    ap.add_argument("--concerne", metavar="FICHIER", help="lit la liste des fichiers modifiés (un chemin par ligne, « - » pour "
                    "l'entrée standard) et répond « oui : … » si l'un d'eux peut changer le résultat d'un test C++ (test, "
                    "en-tête inclus, dossier d'inclusion), sinon « non : … » ; tout doute répond oui")
    ap.add_argument("--seulement", help="fichiers de test à traiter, séparés par des virgules (un morceau du nom suffit) : "
                    "ne compile et ne mute que ceux-là (pour travailler sur un sujet sans relancer les 30 minutes de tout)")
    ap.add_argument("--tranche", metavar="I/N", help="avec --mutations : ne garde que la tranche I sur N (de 1 à N). Les "
                    "fichiers de test sont répartis entre les N tranches par un calcul fixe (coût estimé = nombre de mutations x "
                    "taille du fichier de test), aucune liste écrite à la main : les N tranches couvrent toutes les mutations, "
                    "chacune une seule fois, et un nouveau test entre de lui-même dans l'une d'elles")
    a = ap.parse_args()
    if a.seulement:
        morceaux = [m.strip() for m in a.seulement.split(",") if m.strip()]
        TESTS[:] = [t for t in TESTS if any(m in t.name for m in morceaux)]
        MUTATIONS[:] = [m for m in MUTATIONS if any(x in m[3] for x in morceaux)]
        print(f"--seulement : {len(TESTS)} fichier(s) de test, {len(MUTATIONS)} mutation(s) retenus")
    if a.tranche:
        try:
            numero, total = (int(x) for x in a.tranche.split("/"))
        except ValueError:
            numero = total = 0
        if not 1 <= numero <= total:
            print(f"--tranche « {a.tranche} » : attendu I/N avec 1 <= I <= N", file=sys.stderr)
            return 2
        gardes = tranche_des_tests(numero, total)
        TESTS[:] = [t for t in TESTS if t.name in gardes]
        MUTATIONS[:] = [m for m in MUTATIONS if m[3] in gardes]
        print(f"--tranche {numero}/{total} : {len(TESTS)} fichier(s) de test, {len(MUTATIONS)} mutation(s) du noyau retenus"
              + (" (+ les mutations de la couche carte 7B)" if "test_carte_7b.cpp" in gardes else ""))
    cxx, mode = trouver_compilateur(a.cxx)
    if not cxx:
        print("Aucun compilateur C++ trouvé (ni g++, ni clang++, ni le g++ croisé d'ESP-IDF).", file=sys.stderr)
        return 2
    if a.concerne:
        # Tout doute répond « oui » (on compile) : une erreur de lecture, de préprocesseur ou de chemin ne doit jamais sauter les tests.
        try:
            modifies = (sys.stdin.read() if a.concerne == "-" else Path(a.concerne).read_text(encoding="utf-8")).splitlines()
            verdict = concerne(cxx, modifies, a.jobs)
        except (OSError, RuntimeError, subprocess.SubprocessError) as e:
            verdict = f"oui : calcul impossible ({str(e).splitlines()[0] if str(e) else type(e).__name__})"
        print(verdict or "non : aucun fichier modifié n'est lu par un test C++ (ni test, ni en-tête inclus, ni dossier d'inclusion)")
        return 0
    if a.mutations:
        resultat = mutations(cxx, mode)
        if any(t.name == "test_carte_7b.cpp" for t in TESTS):  # mutations de la couche carte 7B (leur propre dossier include/)
            from mutations_carte_7b import mutations_carte_7b  # noqa: PLC0415

            resultat |= mutations_carte_7b(cxx, mode, RACINE, [*DRAPEAUX, *drapeaux_constexpr(cxx)], INCLUDE_NOYAU)
        return resultat
    return lancer(cxx, mode, a.jobs, a.durees, a.controles_seuls)


if __name__ == "__main__":
    sys.exit(main())
