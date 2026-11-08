#!/usr/bin/env python3
"""Capture des VRAIES trames de Vigie avec le bloc mer (SEA, ACT, CRIT) pour les tests du noyau « mer » (mer_trame.hpp, mer_vue.hpp).

    python test/host/capturer_mer.py                          # capture tous les spots de SPOTS dans donnees/
    python test/host/capturer_mer.py --seulement hossegor,la_torche
    python test/host/capturer_mer.py --a-cote CHEMIN          # écrit ailleurs (pour comparer sans toucher aux fixtures)

Une trame = le corps de `GET /v1/embedded/frame?device_type=generic&spot=<id>` tel que l'API l'a rendu, octet pour octet (le contrat est
`docs/embedded-frame.md` de vigie-api). L'adresse publique est LUE dans components/core/include/ecran/vigie_requete.hpp (ADRESSE_PUBLIQUE),
jamais recopiée ici. Pas de position GPS dans la requête : les zones et les avertissements du monde que l'API rend sans elle font partie de
la trame, et le décodeur de la mer doit les sauter.

Pour chaque spot : `mer_<nom>.txt` = la trame, et une ligne dans `mer_captures.json` (adresse, instant de la trame, taille, nombre de
lignes par balise). Ce script appelle le réseau : à lancer à la main, pas dans le contrôle. Une trame n'est valable qu'à son instant : les
tests prennent l'instant de sa ligne `SEA` (« généré le ») pour « maintenant ». L'instance publique est limitée par adresse et « pas faite
pour bâtir un service dessus » : une demi-douzaine d'appels espacés de deux secondes.
"""
import argparse
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[1]
DONNEES = ICI / "donnees"
REQUETE_HPP = RACINE / "components/core/include/ecran/vigie_requete.hpp"

# nom du fichier, identifiant du spot dans le registre de Vigie, ce que ce spot apporte aux tests
SPOTS = [
    ("hossegor", "soorts-hossegor-plage-sud-point-d-or", "Landes : côte atlantique, marée de 4 m, le spot du contrat"),
    ("la_torche", "beuzec-plage-de-la-torche", "Bretagne ouest : marnage plus grand, coefficients"),
    ("rochebonne", "saint-malo-plage-de-rochebonne", "Manche : très grand marnage (hauteurs de marée hors de l'ordinaire)"),
    ("port_vieux", "biarritz-plage-du-port-vieux", "Pays basque : plage abritée, verdicts différents d'un spot ouvert"),
    ("govelle", "le-pouliguen-plage-de-la-govelle", "Loire-Atlantique : plage orientée au sud, houle d'ouest filtrée"),
    ("ondarraitz", "hendaye-ondarraitz", "Frontière espagnole : dernier spot de la liste, marée des Pyrénées-Atlantiques"),
]


def morceaux(source: str, nom: str) -> str:
    """Valeur d'une constante `std::string_view NOM = "…";` : les littéraux voisins sont recollés."""
    m = re.search(r"std::string_view\s+" + nom + r"\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", source)
    if not m:
        raise SystemExit(f"{nom} introuvable dans {REQUETE_HPP}")
    return "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))


def adresse(spot: str) -> str:
    base = morceaux(REQUETE_HPP.read_text(encoding="utf-8"), "ADRESSE_PUBLIQUE")
    return f"{base}/v1/embedded/frame?device_type=generic&spot={spot}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seulement", help="noms de spots séparés par des virgules")
    ap.add_argument("--a-cote", help="dossier de sortie (par défaut test/host/donnees)")
    a = ap.parse_args()
    sortie = Path(a.a_cote) if a.a_cote else DONNEES
    sortie.mkdir(parents=True, exist_ok=True)
    voulus = {m.strip() for m in a.seulement.split(",")} if a.seulement else None
    index_fichier = sortie / "mer_captures.json"
    index = json.loads(index_fichier.read_text(encoding="utf-8")) if index_fichier.exists() else {}
    echecs = 0
    for nom, spot, _ in SPOTS:
        if voulus is not None and nom not in voulus:
            continue
        url = adresse(spot)
        requete = urllib.request.Request(url, headers={"User-Agent": "meridien-tests/1"})
        try:
            with urllib.request.urlopen(requete, timeout=30) as r:
                corps, code, type_contenu = r.read(), r.status, r.headers.get("Content-Type")
        except urllib.error.HTTPError as erreur:
            corps, code, type_contenu = erreur.read(), erreur.code, erreur.headers.get("Content-Type")
        except OSError as erreur:
            print(f"  ECHEC   {nom}: {erreur}")
            echecs += 1
            continue
        if code != 200:
            print(f"  ECHEC   {nom}: HTTP {code} {corps[:200]!r}")
            echecs += 1
            continue
        lignes = corps.decode("utf-8").split("\n")
        balises: dict[str, int] = {}
        for ligne in lignes:
            if ligne:
                balise = ligne.split("|", 1)[0]
                balises[balise] = balises.get(balise, 0) + 1
        if balises.get("SEA") != 1:
            print(f"  ECHEC   {nom}: {balises.get('SEA', 0)} ligne(s) SEA (la houle n'a pas pu être lue ?)")
            echecs += 1
            continue
        (sortie / f"mer_{nom}.txt").write_bytes(corps)
        sea = next(ligne for ligne in lignes if ligne.startswith("SEA|")).split("|")
        index[nom] = {
            "url": url,
            "http": code,
            "content_type": type_contenu,
            "genere_utc": int(sea[3]),
            "octets": len(corps),
            "lignes_par_balise": balises,
        }
        print(f"  ok      {nom}: {len(corps)} octets, SEA {balises['SEA']}, ACT {balises.get('ACT', 0)}, CRIT {balises.get('CRIT', 0)}")
        time.sleep(2)
    index_fichier.write_text(json.dumps(index, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
