#!/usr/bin/env python3
"""Capture les VRAIES réponses de MET Norway (Locationforecast) et du NWS (api.weather.gov) pour les lieux des tests.

    python test/host/capturer_fournisseurs_meteo.py                       # tout capturer dans test/host/donnees/
    python test/host/capturer_fournisseurs_meteo.py --seulement metno_oslo,nws_miami
    python test/host/capturer_fournisseurs_meteo.py --sortie CHEMIN       # autre dossier (essais)
    python test/host/capturer_fournisseurs_meteo.py --couverture          # seulement les essais de couverture du NWS (aucun fichier)

Les adresses sont celles de l'appareil : les morceaux d'adresse et le User-Agent sont LUS dans
components/core/include/ecran/meteo_requete_fournisseurs.hpp (jamais recopiés ici) ; les coordonnées partent en e5 comme dans
le firmware et sont tronquées à quatre décimales (troncature vers zéro, comme `degres_e4`). En-têtes envoyés : ceux de
main/http_commun.cpp (`Accept` du fournisseur, `Accept-Encoding: gzip`).

Écrit, pour chaque appel : la réponse décompressée, octet pour octet (`metno_<lieu>.json`, `nws_<lieu>_points.json`,
`nws_<lieu>_grille.json`) ; et `fournisseurs_capture.json` : adresse, statut, octets sur le fil et décompressés, en-têtes de cache
(Expires, Last-Modified, Cache-Control, Date), instant de réception, plus l'essai de requête conditionnelle (If-Modified-Since).

Ce script appelle le réseau : à lancer à la main, pas dans le contrôle. Politesse : une seconde entre deux appels, un seul appel
par adresse (MET Norway demande d'attendre au moins la moitié de l'intervalle de mise à jour, soit 3 h hors zone nordique et 30 min
dans la zone nordique : https://api.met.no/doc/locationforecast/FAQ ; ne pas relancer à la chaîne). Sans clé.
"""
import argparse
import email.utils
import gzip
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parents[1]
ENTETE = RACINE / "components/core/include/ecran/meteo_requete_fournisseurs.hpp"
DONNEES = ICI / "donnees"

# nom, latitude e5, longitude e5, pays (code ISO du géocodeur)
METNO = [
    ("oslo", 5991273, 1074609, "NO"),
    ("reykjavik", 6414660, -2194260, "IS"),
    ("helsinki", 6017000, 2494000, "FI"),
    ("tromso", 6964920, 1895530, "NO"),
    ("longyearbyen", 7822320, 1562670, "SJ"),
    ("londres", 5150720, -12760, "GB"),
    ("tokyo", 3568900, 13969200, "JP"),
    ("sydney", -3386785, 15120732, "AU"),
]

NWS = [
    ("new_york", 4071280, -7400600, "US"),
    ("miami", 2576170, -8019180, "US"),
    ("seattle", 4760620, -12233210, "US"),
    ("anchorage", 6121810, -14990030, "US"),
    ("honolulu", 2130690, -15785830, "US"),
    ("san_juan", 1846550, -6610570, "PR"),
]

# Hors couverture : l'API répond 404 (corps de type problem+json)
NWS_HORS = [("lyon", 4574906, 484789, "FR")]

# Essais de couverture (aucun fichier gardé) : territoires américains du NWS, et deux lieux voisins qui n'en sont pas.
NWS_COUVERTURE = [
    ("Pago Pago (Samoa américaines)", -1427560, -17070200, "AS"),
    ("Hagåtña (Guam)", 1344430, 14479370, "GU"),
    ("Saipan (Îles Mariannes du Nord)", 1517780, 14575100, "MP"),
    ("Charlotte Amalie (Îles Vierges américaines)", 1833580, -6489630, "VI"),
    ("Toronto (Canada)", 4365000, -7938000, "CA"),
    ("La Havane (Cuba)", 2311330, -8236630, "CU"),
    ("Océan Atlantique (30 N, 60 O)", 3000000, -6000000, ""),
]


def morceau(source: str, nom: str) -> str:
    """Valeur d'une constante `std::string_view NOM = "…" "…";` : les littéraux voisins sont recollés."""
    m = re.search(r"std::string_view\s+" + nom + r"\s*=\s*((?:\s*\"(?:[^\"\\]|\\.)*\")+)\s*;", source)
    if not m:
        raise SystemExit(f"{nom} introuvable dans {ENTETE}")
    return "".join(re.findall(r"\"((?:[^\"\\]|\\.)*)\"", m.group(1)))


def degres_e4(v: int) -> str:
    """e5 -> quatre décimales, la cinquième supprimée (troncature vers zéro), pas de signe devant un zéro."""
    a = abs(v)
    entier, frac = a // 100000, (a % 100000) // 10
    signe = "-" if v < 0 and (entier or frac) else ""
    return f"{signe}{entier}.{frac:04d}"


class Constantes:
    def __init__(self) -> None:
        src = ENTETE.read_text(encoding="utf-8")
        self.ua = morceau(src, "USER_AGENT")
        self.metno_debut = morceau(src, "METNO_DEBUT")
        self.metno_milieu = morceau(src, "METNO_MILIEU")
        self.nws_points = morceau(src, "NWS_POINTS_DEBUT")
        self.nws_grille = morceau(src, "NWS_GRILLE_DEBUT")
        self.accept_metno = morceau(src, "METNO_ACCEPT")
        self.accept_nws = morceau(src, "NWS_ACCEPT")

    def url_metno(self, lat: int, lon: int) -> str:
        return self.metno_debut + degres_e4(lat) + self.metno_milieu + degres_e4(lon)

    def url_points(self, lat: int, lon: int) -> str:
        return self.nws_points + degres_e4(lat) + "," + degres_e4(lon)

    def url_grille(self, bureau: str, x: int, y: int) -> str:
        return f"{self.nws_grille}{bureau}/{x},{y}"


def appeler(url: str, ua: str, accept: str, extra=None):
    """(statut, en-têtes, octets sur le fil, corps décompressé). Aucune exception pour un statut HTTP d'erreur."""
    entetes = {"User-Agent": ua, "Accept": accept, "Accept-Encoding": "gzip"}
    if extra:
        entetes.update(extra)
    req = urllib.request.Request(url, headers=entetes)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            fil, code, h = r.read(), r.status, dict(r.headers)
    except urllib.error.HTTPError as e:
        fil, code, h = e.read(), e.code, dict(e.headers)
    corps = gzip.decompress(fil) if h.get("Content-Encoding") == "gzip" else fil
    return code, h, len(fil), corps


def epoch_http(date_http):
    """Date d'en-tête HTTP (« Sat, 07 Nov 2026 18:24:53 GMT ») -> secondes UNIX, ou None."""
    if not date_http:
        return None
    try:
        return int(email.utils.parsedate_to_datetime(date_http).timestamp())
    except (TypeError, ValueError):
        return None


def fiche(url, code, h, fil, corps):
    return {
        "url": url,
        "http": code,
        "octets_fil": fil,
        "octets_texte": len(corps),
        "recu_a_s": int(time.time()),
        "date_serveur_s": epoch_http(h.get("Date")),
        "content_type": h.get("Content-Type"),
        "content_encoding": h.get("Content-Encoding"),
        "cache_control": h.get("Cache-Control"),
        "expires": h.get("Expires"),
        "expires_s": epoch_http(h.get("Expires")),
        "last_modified": h.get("Last-Modified"),
        "last_modified_s": epoch_http(h.get("Last-Modified")),
        "age": h.get("Age"),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seulement", help="noms séparés par des virgules : metno_<lieu>, nws_<lieu>")
    ap.add_argument("--sortie", help="dossier des fichiers (par défaut : test/host/donnees)")
    ap.add_argument("--couverture", action="store_true", help="seulement les essais de couverture du NWS (rien n'est écrit)")
    ap.add_argument("--pause", type=float, default=1.5, help="secondes entre deux appels (défaut 1,5)")
    a = ap.parse_args()
    c = Constantes()
    sortie = Path(a.sortie) if a.sortie else DONNEES
    sortie.mkdir(parents=True, exist_ok=True)
    voulus = {s.strip() for s in a.seulement.split(",")} if a.seulement else None
    meta_fichier = sortie / "fournisseurs_capture.json"
    meta = json.loads(meta_fichier.read_text(encoding="utf-8")) if meta_fichier.exists() and voulus else {}
    meta["user_agent"] = c.ua

    def garder(nom: str) -> bool:
        return voulus is None or nom in voulus

    if a.couverture:
        for ville, lat, lon, pays in NWS_COUVERTURE:
            url = c.url_points(lat, lon)
            code, h, fil, corps = appeler(url, c.ua, c.accept_nws)
            r = {"lieu": ville, "pays": pays, "url": url, "http": code}
            if code == 200:
                p = json.loads(corps)["properties"]
                r.update({k: p.get(k) for k in ("gridId", "gridX", "gridY", "timeZone", "cwa")})
            else:
                r["corps"] = corps[:160].decode("utf-8", "replace")
            print(json.dumps(r, ensure_ascii=False))
            time.sleep(a.pause)
        return 0

    for nom, lat, lon, pays in METNO:
        cle = f"metno_{nom}"
        if not garder(cle):
            continue
        url = c.url_metno(lat, lon)
        code, h, fil, corps = appeler(url, c.ua, c.accept_metno)
        (sortie / f"{cle}.json").write_bytes(corps)
        f = fiche(url, code, h, fil, corps)
        f.update({"lieu": nom, "pays": pays, "lat_e5": lat, "lon_e5": lon})
        meta[cle] = f
        print(json.dumps({cle: {k: f[k] for k in ("http", "octets_fil", "octets_texte", "expires", "last_modified")}}))
        time.sleep(a.pause)
        if nom == "oslo" and code == 200 and h.get("Last-Modified"):
            # Requête conditionnelle : si rien n'a changé, 304 sans corps (https://api.met.no/doc/locationforecast/FAQ renvoie vers le guide « HowTo » pour If-Modified-Since)
            code2, h2, fil2, corps2 = appeler(url, c.ua, c.accept_metno, {"If-Modified-Since": h["Last-Modified"]})
            f2 = fiche(url, code2, h2, fil2, corps2)
            f2["if_modified_since"] = h["Last-Modified"]
            meta[cle + "_conditionnelle"] = f2
            print(json.dumps({cle + "_conditionnelle": {k: f2[k] for k in ("http", "octets_fil", "octets_texte", "expires")}}))
            time.sleep(a.pause)

    for nom, lat, lon, pays in NWS + NWS_HORS:
        cle = f"nws_{nom}"
        if not garder(cle):
            continue
        url = c.url_points(lat, lon)
        code, h, fil, corps = appeler(url, c.ua, c.accept_nws)
        (sortie / f"{cle}_points.json").write_bytes(corps)
        f = fiche(url, code, h, fil, corps)
        f.update({"lieu": nom, "pays": pays, "lat_e5": lat, "lon_e5": lon})
        meta[cle + "_points"] = f
        print(json.dumps({cle + "_points": {k: f[k] for k in ("http", "octets_fil", "octets_texte", "expires")}}))
        time.sleep(a.pause)
        if code != 200:
            continue
        p = json.loads(corps)["properties"]
        url2 = c.url_grille(p["gridId"], p["gridX"], p["gridY"])
        code2, h2, fil2, corps2 = appeler(url2, c.ua, c.accept_nws)
        (sortie / f"{cle}_grille.json").write_bytes(corps2)
        f2 = fiche(url2, code2, h2, fil2, corps2)
        f2.update({"lieu": nom, "fuseau": p.get("timeZone")})
        meta[cle + "_grille"] = f2
        print(json.dumps({cle + "_grille": {k: f2[k] for k in ("http", "octets_fil", "octets_texte", "expires")}}))
        time.sleep(a.pause)

    meta_fichier.write_text(json.dumps(meta, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print("écrit :", meta_fichier)
    return 0


if __name__ == "__main__":
    sys.exit(main())
