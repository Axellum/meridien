#!/usr/bin/env python3
"""Parcours de la page de réglages contre le faux appareil, avec Playwright (Chromium, rendu téléphone).

    python web/tests/parcours.py

Lance web/faux-serveur/serveur.mjs (Node) sur des ports de test, déroule le parcours, écrit les captures dans
web/captures/ et rend un tableau « vérification : OK / ÉCHEC ». Code de sortie 0 seulement si tout passe.
Réseau requis : la recherche de lieu et la météo appellent vraiment Open-Meteo.

Contre le VRAI appareil (le firmware qui sert la page lui-même) :

    $env:URL_APPAREIL = "http://192.168.x.y"      # l'adresse affichée sur l'écran de l'appareil
    $env:CODE_APPAIRAGE = "123456"                 # le code affiché sur l'écran de l'appareil
    python web/tests/parcours.py

Aucun faux serveur n'est lancé. Les étapes que le firmware ne fait pas encore (services STT/LLM/TTS, export / import, changement
de Wi-Fi, mise à jour) sont SAUTÉES et listées comme telles : elles ne comptent ni comme réussies ni comme ratées. Le parcours
change vraiment les réglages de l'appareil (thème, luminosité, langues, ville) ; il remet l'affichage dans son état de départ à
la fin. L'essai du verrou d'appairage bloque l'appairage 60 secondes.
"""
import gzip
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

from controle_langues import lire_yaml  # même dossier : python web/tests/parcours.py met ce dossier dans sys.path
from playwright.sync_api import expect, sync_playwright

RACINE = Path(__file__).resolve().parents[2]
CAPTURES = RACINE / "web" / "captures"
LANGUES_PAGE = RACINE / "web" / "app" / "lang"
CODES_LANGUES = ["fr", "en", "de", "nl", "es", "it", "tr"]   # ordre figé par l'ADR-0006 (indices 0 à 6)
PORT, PORT_SERVICE = 18100, 18101
URL_APPAREIL = os.environ.get("URL_APPAREIL", "").strip().rstrip("/")   # vide : faux appareil (comportement d'origine)
REEL = bool(URL_APPAREIL)
BASE = URL_APPAREIL if REEL else f"http://127.0.0.1:{PORT}"
CODE = os.environ.get("CODE_APPAIRAGE", "").strip() if REEL else "246810"
CLE_BONNE = "sk-secret-TEST-9f3a"   # la clé que le faux service accepte (valeur de test, pas un secret réel)
CLE_FAUSSE = "sk-faux-0000-leak"    # le faux service la cite dans sa réponse d'erreur, comme certains vrais services
VERROU_S = 60 if REEL else 3        # durée du verrou d'appairage : 60 s sur l'appareil (noyau, appairage.hpp), 3 s sur le faux
BUDGET_GZ = 30 * 1024               # budget annoncé : 30 Ko compressés (HTML + CSS + JS)

sautes = []        # (étape, raison) : étapes que l'appareil réel ne fait pas encore
resultats = []     # (nom, ok, détail)
registre = []      # toutes les réponses /api/ vues par le navigateur : {url, statut, corps, requete}
hotes = set()      # tous les hôtes joints par le navigateur (la page ne doit parler qu'à l'appareil)
erreurs_console = []


def verifier(nom, condition, detail=""):
    resultats.append((nom, bool(condition), detail))
    print(f"  {'OK   ' if condition else 'ÉCHEC'} {nom}" + (f"  [{detail}]" if detail and not condition else ""))


def etape(titre, fonction, page=None):
    print(f"\n== {titre}")
    try:
        fonction()
    except Exception as e:  # noqa: BLE001 : on veut le rapport complet, pas l'arrêt au premier raté
        verifier(titre + " (déroulement)", False, str(e).strip().splitlines()[0][:220])
        if page is not None:
            try:
                page.screenshot(path=str(CAPTURES / "_echec.png"), full_page=True)
            except Exception:  # noqa: BLE001
                pass


def sauter(titre, raison):
    """Étape que l'appareil réel ne fait pas encore : dite, jamais comptée comme réussie."""
    print(f"\n== {titre}\n  SAUTÉE : {raison}")
    sautes.append((titre, raison))


def attendre_serveur():
    for _ in range(60):
        try:
            urllib.request.urlopen(BASE + "/api/status", timeout=1).read()
            return
        except Exception:  # noqa: BLE001
            time.sleep(0.25)
    raise RuntimeError(f"{'l’appareil ne répond pas à ' + BASE if REEL else 'le faux appareil ne démarre pas'}")


def nouveau_contexte(nav, largeur=390, sombre=False):
    ctx = nav.new_context(viewport={"width": largeur, "height": 844}, device_scale_factor=1.5, is_mobile=True, has_touch=True,
                          locale="fr-FR", color_scheme="dark" if sombre else "light", accept_downloads=True)
    page = ctx.new_page()
    page.set_default_timeout(20000)

    def intercepter(route):
        try:
            rep = route.fetch()
            corps = rep.text() if "json" in (rep.headers.get("content-type") or "") else f"<{len(rep.body())} octets>"
            envoye = (route.request.post_data_buffer or b"").decode("utf-8", errors="replace")  # un firmware est binaire : pas de post_data
            registre.append({"url": route.request.url, "statut": rep.status, "corps": corps, "requete": envoye})
            route.fulfill(response=rep)
        except Exception:  # noqa: BLE001 : requête annulée par la page (anti-rebond) ou appareil qui redémarre
            try:
                route.abort()
            except Exception:  # noqa: BLE001
                pass

    page.route("**/api/**", intercepter)
    page.on("request", lambda r: hotes.add(urllib.parse.urlsplit(r.url).netloc))
    page.on("console", lambda m: erreurs_console.append(m.text) if m.type in ("error", "warning") else None)
    page.on("pageerror", lambda e: erreurs_console.append("pageerror : " + str(e)))
    return ctx, page


def jeton(page):
    return page.evaluate("sessionStorage.getItem('jeton')")


def api(page, methode, chemin, corps=None):
    r = page.request.fetch(BASE + chemin, method=methode, headers={"Authorization": "Bearer " + jeton(page), "Content-Type": "application/json"},
                           data=json.dumps(corps) if corps is not None else None)
    return r.status, (r.json() if "json" in (r.headers.get("content-type") or "") else {})


def catalogue(code):
    """Le petit fichier JSON d'une langue, tel que la page le charge (web/app/lang/<code>.json)."""
    return json.loads((LANGUES_PAGE / f"{code}.json").read_text(encoding="utf-8"))


def norm(texte):
    """Espaces normalisés (l'espace insécable devient une espace) : on compare ce qu'on lit, pas la typographie."""
    return " ".join(texte.split())


# Le service qui a donné la météo, tel que /api/weather le nomme (`fournisseur`) : identifiant -> (phrase du catalogue, nom, lien).
FOURNISSEURS = {
    "open-meteo": ("meteo.attribution", "Open-Meteo", "https://open-meteo.com/"),
    "met-norway": ("meteo.attribution_metno", "MET Norway", "https://api.met.no/"),
    "nws": ("meteo.attribution_nws", "NWS", "https://www.weather.gov/"),
}


def verifier_attribution(page, code, quand, fournisseur="open-meteo"):
    """L'attribution du service qui a donné la météo : visible, traduite, son nom est un lien vers son site.
    Open-Meteo : CC BY 4.0. MET Norway : CC BY 4.0 et NLOD 2.0. NWS : domaine public, et la phrase « ni approuvé ni parrainé »."""
    cle, nom, adresse = FOURNISSEURS[fournisseur]
    cat = catalogue(code)
    zone = page.locator("#attribution")
    lien = zone.locator("a")
    expect(lien).to_have_text(nom)  # la page change d'attribution quand la réponse météo arrive : on attend le bon service avant de comparer
    attendu = norm(cat[cle].replace("{lien}", nom))
    if fournisseur == "nws":
        attendu += " " + norm(cat["meteo.sans_parrainage"])
    texte = norm(zone.inner_text())
    verifier(f"attribution ({quand}) : texte de la licence, dans la langue de la page", texte == attendu, f"{texte!r} au lieu de {attendu!r}")
    if fournisseur != "nws":
        verifier(f"attribution ({quand}) : la licence est nommée", "CC BY 4.0" in texte, texte)
    if fournisseur == "met-norway":
        verifier(f"attribution ({quand}) : la seconde licence (NLOD 2.0) est nommée", "NLOD 2.0" in texte, texte)
    verifier(f"attribution ({quand}) : « {nom} » est un lien vers {adresse}",
             lien.count() == 1 and lien.inner_text() == nom and lien.get_attribute("href") == adresse, str(lien.count()))


def sans_null(page, quand):
    """append()/replaceChildren() écrivent « null » à l'écran quand on leur passe null : bug vu au premier lancement."""
    texte = page.locator("body").inner_text()
    mots = re.findall(r"\bnull\b|\bundefined\b|\bNaN\b|\[object", texte)  # mots entiers : « nulle part » est du français
    verifier(f"aucun « null / undefined / NaN » affiché ({quand})", not mots, ", ".join(mots))


def appairer(page):
    page.goto(BASE)
    page.locator("#code").fill(CODE)
    expect(page.locator("#ville")).to_be_visible()


def ouvrir(page, nom):
    page.get_by_role("button", name=re.compile("^" + nom)).click()


DEPASSEMENTS = """() => {
  const l = document.documentElement.clientWidth, sortis = [];
  const rogne = (e) => { for (let p = e.parentElement; p && p !== document.body; p = p.parentElement) {
    const o = getComputedStyle(p).overflowX, r = p.getBoundingClientRect(); if (o !== 'visible' && r.left >= -1 && r.right <= l + 1) return true; } return false; };
  for (const e of document.querySelectorAll('body *')) { const r = e.getBoundingClientRect();
    if (r.width > 0 && (r.right > l + 1 || r.left < -1) && !rogne(e)) sortis.push(e.tagName + '.' + e.className + ' ' + Math.round(r.left) + '..' + Math.round(r.right)); }
  return {largeur: l, defilement: document.documentElement.scrollWidth, sortis: sortis.slice(0, 6)};
}"""


def main():
    CAPTURES.mkdir(exist_ok=True)
    if REEL:
        if not re.fullmatch(r"\d{6}", CODE):
            print("URL_APPAREIL est défini : CODE_APPAIRAGE doit contenir les 6 chiffres affichés sur l'écran de l'appareil.")
            return 2
        print(f"Appareil réel : {BASE} (aucun faux serveur lancé)")
        srv = None
    else:
        env = {**os.environ, "PORT": str(PORT), "PORT_SERVICE": str(PORT_SERVICE), "CODE_APPAIRAGE": CODE,
               "CLE_MOCK": CLE_BONNE, "VERROU_S": str(VERROU_S), "REBOOT_MS": "2500"}
        srv = subprocess.Popen(["node", str(RACINE / "web" / "faux-serveur" / "serveur.mjs")], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        attendre_serveur()
        with sync_playwright() as p:
            nav = p.chromium.launch()
            ctx, page = nouveau_contexte(nav)
            poids = {}

            def t_appairage():
                page.goto(BASE)
                expect(page.locator("#code")).to_be_visible()
                poids["res"] = page.evaluate("performance.getEntriesByType('resource').filter(e => !e.name.includes('/api/')).map(e => [e.name.split('/').pop() || 'index', e.encodedBodySize, e.decodedBodySize])")
                poids["nav"] = page.evaluate("(() => { const n = performance.getEntriesByType('navigation')[0]; return [n.encodedBodySize, n.decodedBodySize]; })()")
                page.screenshot(path=str(CAPTURES / "01-appairage.png"))
                page.locator("#code").fill("000000")
                expect(page.get_by_text(re.compile("Ce code n.est pas le bon"))).to_be_visible()
                msg = page.locator(".appairage .res").inner_text()
                verifier("code faux : phrase française, pas de code brut", "bon" in msg and not re.search(r"\b403\b|code_faux", msg), msg)
                page.locator("#code").fill(CODE)
                expect(page.locator("#ville")).to_be_visible()
                verifier("bon code : la page de réglages s'ouvre", True)
            etape("Appairage par le code affiché à l'écran", t_appairage, page)

            def t_ville():
                if not REEL:  # l'appareil réel a toujours une ville (Lyon par défaut, contrat meteo_zone.h)
                    expect(page.locator(".hero .ville")).to_have_text("Choisissez-la")
                page.screenshot(path=str(CAPTURES / "02-accueil-sans-ville.png"))
                champ = page.locator("#ville-q")
                champ.press_sequentially("Ly", delay=80)
                page.wait_for_timeout(900)
                expect(page.get_by_text("Tapez au moins 3 lettres.")).to_be_visible()
                verifier("2 lettres : aucune recherche envoyée", not any("/api/geocode" in r["url"] for r in registre))
                champ.press_sequentially("o", delay=80)
                expect(page.locator(".lieu").first).to_be_visible(timeout=20000)
                verifier("3 lettres : la recherche réelle (Open-Meteo) répond", page.locator(".lieu").count() >= 1, f"{page.locator('.lieu').count()} lieux")
                page.screenshot(path=str(CAPTURES / "03-recherche-lyon.png"))
                lyon = page.locator(".lieu").filter(has_text="Auvergne-Rhône-Alpes").filter(has_text="France").first
                expect(lyon).to_be_visible()
                lyon.click()
                expect(page.locator(".hero .ville")).to_have_text("Lyon")
                code, zone = api(page, "GET", "/api/location")
                verifier("Lyon : la zone enregistrée est Lyon", code == 200 and zone["nom"] == "Lyon" and zone["code_pays"] == "FR", str(zone))
                verifier("Lyon : fuseau IANA retenu = Europe/Paris", zone["fuseau"] == "Europe/Paris", zone["fuseau"])
                verifier("Lyon : chaîne POSIX tirée de la table du noyau", zone["posix"] == "CET-1CEST,M3.5.0,M10.5.0/3", zone["posix"])
                verifier("Lyon : coordonnées proches de 45,75 N / 4,85 E", abs(zone["latitude"] - 45.75) < 0.3 and abs(zone["longitude"] - 4.85) < 0.3)
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                aide = page.locator("#ville > .aide:not(#attribution)").inner_text()  # l'attribution Open-Meteo est aussi une « aide »
                verifier("fuseau et heure locale affichés", re.search(r"Europe/Paris · il est \d{1,2}\sh\s\d{2}", aide) is not None, aide)
                page.screenshot(path=str(CAPTURES / "04-ville-lyon.png"))
                # la météo affichée correspond à celle d'Open-Meteo lue directement au même moment
                direct = json.load(urllib.request.urlopen(f"https://api.open-meteo.com/v1/forecast?latitude={zone['latitude']}&longitude={zone['longitude']}&current=temperature_2m,weather_code&timezone=auto", timeout=10))["current"]
                affichee = float(re.findall(r"-?\d+", page.locator(".hero .temp b").inner_text())[0])
                verifier("météo affichée = météo directe d'Open-Meteo (±1,5 °C)", abs(affichee - direct["temperature_2m"]) <= 1.5, f"affichée {affichee}, directe {direct['temperature_2m']}")
                src = (RACINE / "components/core/include/ecran/meteo_wmo.hpp").read_text(encoding="utf-8")
                libelle = re.findall(rf"case {direct['weather_code']}: return \{{\"([^\"]+)\"", src)[0]
                verifier("libellé = table WMO du noyau pour le code direct", page.locator(".hero .temp small").inner_text() == libelle, libelle)
            etape("Ville : choisir « Lyon »", t_ville, page)

            def t_source():
                cat = catalogue("fr")
                champ = page.locator("#ville-q")
                liste_js = "[...document.querySelectorAll('#source-meteo option')].map((o) => [o.value, o.textContent])"
                expect(page.locator("#source-champ")).to_be_visible()
                verifier("source météo : le titre vient du catalogue", page.locator("label[for=source-meteo]").inner_text() == cat["source.titre"])
                options = page.evaluate(liste_js)
                verifier("source météo : à Lyon, « Automatique » puis le modèle mondial seulement", options == [["auto", "Automatique"], ["noaa", "NOAA GFS"]], str(options))
                verifier("source météo : « Automatique » au départ", page.locator("#source-meteo").input_value() == "auto")
                code, zone = api(page, "GET", "/api/location")
                verifier("source météo : l'API donne la même liste que la page", [s["id"] for s in zone["sources_meteo"]] == [o[0] for o in options], str(zone.get("sources_meteo")))
                code, erreur = api(page, "PUT", "/api/location", {"source_meteo": "knmi"})
                verifier("source météo : un service qui ne couvre pas la France est refusé", code == 422 and erreur["erreur"]["id"] == "source_indisponible" and erreur["erreur"]["champ"] == "source_meteo", str(erreur))
                code, erreur = api(page, "PUT", "/api/location", {"source_meteo": "n_importe_quoi"})
                verifier("source météo : un identifiant inconnu est refusé", code == 422 and erreur["erreur"]["id"] == "source_inconnue", str(erreur))
                verifier("source météo : les refus n'ont rien changé", api(page, "GET", "/api/location")[1]["source_meteo"] == "auto")
                # choisir le modèle mondial depuis la page
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                code, zone = api(page, "GET", "/api/location")
                verifier("source météo : NOAA GFS enregistrée, la ville n'a pas bougé", zone["source_meteo"] == "noaa" and zone["nom"] == "Lyon", str(zone))
                expect(page.locator("#attribution-modele")).to_contain_text(cat["source.modele"].replace("{modele}", "NOAA GFS"))
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                verifier("source météo : la météo revient avec le modèle choisi", True)
                page.screenshot(path=str(CAPTURES / "04b-source-noaa.png"))
                # Amsterdam : le service néerlandais devient proposé
                champ.fill("")
                champ.press_sequentially("Amsterdam", delay=60)
                amsterdam = page.locator(".lieu").filter(has_text="Pays-Bas").first
                expect(amsterdam).to_be_visible(timeout=20000)
                amsterdam.click()
                expect(page.locator(".hero .ville")).to_have_text("Amsterdam")
                options = page.evaluate(liste_js)
                verifier("source météo : à Amsterdam, KNMI s'ajoute avant le modèle mondial", [o[0] for o in options] == ["auto", "knmi", "noaa"], str(options))
                verifier("source météo : NOAA GFS gardée, elle couvre aussi les Pays-Bas", page.locator("#source-meteo").input_value() == "noaa" and page.locator("#source-retablie").is_hidden())
                page.locator("#source-meteo").select_option("knmi")
                expect(page.locator("#toast")).to_contain_text("KNMI")
                expect(page.locator("#attribution-modele")).to_contain_text(cat["source.modele"].replace("{modele}", "KNMI"))
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                page.screenshot(path=str(CAPTURES / "04c-source-knmi.png"))
                # retour à Lyon : KNMI ne couvre pas la France, la page revient sur « Automatique » ET le dit
                champ.fill("")
                champ.press_sequentially("Lyon", delay=60)
                page.locator(".lieu").filter(has_text="Auvergne-Rhône-Alpes").filter(has_text="France").first.click()
                expect(page.locator(".hero .ville")).to_have_text("Lyon")
                expect(page.locator("#source-retablie")).to_be_visible()
                verifier("source météo : le retour sur « Automatique » est dit, pas muet", cat["source.retablie"] in norm(page.locator("#source-retablie").inner_text()), page.locator("#source-retablie").inner_text())
                verifier("source météo : la liste montre « Automatique » et l'API aussi", page.locator("#source-meteo").input_value() == "auto" and api(page, "GET", "/api/location")[1]["source_meteo_retablie"] is True)
                page.screenshot(path=str(CAPTURES / "04d-source-retablie.png"))
                # choisir une source efface le message ; on remet « Automatique » pour la suite du parcours
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#source-retablie")).to_be_hidden()
                page.locator("#source-meteo").select_option("auto")
                expect(page.locator("#toast")).to_contain_text("Automatique")
                code, zone = api(page, "GET", "/api/location")
                verifier("source météo : remise sur « Automatique », message effacé", zone["source_meteo"] == "auto" and zone["source_meteo_retablie"] is False and page.locator("#attribution-modele").inner_text() == "", str(zone))
                verifier_attribution(page, "fr", "après les changements de source")
                sans_null(page, "source météo")
            etape("Source météo : liste par pays, choix, refus, retour automatique", t_source, page)

            def t_source_directe():
                # MET Norway : un service lu directement par l'appareil (ADR-0014), proposé pour la Norvège seulement. Le faux serveur ne sait lire
                # qu'Open-Meteo : il fait ce que l'appareil fait quand le service ne répond pas (météo d'Open-Meteo, `repli` vrai). Le cas où le
                # service répond est joué en remplaçant la réponse de /api/weather : on ne teste ici que ce que la PAGE affiche.
                cat = catalogue("fr")
                champ = page.locator("#ville-q")
                liste_js = "[...document.querySelectorAll('#source-meteo option')].map((o) => [o.value, o.textContent])"
                champ.fill("")
                champ.press_sequentially("Oslo", delay=60)
                oslo = page.locator(".lieu").filter(has_text="Norvège").first
                expect(oslo).to_be_visible(timeout=20000)
                oslo.click()
                expect(page.locator(".hero .ville")).to_have_text("Oslo")
                options = page.evaluate(liste_js)
                verifier("source directe : à Oslo, les deux « MET Norway » sont proposés (le modèle d'Open-Meteo et le service lu directement)",
                         [o[0] for o in options] == ["auto", "metno", "metnorway", "noaa"], str(options))
                code, zone = api(page, "GET", "/api/location")
                directs = {s["id"]: s["direct"] for s in zone["sources_meteo"]}
                verifier("source directe : l'API dit quelles sources sont lues directement", directs == {"auto": False, "metno": False, "metnorway": True, "noaa": False}, str(directs))
                page.locator("#source-meteo").select_option("metno")
                expect(page.locator("#attribution-modele")).to_contain_text(cat["source.modele"].replace("{modele}", "MET Norway"))
                verifier("source directe : le modèle d'Open-Meteo est nommé « Modèle : … »", page.locator("#source-repli").is_hidden())
                # 1. le service direct ne répond pas : la météo vient d'Open-Meteo, la page le dit et cite Open-Meteo
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                expect(page.locator("#source-repli")).to_be_visible()
                phrase = norm(cat["source.repli"].replace("{service}", "MET Norway (api.met.no)"))
                verifier("source directe : le repli est dit, avec le nom du service choisi", norm(page.locator("#source-repli").inner_text()) == phrase, page.locator("#source-repli").inner_text())
                verifier("source directe : pas de « Modèle : … » pour un service lu directement", page.locator("#attribution-modele").inner_text() == "", page.locator("#attribution-modele").inner_text())
                verifier_attribution(page, "fr", "repli : la météo vient d'Open-Meteo")
                code, meteo = api(page, "GET", "/api/weather")
                verifier("source directe : /api/weather nomme Open-Meteo et dit « repli »", meteo.get("repli") is True and meteo["fournisseur"]["id"] == "open-meteo", str(meteo.get("fournisseur")))
                page.screenshot(path=str(CAPTURES / "04e-source-repli.png"))

                # 2. le service répond : on joue la réponse de l'appareil (même forme que /api/weather, `fournisseur` de MET Norway)
                def reponse(fournisseur, repli=False):
                    return {"disponible": True, "perimee": False, "mise_a_jour": 1794000000, "fournisseur": fournisseur, "repli": repli,
                            "actuelle": {"temperature": 4.0, "ressentie": 2.0, "code": 3, "libelle": "Couvert", "jour": True}}

                met = {"id": "met-norway", "nom": "MET Norway", "lien": "https://api.met.no/"}
                nws = {"id": "nws", "nom": "NWS", "lien": "https://www.weather.gov/"}
                page.route("**/api/weather", lambda route: route.fulfill(status=200, content_type="application/json", body=json.dumps(reponse(met))))
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"4\s°C"), timeout=20000)
                expect(page.locator("#source-repli")).to_be_hidden()
                verifier_attribution(page, "fr", "MET Norway a répondu", "met-norway")
                verifier("source directe : pas de repli ni de « Modèle : … » quand le service a répondu",
                         page.locator("#source-repli").is_hidden() and page.locator("#attribution-modele").inner_text() == "")
                page.locator("#attribution").scroll_into_view_if_needed()
                page.screenshot(path=str(CAPTURES / "04f-source-metno.png"))
                # 3. la phrase du NWS (domaine public, sans parrainage) : la page n'a qu'à suivre ce que l'appareil annonce
                page.unroute("**/api/weather")
                page.route("**/api/weather", lambda route: route.fulfill(status=200, content_type="application/json", body=json.dumps(reponse(nws))))
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"4\s°C"), timeout=20000)
                verifier_attribution(page, "fr", "NWS a répondu", "nws")
                # 4. un fournisseur que la page ne connaît pas, ou un lien qui n'est pas https : retour à Open-Meteo, jamais un lien douteux
                page.unroute("**/api/weather")
                inconnu = {"id": "inconnu", "nom": "Inconnu", "lien": "javascript:alert(1)"}
                page.route("**/api/weather", lambda route: route.fulfill(status=200, content_type="application/json", body=json.dumps(reponse(inconnu))))
                page.locator("#source-meteo").select_option("noaa")
                expect(page.locator("#toast")).to_contain_text("NOAA GFS")
                page.locator("#source-meteo").select_option("metnorway")
                expect(page.locator("#toast")).to_contain_text("MET Norway (api.met.no)")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"4\s°C"), timeout=20000)
                verifier_attribution(page, "fr", "fournisseur inconnu : Open-Meteo par défaut")
                page.unroute("**/api/weather")
                # retour à Lyon et à « Automatique » pour la suite du parcours
                page.locator("#source-meteo").select_option("auto")
                expect(page.locator("#toast")).to_contain_text("Automatique")
                champ.fill("")
                champ.press_sequentially("Lyon", delay=60)
                page.locator(".lieu").filter(has_text="Auvergne-Rhône-Alpes").filter(has_text="France").first.click()
                expect(page.locator(".hero .ville")).to_have_text("Lyon")
                expect(page.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                verifier("source directe : retour à Lyon, « Automatique », aucun message de repli",
                         page.locator("#source-meteo").input_value() == "auto" and page.locator("#source-repli").is_hidden())
                verifier_attribution(page, "fr", "retour à Lyon")
                sans_null(page, "source directe")
            if REEL:
                sauter("Source directe : MET Norway (repli, réponse), NWS, fournisseur inconnu", "le test joue des réponses de l'appareil et change de ville : il ne se joue que contre le faux appareil")
            else:
                etape("Source directe : MET Norway (repli, réponse), NWS, fournisseur inconnu", t_source_directe, page)

            def t_services():
                ouvrir(page, "Réfléchir")
                page.get_by_role("radio", name="Autre").click()
                page.locator("#llm-url").fill(f"http://127.0.0.1:{PORT_SERVICE}/v1")
                page.locator("#llm-cle").fill(CLE_FAUSSE)
                page.get_by_role("button", name="Tester").click()
                expect(page.locator("#det-llm .res.err")).to_be_visible()
                msg = page.locator("#det-llm .res.err").inner_text()
                verifier("clé refusée : phrase lisible", "refuse la clé" in msg, msg)
                verifier("clé refusée : le texte du service (qui cite la clé) n'est pas recopié", CLE_FAUSSE not in msg and "Incorrect" not in msg)
                page.screenshot(path=str(CAPTURES / "05-test-cle-refusee.png"))
                page.locator("#llm-cle").fill(CLE_BONNE)
                page.get_by_role("button", name="Tester").click()
                expect(page.locator("#det-llm .res.ok")).to_contain_text("Le service répond en", timeout=20000)
                verifier("test réussi : les modèles du service sont proposés", page.evaluate("document.querySelectorAll('#mod-llm option').length") == 3)
                page.locator("#llm-modele").fill("modele-essai-b")
                sans_null(page, "service testé")
                page.screenshot(path=str(CAPTURES / "06-test-ok.png"))
                page.get_by_role("button", name="Enregistrer").click()
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                expect(page.get_by_role("button", name=re.compile("^Réfléchir"))).to_contain_text("clé enregistrée")
                page.reload()
                expect(page.locator("#ville")).to_be_visible()
                ouvrir(page, "Réfléchir")
                expect(page.locator("#det-llm")).to_contain_text("Clé enregistrée")
                verifier("après rechargement : l'adresse est gardée, la clé n'est plus affichée",
                         page.locator("#llm-url").input_value().endswith(f":{PORT_SERVICE}/v1") and page.locator("#llm-cle").count() == 0)
                dom = page.content()
                verifier("la clé n'est nulle part dans la page (DOM)", CLE_BONNE not in dom and CLE_FAUSSE not in dom)
                verifier("aucun champ de la page ne contient la clé", not page.evaluate(f"[...document.querySelectorAll('input')].some(i => i.value.includes('{CLE_BONNE}'))"))
                sans_null(page, "service enregistré")
                page.screenshot(path=str(CAPTURES / "07-assistant-enregistre.png"), full_page=True)
                # service injoignable : phrase du tableau §8 de l'API
                ouvrir(page, "Réfléchir")
                ouvrir(page, "Parler")
                page.get_by_role("radio", name="Autre").click()
                page.locator("#tts-url").fill("http://127.0.0.1:1/v1")
                page.get_by_role("button", name="Tester").click()
                expect(page.locator("#det-tts .res.err")).to_contain_text("Le service ne répond pas", timeout=20000)
                verifier("service injoignable : « Le service ne répond pas. Est-il lancé… »", "Est-il lancé" in page.locator("#det-tts .res.err").inner_text())
                page.screenshot(path=str(CAPTURES / "08-test-injoignable.png"))
            SANS_SERVICES = "l'appareil répond 501 « non_disponible » (services STT/LLM/TTS : pas dans ce firmware)"
            if REEL:
                sauter("Assistant : saisir un service LLM et « Tester »", SANS_SERVICES)
            else:
                etape("Assistant : saisir un service LLM et « Tester »", t_services, page)

            def t_secret():
                code, svc = api(page, "GET", "/api/services")
                verifier("GET /api/services : cle_definie vrai, aucun champ de clé", code == 200 and svc["llm"]["cle_definie"] is True and "cle" not in svc["llm"], json.dumps(svc["llm"]))
                avec_requete = [r for r in registre if CLE_BONNE in r["requete"] or CLE_FAUSSE in r["requete"]]
                verifier("contrôle du contrôle : la clé a bien voyagé dans des requêtes", len(avec_requete) >= 3, f"{len(avec_requete)} requêtes")
                fuites = [r["url"] for r in registre if CLE_BONNE in r["corps"] or CLE_FAUSSE in r["corps"] or "Incorrect API key" in r["corps"]]
                verifier(f"la clé n'apparaît dans AUCUNE des {len(registre)} réponses de l'API", not fuites, str(fuites[:3]))
                code, exp = api(page, "GET", "/api/settings/export")
                verifier("export : sans clé ni mot de passe", CLE_BONNE not in json.dumps(exp) and "mot_de_passe" not in json.dumps(exp) and "sk-" not in json.dumps(exp))
            if REEL:
                sauter("Secrets : la clé n'est jamais lisible", "aucune clé n'existe dans ce firmware (le contrôle sur les réponses reste couvert par les tests du noyau)")
            else:
                etape("Secrets : la clé n'est jamais lisible", t_secret, page)

            def t_export_import():
                with page.expect_download() as dl:
                    page.get_by_role("button", name="Sauvegarder mes réglages").click()
                chemin = CAPTURES / "reglages-exporte.json"
                dl.value.save_as(str(chemin))
                texte = chemin.read_text(encoding="utf-8")
                f = json.loads(texte)
                verifier("export téléchargé : format, ville et service LLM présents", f["format"] == "ecran-autonome-reglages" and f["zone"]["nom"] == "Lyon" and f["services"]["llm"]["url"].endswith("/v1"))
                verifier("export téléchargé : aucune clé (ni la bonne, ni la fausse, ni « sk- »)", not re.search(r"sk-|cle-de-test|mot_de_passe", texte) and "cle" not in f["services"]["llm"])
                verifier("export : indique les clés à ressaisir", f["cles_a_ressaisir"] == ["llm"], str(f["cles_a_ressaisir"]))
                # import : une luminosité modifiée + une clé glissée dans le fichier (doit être ignorée et signalée)
                f["affichage"]["luminosite"] = 30
                f["services"]["llm"]["cle"] = "sk-injectee-par-le-fichier"
                entree = page.locator('input[aria-label="Fichier de réglages"]')
                entree.set_input_files(files=[{"name": "r.json", "mimeType": "application/json", "buffer": json.dumps(f).encode()}])
                expect(page.locator("#appareil")).to_contain_text("Ce fichier changerait")
                expect(page.locator("#appareil")).to_contain_text("Les clés ne s’importent pas.")
                sans_null(page, "simulation d'import")
                page.screenshot(path=str(CAPTURES / "09-import-simulation.png"), full_page=False)
                page.get_by_role("button", name="Appliquer").click()
                expect(page.locator("#luminosite")).to_have_value("30", timeout=15000)
                _, svc = api(page, "GET", "/api/services")
                verifier("import : la luminosité du fichier est appliquée", True)
                verifier("import : la clé glissée dans le fichier n'est pas enregistrée ni renvoyée",
                         "sk-injectee" not in json.dumps(svc) and not any("sk-injectee" in r["corps"] for r in registre if "/api/settings/import" in r["url"]) and svc["llm"]["cle_definie"] is True)
            if REEL:
                sauter("Export et import des réglages sans clés", "l'appareil répond 501 « non_disponible »")
            else:
                etape("Export et import des réglages sans clés", t_export_import, page)

            depart_affichage = {}  # état de l'appareil réel avant le parcours, remis à la fin

            def t_ecran():
                if REEL:
                    depart_affichage.update({k: v for k, v in api(page, "GET", "/api/display")[1].items() if k in ("langue_ecran", "langue_assistant", "theme", "luminosite")})
                else:
                    ouvrir(page, "Réfléchir")  # referme
                page.get_by_role("radio", name="Sombre").click()
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, d = api(page, "GET", "/api/display")
                verifier("thème Sombre enregistré sur l'appareil", d["theme"] == "sombre", d["theme"])
                page.locator("#luminosite").fill("55")
                expect(page.locator("#toast")).to_contain_text("Enregistré")
                _, d = api(page, "GET", "/api/display")
                verifier("luminosité 55 % enregistrée", d["luminosite"] == 55, str(d["luminosite"]))
                page.locator("#ecran").scroll_into_view_if_needed()
                page.screenshot(path=str(CAPTURES / "10-ecran.png"))
                if REEL:  # remettre l'écran de l'appareil comme avant le parcours
                    api(page, "PUT", "/api/display", {k: depart_affichage[k] for k in ("theme", "luminosite") if k in depart_affichage})
            etape("Écran : thème et luminosité", t_ecran, page)

            def t_wifi():
                page.get_by_role("button", name="Changer de réseau").click()
                page.get_by_role("button", name=re.compile("^Voisin_5G")).click()
                page.locator("#wifi-mdp").fill("mauvais")
                page.get_by_role("button", name="Se connecter").click()
                expect(page.locator("#wifi .res.err")).to_contain_text("Mot de passe refusé", timeout=20000)
                page.screenshot(path=str(CAPTURES / "11-wifi-refuse.png"))
                _, w = api(page, "GET", "/api/wifi")
                verifier("Wi-Fi refusé : l'ancien réseau est conservé", w["ssid"] == "Maison", json.dumps(w))
                page.get_by_role("button", name="Changer de réseau").click()
                page.get_by_role("button", name=re.compile("^Café Libre")).click()
                page.get_by_role("button", name="Se connecter").click()
                expect(page.locator("#toast")).to_contain_text("Connecté à", timeout=20000)
                verifier("réseau ouvert : connexion réussie", api(page, "GET", "/api/wifi")[1]["ssid"] == "Café Libre")
            if REEL:
                sauter("Wi-Fi : mot de passe refusé puis réseau ouvert", "l'appareil répond 501 « non_disponible » (changer de réseau : pas dans ce firmware ; mode station seulement)")
            else:
                etape("Wi-Fi : mot de passe refusé puis réseau ouvert", t_wifi, page)

            def depot(contenu, nom="ecran.bin"):
                page.locator('input[aria-label="Fichier de mise à jour"]').set_input_files(files=[{"name": nom, "mimeType": "application/octet-stream", "buffer": contenu}])
                page.get_by_role("button", name="Envoyer à l’écran").click()

            def reappairer():
                expect(page.get_by_text(re.compile("a redémarré"))).to_be_visible(timeout=40000)
                page.locator("#code").fill(CODE)
                expect(page.locator("#appareil")).to_be_visible()

            def t_maj():
                page.locator("#appareil").scroll_into_view_if_needed()
                depot(b"MZ" + bytes(2000), "pas-un-firmware.bin")
                expect(page.locator("#appareil .res.err")).to_contain_text("n’est pas un firmware", timeout=15000)
                verifier("fichier qui n'est pas un firmware : refusé avec une phrase", True)
                depot(b"\xe9" + b"CASSEE" + bytes(4000), "cassee.bin")
                reappairer()
                expect(page.locator("#appareil .res.err")).to_contain_text("n’a pas démarré correctement", timeout=15000)
                verifier("image cassée : retour arrière automatique, version inchangée (0.1.0)", "Version 0.1.0" in page.locator("#appareil").inner_text())
                page.screenshot(path=str(CAPTURES / "12-retour-arriere-auto.png"))
                depot(b"\xe9" + bytes(4000), "bonne.bin")
                reappairer()
                expect(page.locator("#appareil .res.ok")).to_contain_text("Mise à jour réussie", timeout=15000)
                verifier("bonne image : nouvelle version 0.1.1", "Version 0.1.1" in page.locator("#appareil").inner_text())
                page.get_by_role("button", name="Revenir à la version 0.1.0").click()
                page.get_by_role("button", name="Revenir", exact=True).click()
                reappairer()
                expect(page.locator("#appareil")).to_contain_text("Version 0.1.0", timeout=15000)
                verifier("retour arrière demandé : version 0.1.0 remise", "Retour à la version 0.1.0" in page.locator("#appareil").inner_text())
                verifier("le jeton a disparu au redémarrage (la page a redemandé le code)", True)
            if REEL:
                sauter("Mise à jour par la page, avec retour arrière", "l'appareil répond 501 « non_disponible » (mise à jour par la page : pas dans ce firmware)")
            else:
                etape("Mise à jour par la page, avec retour arrière", t_maj, page)

            def t_verrou():
                brut = lambda c: page.request.post(BASE + "/api/pair", data=json.dumps({"code": c}), headers={"Content-Type": "application/json"})  # noqa: E731
                statuts = [brut("111111").status for _ in range(5)]
                verifier("appairage : 4 mauvais codes en 403, le 5e verrouille (429)", statuts == [403, 403, 403, 403, 429], str(statuts))
                r = brut(CODE)
                verifier("appairage verrouillé : même le bon code est refusé, avec une phrase", r.status == 429 and "Trop d" in r.json()["erreur"]["message"], r.text()[:120])
                time.sleep(VERROU_S + 0.7)
                verifier("appairage : le verrou se lève", brut(CODE).status == 200)
            etape("Appairage : verrou après 5 essais ratés", t_verrou, page)

            # ---- captures clair / sombre, plein cadre, et contrôle du débordement ----
            def t_themes():
                ctx2, p2 = nouveau_contexte(nav, sombre=True)
                appairer(p2)
                ouvrir(p2, "Réfléchir")
                p2.screenshot(path=str(CAPTURES / "13-sombre-assistant.png"), full_page=True)
                p2.locator("#ville").scroll_into_view_if_needed()
                ctx2.close()
                ctx3, p3 = nouveau_contexte(nav, sombre=False)
                appairer(p3)
                p3.screenshot(path=str(CAPTURES / "14-clair-complet.png"), full_page=True)
                ctx3.close()
                verifier("captures clair et sombre enregistrées", True)
            etape("Rendu clair et sombre", t_themes, page)

            def t_largeurs():
                for largeur in (360, 390, 430):
                    c, pg = nouveau_contexte(nav, largeur=largeur)
                    appairer(pg)
                    ouvrir(pg, "Réfléchir")
                    r = pg.evaluate(DEPASSEMENTS)
                    verifier(f"{largeur} px : pas de défilement horizontal (carte « Réfléchir » ouverte)", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                    c.close()
            etape("Largeurs de téléphone", t_largeurs)

            # ---- sept langues : sélecteurs distincts, fichiers chargés à la demande, repli, attribution ----
            def t_langues():
                src = (RACINE / "components/core/include/ecran/meteo_wmo.hpp").read_text(encoding="utf-8")
                libelles_fr = re.findall(r'return \{"([^"]+)", ', src)
                ids = list(lire_yaml(RACINE / "i18n" / "page.yaml"))
                fr = catalogue("fr")
                cle_code_faux = "Ce code n’est pas le bon. Regardez l’écran de l’appareil."
                c4, p4 = nouveau_contexte(nav, largeur=360)
                depart_langues = {}  # langues de l'appareil avant le parcours (remises à la fin sur l'appareil réel)
                fichiers_vus = []
                p4.on("request", lambda r: fichiers_vus.append(r.url.rsplit("/", 1)[-1]) if "/lang/" in r.url else None)
                try:
                    appairer(p4)
                    d = api(p4, "GET", "/api/display")[1]
                    noms = [x["nom"] for x in d["langues"]]
                    verifier("langues : l'appareil propose sept langues dans l'ordre figé (fr en de nl es it tr)", [x["code"] for x in d["langues"]] == CODES_LANGUES, str(d["langues"]))
                    verifier("langues : chaque langue porte son nom dans sa propre langue", noms == ["Français", "English", "Deutsch", "Nederlands", "Español", "Italiano", "Türkçe"], str(noms))
                    depart_langues = {k: d[k] for k in ("langue_ecran", "langue_assistant")}
                    if not REEL:  # sur l'appareil réel, les langues sont celles que la personne a choisies
                        verifier("langues : français par défaut, pour l'écran comme pour l'assistant", d["langue_ecran"] == "fr" and d["langue_assistant"] == "fr", f"{d['langue_ecran']} / {d['langue_assistant']}")
                    opt_ecran = p4.evaluate("[...document.querySelectorAll('#langue-ecran option')].map(o => [o.value, o.textContent])")
                    opt_assistant = p4.evaluate("[...document.querySelectorAll('#langue-assistant option')].map(o => o.value)")
                    verifier("page : deux sélecteurs distincts (écran, assistant), sept options chacun", opt_ecran == [[x["code"], x["nom"]] for x in d["langues"]] and opt_assistant == CODES_LANGUES, f"{opt_ecran} / {opt_assistant}")
                    verifier("chargement à la demande : au départ, seul fr.json est demandé", set(fichiers_vus) == {"fr.json"}, str(fichiers_vus))
                    verifier_attribution(p4, "fr", "fr")
                    p4.locator("#attribution").scroll_into_view_if_needed()
                    p4.screenshot(path=str(CAPTURES / "15-attribution-fr.png"))

                    # deux réglages distincts : changer l'un ne change pas l'autre
                    p4.locator("#langue-assistant").select_option("tr")
                    expect(p4.locator("#toast")).to_contain_text("Enregistré")
                    d = api(p4, "GET", "/api/display")[1]
                    verifier("langue de l'assistant = turc ; langue de l'écran inchangée (fr)", d["langue_assistant"] == "tr" and d["langue_ecran"] == "fr", f"{d['langue_assistant']} / {d['langue_ecran']}")
                    verifier("la page reste en français quand on change la langue de l'assistant", p4.locator("html").get_attribute("lang") == "fr")
                    p4.locator("#langue-ecran").select_option("de")
                    expect(p4.locator("html")).to_have_attribute("lang", "de")
                    de = catalogue("de")
                    expect(p4.locator("#toast")).to_contain_text(de["toast.enregistre"])
                    d = api(p4, "GET", "/api/display")[1]
                    verifier("langue de l'écran = allemand ; langue de l'assistant (turc) inchangée", d["langue_ecran"] == "de" and d["langue_assistant"] == "tr", f"{d['langue_ecran']} / {d['langue_assistant']}")
                    verifier("la page passe à l'allemand tout de suite : titre de carte, étiquette du sélecteur, titre de l'onglet",
                             p4.locator("#t-ville").inner_text() == de["ville.titre"] and norm(p4.locator("label[for=langue-ecran]").inner_text()) == norm(de["ecran.langue_ecran"]) and p4.title() == de["app.titre"],
                             f"{p4.locator('#t-ville').inner_text()!r} / {p4.title()!r}")
                    verifier("chargement à la demande : fr.json puis de.json seulement (pas les cinq autres)", set(fichiers_vus) == {"fr.json", "de.json"}, str(fichiers_vus))
                    verifier("le sélecteur de la langue de l'assistant a gardé le turc", p4.locator("#langue-assistant").input_value() == "tr")
                    p4.locator("#ecran").scroll_into_view_if_needed()
                    p4.screenshot(path=str(CAPTURES / "16-ecran-allemand.png"))

                    # les sept langues, une par une : page, identifiants, français résiduel, attribution, météo, largeur
                    for code in CODES_LANGUES:
                        api(p4, "PUT", "/api/display", {"langue_ecran": code})
                        p4.reload()
                        expect(p4.locator("#ville")).to_be_visible()
                        expect(p4.locator("html")).to_have_attribute("lang", code)
                        cat = catalogue(code)
                        sections = ["ville", "assistant", "ecran", "wifi", "appareil"]
                        liens = [norm(x) for x in p4.locator("nav.ancres a").all_inner_texts()]
                        verifier(f"{code} : onglets, titre de carte et titre de l'onglet du navigateur dans la langue",
                                 liens == [norm(cat["nav." + s]) for s in sections] and p4.locator("#t-ville").inner_text() == cat["ville.titre"] and p4.title() == cat["app.titre"], str(liens))
                        texte = p4.evaluate("document.getElementById('racine').textContent")  # y compris les cartes repliées
                        bruts = [i for i in ids if i in texte]
                        verifier(f"{code} : aucun identifiant brut (« ville.titre ») affiché", not bruts, str(bruts[:4]))
                        if code != "fr":
                            propres_au_francais = [v for k, v in fr.items() if "{" not in v and len(v) >= 6 and v != cat.get(k)]
                            restes = sorted({v for v in propres_au_francais if v in texte})
                            verifier(f"{code} : aucun texte français visible dans la page", not restes, str(restes[:4]))
                        verifier_attribution(p4, code, code)
                        expect(p4.locator(".hero .temp b")).to_have_text(re.compile(r"-?\d+\s°C"), timeout=20000)
                        etiquette = p4.locator(".hero .temp small").inner_text()
                        traduits = lire_yaml(RACINE / "i18n" / f"{code}.yaml")
                        verifier(f"{code} : le libellé météo est un libellé de la table WMO du noyau, dans la langue", etiquette in {traduits.get(fr_l, fr_l) for fr_l in libelles_fr}, etiquette)
                        p4.locator("button[aria-expanded]").nth(1).click()
                        r = p4.evaluate(DEPASSEMENTS)
                        verifier(f"{code} : 360 px, pas de défilement horizontal (carte « LLM » ouverte)", r["defilement"] <= r["largeur"] and not r["sortis"], json.dumps(r))
                        if code in ("nl", "tr"):
                            p4.screenshot(path=str(CAPTURES / f"17-page-{code}.png"), full_page=True)

                    # une phrase qui vient de l'appareil (pas de la page) arrive dans la langue de l'écran
                    api(p4, "PUT", "/api/display", {"langue_ecran": "de"})
                    c5, p5 = nouveau_contexte(nav)
                    p5.goto(BASE)
                    de_yaml = lire_yaml(RACINE / "i18n" / "de.yaml")
                    expect(p5.locator("#code")).to_be_visible()
                    verifier("appairage (page non connectée) : l'invite est en allemand, d'après /api/status", norm(p5.locator(".appairage > p").inner_text()) == norm(de["appairage.invite"]), p5.locator(".appairage > p").inner_text())
                    p5.locator("#code").fill("000000")
                    expect(p5.locator(".appairage .res")).to_be_visible()
                    verifier("code faux : la phrase envoyée par l'appareil est affichée en allemand, telle quelle",
                             norm(p5.locator(".appairage .res").inner_text()) == norm(de_yaml[cle_code_faux]), p5.locator(".appairage .res").inner_text())
                    p5.locator("#code").fill(CODE)  # le bon code remet le compteur d'essais à zéro
                    expect(p5.locator("#ville")).to_be_visible()
                    c5.close()

                    # repli : fichier de la langue absent -> anglais ; anglais absent aussi -> français
                    for absentes, attendu in ((["de"], "en"), (["de", "en"], "fr")):
                        c6, p6 = nouveau_contexte(nav)
                        for absente in absentes:
                            p6.route(f"**/lang/{absente}.json", lambda r: r.fulfill(status=404, content_type="text/plain", body="absent"))
                        appairer(p6)
                        expect(p6.locator("html")).to_have_attribute("lang", attendu)
                        cat = catalogue(attendu)
                        verifier(f"repli : {' et '.join(a + '.json' for a in absentes)} introuvable (écran en allemand) -> la page s'affiche en {attendu}",
                                 p6.locator("#t-ville").inner_text() == cat["ville.titre"] and p6.title() == cat["app.titre"], p6.locator("#t-ville").inner_text())
                        verifier_attribution(p6, attendu, f"repli {attendu}")
                        c6.close()
                finally:
                    try:  # remettre l'appareil dans son état de départ (français partout pour le faux appareil)
                        api(p4, "PUT", "/api/display", depart_langues if REEL else {"langue_ecran": "fr", "langue_assistant": "fr"})
                    except Exception:  # noqa: BLE001
                        pass
                    c4.close()
            etape("Sept langues : sélecteurs, chargement à la demande, repli, attribution", t_langues)

            ctx.close()
            nav.close()

        # ---- poids ----
        print("\n== Poids de la page + un fichier de langue (servis en gzip, comme sur l'appareil)")
        fichiers = ("index.html", "style.css", "app.js", "fr.json")
        brut = {f: (RACINE / "web" / "app" / ("lang" if f.endswith(".json") else ".") / f).stat().st_size for f in fichiers}
        gz = {f: next((e[1] for e in poids.get("res", []) if e[0] == f), None) for f in fichiers[1:]}
        gz["index.html"] = poids["nav"][0]
        total_gz = sum(v for v in gz.values() if v)
        for f in fichiers:
            print(f"  {f:11} {brut[f]:>7} octets bruts -> {gz[f]:>6} octets gzip")
        print(f"  TOTAL       {sum(brut.values()):>7} octets bruts -> {total_gz:>6} octets gzip (budget {BUDGET_GZ})")
        verifier(f"poids gzip : page + fr.json ({total_gz} octets) <= budget de {BUDGET_GZ} octets", all(gz.values()) and total_gz <= BUDGET_GZ, str(gz))
        # les six autres langues, compressées ici avec le même niveau 9 : la page + la plus lourde doit tenir aussi
        langues_gz = {c: len(gzip.compress((LANGUES_PAGE / f"{c}.json").read_bytes(), 9)) for c in CODES_LANGUES}
        page_gz = total_gz - (gz["fr.json"] or 0)
        lourde = max(langues_gz, key=lambda c: langues_gz[c])
        print("  fichiers de langue (gzip niveau 9) : " + ", ".join(f"{c} {n}" for c, n in langues_gz.items()))
        verifier(f"poids gzip : page + la langue la plus lourde ({lourde}, {page_gz + langues_gz[lourde]} octets) <= budget de {BUDGET_GZ} octets", page_gz + langues_gz[lourde] <= BUDGET_GZ)
        (CAPTURES / "poids.json").write_text(json.dumps({"brut": brut, "gzip": gz, "total_gzip": total_gz, "budget_gzip": BUDGET_GZ, "langues_gzip": langues_gz}, indent=2), encoding="utf-8")
    finally:
        if srv is not None:
            srv.terminate()
            try:
                srv.wait(timeout=5)
            except subprocess.TimeoutExpired:
                srv.kill()

    verifier("la page ne parle qu'à l'appareil (aucune police, image, script ou CDN externe)", hotes == {urllib.parse.urlsplit(BASE).netloc}, str(sorted(hotes)))
    graves = [m for m in erreurs_console if "Failed to load resource" not in m]
    verifier("console du navigateur : aucune erreur JavaScript", not graves, "; ".join(graves[:3]))
    echecs = [r for r in resultats if not r[1]]
    print(f"\nRÉSULTAT : {len(resultats) - len(echecs)}/{len(resultats)} vérifications OK" + (f", {len(sautes)} étape(s) sautée(s)" if sautes else ""))
    for titre, raison in sautes:
        print(f"  SAUTÉE : {titre}  [{raison}]")
    for nom, _, detail in echecs:
        print(f"  ÉCHEC : {nom}  [{detail}]")
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
