#!/usr/bin/env python3
"""Contrôle avant publication : cherche dans les fichiers ce qui ne doit pas sortir du dépôt privé.

gitleaks cherche des clés et des jetons ; il ne voit ni un mot de passe Wi-Fi, ni une adresse MAC, ni un chemin de PC.
Ce contrôle regarde ces cas-là, dans le texte des fichiers ET dans leurs noms. Il rend deux sortes de trouvailles :

  BLOQUANT : une adresse MAC (hors adresses d'exemple), un chemin `<lecteur>:\\Users\\<nom>`, la valeur d'une variable
             secrète donnée par --env/--variable (jamais affichée : seul le nom de la variable l'est), une chaîne
             donnée par --interdit, et, avec --historique, une identité de commit qui n'est pas une adresse `noreply`.
  À JUGER  : une adresse e-mail, une adresse IP privée (10.x, 172.16-31.x, 192.168.x), un chemin de lecteur Windows
             (`H:\\...`). Listées par valeur, avec le nombre de fichiers : un exemple de test n'est pas une fuite, mais
             c'est à un humain de le dire.

Code de sortie : 0 = rien de bloquant, 1 = au moins une trouvaille bloquante, 2 = mauvais emploi.

    python tools/controle_publication.py                       # fichiers suivis ou ajoutables du dépôt courant
    python tools/controle_publication.py ../instantane         # un dossier quelconque (l'instantané à publier)
    python tools/controle_publication.py --env <.env> --variable MERIDIEN_WIFI_SSID --variable MERIDIEN_WIFI_MOT_DE_PASSE
    python tools/controle_publication.py --interdit 192.168.9.9 --historique
    python tools/controle_publication.py --autotest            # le contrôle se contrôle lui-même sur un dossier fabriqué
"""

import argparse
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

TAILLE_MAX = 5 * 1024 * 1024

MAC = re.compile(r"(?<![0-9A-Fa-f:-])(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}(?![0-9A-Fa-f:-])")
MAC_EXEMPLES = {"aa:bb:cc:dd:ee:ff", "12:34:56:78:9a:bc", "01:23:45:67:89:ab", "de:ad:be:ef:00:01", "00:11:22:33:44:55"}
COURRIEL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}")
IPV4_PRIVEE = re.compile(r"(?<![\d.])(?:10\.\d{1,3}|192\.168|172\.(?:1[6-9]|2\d|3[01]))\.\d{1,3}\.\d{1,3}(?![\d.])")
CHEMIN_UTILISATEUR = re.compile(r"[A-Za-z]:[\\/]+Users[\\/]+[^\\/\s\"'`]+", re.IGNORECASE)
CHEMIN_LECTEUR = re.compile(r"(?<![A-Za-z0-9])([A-Za-z]:\\{1,2}[A-Za-z_][A-Za-z0-9_.-]{2,})")
COURRIELS_ADMIS = ("@users.noreply.github.com", "noreply@anthropic.com", "@example.com", "@example.org", "git@github.com")


@dataclass(frozen=True)
class Trouvaille:
    gravite: str  # "BLOQUANT" ou "A JUGER"
    categorie: str
    fichier: str
    ligne: int
    extrait: str


def mac_est_exemple(mac: str) -> bool:
    """Adresse d'exemple : toutes les paires identiques, `aa:bb:cc:dd:ee:..` ou une des adresses de MAC_EXEMPLES."""
    norme = mac.lower().replace("-", ":")
    paires = norme.split(":")
    return norme in MAC_EXEMPLES or len(set(paires)) == 1 or norme.startswith("aa:bb:cc:dd:ee:")


def analyser_texte(nom: str, texte: str, interdits: list[str], secrets: list[tuple[str, str]]) -> list[Trouvaille]:
    """Trouvaille de chaque ligne du texte ; `nom` est le chemin relatif du fichier (aussi regardé)."""
    trouvailles: list[Trouvaille] = []
    ensemble = [(0, nom)] + list(enumerate(texte.splitlines(), 1))
    for numero, ligne in ensemble:
        bas = ligne.lower()
        for mac in MAC.findall(ligne):
            if not mac_est_exemple(mac):
                trouvailles.append(Trouvaille("BLOQUANT", "adresse MAC", nom, numero, mac))
        for chemin in CHEMIN_UTILISATEUR.findall(ligne):
            trouvailles.append(Trouvaille("BLOQUANT", "chemin d'utilisateur", nom, numero, chemin))
        for interdit in interdits:
            if interdit.lower() in bas:
                trouvailles.append(Trouvaille("BLOQUANT", "chaîne interdite", nom, numero, interdit))
        for variable, valeur in secrets:
            if valeur in ligne:
                trouvailles.append(Trouvaille("BLOQUANT", "valeur secrète", nom, numero, f"valeur de {variable}"))
        for courriel in COURRIEL.findall(ligne):
            if not courriel.lower().endswith(COURRIELS_ADMIS):
                trouvailles.append(Trouvaille("A JUGER", "adresse e-mail", nom, numero, courriel))
        for adresse in IPV4_PRIVEE.finditer(ligne):
            trouvailles.append(Trouvaille("A JUGER", "adresse IP privée", nom, numero, adresse.group(0)))
        for lecteur in CHEMIN_LECTEUR.findall(ligne):
            trouvailles.append(Trouvaille("A JUGER", "chemin de lecteur Windows", nom, numero, lecteur))
    return trouvailles


def lister_fichiers(dossier: Path | None) -> tuple[Path, list[Path]]:
    if dossier is None:
        racine = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"], capture_output=True, text=True, check=True).stdout.strip())
        brut = subprocess.run(
            ["git", "-C", str(racine), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
            capture_output=True,
            check=True,
        ).stdout
        noms = [n for n in brut.decode("utf-8").split("\0") if n]
        return racine, [racine / n for n in noms if (racine / n).is_file()]
    fichiers = [p for p in dossier.rglob("*") if p.is_file() and ".git" not in p.relative_to(dossier).parts]
    return dossier, fichiers


LONGUEUR_MIN_SECRET = 8


def lire_variables(fichier: Path, noms: list[str]) -> tuple[list[tuple[str, str]], list[str], list[str]]:
    """Valeurs des variables demandées dans un `.env` (NOM=valeur) : (cherchées, noms absents, noms trop courts).

    Une valeur de moins de 8 caractères (un nom de réseau Wi-Fi, par exemple) est presque toujours un mot courant :
    la chercher noierait le résultat sous les faux positifs, elle est donc écartée (et signalée par son nom).
    """
    valeurs: dict[str, str] = {}
    for ligne in fichier.read_text(encoding="utf-8", errors="replace").splitlines():
        brut = ligne.strip()
        if not brut or brut.startswith("#") or "=" not in brut:
            continue
        cle, _, valeur = brut.partition("=")
        valeurs[cle.strip().removeprefix("export ").strip()] = valeur.strip().strip("\"'")
    absentes = [n for n in noms if n not in valeurs or not valeurs[n]]
    courtes = [n for n in noms if n in valeurs and 0 < len(valeurs[n]) < LONGUEUR_MIN_SECRET]
    cherchees = [(n, valeurs[n]) for n in noms if len(valeurs.get(n, "")) >= LONGUEUR_MIN_SECRET]
    return cherchees, absentes, courtes


def controler(racine: Path, fichiers: list[Path], interdits: list[str], secrets: list[tuple[str, str]]) -> tuple[list[Trouvaille], int]:
    trouvailles: list[Trouvaille] = []
    ignores = 0
    for chemin in fichiers:
        nom = chemin.relative_to(racine).as_posix()
        donnees = chemin.read_bytes()
        if len(donnees) > TAILLE_MAX or b"\0" in donnees[:8192]:
            ignores += 1
            trouvailles += analyser_texte(nom, "", interdits, secrets)  # le nom du fichier est regardé quand même
            continue
        trouvailles += analyser_texte(nom, donnees.decode("utf-8", errors="replace"), interdits, secrets)
    return trouvailles, ignores


def identites_de_commits(racine: Path) -> list[Trouvaille]:
    sortie = subprocess.run(
        ["git", "-C", str(racine), "log", "--all", "--format=%ae%n%ce"], capture_output=True, text=True, check=True
    ).stdout
    courriels = sorted({c.strip() for c in sortie.splitlines() if c.strip()})
    return [
        Trouvaille("BLOQUANT", "identité de commit", "(historique git)", 0, c)
        for c in courriels
        if not c.lower().endswith(COURRIELS_ADMIS)
    ]


def afficher(trouvailles: list[Trouvaille]) -> int:
    bloquants = [t for t in trouvailles if t.gravite == "BLOQUANT"]
    for t in bloquants:
        lieu = t.fichier if t.ligne == 0 else f"{t.fichier}:{t.ligne}"
        print(f"BLOQUANT  {t.categorie}  {lieu}  {t.extrait}")
    a_juger: dict[tuple[str, str], set[str]] = defaultdict(set)
    for t in trouvailles:
        if t.gravite == "A JUGER":
            a_juger[(t.categorie, t.extrait)].add(t.fichier)
    for (categorie, extrait), fichiers in sorted(a_juger.items()):
        print(f"A JUGER   {categorie}  {extrait}  ({len(fichiers)} fichier(s), ex. {sorted(fichiers)[0]})")
    print(f"--- {len(bloquants)} trouvaille(s) bloquante(s), {len(a_juger)} valeur(s) à juger")
    return 1 if bloquants else 0


def autotest() -> int:
    """Fabrique un dossier avec chaque cas, vérifie que le contrôle les voit et laisse passer ce qui est permis."""
    with tempfile.TemporaryDirectory() as tmp:
        dossier = Path(tmp) / "arbre"
        dossier.mkdir()
        env = Path(tmp) / "env_essai"
        env.write_text("MON_SSID=Maison\nexport MON_MDP='MonReseauSecret'\n# commentaire\n", encoding="utf-8")
        cherchees, absentes, courtes = lire_variables(env, ["MON_SSID", "MON_MDP", "ABSENTE"])
        (dossier / "propre.md").write_text("Rien ici. 12:34:56 est une heure. aa:bb:cc:dd:ee:ff est un exemple.\n", encoding="utf-8")
        # Les cas sont assemblés en morceaux : le source de ce contrôle ne doit pas contenir ce qu'il cherche.
        (dossier / "mac.md").write_text("carte " + "e8:f6:0a:" + "12:34:56 chez moi\n", encoding="utf-8")
        (dossier / "chemin.md").write_text("voir C:\\" + "Users\\dupont\\projet\n", encoding="utf-8")
        (dossier / "secret.txt").write_text("mot_de_passe=MonReseauSecret\n", encoding="utf-8")
        (dossier / "juger.md").write_text("ip 192.168.1.20, courriel moi@exemple.fr, H:\\Outils\\clang\n", encoding="utf-8")
        (dossier / "noreply.md").write_text("12345+nom@users.noreply.github.com\n", encoding="utf-8")
        (dossier / "interdit.md").write_text("adresse 10.9.8.7 ici\n", encoding="utf-8")
        (dossier / "binaire.bin").write_bytes(b"\0\x01e8:f6:0a:" + b"12:34:56")
        racine, fichiers = dossier, lister_fichiers(dossier)[1]
        trouvailles, ignores = controler(racine, fichiers, ["10.9.8.7"], [(n, v) for n, v in cherchees if n == "MON_MDP"])
        par_fichier: dict[str, set[str]] = defaultdict(set)
        for t in trouvailles:
            par_fichier[t.fichier].add(f"{t.gravite}/{t.categorie}")
        attendu = {
            "mac.md": {"BLOQUANT/adresse MAC"},
            "chemin.md": {"BLOQUANT/chemin d'utilisateur", "A JUGER/chemin de lecteur Windows"},
            "secret.txt": {"BLOQUANT/valeur secrète"},
            "interdit.md": {"BLOQUANT/chaîne interdite", "A JUGER/adresse IP privée"},
            "juger.md": {"A JUGER/adresse IP privée", "A JUGER/adresse e-mail", "A JUGER/chemin de lecteur Windows"},
        }
        erreurs = [f"{n}: vu {sorted(par_fichier.get(n, set()))}, attendu {sorted(a)}" for n, a in attendu.items() if par_fichier.get(n, set()) != a]
        erreurs += [f"{n}: ne devait rien donner, a donné {sorted(par_fichier[n])}" for n in ("propre.md", "noreply.md", "binaire.bin") if n in par_fichier]
        if ignores != 1:
            erreurs.append(f"fichiers binaires ignorés : {ignores}, attendu 1")
        if ([n for n, _ in cherchees], absentes, courtes) != (["MON_MDP"], ["ABSENTE"], ["MON_SSID"]):
            erreurs.append(f"lecture du .env : cherchées {[n for n, _ in cherchees]}, absentes {absentes}, courtes {courtes}")
        if any("MonReseauSecret" in t.extrait for t in trouvailles):
            erreurs.append("la valeur du secret a été affichée")
    for erreur in erreurs:
        print("AUTOTEST ÉCHEC :", erreur)
    print("autotest : " + ("échec" if erreurs else "ok (8 fichiers fabriqués, chaque cas vu, rien d'inventé)"))
    return 1 if erreurs else 0


def main() -> int:
    analyse = argparse.ArgumentParser(description="Contrôle avant publication (voir l'en-tête du fichier).")
    analyse.add_argument("dossier", nargs="?", type=Path, help="dossier à contrôler (défaut : fichiers du dépôt courant)")
    analyse.add_argument("--env", type=Path, help="fichier .env où lire les valeurs secrètes à chercher")
    analyse.add_argument("--variable", action="append", default=[], help="nom d'une variable de --env (répétable)")
    analyse.add_argument("--interdit", action="append", default=[], help="chaîne qui ne doit apparaître nulle part (répétable)")
    analyse.add_argument("--historique", action="store_true", help="contrôler aussi les e-mails des commits (dépôt git)")
    analyse.add_argument("--autotest", action="store_true", help="contrôler le contrôle sur un dossier fabriqué")
    args = analyse.parse_args()
    if args.autotest:
        return autotest()
    if bool(args.env) != bool(args.variable):
        print("--env et --variable vont ensemble", file=sys.stderr)
        return 2
    secrets: list[tuple[str, str]] = []
    if args.env:
        secrets, absentes, courtes = lire_variables(args.env, args.variable)
        if absentes:
            print(f"variable(s) absente(s) ou vide(s) dans {args.env.name} : {', '.join(absentes)}", file=sys.stderr)
            return 2
        if courtes:
            print(f"écartée(s), moins de {LONGUEUR_MIN_SECRET} caractères : {', '.join(courtes)}", file=sys.stderr)
    racine, fichiers = lister_fichiers(args.dossier)
    trouvailles, ignores = controler(racine, fichiers, args.interdit, secrets)
    if args.historique:
        trouvailles += identites_de_commits(racine)
    print(f"{len(fichiers)} fichier(s) regardé(s) dans {racine.name}, dont {ignores} binaire(s) ou trop gros (nom seulement)")
    return afficher(trouvailles)


if __name__ == "__main__":
    sys.exit(main())
