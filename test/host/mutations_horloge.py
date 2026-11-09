r"""Mutations de main/horloge.cpp (hors du noyau, donc hors de lancer.py --mutations) contre test_horloge.cpp.

    . <outils>\env_hote.ps1 -Sanitizers      # PowerShell : compilateur hôte avec ASan/UBSan
    python test/host/mutations_horloge.py     # racine du dépôt en argument facultatif

Chaque mutation est appliquée à une COPIE du dépôt (rien n'est modifié dans l'arbre), le test est compilé avec ASan/UBSan puis lancé : il doit
échouer (code de retour non nul : une ligne ECHEC ou un arrêt d'ASan). La copie non modifiée doit réussir.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

racine = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
sys.path.insert(0, str(racine / "test" / "host"))
import lancer  # noqa: E402

PARIS = 'fuseaux::posix_pour("Europe/Paris")'
MUTATIONS = [
    ("le fuseau est gardé après le premier appel (static)",
     "    analyse = heure::analyser_tz(fuseaux::posix_pour(std::string_view(zone.fuseau, n)));\n    return analyse.ok();",
     "    static heure::AnalyseTz premiere;\n    static bool vue = false;\n    if (!vue) {\n        premiere = heure::analyser_tz(fuseaux::posix_pour(std::string_view(zone.fuseau, n)));\n        vue = premiere.ok();\n    }\n    analyse = premiere;\n    return vue;"),
    ("le fuseau est celui de Paris, écrit en dur",
     "fuseaux::posix_pour(std::string_view(zone.fuseau, n))", PARIS),
    ("un nom de fuseau inconnu retombe sur Paris",
     "    return analyse.ok();\n}\n\n} // namespace",
     "    if (!analyse.ok()) {\n        analyse = heure::analyser_tz(" + PARIS + ");\n    }\n    return analyse.ok();\n}\n\n} // namespace"),
    ("le retour de zone_lire est ignoré",
     "    if (!zone_lire(&zone)) {\n        return false;\n    }", "    zone_lire(&zone);"),
    ("la longueur du nom n'est pas bornée au champ",
     "while (n < sizeof zone.fuseau && zone.fuseau[n] != '\\0') {", "while (zone.fuseau[n] != '\\0') {"),
    ("le tampon de hh:mm a un octet de moins",
     "if (capacite < 6 || !fuseau_du_lieu(analyse)) {", "if (capacite < 5 || !fuseau_du_lieu(analyse)) {"),
    ("les dizaines de minutes sont les unités",
     "sortie[3] = static_cast<char>('0' + locale.minute / 10);", "sortie[3] = static_cast<char>('0' + locale.minute % 10);"),
    ("le décalage rendu ignore l'heure d'été",
     "    return heure::en_heure_locale(instant_utc_s, analyse.fuseau).decalage_s;",
     "    (void)instant_utc_s;\n    return analyse.fuseau.decalage_std_s;"),
    ("la date longue est écrite sans l'année",
     "heure::formater_date_longue(locale, sortie.date, sizeof(sortie.date), true)", "heure::formater_date_longue(locale, sortie.date, sizeof(sortie.date), false)"),
    ("la date localisée est toujours en anglais",
     "    return visuel::formater_date(langue, locale, sortie, capacite, false, true) > 0;",
     "    (void)langue;\n    return visuel::formater_date(i18n::Langue::En, locale, sortie, capacite, false, true) > 0;"),
    ("la date de l'écran principal garde l'année",
     "    return visuel::formater_date(langue, locale, sortie, capacite, false, true) > 0;",
     "    return visuel::formater_date(langue, locale, sortie, capacite, true, true) > 0;"),
    ("la date de l'écran principal perd ses majuscules",
     "    return visuel::formater_date(langue, locale, sortie, capacite, false, true) > 0;",
     "    return visuel::formater_date(langue, locale, sortie, capacite, false, false) > 0;"),
    ("l'heure locale est lue une heure plus tard",
     "    sortie = heure::en_heure_locale(instant_utc_s, analyse.fuseau);\n    return true;", "    sortie = heure::en_heure_locale(instant_utc_s + 3600, analyse.fuseau);\n    return true;"),
    ("horloge_fuseau rend un fuseau vide",
     "    sortie = analyse.fuseau;\n    return true;", "    sortie = heure::Fuseau{};\n    return true;"),
    ("horloge_formater ne vide pas ses textes en cas d'échec",
     "    sortie.heure[0] = '\\0';\n    sortie.date[0] = '\\0';\n    heure::AnalyseTz analyse;", "    heure::AnalyseTz analyse;"),
    ("horloge_hhmm ne vide pas son tampon en cas d'échec",
     "    sortie[0] = '\\0';\n    heure::AnalyseTz analyse;\n    if (capacite < 6", "    heure::AnalyseTz analyse;\n    if (capacite < 6"),
]

cxx, mode = lancer.trouver_compilateur(None)
source = racine / "main" / "horloge.cpp"
original = source.read_text(encoding="utf-8")


def construire_et_lancer(mutation_texte, nom):
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "main").mkdir()
        for f in (racine / "main").glob("horloge.h"):
            shutil.copy(f, tmp / "main" / f.name)
        shutil.copy(racine / "main" / "meteo_zone.h", tmp / "main" / "meteo_zone.h")
        (tmp / "main" / "horloge.cpp").write_text(mutation_texte, encoding="utf-8", newline="\n")
        hote = tmp / "test" / "host"
        (hote / "donnees").mkdir(parents=True)
        shutil.copy(racine / "test" / "host" / "test_horloge.cpp", hote / "test_horloge.cpp")
        shutil.copy(racine / "test" / "host" / "mini.hpp", hote / "mini.hpp")
        shutil.copy(racine / "test" / "host" / "donnees" / "attendu_heure_lieu.inc", hote / "donnees" / "attendu_heure_lieu.inc")
        exe = tmp / "t.exe"
        r = lancer.compiler(cxx, mode, hote / "test_horloge.cpp", lancer.INCLUDE_NOYAU, exe)
        if r.returncode != 0:
            return "COMPILATION", (r.stdout + r.stderr)[:600]
        p = subprocess.run([str(exe)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        sortie = p.stdout + p.stderr
        echecs = [ligne.strip() for ligne in sortie.splitlines() if ligne.lstrip().startswith("ECHEC")]
        return p.returncode, "\n".join(echecs) or sortie[-300:]


code, detail = construire_et_lancer(original, "base")
print("base sans mutation : code", code, "(0 attendu)")
if code != 0:
    print(detail)
    sys.exit(1)
mauvais = 0
for i, (quoi, ancien, nouveau) in enumerate(MUTATIONS, 1):
    if original.count(ancien) != 1:
        print(f"[{i:2}] INAPPLICABLE ({original.count(ancien)} occurrence(s)) : {quoi}")
        mauvais += 1
        continue
    code, detail = construire_et_lancer(original.replace(ancien, nouveau), quoi)
    if code == "COMPILATION":
        print(f"[{i:2}] NE COMPILE PAS (ce n'est pas une détection par le test) : {quoi}")
        print(detail)
        mauvais += 1
    elif code == 0:
        print(f"[{i:2}] NON DÉTECTÉE : {quoi}")
        mauvais += 1
    else:
        premiere = detail.splitlines()[0][:110] if detail else ""
        print(f"[{i:2}] détectée (code {code}) : {quoi} -- {premiere}")
print(f"{len(MUTATIONS)} mutations de horloge.cpp, {mauvais} problème(s)")
sys.exit(1 if mauvais else 0)
