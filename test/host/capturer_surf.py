#!/usr/bin/env python3
"""Capture les VRAIES réponses d'Open-Meteo (mer et vent) de quinze positions du monde, avec les adresses exactes du firmware.

    python test/host/capturer_surf.py                # capture tout dans test/host/donnees/ (écrase les captures)
    python test/host/capturer_surf.py --sortie CHEMIN
    python test/host/capturer_surf.py --seulement hossegor,paris

Les adresses sont LUES dans components/core/include/ecran/surf_requete.hpp (MARINE_DEBUT, MARINE_FIN, VENT_DEBUT, VENT_FIN, MILIEU),
jamais recopiées ici : si le firmware change de requête, la capture suit. Les coordonnées passent par la même écriture « degrés
e5 » que le noyau (5 décimales).

Sorties, dans test/host/donnees/ :
  surf_marine_<id>.json    la réponse de l'API Marine, octet pour octet
  surf_vent_<id>.json      la réponse de l'API de prévision (vent), octet pour octet
  surf_manifeste.json      la liste des positions (nom, e5, orientation de plage prise pour le test), l'instant de capture, les
                           adresses et les tailles. Lu par controle_python_surf.py.
Puis `python test/host/controle_python_surf.py` régénère les .inc de test.

Le script appelle le réseau : à lancer à la main, pas dans le contrôle. API gratuite, usage non commercial, attribution CC BY 4.0
(https://open-meteo.com) ; limites 600 appels/min, 5 000/h, 10 000/jour : ce script fait une trentaine d'appels, espacés d'une
seconde. Toutes les réponses doivent commencer à la MÊME heure (sinon une frontière d'heure a été franchie pendant la capture : le
script recommence, trois fois au plus) : les tests supposent un seul « maintenant » pour tous les lieux.

L'ORIENTATION DE PLAGE de chaque lieu est un choix du test, tiré de la carte, JAMAIS vérifié sur le terrain : elle sert à exercer
le calcul, pas à dire comment un spot réel se comporte.
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
REQUETE_HPP = RACINE / "components/core/include/ecran/surf_requete.hpp"
DONNEES = ICI / "donnees"

# id (ASCII, sert aux noms de fichiers), nom affiché, latitude e5, longitude e5, orientation de plage prise pour le test (°), particularité
SPOTS = [
    ("hossegor", "Hossegor", 4366760, -144500, 275, "côte landaise, océan à l'ouest ; marée forte (macro-tidale)"),
    ("hawai_pipeline", "Pipeline (Oahu)", 2166500, -15805300, 350, "côte nord d'Oahu, plage face au nord ; marée faible"),
    ("gold_coast", "Snapper Rocks", -2817100, 15354900, 105, "Australie, hémisphère sud, plage face à l'est"),
    ("nazare", "Nazaré", 3960500, -908500, 290, "très grosse houle de l'Atlantique"),
    ("jeffreys_bay", "Jeffreys Bay", -3405000, 2493000, 110, "Afrique du Sud, océan Indien"),
    ("uluwatu", "Uluwatu", -881500, 11508800, 250, "Bali, sud de l'équateur, décalage +8 h"),
    ("le_cap", "Muizenberg", -3410800, 1847000, 155, "baie ouverte au sud-est"),
    ("pichilemu", "Punta de Lobos", -3443000, -7204500, 260, "Chili, Pacifique sud"),
    ("mentawai", "Macaronis", -205000, 9930000, 240, "Mentawai, équateur"),
    ("irlande_lahinch", "Lahinch", 5293400, -935000, 285, "Irlande de l'ouest, marée forte, houle d'hiver"),
    ("baltique_warnemunde", "Warnemünde", 5418000, 1208000, 5, "mer fermée : houle faible, pas de marée sensible"),
    ("mediterranee_marseille", "Marseille", 4325000, 537000, 215, "Méditerranée : houle faible, pas de marée sensible"),
    ("saint_malo", "Saint-Malo", 4865000, -202000, 330, "marnage de plus de 8 m"),
    ("michigan", "Chicago (lac Michigan)", 4200000, -8750000, 90, "lac : vagues oui, température et marée null (cas partiel)"),
    ("paris", "Paris (hors mer)", 4885660, 235220, 0, "terre ferme : toutes les variables de mer null"),
]


def morceaux(source: str, nom: str) -> str:
    """Valeur d'une constante `std::string_view NOM = "…" "…";` : les littéraux voisins sont recollés."""
    m = re.search(r"std::string_view\s+" + nom + r"\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", source)
    if not m:
        raise SystemExit(f"{nom} introuvable dans {REQUETE_HPP}")
    return "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))


def degres_e5(v: int) -> str:
    signe = "-" if v < 0 else ""
    a = abs(v)
    return f"{signe}{a // 100000}.{a % 100000:05d}"


def adresses(lat_e5: int, lon_e5: int) -> tuple[str, str]:
    src = REQUETE_HPP.read_text(encoding="utf-8")
    milieu = morceaux(src, "MILIEU")
    coord = degres_e5(lat_e5) + milieu + degres_e5(lon_e5)
    marine = morceaux(src, "MARINE_DEBUT") + coord + morceaux(src, "MARINE_FIN")
    vent = morceaux(src, "VENT_DEBUT") + coord + morceaux(src, "VENT_FIN")
    return marine, vent


def telecharger(url: str) -> tuple[int, bytes]:
    req = urllib.request.Request(url, headers={"Accept-Encoding": "identity", "User-Agent": "meridien-capture/1 (essai surf)"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def une_passe(liste) -> tuple[dict, dict] | None:
    """Capture toutes les positions ; rend (réponses, manifeste) ou None si les premières heures ne concordent pas."""
    reponses, lignes, premieres = {}, [], set()
    debut = int(time.time())
    for id_, nom, lat, lon, orient, note in liste:
        u_mer, u_vent = adresses(lat, lon)
        corps = {}
        for genre, url in (("marine", u_mer), ("vent", u_vent)):
            code, octets = telecharger(url)
            time.sleep(1)
            if code != 200:
                raise SystemExit(f"{id_} {genre} : HTTP {code} : {octets[:200]!r}")
            doc = json.loads(octets)
            premieres.add(doc["hourly"]["time"][0])
            corps[genre] = octets
            reponses[f"surf_{genre}_{id_}.json"] = octets
        lignes.append({"id": id_, "nom": nom, "latitude_e5": lat, "longitude_e5": lon, "orientation_deg": orient,
                       "note": note, "url_marine_octets": len(u_mer), "url_vent_octets": len(u_vent),
                       "marine_octets": len(corps["marine"]), "vent_octets": len(corps["vent"])})
        print(f"  {id_}: mer {len(corps['marine'])} o, vent {len(corps['vent'])} o")
    if len(premieres) != 1:
        return None
    premiere = premieres.pop()
    return reponses, {"capture_debut_s": debut, "capture_fin_s": int(time.time()), "premiere_heure_s": premiere,
                      "heures": 54, "spots": lignes}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sortie", help="dossier de sortie (par défaut test/host/donnees)")
    ap.add_argument("--seulement", help="identifiants séparés par des virgules (le manifeste ne contiendra que ceux-là)")
    a = ap.parse_args()
    liste = SPOTS
    if a.seulement:
        voulus = {m.strip() for m in a.seulement.split(",")}
        liste = [s for s in SPOTS if s[0] in voulus]
        if not liste:
            raise SystemExit("aucun identifiant connu dans --seulement")
    sortie = Path(a.sortie) if a.sortie else DONNEES
    sortie.mkdir(parents=True, exist_ok=True)
    resultat = None
    for essai in range(1, 4):
        print(f"passe {essai} : {len(liste)} position(s), deux appels chacune")
        resultat = une_passe(liste)
        if resultat:
            break
        print("  les réponses ne commencent pas toutes à la même heure : une frontière d'heure a été franchie, on recommence")
    if not resultat:
        raise SystemExit("trois passes de suite à cheval sur une frontière d'heure : réessayer plus tard")
    reponses, manifeste = resultat
    for nom, octets in reponses.items():
        (sortie / nom).write_bytes(octets)
    (sortie / "surf_manifeste.json").write_text(json.dumps(manifeste, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"écrit : {len(reponses)} réponses + surf_manifeste.json dans {sortie}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
