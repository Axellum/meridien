#!/usr/bin/env python3
"""Contrôle avant publication : cherche dans les fichiers (et, au choix, dans tout l'historique) ce qui ne doit pas sortir.

gitleaks cherche des clés et des jetons ; il ne voit ni un mot de passe Wi-Fi, ni une adresse MAC, ni un chemin de PC.
Ce contrôle regarde ces cas-là, dans le texte des fichiers ET dans leurs noms. Il rend deux sortes de trouvailles :

  BLOQUANT : une adresse MAC (hors adresses d'exemple), un chemin `<lecteur>:\\Users\\<nom>`, la valeur d'une variable
             secrète donnée par --env/--variable (jamais affichée : seul le nom de la variable l'est), une chaîne
             donnée par --interdit.
  À JUGER  : une adresse e-mail, une adresse IP privée (10.x, 172.16-31.x, 192.168.x), un chemin de lecteur Windows
             (`H:\\...`). Listées par valeur, avec le nombre de lieux : un exemple de test n'est pas une fuite, mais
             c'est à un humain de le dire.

Avec --historique, il lit en plus le texte ajouté ou retiré par CHAQUE commit de toutes les références
(`git log --all -p`) avec les mêmes règles, et liste les identités (e-mails) des commits. Dans l'historique, qu'on ne
peut plus changer sans réécrire tous les numéros de commit, seule la valeur d'une variable secrète est BLOQUANTE ; le
reste (adresses MAC, chemins, IP, chaînes interdites) est « à juger » : c'est au propriétaire d'accepter ce qui y reste.
Il ne lit ni les images, ni les noms des fichiers de l'historique, ni les commits de fusion (leur contenu est dans leurs
parents).

Code de sortie : 0 = rien de bloquant, 1 = au moins une trouvaille bloquante, 2 = mauvais emploi.

    python tools/controle_publication.py                       # fichiers suivis ou ajoutables du dépôt courant
    python tools/controle_publication.py ../copie              # un dossier quelconque
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
    """Chaque e-mail d'auteur ou de validateur de l'historique, « à juger » (une `noreply` de GitHub est admise)."""
    sortie = subprocess.run(
        ["git", "-C", str(racine), "log", "--all", "--format=%ae%n%ce"], capture_output=True, text=True, check=True
    ).stdout
    courriels = sorted({c.strip() for c in sortie.splitlines() if c.strip()})
    return [
        Trouvaille("A JUGER", "identité de commit", "(historique git)", 0, c)
        for c in courriels
        if not c.lower().endswith(COURRIELS_ADMIS)
    ]


def contenu_de_l_historique(racine: Path, interdits: list[str], secrets: list[tuple[str, str]]) -> tuple[list[Trouvaille], int]:
    """Les mêmes règles que pour les fichiers, sur le texte ajouté ou retiré par chaque commit : (trouvailles, commits lus).

    Le « lieu » d'une trouvaille est `commit <numéro court>`. Seule une valeur secrète reste BLOQUANTE (voir l'en-tête).
    Tout l'historique est lu en mémoire d'un coup : voulu pour un dépôt de cette taille (quelques dizaines de Mo de texte).
    """
    sortie = subprocess.run(
        ["git", "-C", str(racine), "log", "--all", "-p", "--no-color", "--no-ext-diff", "--format=\x01%h"],
        capture_output=True,
        check=True,
    ).stdout.decode("utf-8", errors="replace")
    entetes = ("--- a/", "--- /dev/null", "+++ b/", "+++ /dev/null")
    trouvailles: list[Trouvaille] = []
    blocs = sortie.split("\x01")[1:]
    for bloc in blocs:
        numero, _, corps = bloc.partition("\n")
        lignes = [ligne[1:] for ligne in corps.split("\n") if ligne[:1] in ("+", "-") and not ligne.startswith(entetes)]
        for t in analyser_texte(f"commit {numero}", "\n".join(lignes), interdits, secrets):
            gravite = "BLOQUANT" if t.categorie == "valeur secrète" else "A JUGER"
            trouvailles.append(Trouvaille(gravite, t.categorie, t.fichier, 0, t.extrait))
    return trouvailles, len(blocs)


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
        print(f"A JUGER   {categorie}  {extrait}  ({len(fichiers)} lieu(x), ex. {sorted(fichiers)[0]})")
    print(f"--- {len(bloquants)} trouvaille(s) bloquante(s), {len(a_juger)} valeur(s) à juger")
    return 1 if bloquants else 0


def autotest_historique(tmp: Path, secrets: list[tuple[str, str]]) -> list[str]:
    """Un dépôt git de deux commits : le premier ajoute une MAC et un secret, le second les retire.

    Le contrôle des fichiers (arbre du dernier commit) ne doit rien voir ; celui de l'historique doit voir la MAC (« à
    juger »), le secret (BLOQUANT, sans l'afficher) et l'identité des commits (« à juger »).
    """
    depot = tmp / "depot_historique"
    depot.mkdir()

    def git(*arguments: str) -> None:
        reglages = ["-c", f"core.hooksPath={tmp / 'sans_crochets'}", "-c", "commit.gpgsign=false"]
        reglages += ["-c", "user.name=Essai", "-c", "user.email=essai@exemple.fr"]
        subprocess.run(["git", "-C", str(depot), *reglages, *arguments], capture_output=True, check=True)

    git("init", "-q")
    fichier = depot / "carte.txt"
    fichier.write_text("carte " + "e8:f6:0a:" + "12:34:56\nmot_de_passe=MonReseauSecret\n", encoding="utf-8")
    git("add", "carte.txt")
    git("commit", "-q", "-m", "un")
    fichier.write_text("propre\n", encoding="utf-8")
    git("add", "carte.txt")
    git("commit", "-q", "-m", "deux")

    erreurs: list[str] = []
    fichiers = lister_fichiers(depot)[1]
    de_l_arbre, _ = controler(depot, fichiers, [], secrets)
    if de_l_arbre:
        erreurs.append(f"historique : l'arbre du dernier commit devait être propre, a donné {[t.categorie for t in de_l_arbre]}")
    de_l_historique, commits = contenu_de_l_historique(depot, [], secrets)
    vu = {(t.gravite, t.categorie) for t in de_l_historique}
    if vu != {("A JUGER", "adresse MAC"), ("BLOQUANT", "valeur secrète")}:
        erreurs.append(f"historique : vu {sorted(vu)}, attendu la MAC (à juger) et le secret (bloquant)")
    if commits != 2:
        erreurs.append(f"historique : {commits} commit(s) lu(s), attendu 2")
    if any("MonReseauSecret" in t.extrait for t in de_l_historique):
        erreurs.append("historique : la valeur du secret a été affichée")
    identites = [(t.gravite, t.extrait) for t in identites_de_commits(depot)]
    if identites != [("A JUGER", "essai@exemple.fr")]:
        erreurs.append(f"historique : identités vues {identites}, attendu essai@exemple.fr (à juger)")
    return erreurs


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
        erreurs += autotest_historique(Path(tmp), [(n, v) for n, v in cherchees if n == "MON_MDP"])
    for erreur in erreurs:
        print("AUTOTEST ÉCHEC :", erreur)
    print("autotest : " + ("échec" if erreurs else "ok (8 fichiers et un dépôt de 2 commits fabriqués, chaque cas vu, rien d'inventé)"))
    return 1 if erreurs else 0


def main() -> int:
    analyse = argparse.ArgumentParser(description="Contrôle avant publication (voir l'en-tête du fichier).")
    analyse.add_argument("dossier", nargs="?", type=Path, help="dossier à contrôler (défaut : fichiers du dépôt courant)")
    analyse.add_argument("--env", type=Path, help="fichier .env où lire les valeurs secrètes à chercher")
    analyse.add_argument("--variable", action="append", default=[], help="nom d'une variable de --env (répétable)")
    analyse.add_argument("--interdit", action="append", default=[], help="chaîne qui ne doit apparaître nulle part (répétable)")
    analyse.add_argument("--historique", action="store_true", help="lire aussi tout l'historique git et lister les identités de commit")
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
    print(f"{len(fichiers)} fichier(s) regardé(s) dans {racine.name}, dont {ignores} binaire(s) ou trop gros (nom seulement)")
    if args.historique:
        de_l_historique, commits = contenu_de_l_historique(racine, args.interdit, secrets)
        trouvailles += de_l_historique + identites_de_commits(racine)
        print(f"historique : {commits} commit(s) lu(s), toutes références (images et noms de fichiers non lus)")
    return afficher(trouvailles)


if __name__ == "__main__":
    sys.exit(main())
