#!/usr/bin/env python3
"""Capture les VRAIES réponses du produit « Sunrise 3.0 » de MET Norway pour vérifier le calcul du lever et du coucher du soleil
fait sur l'appareil (components/core/include/ecran/soleil.hpp, ADR-0012).

    python test/host/capturer_soleil_metno.py           # écrit donnees/soleil_metno_reference.json

Le firmware n'appelle PAS ce produit (il calcule le soleil lui-même, voir ADR-0014) : ces réponses ne servent qu'à juger le
calcul. Les lieux et les jours sont ceux de LIEUX_SOLEIL (controle_python_fournisseurs.py) : une quinzaine de lieux, des cercles
polaires à l'équateur, dont des jours de changement d'heure. Une requête par seconde au plus ; une quatre-vingtaine de requêtes
(le script reprend là où il s'est arrêté : une réponse déjà reçue n'est pas redemandée) ; le
User-Agent est celui de l'appareil (lu dans meteo_requete_fournisseurs.hpp). Ce script appelle le réseau : à lancer à la main, pas
dans le contrôle. Les valeurs attendues sont ensuite recalculées par controle_python_fournisseurs.py.

Licence des données : CC BY 4.0 et NLOD 2.0 (https://api.met.no/license_data.html), attribution « MET Norway ».
"""
import json
import re
import sys
import time
import urllib.error
import urllib.request
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

ICI = Path(__file__).resolve().parent
sys.path.insert(0, str(ICI))
from controle_python_fournisseurs import LIEUX_SOLEIL, e5, minuit_local  # noqa: E402

DONNEES = ICI / "donnees"
ENTETE_HPP = ICI.parents[1] / "components/core/include/ecran/meteo_requete_fournisseurs.hpp"


def user_agent():
    m = re.search(r'USER_AGENT\s*=\s*"([^"]+)"', ENTETE_HPP.read_text(encoding="utf-8"))
    if not m:
        raise SystemExit("USER_AGENT introuvable dans meteo_requete_fournisseurs.hpp")
    return m.group(1)


def decalage(fuseau, jour):
    """Décalage UTC du fuseau à midi du jour local, au format « +01:00 »."""
    d = datetime.fromisoformat(jour + "T12:00:00").replace(tzinfo=ZoneInfo(fuseau))
    decal = d.utcoffset()
    assert decal is not None
    s = int(decal.total_seconds())
    signe = "+" if s >= 0 else "-"
    s = abs(s)
    return f"{signe}{s // 3600:02d}:{(s % 3600) // 60:02d}"


def main():
    ua = user_agent()
    fichier = DONNEES / "soleil_metno_reference.json"
    sortie = json.loads(fichier.read_text(encoding="utf-8")) if fichier.exists() else []
    deja = {(r["lieu"], r["jour"]) for r in sortie if r["http"] == 200}  # reprise : on ne redemande pas ce qu'on a
    sortie = [r for r in sortie if r["http"] == 200]
    for nom, fuseau, lat, lon, jours in LIEUX_SOLEIL:
        for jour in jours:
            if (nom, jour) in deja:
                continue
            off = decalage(fuseau, jour)
            adresse = (f"https://api.met.no/weatherapi/sunrise/3.0/sun?lat={lat:.4f}&lon={lon:.4f}&date={jour}"
                       f"&offset={off.replace('+', '%2B')}")
            req = urllib.request.Request(adresse, headers={"User-Agent": ua, "Accept": "application/json"})
            try:
                with urllib.request.urlopen(req, timeout=30) as f:
                    http, corps, expires = f.status, f.read().decode("utf-8"), f.headers.get("Expires")
            except urllib.error.HTTPError as e:
                http, corps, expires = e.code, e.read().decode("utf-8", "replace")[:300], None
            sortie.append({"lieu": nom, "jour": jour, "fuseau": fuseau, "lat_e5": e5(lat), "lon_e5": e5(lon),
                           "debut_jour_s": minuit_local(jour, fuseau), "offset": off, "adresse": adresse, "http": http,
                           "expires": expires, "corps": corps})
            print(f"{nom} {jour} -> {http}")
            time.sleep(1.1)
    fichier.write_text(json.dumps(sortie, ensure_ascii=False, indent=1), encoding="utf-8")
    print(len(sortie), "réponses dans", fichier.name)


if __name__ == "__main__":
    main()
